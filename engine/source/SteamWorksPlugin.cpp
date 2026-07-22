#include "SteamworksPlugin.h"
#include "Registry.h"


SteamworksPlugin::SteamworksPlugin(long app_id, const std::string& command_line):
	steamapp_id(0),
	initialized(false),
	m_CallbackUserStatsReceived(this, &SteamworksPlugin::OnUserStatsReceived),
	m_CallbackUserStatsStored(this, &SteamworksPlugin::OnUserStatsStored),
	m_CallbackAchievementStored(this, &SteamworksPlugin::OnAchievementStored)
{	
	if(!enabled){
		return ;
	}

	if (SteamAPI_RestartAppIfNecessary(app_id)){
		printf("Attempting to restart app in Steam\n");
		wants_to_exit = true;
	}
	
	
	if (!SteamAPI_Init()){
		printf("Steam API init failed. You will not be able to use Steam features or get achievements.\n");
	}else{
		printf("SteamAPI init successful.\n");
	}

	steamapp_id = SteamUtils()->GetAppID();
	printf("Steam App ID: %ld\n", steamapp_id);

	SteamNetworkingUtils()->InitRelayNetworkAccess(); // allows P2P hosting

	catchCommandLineJoin(command_line.c_str()) ; // if the app was booted with a join command make sure we catch it
	
}


SteamworksPlugin::SteamworksPlugin(long app_id, const SteamServerInfo& server_info, int master_server_port, const std::string& command_line):
	steamapp_id(0),
	initialized(false),
	m_CallbackUserStatsReceived(this, &SteamworksPlugin::OnUserStatsReceived),
	m_CallbackUserStatsStored(this, &SteamworksPlugin::OnUserStatsStored),
	m_CallbackAchievementStored(this, &SteamworksPlugin::OnAchievementStored) 
{
	if (!enabled) {
		return;
	}

	dedicated_server = true ;
	

	SteamErrMsg error_message = { 0 };
	if (SteamGameServer_InitEx(0, server_info.port, master_server_port, EServerMode::eServerModeAuthenticationAndSecure, server_info.version.c_str(), &error_message) != k_ESteamAPIInitResult_OK){
		printf("Server initialization failed : %s\n", error_message) ;
	}

	if (SteamGameServer()){
		SteamGameServer()->SetModDir(server_info.game_directory.c_str());
		SteamGameServer()->SetProduct(server_info.product_name.c_str());
		SteamGameServer()->SetGameDescription(server_info.product_description.c_str());
		SteamGameServer()->SetServerName(server_info.name.c_str());
		SteamGameServer()->SetMapName(server_info.map.c_str());
		SteamGameServer()->SetMaxPlayerCount(server_info.max_players);
		SteamGameServer()->SetBotPlayerCount(0);
		SteamGameServer()->SetPasswordProtected(server_info.has_password);
		SteamGameServer()->SetDedicatedServer(true);
		SteamGameServer()->LogOnAnonymous();
		SteamNetworkingUtils()->InitRelayNetworkAccess();
		lobby_info = server_info ;
		steam_socket = std::make_shared<SteamSocket>(); // Not ready yet, but go ahead and make it so it can be passed to what needs it
	}else{
		printf("SteamGameServer() interface is invalid\n");
	}

}

SteamworksPlugin::~SteamworksPlugin(){
	if (!enabled) {
		return;
	}
	SteamAPI_Shutdown() ;
	printf("Steamworks shutdown.\n");
}

// Called on every plug-in before any plug-ins are run
void SteamworksPlugin::initialize() {
	printf("Steamworks plugin started.\n");
}

void SteamworksPlugin::pumpCallbacks(){
	if (!enabled) {
		return;
	}
	if (dedicated_server) {
		SteamGameServer_RunCallbacks();
	}
	else {
		SteamAPI_RunCallbacks();
	}
	if (steam_socket) {
		steam_socket->processIncomingPackets();
	}


}

// runs the plugin on its own thread
void SteamworksPlugin::run() {

	pumpCallbacks();
	initialized = true;

	if(join_lobby_pending && millisBetween(creation_time, now()) > 2000){
		printf("Time waited before attempting to join %d\n", millisBetween(creation_time, now()));
		joinLobby(pending_join_lobby_id);
		join_lobby_pending = false ;
	}
	

	if(client_can_join_game){
		lobby_info.host_id = SteamMatchmaking()->GetLobbyOwner(lobby_info.id);
		steam_socket->join(lobby_info.host_id);
		client_can_join_game = false ;

		if (event_receiver != nullptr) {
			event_receiver->onSteamGameExternalJoin(steam_socket, lobby_info);
		}

	}

	if (join_address_pending && millisBetween(creation_time,now()) > 2000 ) {
		if(steam_socket && steam_socket->connected()){
			join_address_pending = false; // just clear a join attempt when already connected
		}else{
			joinAddress(pending_join_address);
			join_address_pending = false;
		}
	}

	pumpCallbacks();
	
}

//attempts to mark a steam achievement as completed
// returns if successful
bool SteamworksPlugin::achieve(const std::string& id){
	if (!enabled) {
		return false;
	}
	if (initialized && !achieved_this_run.contains(id)){
		SteamUserStats()->SetAchievement(id.c_str());
		bool success = SteamUserStats()->StoreStats();
		if (success) {
			achieved_this_run.insert(id);
		}
		return success;
	}
	return false;
}

//Returns the user's current steam display name
std::string SteamworksPlugin::getLocalName(){
	if (!enabled) {
		return "SteamDisabled" ;
	}
	CSteamID my_id =  SteamUser()->GetSteamID() ;
	std::string my_name = std::string(SteamFriends()->GetFriendPersonaName(my_id)) ;
	return my_name ;
}

// Returns the current user's Steam ID
uint64 SteamworksPlugin::getLocalSteamID() {
	if (!enabled) {
		return 0;
	}
	return SteamUser()->GetSteamID().ConvertToUint64();
}

//For the host, returns the index of the connection of a given SteamID
	//Returns -1 if the user is not connected or if you are not the host
int SteamworksPlugin::getClientConnectionIndex(uint64 remote_steam_id) {
	if (!enabled || !steam_socket || steam_socket->steamIDToPlayer.find(remote_steam_id) == steam_socket->steamIDToPlayer.end()) {
		return -1;
	}
	return steam_socket->steamIDToPlayer[remote_steam_id];
}

void SteamworksPlugin::setSteamEventReceiver(SteamEventReceiver* receiver){
	event_receiver = receiver ;
}


// Create a Steam lobby and return a socket to communicate with those who join it
std::shared_ptr<SteamworksPlugin::SteamSocket> SteamworksPlugin::hostPrivateLobby(const SteamServerInfo& info){
	printf("Attempting to host a private lobby\n");
	SteamAPICall_t hCall = SteamMatchmaking()->CreateLobby(
		k_ELobbyTypeFriendsOnly,
		info.max_players); // follows up with LobbyCreated_t callback
	lobby_info = info ;
	
	steam_socket = std::make_shared<SteamSocket>(); // Not ready yet, but go ahead and make it so it can be passed to what needs it
	steam_socket->steamIDToPlayer[getLocalSteamID()] = 0; // On private matches we are player zero
	return steam_socket;

}

void SteamworksPlugin::disconnect(){
	if(steam_socket){
		steam_socket->close(); // reset may close but we manually close in case there are any stored references
		steam_socket.reset() ;
	}
	SteamMatchmaking()->LeaveLobby(lobby_info.id) ;
}


void SteamworksPlugin::refreshServerList(){
	if (requesting_server_list) {
		return ;
	}
	requesting_server_list = true;
	if (server_list_request) {
		SteamMatchmakingServers()->ReleaseRequest(server_list_request);
		server_list_request = NULL;
	}

	server_list_request = SteamMatchmakingServers()->RequestInternetServerList(SteamUtils()->GetAppID(), nullptr, 0, this);
	printf("requested server list\n");
}

//Returns the current list of servers
std::vector<SteamworksPlugin::SteamServerInfo> SteamworksPlugin::getServerList(){
	return last_servers ;
}

bool SteamworksPlugin::serverListReady(){
	return !requesting_server_list ;

}


// Server has responded ok with updated data
void SteamworksPlugin::ServerResponded(HServerListRequest hRequest, int iServer){

	const gameserveritem_t* item = SteamMatchmakingServers()->GetServerDetails(hRequest, iServer);
	SteamServerInfo info ;
	info.ping = item->m_nPing ;
	info.port = item->m_NetAdr.GetConnectionPort() ;
	info.address = item->m_NetAdr.GetIP();

	info.connect = item->m_NetAdr.GetConnectionAddressString() ;
	info.map = item->m_szMap ;
	info.players = item->m_nPlayers ;
	info.version = item->m_nServerVersion ;
	info.max_players = item->m_nMaxPlayers ;
	info.name = item->GetName() ;

	servers.push_back(info);
	printf("Found server:  %s  version: %s ping:%d\n", info.name.c_str(), info.version.c_str(), info.ping) ;
 
}

// Server has failed to respond
void SteamworksPlugin::ServerFailedToRespond(HServerListRequest hRequest, int iServer){
	printf("A Server failed to respond\n");
}

// A list refresh you had initiated is now 100% completed
void SteamworksPlugin::RefreshComplete(HServerListRequest hRequest, EMatchMakingServerResponse response){
	requesting_server_list = false;
	last_servers = servers ;
	servers.clear();
	printf("Server Refresh complete\n");
}


// Join a Steam lobby and create a socket to communicate with its host
std::shared_ptr<SteamworksPlugin::SteamSocket> SteamworksPlugin::joinLobby(CSteamID lobby_id){
	printf("Attempting to actually join the lobby\n");
	SteamAPICall_t hSteamAPICall = SteamMatchmaking()->JoinLobby(lobby_id); // follows up with LobbyEnter_t callback
	lobby_info.id = lobby_id ;
	steam_socket = std::make_shared<SteamSocket>(); // Not ready yet, but go ahead and make it so it can be passed to what needs it
	client_can_join_game = false;
	return steam_socket;
}


// Join a Steam ganme by ip and port 
std::shared_ptr<SteamworksPlugin::SteamSocket> SteamworksPlugin::joinAddress(SteamNetworkingIPAddr& addr) {
	char address_string[100] ;
	addr.ToString(address_string,100,true) ;
	printf("Attempting to actually join by ip: %s\n", address_string );
	steam_socket = std::make_shared<SteamSocket>(); 
	steam_socket->join(addr);
	client_waiting_on_ip_join = true ;
	return steam_socket;
}

void SteamworksPlugin::onLobbyEntered(LobbyEnter_t* call_back){
	printf("lobby entered!\n");
	if(steam_socket && !steam_socket->is_server){
		CSteamID lobbyID = call_back->m_ulSteamIDLobby;
		uint32 unGameServerIP;
		uint16 unGameServerPort;
		CSteamID hostSteamID;

		// Check if the game has already started
		if (SteamMatchmaking()->GetLobbyGameServer(lobbyID, &unGameServerIP, &unGameServerPort, &hostSteamID)){
			lobby_info.host_id = hostSteamID ;
			client_can_join_game = true ;
		}
	}

}

std::shared_ptr<SteamworksPlugin::SteamSocket> SteamworksPlugin::getActiveSocket(){
	return steam_socket ;
}


bool SteamworksPlugin::commandLineHasLobbyJoin(const char* command_line) {
	const char* pchConnectLobby = strstr(command_line, connect_lobby_param.c_str());
	return pchConnectLobby && strlen(command_line) > (pchConnectLobby - command_line) + connect_lobby_param.length();
}
CSteamID SteamworksPlugin::getCommandLineLobbyJoin(const char* command_line) {
	const char* pchConnectLobby = strstr(command_line, connect_lobby_param.c_str());
	if (pchConnectLobby && strlen(command_line) > (pchConnectLobby - command_line) + connect_lobby_param.length()){
		// lobby ID should be right after the +connect_lobby
		return CSteamID(std::stoull(std::string( command_line + (pchConnectLobby - command_line) + connect_lobby_param.length())));
	}
	printf("Failed to fetch command line lobby on join!\n");
	return CSteamID();
}
bool SteamworksPlugin::commandLineHasAddressJoin(const char* command_line) {
	const char* pchConnect = strstr(command_line, connect_param.c_str());
	return pchConnect && strlen(command_line) > (pchConnect - command_line) + connect_param.length();
}

SteamNetworkingIPAddr SteamworksPlugin::getCommandLineAddressJoin(const char* command_line) {
	
	const char* pchConnect = strstr(command_line, connect_param.c_str());
	SteamNetworkingIPAddr address ;
	address.Clear();
	if (pchConnect && strlen(command_line) > (pchConnect - command_line) + strlen(connect_param.c_str())){
		// Address should be right after the +connect
		std::string address_string = std::string(command_line + (pchConnect - command_line) + connect_param.length());
		//printf("Parsing address string: %s\n", address_string.c_str()) ;
		address.ParseString(address_string.c_str());
	}
	return address ;
}

//Parses a join command out of a command and sets the coresponding static pending join variables if appropriate
void SteamworksPlugin::catchCommandLineJoin(const char* command_line){
	join_lobby_pending = false;
	join_address_pending = false;
	if (commandLineHasLobbyJoin(command_line)) {
		pending_join_lobby_id = getCommandLineLobbyJoin(command_line);
		join_lobby_pending = true;
		printf("Steam join game event received for lobby: %lld\n", pending_join_lobby_id.ConvertToUint64());
	}

	if (commandLineHasAddressJoin(command_line)) {
		pending_join_address = getCommandLineAddressJoin(command_line);
		join_address_pending = true;
		printf("Steam join game event received for address.\n");
	}
}

void SteamworksPlugin::OnUserStatsReceived(UserStatsReceived_t* pCallback){
	//printf("recieved user stats recieved callback\n");
	// we may get callbacks for other games' stats arriving, ignore them
	if (steamapp_id == pCallback->m_nGameID){
		if (k_EResultOK == pCallback->m_eResult){
			initialized = true;
			int num_achievements = SteamUserStats()->GetNumAchievements();
			// load achievements
			for (int a = 0 ; a < num_achievements; a++){
				SteamAchievement chieve;
				chieve.id = SteamUserStats()->GetAchievementName(a) ;
				chieve.name = SteamUserStats()->GetAchievementDisplayAttribute(chieve.id.c_str(), "name");
				chieve.description = SteamUserStats()->GetAchievementDisplayAttribute(chieve.id.c_str(), "desc");
				chieve.icon = SteamUserStats()->GetAchievementIcon(chieve.id.c_str());
				bool got_it = SteamUserStats()->GetAchievement(chieve.id.c_str(), &(chieve.achieved));
				achievements.push_back(chieve);

				//printf("Found achievement: %s\n", chieve.name.c_str());
			}
		}else{
			printf("Steamworks UserStats request failed, EResult code: %d\n", pCallback->m_eResult);
		}
	}
}
void SteamworksPlugin::OnUserStatsStored(UserStatsStored_t* pCallback){
	//printf("recieved user stats stored callback\n");
	// we may get callbacks for other games' stats arriving, ignore them
	if (steamapp_id == pCallback->m_nGameID){
		if (k_EResultOK == pCallback->m_eResult){
			printf("Successfully stored steam stats.\n");
		}else{
			printf("Store steam stats failed, EResult code: %d\n", pCallback->m_eResult);
		}
	}
}

void SteamworksPlugin::OnAchievementStored(UserAchievementStored_t* pCallback){
	//printf("recieved achievement stored callback\n");
	// we may get callbacks for other games' stats arriving, ignore them
	if (steamapp_id == pCallback->m_nGameID){
		printf("Successfully saved Steam achievement.\n");
	}
	
}

void SteamworksPlugin::onLobbyCreated(LobbyCreated_t* call_back){

	if (call_back->m_eResult != k_EResultOK) {
		std::cerr << "Steam lobby creation failed: " << call_back->m_eResult << "\n";
		return;
	}
	lobby_info.id = call_back->m_ulSteamIDLobby;
	lobby_info.host_id = SteamUser()->GetSteamID();
	printf("Steam lobby creation succeeded with id %lld\n", lobby_info.id.ConvertToUint64()) ;

	SteamMatchmaking()->SetLobbyData(lobby_info.id, "name", lobby_info.name.c_str());

	SteamMatchmaking()->SetLobbyData(lobby_info.id, "map", lobby_info.map.c_str());
	SteamMatchmaking()->SetLobbyData(lobby_info.id, "gamemode", lobby_info.game_mode.c_str());
	SteamMatchmaking()->SetLobbyData(lobby_info.id,
		"players", std::to_string(lobby_info.players).c_str());
	SteamMatchmaking()->SetLobbyData(lobby_info.id,
		"maxplayers", std::to_string(lobby_info.max_players).c_str());

	/*
	if (info.passwordProtected) {
		SteamMatchmaking()->SetLobbyData(lobby_info.id,
			"passwordprotected", "1");
	}*/
	for (const auto& [key, value] : lobby_info.extra_values) {
		SteamMatchmaking()->SetLobbyData(lobby_info.id,
			key.c_str(),
			value.c_str());
	}

	std::string connect_string = "steam://joinlobby/";
	connect_string += std::to_string(SteamUtils()->GetAppID());
	connect_string += "/";
	connect_string += std::to_string(lobby_info.id.ConvertToUint64());

	SteamFriends()->SetRichPresence("connect", connect_string.c_str());


	steam_socket->host(lobby_info.id, lobby_info.host_id) ;


}


void SteamworksPlugin::onServerLobbyCreated(LobbyCreated_t* call_back) {

	if (call_back->m_eResult != k_EResultOK) {
		std::cerr << "Steam lobby creation failed: " << call_back->m_eResult << "\n";
		return;
	}
	printf("Server lobby apparently created ? \n");
	/*
	lobby_info.id = call_back->m_ulSteamIDLobby;
	lobby_info.host_id = SteamUser()->GetSteamID();
	printf("Steam server lobby creation succeeded with id %lld\n", lobby_info.id.ConvertToUint64());

	SteamMatchmaking()->SetLobbyData(lobby_info.id, "name", lobby_info.name.c_str());

	SteamMatchmaking()->SetLobbyData(lobby_info.id, "map", lobby_info.map.c_str());
	SteamMatchmaking()->SetLobbyData(lobby_info.id, "gamemode", lobby_info.game_mode.c_str());
	SteamMatchmaking()->SetLobbyData(lobby_info.id,
		"players", std::to_string(lobby_info.players).c_str());
	SteamMatchmaking()->SetLobbyData(lobby_info.id,
		"maxplayers", std::to_string(lobby_info.max_players).c_str());
	*/
}


//-----------------------------------------------------------------------------
// Purpose: Take any action we need to on Steam notifying us we are now logged in
//-----------------------------------------------------------------------------
void SteamworksPlugin::OnSteamServersConnected(SteamServersConnected_t* pLogonSuccess){
	//printf("OnSteamServersConnected\n");
	SteamNetworkingIPAddr addr;
	addr.Clear();                         // 0.0.0.0
	addr.m_port = static_cast<uint16_t>(lobby_info.port);
	steam_socket->hostDedicated(addr);
	SteamGameServer()->SetAdvertiseServerActive(true);
	std::string connect_string = concat("{0.0.0.0}:{", lobby_info.port) + "}" ;
	//printf("connect string: %s\n", connect_string.c_str()) ;
	SteamGameServer()->SetKeyValue("connect",connect_string.c_str()) ;
	server_connected_to_steam = true;

}

//-----------------------------------------------------------------------------
// Purpose: Called when an attempt to login to Steam fails
//-----------------------------------------------------------------------------
void SteamworksPlugin::OnSteamServersConnectFailure(SteamServerConnectFailure_t* pConnectFailure){
	printf("OnSteamServersConnectFailure\n");
	server_connected_to_steam = false;
}

//-----------------------------------------------------------------------------
// Purpose: Called when we were previously logged into steam but get logged out
//-----------------------------------------------------------------------------
void SteamworksPlugin::OnSteamServersDisconnected(SteamServersDisconnected_t* pLoggedOff){
	printf("OnSteamServersDisconnected\n");
	server_connected_to_steam = false;
}

//-----------------------------------------------------------------------------
// Purpose: Callback from Steam when logon is fully completed and VAC secure policy is set
//-----------------------------------------------------------------------------
void SteamworksPlugin::OnPolicyResponse(GSPolicyResponse_t* pPolicyResponse){
	//printf("OnPolicyResponse\n");
	//TODO
}

//-----------------------------------------------------------------------------
// Purpose: Tells us Steam3 (VAC and newer license checking) has accepted the user connection
//-----------------------------------------------------------------------------
void SteamworksPlugin::OnValidateAuthTicketResponse(ValidateAuthTicketResponse_t* pResponse){
	printf("OnValidateAuthTicketResponse\n");
	//TODO
}

//-----------------------------------------------------------------------------
// Purpose: Handle any connection status change
//-----------------------------------------------------------------------------
void SteamworksPlugin::OnServerConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* call_back){
	//printf("OnServerConnectionStatusChanged\n");
	
	HSteamNetConnection connection = call_back->m_hConn;
	SteamNetConnectionInfo_t info = call_back->m_info;
	ESteamNetworkingConnectionState old_state = call_back->m_eOldState;
	ESteamNetworkingConnectionState new_state = info.m_eState;

	//printf("Connection: %d  Listen socket:%d\n", connection, info.m_hListenSocket);
	//printf("Old State: %d New State: %d\n", old_state, new_state);

	// Parse information to know what was changed

	// Check if a client has connected
	if (info.m_hListenSocket &&
		old_state == k_ESteamNetworkingConnectionState_None &&
		new_state== k_ESteamNetworkingConnectionState_Connecting){
		steam_socket->addServerClient(connection, info.m_identityRemote.GetSteamID()) ;
		
	}
	// Check if a client has disconnected
	else if ((old_state == k_ESteamNetworkingConnectionState_Connecting || old_state == k_ESteamNetworkingConnectionState_Connected) &&
		new_state == k_ESteamNetworkingConnectionState_ClosedByPeer){
		steam_socket->removeServerClient(info.m_identityRemote.GetSteamID()) ;
		
	}

}

//-----------------------------------------------------------------------------
// Purpose: Joins a game from a lobby
//-----------------------------------------------------------------------------
void SteamworksPlugin::OnLobbyGameCreated(LobbyGameCreated_t* call_back){
	printf("OnLobbyGameCreated\n");
	if(steam_socket && ! steam_socket->is_server){
		client_can_join_game = true ;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Steam is asking us to join a game, based on the user selecting
//			'join game' on a friend in their friends list 
//			the string comes from the "connect" field set in the friends' rich presence
//-----------------------------------------------------------------------------
void SteamworksPlugin::OnGameJoinRequested(GameRichPresenceJoinRequested_t* call_back){
	std::string command = call_back->m_rgchConnect;
	printf("ongamejoinrequested : %s\n" , command.c_str());
	if(!commandLineHasAddressJoin(command.c_str())){ // it may have teh address but not be formatted
		command = connect_param + command ; // add +connect and try again
	}
	catchCommandLineJoin(command.c_str()) ;
	if(join_lobby_pending){
		client_can_join_game = true ;
	}
}


void SteamworksPlugin::OnGameLobbyJoinRequested( GameLobbyJoinRequested_t* call_back){
	printf("ongamelobbyjoinrequested");
	pending_join_lobby_id = call_back->m_steamIDLobby;
	join_lobby_pending = true;
	client_can_join_game = false ;
	printf("Steam join game event received for lobby: %lld\n", pending_join_lobby_id.ConvertToUint64());
}

void SteamworksPlugin::OnGameServerChangeRequested(GameServerChangeRequested_t* call_back){
	printf("OnGameServerChangeRequested\n");
	printf("Attempting to connect to : %s\n", call_back->m_rgchServer) ;
	std::string command = "+connect " + std::string(call_back->m_rgchServer) ;
	//printf("command : %s\n", command.c_str());
	catchCommandLineJoin(command.c_str());

}


//-----------------------------------------------------------------------------
// Purpose: a large avatar image has been loaded for us
//-----------------------------------------------------------------------------
void SteamworksPlugin::OnAvatarImageLoaded(AvatarImageLoaded_t* pCallback){
	printf("OnAvatarImageLoaded\n");
//TODO
}

//-----------------------------------------------------------------------------
// Purpose: a Steam URL to launch this app was executed while the game is already running, eg steam://run/480//+connect%20127.0.0.1
//      	Anybody can build random Steam URLs	and these extra parameters must be carefully parsed to avoid unintended side-effects
//-----------------------------------------------------------------------------
void SteamworksPlugin::OnNewUrlLaunchParameters(NewUrlLaunchParameters_t* pCallback){
	printf("OnNewUrlLaunchParameters\n");
	char command_line[1024] = {};
	if (SteamApps()->GetLaunchCommandLine(command_line, sizeof(command_line)) > 0){
		catchCommandLineJoin(command_line);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles notification that the Steam overlay is shown/hidden, note, this
// doesn't mean the overlay will or will not draw, it may still draw when not active.
// This does mean the time when the overlay takes over input focus from the game.
//-----------------------------------------------------------------------------
void SteamworksPlugin::OnGameOverlayActivated(GameOverlayActivated_t* callback){
	//printf("OnGameOverlayActivated(\n");
	//TODO
}

//-----------------------------------------------------------------------------
// Purpose: Handle the callback from the user clicking a steam://gamewebcallback/ link in the overlay browser
//	You can use this to add support for external site signups where you want to pop back into the browser
//  after some web page signup sequence, and optionally get back some detail about that.
//-----------------------------------------------------------------------------
void SteamworksPlugin::OnGameWebCallback(GameWebCallback_t* callback){
	printf("OnGameWebCallback\n");
//TODO
}

//-----------------------------------------------------------------------------
// Purpose: Handle any connection status change
//-----------------------------------------------------------------------------
void SteamworksPlugin::OnClientConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* call_back){
	//printf("OnClientConnectionStatusChanged\n");

	HSteamNetConnection connection = call_back->m_hConn;
	SteamNetConnectionInfo_t info = call_back->m_info;
	ESteamNetworkingConnectionState old_state = call_back->m_eOldState;
	ESteamNetworkingConnectionState new_state = info.m_eState;

	//printf("Connection: %d  Listen socket:%d\n", connection, info.m_hListenSocket);
	//printf("Old State: %d New State: %d\n", old_state, new_state);

	// Parse information to know what was changed

	// Check if a client has connected
	if (info.m_hListenSocket &&
		old_state == k_ESteamNetworkingConnectionState_None &&
		new_state == k_ESteamNetworkingConnectionState_Connecting) {
		steam_socket->addServerClient(connection, info.m_identityRemote.GetSteamID());

	}
	// Check if a client has disconnected
	else if (info.m_hListenSocket && (old_state == k_ESteamNetworkingConnectionState_Connecting || old_state == k_ESteamNetworkingConnectionState_Connected) &&
		new_state == k_ESteamNetworkingConnectionState_ClosedByPeer) {
		steam_socket->removeServerClient(info.m_identityRemote.GetSteamID());

	}

	if(client_waiting_on_ip_join && new_state ==  3 ){ // client is connected by ip
		if (event_receiver != nullptr) { // notify game state that we have made a join
			printf("Signalling successful join\n");
			event_receiver->onSteamGameExternalJoin(steam_socket, lobby_info);
		}
		client_waiting_on_ip_join = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles notification of a steam ipc failure
// we may get multiple callbacks, one for each IPC operation we attempted
// since the actual failure, so protect ourselves from alerting more than once.
//-----------------------------------------------------------------------------
void SteamworksPlugin::OnIPCFailure(IPCFailure_t* failure){
	printf("OnIPCFailure\n");
//TODO
}

//-----------------------------------------------------------------------------
// Purpose: Handles notification of a Steam shutdown request since a Windows
// user in a second concurrent session requests to play this game. Shutdown
// this process immediately if possible.
//-----------------------------------------------------------------------------
void SteamworksPlugin::OnSteamShutdown(SteamShutdown_t* callback){
	printf("OnSteamShutdown\n");
//TODO
}



SteamworksPlugin::SteamSocket::SteamSocket() {
	
}

// Open a steam client socket to connect to the given id
void SteamworksPlugin::SteamSocket::join(CSteamID lobby_to_join){
	printf("Attempting to join on socket to %lld .\n", lobby_to_join.ConvertToUint64());
	identity.SetSteamID(lobby_to_join);
	connections[0] = SteamNetworkingSockets()->ConnectP2P(identity, 0, 0, nullptr);
	is_server = false ;
	is_dedicated = false;
}

// Open a steam client socket to connect to the given id
void SteamworksPlugin::SteamSocket::join(SteamNetworkingIPAddr address_to_join) {
	char address_string[50];
	address_to_join.ToString(address_string, 50, true) ;
	printf("Socket attempting to connect to ip: %s\n", address_string);
	std::string command = "+connect " + std::string(address_string) ;
	SteamFriends()->SetRichPresence("connect", command.c_str());
	connections[0] = SteamNetworkingSockets()->ConnectByIPAddress(address_to_join, 0, nullptr);
	is_server = false;
	is_dedicated = false;

	// Send an auth ticket as the first packet
	constexpr int max_ticket_size = 1024 ;
	uint8 ticket_buffer[max_ticket_size];
	uint32 ticket_length = 0;
	HAuthTicket hTicket = SteamUser()->GetAuthSessionTicket(
		ticket_buffer, max_ticket_size, &ticket_length, nullptr);
	std::vector<char> ticket_packet(ticket_buffer,ticket_buffer + ticket_length) ;
	/*printf("auth size: %d\n", ticket_packet.size());
	for(char& c : ticket_packet){
		printf("%c", c);
	}
	printf("\n");
*/
	send(0, serialize(AUTH_PACKET_MAGIC_NUMBER,ticket_packet));

}

void SteamworksPlugin::SteamSocket::host(CSteamID lobby_id, CSteamID host_id){
	
	// create the listen socket for listening for players connecting
	listen_socket = SteamNetworkingSockets()->CreateListenSocketP2P(0, 0, nullptr);

	// create the poll group
	poll_group = SteamNetworkingSockets()->CreatePollGroup();

	SteamMatchmaking()->SetLobbyGameServer(lobby_id, 0, 0, host_id);
	printf("Starting game server on %lld .\n", host_id.ConvertToUint64());
	is_server = true ;
	is_dedicated = false;
}



void SteamworksPlugin::SteamSocket::hostDedicated(const SteamNetworkingIPAddr address){
	listen_socket = SteamGameServerNetworkingSockets()->CreateListenSocketIP(address, 0, nullptr);
	poll_group = SteamGameServerNetworkingSockets()->CreatePollGroup();
	printf("Started Steam dedicated server.\n");
	is_server = true ;
	is_dedicated = true ;
}

void SteamworksPlugin::SteamSocket::addServerClient(HSteamNetConnection connection, CSteamID user_id){
	if(is_dedicated){
		EResult res = SteamGameServerNetworkingSockets()->AcceptConnection(connection);
		SteamGameServerNetworkingSockets()->SetConnectionPollGroup(connection, poll_group);
		steamIDToPlayer[user_id] = next_player;
		connections[next_player] = connection;
	}else{
		EResult res = SteamNetworkingSockets()->AcceptConnection(connection);
		SteamNetworkingSockets()->SetConnectionPollGroup(connection, poll_group);
		steamIDToPlayer[user_id] = next_player;
		connections[next_player] = connection ;
	}
	if(packet_receiver != nullptr){
		packet_receiver->onSocketConnect(next_player);
	}
	//printf("Steam id %lld added as player %d\n", user_id.ConvertToUint64(), next_player);
	next_player++;
}

void SteamworksPlugin::SteamSocket::removeServerClient(CSteamID user_id) {
	int player_id = steamIDToPlayer[user_id] ;
	if(is_dedicated){
		SteamGameServerNetworkingSockets()->CloseConnection(connections[player_id], 0, nullptr, false);
		SteamGameServer()->EndAuthSession(user_id) ;
		authenticated.erase(user_id) ;
	}else{
		SteamNetworkingSockets()->CloseConnection(connections[player_id], 0, nullptr, false);
	}
	connections.erase(player_id);
	steamIDToPlayer.erase(user_id) ;
	if (packet_receiver != nullptr) {
		packet_receiver->onSocketClose(player_id);
	}
	//printf("Steam id %lld added as player %d\n", user_id.ConvertToUint64(), player_id);
}

void SteamworksPlugin::SteamSocket::removeServerClient(int player) {
	for(auto& [user_id, pid] : steamIDToPlayer){
		if(pid == player){
			removeServerClient(user_id) ;
			return ;
		}
	}
	return ;
}


// Returns if connection was successful
bool SteamworksPlugin::SteamSocket::connected(){
	return is_server || connections[0] != k_HSteamNetConnection_Invalid ; // TODO this may not be sufficient to detect disconnections
}

// Send data through this socket
// Returns if data seems to have sent (we can't know now if it arrived)
bool SteamworksPlugin::SteamSocket::send(int receiver_id, const std::vector<char>& data){
	if(connections.find(receiver_id) == connections.end()){
		//printf("Attempted to send packet to a connection we don't have: %d\n", receiver_id) ;
		return false ;
	}

	EResult res ;
	if(is_dedicated){
		res = SteamGameServerNetworkingSockets()->SendMessageToConnection(connections[receiver_id], data.data(), (uint32_t)data.size(), k_nSteamNetworkingSend_ReliableNoNagle, nullptr);
	}else{
		res = SteamNetworkingSockets()->SendMessageToConnection(connections[receiver_id], data.data(), (uint32_t)data.size(), k_nSteamNetworkingSend_ReliableNoNagle, nullptr);
	}
	switch (res){
		case k_EResultOK:
		case k_EResultIgnored:
			break;
		case k_EResultInvalidParam:
			printf("Failed sending data : Invalid connection handle, or the individual message is too big\n");
			return false;
		case k_EResultInvalidState:
			printf("Failed sending data : Connection is in an invalid state\n");
			return false;
		case k_EResultNoConnection:
			printf("Failed sending data : Connection has ended\n");
			removeServerClient(receiver_id) ;
			return false;
		case k_EResultLimitExceeded:
			printf("Failed sending data : There was already too much data queued to be sent\n");
			return false;
		default:
		{
			printf("SendMessageToConnection returned %d\n", res);
			return false;
		}
	}
	return true;
}

// close all connections in this socket and stop allowing new ones if applicable
void SteamworksPlugin::SteamSocket::close(){
	if (connected()){
		for( auto& [id,connection] : connections){
			SteamNetworkingSockets()->CloseConnection(connection, 0, nullptr, false);
		}
	}
	if(is_dedicated){
		SteamGameServerNetworkingSockets()->CloseListenSocket(listen_socket);
		SteamGameServerNetworkingSockets()->DestroyPollGroup(poll_group);
	}else if(is_server){
		SteamNetworkingSockets()->CloseListenSocket(listen_socket);
		SteamNetworkingSockets()->DestroyPollGroup(poll_group);
	}
}

//Set the object that will get packets when they are sent to this socket
void SteamworksPlugin::SteamSocket::setPacketReceiver(PacketReceiver* receiver){
	packet_receiver = receiver ;
}

void SteamworksPlugin::SteamSocket::processIncomingPackets(){
	if (packet_receiver != nullptr && connected()) {
		SteamNetworkingMessage_t* msgs[MESSAGE_QUEUE_SIZE];
		int res = -1 ;
		if(is_dedicated){
			res = SteamGameServerNetworkingSockets()->ReceiveMessagesOnPollGroup(poll_group, msgs, MESSAGE_QUEUE_SIZE) ;
		}else if(is_server){
			res =  SteamNetworkingSockets()->ReceiveMessagesOnPollGroup(poll_group, msgs, MESSAGE_QUEUE_SIZE) ;
		}else{
			res = SteamNetworkingSockets()->ReceiveMessagesOnConnection(connections[0], msgs, MESSAGE_QUEUE_SIZE);
		}

		for(int k=0;k < res;k ++){
			SteamNetworkingMessage_t* message  = msgs[k] ;
			std::vector<char> packet((char*)message->GetData(), (char*)message->GetData() + message->GetSize()) ;
			CSteamID source = message->m_identityPeer.GetSteamID();

			if(*(uint64_t*)(packet.data()) == AUTH_PACKET_MAGIC_NUMBER){
				printf("Got Steam Auth Ticket for %ld\n", (long)source.ConvertToUint64()) ;
				/*printf("auth packetsize: %d\n", packet.size()) ;
				for(int k=16;k<packet.size(); k++){
						printf("%c", packet[k]);
				}
				
					printf("\n");
				*/
				SteamGameServer()->BeginAuthSession(packet.data()+16, (int)packet.size()-16, source);
				authenticated[source] = true;

			}else{
				packet_receiver->receivePacket(steamIDToPlayer[source],packet);
			}
		}
	}
}

SteamworksPlugin::SteamSocket::~SteamSocket(){
	close();
}

