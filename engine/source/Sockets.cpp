#include "sockets.h"

#include "Utilities.h" // just for clock functions for simulated lag


#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <queue>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")


bool UDPServerSocket::open(int port) {
	std::lock_guard<std::recursive_mutex> guard(lock);
	if (socket) return false; // already open

	WSADATA wsaData;
	auto version = MAKEWORD(2, 2);//using winsock 2.2
	if (WSAStartup(version, &wsaData) != 0) {
		printf("WSAStartup failed: %d\n", WSAGetLastError());
		return false;
	}

	SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET) {
		printf("Failed to create socket: %d\n", WSAGetLastError());
		WSACleanup();
		return false;
	}

	BOOL exclusive = TRUE;
	if (setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
		(const char*)&exclusive, sizeof(exclusive)) == SOCKET_ERROR) {
		printf("Failed to set SO_EXCLUSIVEADDRUSE: %d\n", WSAGetLastError());
		closesocket(s);
		WSACleanup();
		return false;
	}

	sockaddr_in service;
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = INADDR_ANY;
	service.sin_port = htons(port);

	if (bind(s, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
		printf("Bind failed: %d\n", WSAGetLastError());
		closesocket(s);
		WSACleanup();
		return false;
	}

	socket = std::shared_ptr<SOCKET>(new SOCKET(s),
		[](SOCKET* sock) {
			closesocket(*sock);
			delete sock;
			WSACleanup();
		});

	this->port = port;
	stop = false;
	next_client_id = 1;

	thread = std::thread(&UDPServerSocket::run, this);
	return true;
}

bool UDPServerSocket::send(int receiver_id, const std::vector<char>& data) {
	// Queue data for sending asynchronously
	lock.lock();
	auto it = clients.find(receiver_id);
	if (it == clients.end()) {
		lock.unlock();
		return false;
	}
	send_queue.push({ data, it->second });
	lock.unlock();
	return true;
}

void UDPServerSocket::close() {
	stop = true;
	if (thread.joinable()) thread.join();

	if (socket && *socket != INVALID_SOCKET) {
		shutdown(*socket, SD_BOTH) ; // stops any pending recv calls
		closesocket(*socket);
		socket.reset();
	}
	if (packet_receiver) {
		for (auto& kv : clients) {
			packet_receiver->onSocketClose(kv.first);
		}
	}
	clients.clear();
}

void UDPServerSocket::setPacketReceiver(PacketReceiver* receiver) {
	packet_receiver = receiver;
}

UDPServerSocket::~UDPServerSocket(){
	close();
}

void UDPServerSocket::run() {
	while (!stop) {
		//printf("server run loop\n");
		SOCKET sock = *socket;

		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(sock, &readfds);

		timeval tv{};
		tv.tv_sec = 0;
		tv.tv_usec = packet_wait_micros; 

		int activity = select(0, &readfds, nullptr, nullptr, &tv);
		if (activity == SOCKET_ERROR) continue;

		// 1. Handle incoming data
		if (activity > 0 && FD_ISSET(sock, &readfds)) {
			//printf("got server activity\n");
			sockaddr_in client_addr{};
			int addr_len = sizeof(client_addr);
			int bytes = recvfrom(sock, buffer, sizeof(buffer), 0,
				(sockaddr*)&client_addr, &addr_len);
			if (bytes > 0) {
				int client_id = -1;

				lock.lock();
				//printf("got past server activity lock\n");
				for (auto& kv : clients) {
					if (memcmp(kv.second.get(), &client_addr, sizeof(client_addr)) == 0) {
						client_id = kv.first;
						break;
					}
				}
				if (client_id == -1) {
					client_id = ++next_client_id;
					clients[client_id] = std::make_shared<sockaddr_in>(client_addr);
					if (packet_receiver){
						packet_receiver->onSocketConnect(client_id);
					}
				}
				lock.unlock();

				if (packet_receiver) {
					std::vector<char> data(buffer, buffer + bytes);
					packet_receiver->receivePacket(client_id, data);
				}
			}
		}

		// 2. Flush send queue
		lock.lock();
		while (!send_queue.empty()) {
			PendingPacket pkt = send_queue.front();
			send_queue.pop();
			lock.unlock(); // unlock while sending to avoid blocking other threads

			sendto(sock, pkt.data.data(), (int)pkt.data.size(), 0,
				(sockaddr*)pkt.addr.get(), sizeof(sockaddr_in));

			lock.lock();
		}
		lock.unlock();
	}
}

bool UDPClientSocket::connect(const std::string& ip, int port_) {
	
	auto version = MAKEWORD(2, 2);//using winsock 2.2
	WSADATA wsaData;
	if (WSAStartup(version, &wsaData) != 0) {
		printf("WSAStartup failed: %d\n", WSAGetLastError());
		return false;
	}

	lock.lock();
	stop = false;
	server_ip = ip;
	port = port_;

	socket_fd = std::make_shared<SOCKET>(::socket(AF_INET, SOCK_DGRAM, 0));
	if (*socket_fd == INVALID_SOCKET) {
		std::cerr << "socket() failed: " << WSAGetLastError() << "\n";
		lock.unlock();
		return false;
	}

	server_address = std::make_shared<sockaddr_in>();
	server_address->sin_family = AF_INET;
	inet_pton(AF_INET, server_ip.c_str(), &server_address->sin_addr);
	server_address->sin_port = htons(port);

	// UDP "connect" just sets default destination address
	if (::connect(*socket_fd, (sockaddr*)server_address.get(), sizeof(sockaddr_in)) == SOCKET_ERROR) {
		printf("UDP connect failed: %d\n", WSAGetLastError());
		closesocket(*socket_fd);
		WSACleanup();
		lock.unlock();
		return false;
	}

	thread = std::thread(&UDPClientSocket::run, this);
	if (packet_receiver){
		packet_receiver->onSocketConnect(server_id);
	}
	lock.unlock();
	return true;
}

bool UDPClientSocket::send(int, const std::vector<char>& data) {
	//printf("client send called\n");
	lock.lock();
	send_queue.push({ data, server_address });
	lock.unlock();
	return true;
}

void UDPClientSocket::close() {
	stop = true;

	lock.lock();
	if (socket_fd && *socket_fd != INVALID_SOCKET) {
		closesocket(*socket_fd);
		socket_fd.reset();
	}
	if (packet_receiver){
		packet_receiver->onSocketClose(server_id);
	}
	lock.unlock();
}

void UDPClientSocket::setPacketReceiver(PacketReceiver* receiver) {
	packet_receiver = receiver;
}

UDPClientSocket::~UDPClientSocket() {
	close();
}

void UDPClientSocket::run() {
	while (!stop) {
		//printf("client run loop\n");
		SOCKET sock = *socket_fd;
	

		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(sock, &readfds);

		timeval tv{};
		tv.tv_sec = 0;
		tv.tv_usec = packet_wait_micros;

		int activity = select(0, &readfds, nullptr, nullptr, &tv);
		if (activity == SOCKET_ERROR) continue;

		// 1. Handle incoming data
		if (activity > 0 && FD_ISSET(sock, &readfds)) {
			sockaddr_in from{};
			int len = sizeof(from);
			int bytes = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&from, &len);
			if (bytes > 0 && packet_receiver) {
				std::vector<char> data(buffer, buffer + bytes);
				packet_receiver->receivePacket(server_id, data);
			}
		}

		// 2. Flush send queue
		lock.lock();
		while (!send_queue.empty()) {
			PendingPacket pkt = send_queue.front();
			send_queue.pop();
			lock.unlock();
			SOCKET s = *socket_fd;
			int result = ::send(s, pkt.data.data(), (int)pkt.data.size(), 0); // <--- NO sendto needed if connected
			if (result == SOCKET_ERROR) {
				printf("send failed: %d\n", WSAGetLastError());
			}

			lock.lock();
		}
		lock.unlock();
	}
}


//Loops to make sure all of the bytes go through on a send (for TCP only)
bool Socket::sendAll(SOCKET socket, const char* source, int num_bytes){
	int total = 0;
	while (total < num_bytes) {
		int sent = ::send(socket, source + total, num_bytes - total, 0);
		if (sent == SOCKET_ERROR) {
			// real socket error – log and give up
			//printf("send() error %d (bytes left %d)\n",
			//	WSAGetLastError(), num_bytes - total);
			return false;
		}
		if (sent == 0) {
			// connection closed while we were trying to send
			return false;
		}
		total += sent;
	}
	return true;

}

//Loops to make sure all of the bytes get read on a recv (for TCP only)
bool Socket:: recvAll(SOCKET socket, char* destination, int num_bytes){
	int total = 0;
	while (total < num_bytes) {
		int rc = ::recv(socket, destination + total, num_bytes- total, 0);
		if (rc == 0) { // orderly shutdown
			return false;
		}
		if (rc == SOCKET_ERROR) {
			// an actual error (WSAEWOULDBLOCK etc.)
			return false;
		}
		total += rc;
	}
	return true;
}

TCPServerSocket::Client::Client(int my_id, const SOCKET& my_socket, TCPServerSocket* my_parent){
	id = my_id ;
	socket = my_socket ;
	parent = my_parent ;
	thread = std::thread(&TCPServerSocket::Client::run, this);
	send_thread = std::thread(&TCPServerSocket::Client::runSend, this);
}

TCPServerSocket::Client::Client(){ // default constructor so it can be in a map
	socket = INVALID_SOCKET ;
}

bool TCPServerSocket::Client::send(const std::vector<char>& data){
	int32_t size = (uint32_t)data.size() ;
	//printf("Server sent packet of size: %d\n", (int)size) ;
	bool sent_length = sendAll(socket, (char*)&size, 4); // first send the size
	bool sent = sendAll(socket, data.data(), (int)data.size());
	return sent && sent_length;
}

void TCPServerSocket::Client::queueSend(const std::vector<char>& data){
	lock.lock();
	send_queue.push(std::make_shared<std::vector<char>>(data)) ;
	lock.unlock();
	
	cv.notify_all();
	//send(data) ;
}



// runs the thread for waiting for packet for a specific client
void TCPServerSocket::Client::run(){
	while (!stop && socket != INVALID_SOCKET) {
		if (packet_size < 0) { // need a size
			int bytes = recvAll(socket, buffer, 4); // just get bytes for size
			if (bytes > 0) {
				packet_size = *(int32_t*)(buffer);
				packet.clear();
				//printf("Server got packet size of %d\n", (int)packet_size) ;
			}
			else {
				socket = INVALID_SOCKET;
				stop = true;
			}
		}
		else {
			int max_bytes = packet_size - (int)packet.size();
			max_bytes = max_bytes > sizeof(buffer) ? sizeof(buffer) : max_bytes;
			int bytes = recv(socket, buffer, max_bytes, 0);
			if (bytes > 0) {
				//printf("Got %d bytes\n", bytes);
				packet.insert(packet.end(), buffer, buffer + bytes);
				if (packet.size() == packet_size) {
					if (parent->packet_receiver) {
						//printf("server got entire packet of size : %d\n", (int)packet_size);
						parent->packet_receiver->receivePacket(id, packet);
					}
					packet_size = -1;
				}
			}
			else {
				socket = INVALID_SOCKET;
				stop = true;
			}
		}
		
	}
}

void TCPServerSocket::Client::runSend() {
	while (true) {
		std::unique_lock<std::mutex> lk(signal_lock); // this locks the mutex when it creates
		cv.wait(lk, [&] { return !send_queue.empty() || stop || socket == INVALID_SOCKET; });// but this immediately unlocks and then only locks it when checking
		if(stop || socket == INVALID_SOCKET){
			return ;
		}
		lock.lock(); // but we do need it locked when popping and iterting
		std::shared_ptr<std::vector<char>> packet = send_queue.front();
		send_queue.pop();
		lock.unlock(); // unlock while actually doin the send
		send(*packet.get());
	}
}



TCPServerSocket::TCPServerSocket() {
	socket = INVALID_SOCKET;
}
	
//Open a TCP socket on the given port
// Returns if apparently successful
bool TCPServerSocket::open(int port){
	lock.lock();
	auto version = MAKEWORD(2, 2);//using winsock 2.2
	WSADATA wsaData;
	if (WSAStartup(version, &wsaData) != 0) {
		printf("WSAStartup failed: %d\n", WSAGetLastError());
		lock.unlock();
		return false;
	}

	socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (socket == INVALID_SOCKET) {
		printf("Failed to create socket: %d\n", WSAGetLastError());
		WSACleanup();
		lock.unlock();
		return false;
	}

	sockaddr_in service{};
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = INADDR_ANY;
	service.sin_port = htons(port);

	if (bind(socket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
		printf("Bind failed: %d\n", WSAGetLastError());
		closesocket(socket);
		WSACleanup();
		socket = INVALID_SOCKET;
		lock.unlock();
		return false;
	}

	if (listen(socket, SOMAXCONN) == SOCKET_ERROR) {
		printf("Listen failed: %d\n", WSAGetLastError());
		closesocket(socket);
		WSACleanup();
		socket = INVALID_SOCKET;
		lock.unlock();
		return false;
	}

	stop = false;
	next_client_id = 1;
	lock.unlock();
	thread = std::thread(&TCPServerSocket::run, this);
	
	return true; 
}

// Send data through this socket, nonblocking
// Returns if data seems to have sent (we can't know now if it arrived)
bool TCPServerSocket::send(int receiver_id, const std::vector<char>& data){
	//printf("Server send to %d of size %d \n", receiver_id, (int)data.size()) ;
	// Queue data for sending asynchronously
	lock.lock();
	auto it = clients.find(receiver_id);
	if (it == clients.end()) {
		lock.unlock();
		return false;
	}
	std::shared_ptr<Client> client = it->second ;
	lock.unlock();
	client->queueSend(data);
	
	return true ;
}

// close all connections in this socket and stop allowing new ones if applicable
void TCPServerSocket::close(){
	lock.lock();
	stop = true ;
	if(socket != INVALID_SOCKET){
		closesocket(socket);
		socket = INVALID_SOCKET ;
	}
	for(auto& [id, client] : clients){
		client->stop = true ;
		closesocket(client->socket);
		client->socket = INVALID_SOCKET;
		if (packet_receiver) {
			packet_receiver->onSocketClose(id);
		}
		if(client->thread.joinable()){
			client->thread.join();
		}
		if (client->send_thread.joinable()) {
			client->cv.notify_all();
			client->send_thread.join();
		}
	}
	if (thread.joinable()) {
		thread.join();
	}
	clients.clear();
	lock.unlock();
}

//Set the object that will get packets when they are sent to this socket
void TCPServerSocket::setPacketReceiver(PacketReceiver* receiver){
	packet_receiver = receiver ;
}

TCPServerSocket::~TCPServerSocket(){
	close();
	
}

// asynchronous method to catch new connections
void TCPServerSocket::run(){
	while (!stop) {
		//wait for new connection to accept
		SOCKET client_socket  = accept(socket, nullptr, nullptr);
		if (client_socket == INVALID_SOCKET) {
			if (stop) break;
			printf("Server socket accept failed: %d\n", WSAGetLastError() );
			continue;
		}
		
		lock.lock();
		clients[next_client_id] =  std::shared_ptr<Client>(new Client(next_client_id, client_socket, this));
		lock.unlock();

		if (packet_receiver) {
			packet_receiver->onSocketConnect(next_client_id);
		}
		next_client_id++;
	}
}

TCPClientSocket::TCPClientSocket(){
	socket = INVALID_SOCKET ;
}

//Open a TCP socket on the given port connecting to a TCPServerSocket at the address
// Returns if successful
bool TCPClientSocket::connect(const std::string& host, int port){
	lock.lock();
	server_id++;
	auto version = MAKEWORD(2, 2);//using winsock 2.2
	WSADATA wsaData;
	if (WSAStartup(version, &wsaData) != 0) {
		printf("WSAStartup failed: %d\n", WSAGetLastError());
		lock.unlock();
		return false;
	}

	socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (socket == INVALID_SOCKET) {
		printf("Failed to create client socket: %d \n", WSAGetLastError());
		WSACleanup();
		lock.unlock();
		return false;
	}

	sockaddr_in server_addr{};
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr);

	if (::connect(socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
		printf("Client connect failed: %d\n", WSAGetLastError()) ;
		closesocket(socket);
		WSACleanup();
		socket = INVALID_SOCKET;
		lock.unlock();
		return false;
	}

	stop = false;
	if (packet_receiver) {
		// Client always uses sender_id = 0 for server
		packet_receiver->onSocketConnect(0);
	}
	lock.unlock();
	thread = std::thread(&TCPClientSocket::run, this);
	
	return true;

}

// Send data through this socket
// Returns if data seems to have sent (we can't know now if it arrived)
bool TCPClientSocket::send(int receiver_id, const std::vector<char>& data){
	lock.lock();
	if (socket == INVALID_SOCKET){
		lock.unlock();
		return false;
	}
	int32_t size = (int32_t)data.size();
	bool sent_length = sendAll(socket, (char*)&size, 4); // first send the size
	//printf("Client sent packet of size %d\n", (int)size) ;
	bool sent = sendAll(socket, data.data(), (int)data.size()); // then send the data
	lock.unlock();
	return sent_length && sent ;
}

// close all connections in this socket and stop allowing new ones if applicable
void TCPClientSocket::close(){
	stop = true ;
	if(socket != INVALID_SOCKET){
		shutdown(socket, SD_BOTH) ;
		closesocket(socket);
	}
	if (packet_receiver != nullptr) {
		packet_receiver->onSocketClose(server_id);
	}

	if (thread.joinable()) {
		thread.join();
	}
}

//Set the object that will get packets when they are sent to this socket
void TCPClientSocket::setPacketReceiver(PacketReceiver* receiver){
	packet_receiver = receiver;
}

TCPClientSocket::~TCPClientSocket(){
	close();
}

// asynchronous method to catch packets and redirect them to the packet receiver
void TCPClientSocket::run() {
	while(!stop && socket != INVALID_SOCKET){
		if (packet_size < 0) { // need a size
			bool read_length = recvAll(socket, buffer, 4); // get bytes for size
			if(read_length){
				packet_size = *(int32_t*)(buffer) ;
				packet.clear();
				//printf("client got size : %d\n", (int)packet_size) ;
			}else{
				socket = INVALID_SOCKET ;
				stop = true ;
			}
		}else{
			int max_bytes = packet_size - (int)packet.size();
			max_bytes = max_bytes > sizeof(buffer) ? sizeof(buffer) : max_bytes ;
			int bytes = recv(socket, buffer, max_bytes, 0);
			if (bytes > 0) {
				//printf("Got %d bytes\n", bytes) ;
				packet.insert(packet.end(),buffer, buffer + bytes) ;
				if(packet.size() == packet_size){
					if (packet_receiver) {
						//printf("client got entire packet of size : %d\n", (int)packet_size);
						packet_receiver->receivePacket(0, packet);
					}
					packet_size = -1 ;
				}
			}else{
				socket = INVALID_SOCKET;
				stop = true;
			}
		}
	}
}


SlowPacketReceiver::SlowPacketReceiver(PacketReceiver* r, int micros_of_delay, int micros_of_jitter){
	printf("Constructing slow packet receiver with %d micros seconds of simulated delay\n", micros_of_delay) ;
	receiver = r ;
	lag_micros = micros_of_delay ;
	jitter_micros = micros_of_jitter ;
	next_jitter = (int)((randomFloat()-0.5f) * jitter_micros) ;
	thread = std::thread(&SlowPacketReceiver::run, this);
}

//Called when a packet is received
void SlowPacketReceiver::receivePacket(int sender_id, const std::vector<char>& data){
	if(data.size() == 0){
		printf("Got packet of size zero?\n");
	}
	lock.lock();
	packets.push(std::shared_ptr<SlowPacket>(new SlowPacket(sender_id,now(), data)));
	lock.unlock();
}

//Called when a connection is established, 
//sender_id is generated by the socket and can be used to identify recieved packet sources or send data back through the socket
void SlowPacketReceiver::onSocketConnect(int sender_id){
	receiver->onSocketConnect(sender_id);
}

//Called when a connection is closed, either remotely or because the socket holding it was closed
void  SlowPacketReceiver::onSocketClose(int sender_id){
	printf("slow packet reciever socket close\n");
	stop();
}

//runs the thread to deliver the packet withthe delay
void SlowPacketReceiver::run(){
	while(!stopped){
		//printf("slow packet running\n");
		lock.lock();
		if(packets.empty()){
			lock.unlock();
			std::this_thread::sleep_for(std::chrono::microseconds(lag_micros/10));
		}else{
			std::shared_ptr<SlowPacket> packet = packets.front();
			int micros =  microsBetween(packet->arrival_time, now()) ;
			if(micros > lag_micros + next_jitter){
				packets.pop();
				lock.unlock();
				receiver->receivePacket(packet->sender, packet->data) ;
				next_jitter = (int)((randomFloat() - 0.5f) * jitter_micros);
				//printf("%d\n", next_jitter) ;
			}else{
				lock.unlock();
				std::this_thread::sleep_for(std::chrono::microseconds(std::min<int>(micros, lag_micros / 10)));
			}
		}
		
	}
	printf("slow packet receiver stopping\n");
}

void SlowPacketReceiver::stop(){
	printf("slow packet reciever stopped\n"); 
	stopped = true ;
	if(thread.joinable()){
		thread.join();
	}

}