#include "WorldPlugin.h"
#include "Utilities.h"
#include "SteamworksPlugin.h"



WorldPlugin::WorldPlugin(){
	if (Timeline::COPY_PACKET == -1) {
		Timeline::COPY_PACKET = registry->registerClass<Timeline::CopyPacket>("CopyPacket");
		Timeline::UPDATE_PACKET = registry->registerClass<Timeline::UpdatePacket>("UpdatePacket");
	}
}

// Called on every plug-in before any plug-ins are run
void WorldPlugin::initialize(){
}


//Runs the plugin on it's own thread at the same time as all others
void WorldPlugin::run(){
	

	// only hold the lock to copy the packet references and clean the lists, so the sockets can continue
	std::lock(lock, packet_lock);
	std::vector<std::shared_ptr<Timeline::CopyPacket>> copies = copy_packets ;
	copy_packets.clear();
	for(auto& [socket_id, connection] : connections){
		connection.update_packets = connection.update_packet_queue ;
		connection.update_packet_queue.clear();
	}
	lock.unlock();
	packet_lock.unlock();

	//First apply any full world copies we have pending
	for (int k = 0 ;k < copies.size(); k++){
		std::shared_ptr<Timeline::CopyPacket>& copy = copies[k] ;
		std::string& name = copy->extra_string_data[NAME_EXTRA_INDEX];
		World& world = worlds[name] ;
		world.timeline = std::shared_ptr<Timeline>(new Timeline(registry, *copy));
		world.name = name ;
		world.pending_local_events.clear();
		world.time_speed = (float)copy->extra_double_data[TIME_SPEED_EXTRA_INDEX] ;
		world.vantage_point = copy->last_vantage;
		world.current_time = copy->last_vantage_time ;
		world.next_event_time = world.current_time + world.timeline->min_event_duration ;
		world.system_time_of_current_time = now();
		world.max_read_distance = copy->max_read_distance ;
	}
	copies.clear();

	double round_trip_start_time = -1 ;
	std::map<std::string, double> latest_update_time;

	// Apply all the updates we got from packets
	lock.lock();
	for (auto& [socket_id, connection] : connections) {
		for(int k=0;k<connection.update_packets.size();k++){
			std::shared_ptr<Timeline::UpdatePacket> update = std::static_pointer_cast<Timeline::UpdatePacket>(registry->deserializeObj(Timeline::UPDATE_PACKET, *connection.update_packets[k]));
			
			std::string& world_name = update->extra_string_data[NAME_EXTRA_INDEX] ;
			if(worlds.find(world_name) != worlds.end()){ // it might be possible to get packet after disconnecting and clearing the world 
				World& world = worlds[world_name];
				if (WorldPlugin::log_type == WorldPlugin::INJECTED_EXTENDED) {
					world.timeline->logUpdate(*update,"gotInpacket");
				}
				world.timeline->applyUpdate(*update);
				connection.last_absolute_time[world_name] = update->extra_double_data[LAST_RECEIVE_TIME_EXTRA_INDEX]; // clients send their last absolute time
				// save data to adjust clock for clients
				if(!hosting && update->extra_double_data.size() >= 2 && update->extra_double_data[LAST_RECEIVE_TIME_EXTRA_INDEX] > 0){
					round_trip_start_time = update->extra_double_data[LAST_RECEIVE_TIME_EXTRA_INDEX] ;
					latest_update_time[world_name] = update->last_run_time ;
					world.time_speed = (float)update->extra_double_data[TIME_SPEED_EXTRA_INDEX];
				}
			}
		}
	}
	

	auto current_time = now();
	dt = microsBetween(last_run_time, current_time) / 1000000.0f;
	absolute_time += dt;
	last_run_time = current_time;

	if (!hosting) {//Client attempts to match clock of server
		for(auto& [ world_name, update_time] : latest_update_time){
			World& world = worlds[world_name];
			ping = absolute_time - round_trip_start_time;
			double expected_world_time = update_time + (ping *0.5 * world.time_speed);
			double current_time = world.current_time;
			world.current_time = current_time * (1.0 - clock_adjust_rate) + expected_world_time * clock_adjust_rate;
			world.next_event_time = world.current_time + world.timeline->min_event_duration;
			world.system_time_of_current_time = now();
		}
	}
	lock.unlock();

	// Add events from local players
	lock.lock();
	for(auto& [world_name, world] : worlds){
		for(int k=0;k < world.pending_local_events.size(); k ++){
			world.timeline->queue(world.vantage_point,world.current_time, world.pending_local_events[k]) ;
		}
		world.pending_local_events.clear();
	}
	
	lock.unlock();

	// Run all the active timelines
	for(auto& [name, world] : worlds){
		// Run to the new time
		world.current_time += dt * world.time_speed ;
		world.system_time_of_current_time = now();
		world.next_event_time = world.current_time + world.timeline->min_event_duration;
		world.timeline->run(world.vantage_point,world.current_time) ; // Note we are not locked when actually doing the running which is 95% of time
		lock.lock();
		observation_buffer[name] = world.timeline->observe(world.vantage_point, world.current_time + observation_look_ahead); // buffer observations immediately after run so rollback can't be observed
		viewCreateDestroy(name); // Make sure views are always inline with observations
		lock.unlock();
		//printf("Runs: %d  Unruns: %d\n", Timeline::event_runs, Timeline::event_unruns) ;
	}

	lock.lock();
	// Send my updates to the connections
	double hash_time = 0 ; // TODO check for errors using a hash at fixed intervals?
	for (auto& [name, world] : worlds) {
		Timeline::UpdatePacket update = world.timeline->getPendingUpdate() ;
		update.extra_string_data = { name };
		for(auto& [socket_id, connection] : connections){
			if(!connection.wants_full_sync && !connection.mismatched_version){
				if(hosting){
					update.extra_double_data = {connection.last_absolute_time[name],world.time_speed}; // pass the sbsolute time from client back to the client that sent it to measure ping
				}else{
					update.extra_double_data = { absolute_time }; // clients just send their absolute clock time to the server to be bounced back
				}
				std::pair<int, std::vector<char>> packet = registry->serializeObj(&update);
				std::vector<char> serial = serialize(packet); // need to serialize with the type since we also send copy packet on these sockets sometimes
				bool success = socket->send(socket_id, serial) ;
				if(!success){
					//printf("socket send failed without retry sendingserver updates, desync imminent at %lf\n", absolute_time);
					connection.disconnected = true ;
				}
				if (WorldPlugin::log_type == WorldPlugin::INJECTED_EXTENDED) {
					world.timeline->logUpdate(update, "localsent");
				}
			}
		}
	}

	//Forward updates between clients if we're connected to more than 1
	if(hosting && connections.size() > 1){
		for(auto& [id_1, connection_1] : connections){
			for (int k = 0; k < connection_1.update_packets.size(); k++) {
				std::pair<int, std::vector<char>> packet = std::make_pair(Timeline::UPDATE_PACKET, *connection_1.update_packets[k]) ; //TODO avoid removing type and then reattaching it
				std::vector<char> serial = serialize(packet); // need to serialize with the type since we also send copy packet on these sockets sometimes
				for (auto& [id_2, connection_2] : connections) {
					if(id_1 != id_2 && !connection_2.wants_full_sync && !connection_2.mismatched_version){
						bool success = socket->send(id_2, serial);
						if (!success) {
							//printf("socket send failed without retry on forwarding, desync imminent at %lf\n", absolute_time);
							connection_2.disconnected = true;
						}

						if (WorldPlugin::log_type == WorldPlugin::INJECTED_EXTENDED) {
							std::shared_ptr<Timeline::UpdatePacket> update_des = std::static_pointer_cast<Timeline::UpdatePacket>(registry->deserializeObj(Timeline::UPDATE_PACKET, *connection_1.update_packets[k]));
							Timeline::world->logUpdate(*update_des, "forwarding");
						}
					}
				}

				
			}
		}
	}
	
	//Send a full copy packet if there's a new client connecting
	for (auto& [socket_id, connection] : connections) {
		if (connection.wants_full_sync) {
			if(connection.mismatched_version){
				//Send an empty packet that has our version to the client and let them disconnect from us
				std::shared_ptr<Timeline::CopyPacket> empty_copy = std::shared_ptr<Timeline::CopyPacket>(new Timeline::CopyPacket());
				empty_copy->version = version;
				std::pair<int, std::vector<char>> empty_packet = registry->serializeObj(empty_copy.get());
				std::vector<char> serial = serialize(empty_packet);
				bool success = socket->send(socket_id, serial);
			}else{

				for (auto& [name, world] : worlds) {
					float sync_depth = extra_sync_depth + 1.01f* world.max_read_distance / world.max_info_speed ;// fudge it a little to make sure we send everything on the edge
					//printf("Building packet with sync depth = %f seconds\n", sync_depth);
					Timeline::CopyPacket copy = world.timeline->copy(world.current_time - sync_depth);
					copy.extra_string_data = { name }; // these need to match the constants that reference their locations
					copy.extra_double_data = { 0, world.time_speed };
					std::pair<int, std::vector<char>> packet = registry->serializeObj(&copy);

					std::vector<char> serial = serialize(packet); // need to serialize with the type since we send multiply types on this socket
					//printf("Sent Full packet to %d size: %d\n", socket_id, (int)serial.size());
					//copy.print();
					bool success = socket->send(socket_id, serial);
					if (!success) {
						//printf("socket send failed without retry on full sync, desync imminent at %lf\n", absolute_time);
						connection.disconnected = true;
					}
				}
			}
			connection.wants_full_sync = false;
			connection.update_packets.clear();
			//connection.update_packet_queue.clear();
		}
	}
	
	//Handle any disconnections
	std::vector<int> disconnecting ;
	for (auto& [socket_id, connection_1] : connections) {
		if(connection_1.ready && !connected(socket_id)){ // if was ready at some point but is not disconnected
			disconnecting.push_back(socket_id) ;
		}
	}
	for(int k=0;k<disconnecting.size();k++){
		//printf("disconnected client %d\n", disconnecting[k]) ;
		connections.erase(disconnecting[k]) ;
		//TODO clean up the socket itself?
	}

	//If Steam is active then that's probably where we sent our packets 
	SteamworksPlugin* steam = getTool<SteamworksPlugin>();
	if(steam){
		steam->pumpCallbacks(); // pumping callBacks forces our network packets out the door without waiting until next frame
	}

	if(clear_worlds){

		actuallyClearWorlds();
	}


	lock.unlock();
}


//Runs a world forward by the given amount of time
void WorldPlugin::run(const std::string& world_name, double dt){
	if(worlds.find(world_name) != worlds.end()){
		World& world = worlds[world_name] ;
		world.current_time += dt ;
		world.next_event_time = world.current_time + world.timeline->min_event_duration;
		world.timeline->run(world.vantage_point, world.current_time);
	}
}


//Creates an empty world with the given properties, starts paused at time 0
// Returns if successful (may fail if world_name is taken already or parameters are invalid
bool WorldPlugin::createWorld(const std::string& world_name, float info_speed, float min_event_duration, float max_read_distance){
	lock.lock() ;
	if(worlds.find(world_name) != worlds.end()){
		lock.unlock();
		return false;
	}

	World& world = worlds[world_name] ;
	world.name = world_name ;
	
	world.timeline = std::shared_ptr<Timeline>(new Timeline(registry,info_speed, min_event_duration,max_read_distance, history_kept));
	world.max_info_speed = info_speed ;
	world.max_read_distance = max_read_distance ;

	lock.unlock();
	return true ;
}

// Sets the local viewing position within the given world
void WorldPlugin::setVantagePoint(const std::string& world_name, const glm::vec3& vantage){
	auto iter = worlds.find(world_name);
	if(iter != worlds.end()){
		iter->second.vantage_point = vantage ;
	}
}

//Sets the rate of the passage of time in a world 
//0 is paused, 1.0 is real-time, negative numbers are not permitted
void WorldPlugin::setTimeSpeed(const std::string& world_name, float speed){
	auto iter = worlds.find(world_name);
	if (iter != worlds.end()) {
		iter->second.time_speed = speed;
	}
}

//Returns the current time in the give nworld
double WorldPlugin::getWorldTime(const std::string& world_name){
	auto iter = worlds.find(world_name);
	if (iter != worlds.end()) {
		auto& world = iter->second ;
		return world.current_time + microsBetween(world.system_time_of_current_time, now())*world.time_speed/1000000.0 ;
	}
	return -1.0f ;
}

//Returns the current time in the given world at the given point
double WorldPlugin::getWorldTime(const std::string& world_name, const glm::vec3& point){
	auto iter = worlds.find(world_name);
	if (iter != worlds.end()) {
		World& world = iter->second ;
		return world.current_time 
			+ microsBetween(world.system_time_of_current_time, now()) * world.time_speed / 1000000.0
			- glm::distance(point, world.vantage_point) / world.timeline->max_info_speed;
	}
	return -1.0f;
}

//Returns a list of the names of currently active worlds running locally
std::vector<std::string> WorldPlugin::getLocalWorldList(){
	std::vector<std::string> list ;
	for(auto& [name, world] : worlds){
		list.push_back(name);
	}
	return list ;
}

// Saves a world to a raw chunk of data, which can be loaded later
Variant WorldPlugin::saveWorld(const std::string& world_name){
	return Variant(); //TODO write function
}

// Load a world from a chunk of saved data
bool WorldPlugin::loadWorld(const std::string& world_name, Variant& world_data){
	return false ; //TODO write function
}

// Removes a local world, deleting it's local contents
void WorldPlugin::removeWorld(const std::string& local_world){
	worlds.erase(local_world);
}

// Returns the state of all objects observed on the given world from the current time and local vantage point
std::vector<std::shared_ptr<const WorldObject>> WorldPlugin::observe(const std::string& local_world){
	lock.lock();
	auto iter = worlds.find(local_world);
	if (iter != worlds.end() || clear_worlds) {
		auto o = observation_buffer[local_world] ;
		lock.unlock();
		return o ;
	}else{
		lock.unlock();
		return std::vector<std::shared_ptr<const WorldObject>>() ;
	}
}


void WorldPlugin::view(){
	lock.lock();
	static std::unordered_set<int64_t> observed_ids; // hold static to avoid reallocating every frame
	observed_ids.clear();
	for(auto& [ world_name, world] : worlds){
		std::vector<std::shared_ptr<const WorldObject>> observations = observe(world_name);
		for (std::shared_ptr<const WorldObject>& observation : observations) {
			if (views.find(observation->id) == views.end()) { // Observation has no view
				std::shared_ptr<BaseObjectView> new_view = registry->createView(observation->getTypeId(registry.get()));
				if (new_view) {
					views[observation->id] = new_view;
					new_view->createdBase(observation);
				}
			}
			else {
				views[observation->id]->updatedBase(observation);
			}
			observed_ids.insert(observation->id);
		}
	}

	//Call destroy and delete any views no longer present in the observation
	std::vector<int64_t> to_delete;
	for (auto& [id, view] : views) {
		if (observed_ids.find(id) == observed_ids.end()) {
			to_delete.push_back(id);
		}
	}
	for (auto& id : to_delete) {
		views[id]->destroyedBase();
		views.erase(id);
	}

	lock.unlock();
}

//Calls update on any views currently active
void WorldPlugin::viewUpdate() {
	lock.lock(); // TODO this lock is a bit aggressive and could cause world plugin to wait a lot when viewplugin is running
	for (auto& [world_name, world] : worlds) {
		std::vector<std::shared_ptr<const WorldObject>> observations = observe(world_name);
		for (std::shared_ptr<const WorldObject>& observation : observations) {
			auto iter = views.find(observation->id);
			if (iter != views.end()) {
				iter->second->updatedBase(observation);
			}
		}
	}
	lock.unlock();
}

//Creates and destroys views and calls the appropriate functions on them to amke views match current observatrions
void WorldPlugin::viewCreateDestroy(const std::string& local_world){
	lock.lock();
	auto iter = worlds.find(local_world);
	if (iter == worlds.end() || clear_worlds) {
		lock.unlock();
		return;
	}
	const std::string& world_name = iter->first;
	World& world = iter->second;
	
	static std::unordered_set<int64_t> observed_ids; // hold static to avoid reallocating every frame
	observed_ids.clear();
	
	std::vector<std::shared_ptr<const WorldObject>> observations = observe(world_name);
	for (std::shared_ptr<const WorldObject>& observation : observations) {
		if (views.find(observation->id) == views.end()) { // Observation has no view
			std::shared_ptr<BaseObjectView> new_view = registry->createView(observation->getTypeId(registry.get()));
			if (new_view) {
				views[observation->id] = new_view;
				new_view->createdBase(observation);
			}
		}
		observed_ids.insert(observation->id);
	}
	
	//Call destroy and delete any views no longer present in the observation
	std::vector<int64_t> to_delete;
	for (auto& [id, view] : views) { // TODo views should be by world
		if (observed_ids.find(id) == observed_ids.end()) {
			to_delete.push_back(id);
		}
	}
	for (auto& id : to_delete) {
		views[id]->destroyedBase();
		views.erase(id);
	}
	lock.unlock();

}

// Start hosting your worlds on the given port
// returns if probably successful (can't know for sure)
bool WorldPlugin::host(int port, const std::string& version){
	if (socket) {
		disconnect();
	}
	Timeline::vantage_warp_fraction = server_vantage_warp_fraction ;
	
	auto server = std::shared_ptr<TCPServerSocket>(new TCPServerSocket());
	
	if(simulated_lag_micros > 0){
		fake_lag = std::shared_ptr<SlowPacketReceiver>(new SlowPacketReceiver(this, simulated_lag_micros, simulated_jitter)) ;
		server->setPacketReceiver(fake_lag.get());
	}else{
		server->setPacketReceiver(this);
	}
	socket = server ;
	hosting = server->open(port);
	ping = 0 ;
	this->version = version ;
	return hosting ;
}


bool WorldPlugin::host(std::shared_ptr<Socket>& new_socket, const std::string& version){
	if (socket) {
		disconnect();
	}
	Timeline::vantage_warp_fraction = server_vantage_warp_fraction;

	if (simulated_lag_micros > 0) {
		fake_lag = std::shared_ptr<SlowPacketReceiver>(new SlowPacketReceiver(this, simulated_lag_micros, simulated_jitter));
		new_socket->setPacketReceiver(fake_lag.get());
	}
	else {
		new_socket->setPacketReceiver(this);
	}
	socket = new_socket;
	hosting = true ;
	ping = 0 ;
	this->version = version ;
	return true;

}

//request a connection to a remote World server
// returns if probably successful (can't know for sure)
void WorldPlugin::connect(std::shared_ptr<Socket>& new_socket, const std::string& version) {

	if (socket) {
		disconnect();
	}


	if (simulated_lag_micros > 0) {
		fake_lag = std::shared_ptr<SlowPacketReceiver>(new SlowPacketReceiver(this, simulated_lag_micros, simulated_jitter));
		new_socket->setPacketReceiver(fake_lag.get());
	}
	else {
		new_socket->setPacketReceiver(this);
	}

	socket = new_socket;
	connections.clear();
	//connections[0].address = address ;
	//connections[0].socket_id = 0;
	connections[0].ready = false; //touch something to make sure connections 0 is initialized

	if (simulated_lag_micros > 0) {
		std::this_thread::sleep_for(std::chrono::microseconds(simulated_lag_micros));
	}
	lock.lock();
	actuallyClearWorlds();
	std::shared_ptr<Timeline::CopyPacket> empty_copy = std::shared_ptr<Timeline::CopyPacket>(new Timeline::CopyPacket());
	empty_copy->version = version ;
	this->version = version ;
	std::pair<int, std::vector<char>> empty_packet = registry->serializeObj(empty_copy.get());
	std::vector<char> serial = serialize(empty_packet); // need to serialize with the type since we also send copy packet on these sockets sometimes
	lock.unlock();
	bool success = socket->send(0, serial);
	if (success) {
		connections[0].ready = true;
		connections[0].last_packet_received_time = now(); // technically not true but prevents disconnect before first packet
	}
	else {
		disconnect();
	}
	connection_pending = false;
}

//request a connection to a remote World server
// returns if probably successful (can't know for sure)
void WorldPlugin::connect(const std::string& address, int port, const std::string& version){
	if(!connection_pending){
		Timeline::vantage_warp_fraction = client_vantage_warp_fraction;
		connection_pending = true;
		if(connecting_thread.joinable()){
			connecting_thread.join();
		}
		connecting_thread = std::thread(&WorldPlugin::runConnect, this, address, port, version);
	}
}

void WorldPlugin::runConnect(const std::string& address, int port, const std::string& version) {
	
	if(socket){
		disconnect();
	}

	auto client = std::shared_ptr<TCPClientSocket>(new TCPClientSocket());
	
	if (simulated_lag_micros > 0) {
		fake_lag = std::shared_ptr<SlowPacketReceiver>(new SlowPacketReceiver(this, simulated_lag_micros, simulated_jitter));
		client->setPacketReceiver(fake_lag.get());
	}else {
		client->setPacketReceiver(this);
	}

	socket = client ;
	connections.clear();
	//connections[0].address = address ;
	//connections[0].socket_id = 0;
	connections[0].ready = false; //touch something to make sure connections 0 is initialized
	
	if(simulated_lag_micros > 0 ){
		std::this_thread::sleep_for(std::chrono::microseconds(simulated_lag_micros));
	}
	bool connected = client->connect(address, port);
	if(!connected){
		disconnect();
		connection_pending = false;
		return ;
	}
	lock.lock();
	actuallyClearWorlds();
	std::shared_ptr<Timeline::CopyPacket> empty_copy = std::shared_ptr<Timeline::CopyPacket>(new Timeline::CopyPacket());
	empty_copy->version = version ;
	this->version = version ;
	std::pair<int, std::vector<char>> empty_packet = registry->serializeObj(empty_copy.get());
	std::vector<char> serial = serialize(empty_packet); // need to serialize with the type since we also send copy packet on these sockets sometimes
	lock.unlock();
	bool success = socket->send(0, serial) ; 
	if(success){
		connections[0].ready = true ; 
		connections[0].last_packet_received_time = now() ; // technically not true but prevents disconnect before first packet
	}else{
		disconnect();
	}
	connection_pending = false;
}



// For the client: Disconnects from a remote host
// For the server: Stops a server and kicks all clients
// Worlds will continue to run, but will stop synchronizing
void WorldPlugin::disconnect(){
	lock.lock();
	if(socket){
		socket->close();
		socket.reset();
		connections.clear();
		if(fake_lag){
			fake_lag->stop();
			fake_lag.reset();
		}
		hosting = false ;
	}
	ping = 0 ;
	lock.unlock();
}

void WorldPlugin::clearWorlds(){
	clear_worlds = true ;
}

void WorldPlugin::actuallyClearWorlds() {
	lock.lock();
	worlds.clear();
	observation_buffer.clear();
	for (auto& [id, view] : views) {
		view->destroyedBase();
	}
	views.clear();
	clear_worlds = false;
	lock.unlock();
}

// For the client: check if requestConnect has succeeded, but also reports if the host has disconnected
// For host just returns true
//If has never tried to host or join returns false
bool WorldPlugin::connected(){
	return hosting || connected(0);
}



// For the server: Check if a specific remote connection is still active
bool WorldPlugin::connected(int id){
	if(connections.find(id) == connections.end()){
		return false;
	}else{
		return  connections[id].ready && !connections[id].disconnected && microsBetween(connections[id].last_packet_received_time, now()) < micros_before_considered_disconnected ;
	}
}

//Called when a packet is received
void WorldPlugin::receivePacket(int sender_id, const std::vector<char>& data){

	std::tuple<int, std::vector<char>> packet = deserialize<int, std::vector<char>>(data) ;
	int& type = get<0>(packet);
	std::vector<char>& obj_data = get<1>(packet);
	packet_lock.lock();
	if(type == Timeline::UPDATE_PACKET){
		connections[sender_id].update_packet_queue.emplace_back(std::make_shared<std::vector<char>>(obj_data)) ;
		//printf("Got an update from %d\n", sender_id) ;
	}else if(type == Timeline::COPY_PACKET){
		std::shared_ptr<Timeline::CopyPacket> copy = std::static_pointer_cast<Timeline::CopyPacket>(registry->deserializeObj(type, obj_data));
		if(copy->last_vantage_time >= 0){ // proper time means it's full of copy data
			//printf("Got a full copy packet for %s from %d\n", copy->extra_string_data[NAME_EXTRA_INDEX].c_str(), sender_id) ;
			//copy->print();
			copy_packets.emplace_back(copy);
		}else{//negative time means it's empty, so it's a copy request
			//printf("client %d has requested a full copy\n", sender_id); 
			lock.lock(); // new clients request full copy so this is an edit to the connections container which needs to be guarded
			connections[sender_id].wants_full_sync = true ;
			if (copy->version != version) {
				printf("Connection attempted with mismatched version. Our Version: %s, Their Version: %s\n", version.c_str(), copy->version.c_str()) ;
				connections[sender_id].mismatched_version = true;
				connections[sender_id].disconnected = true;
			}
			lock.unlock();
		}
	}
	connections[sender_id].last_packet_received_time = now();
	packet_lock.unlock();
}

//Called when a connection is established, 
//sender_id is generated by the socket and can be used to identify recieved packet sources or send data back through the socket
void WorldPlugin::onSocketConnect(int sender_id){
	//TODO
}

//Called when a connection is closed, either remotely or because the socket holding it was closed
void WorldPlugin::onSocketClose(int sender_id){
	//printf("Got a socket close for %d!\n", sender_id);
	connections[sender_id].disconnected = true ;
	connections[sender_id].ready = true;
}

WorldPlugin::~WorldPlugin(){
	if(socket){
		socket->close();
	}
	if (connecting_thread.joinable()) {
		connecting_thread.join();
	}
	printf("Deleting world plugin.\n");
}

//returns the ping in seconds
double WorldPlugin::getPing(){
	return ping ;
}

//returns the time step of the last frame in seconds
float WorldPlugin::getTimeStep(){
	return dt ;
}