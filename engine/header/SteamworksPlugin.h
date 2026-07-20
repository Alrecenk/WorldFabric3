#ifndef _STEAMWORKS_PLUGIN_H_
#define _STEAMWORKS_PLUGIN_H_ 1

#include "AsyncPlugin.h"
#include "Variant.h"
#include "Utilities.h"
#include "steamworks/steam_api.h"
#include "steamworks/isteamnetworkingsockets.h"
#include "steamworks/isteamnetworkingutils.h"
#include "steamworks/steamclientpublic.h"
#include "steamworks/steam_gameserver.h"
#include "Sockets.h"


#include <string>
#include <unordered_map>
#include <unordered_set>





class SteamworksPlugin : public AsyncPlugin, public ISteamMatchmakingServerListResponse{

public:

	class SteamAchievement {
	public:
		std::string id;
		std::string name;
		std::string description;
		bool achieved;
		int icon; // index in steamapi for the icon

	};


	struct SteamServerInfo {
		CSteamID id;
		CSteamID host_id; // user id of the host player
		std::string name = "";
		std::string map = "";
		std::string game_mode = "";
		std::string product_name = "";
		std::string product_description = "";
		std::string version = "";
		std::string game_directory = "" ;
		bool has_password = false ;
		int players = 0;
		int max_players = 64;
		uint16 port = 0;
		uint32 address = 0;
		std::string connect  = "" ;
		int ping;
		std::map<std::string, std::string> extra_values;
	};

	class SteamSocket : public Socket {

	public:
		SteamSocket();


		// Open a steam client socket to connect to the given lobby id
		void join(CSteamID lobby_to_join);

		// Open a steam client socket to connect to an ip
		void join(SteamNetworkingIPAddr address_to_join) ;

		//Start listening for connections as a private p2P server	
		void host(CSteamID lobby_id, CSteamID host_id);

		//Start listening for connections as a dedicated game server
		void hostDedicated(const SteamNetworkingIPAddr address);

		void addServerClient(HSteamNetConnection connection, CSteamID user_id);

		void removeServerClient(CSteamID user_id);

		// Returns if connection was successful
		bool connected();

		// Send data through this socket
		// Returns if data seems to have sent (we can't know now if it arrived)
		bool send(int receiver_id, const std::vector<char>& data) override;

		// close all connections in this socket and stop allowing new ones if applicable
		void close() override;

		//Set the object that will get packets when they are sent to this socket
		void setPacketReceiver(PacketReceiver* receiver) override;

		void processIncomingPackets();

		~SteamSocket();

		PacketReceiver* packet_receiver = nullptr;
		SteamNetworkingIdentity identity;
		std::map<int, HSteamNetConnection> connections;
		std::map<CSteamID, int> steamIDToPlayer;
		std::map <CSteamID, bool> authenticated ;
		static inline const int MESSAGE_QUEUE_SIZE = 32;

		
		bool is_server = false;
		bool is_dedicated = false;
		HSteamListenSocket listen_socket; // Socket to listen for new connections on 
		HSteamNetPollGroup poll_group; // Poll group used to receive messages from all clients at once
		int next_player = 1;
	};


	class SteamEventReceiver {
	public:
		virtual void onSteamGameExternalJoin(std::shared_ptr<SteamSocket> socket, const SteamServerInfo& server_info) = 0;
	};


	static inline bool enabled = true;
	static inline std::string tag = "SteamLink";
	static inline bool wants_to_exit = false;
	static inline int STEAM_SERVER_MASTER_PORT = 27016 ; // Steam's server browser port, needs to be open for servers to be public
	static inline const uint64_t AUTH_PACKET_MAGIC_NUMBER = 0xBEBADBEBEEFED ; // a magic number ta the beginning of auth packets so we can ctach them when they come in out of order
	
	//Constructore for a typical game
	SteamworksPlugin(long app_id,const std::string& command_line);

	//Constructor for a dedicated sercver
	SteamworksPlugin(long app_id, const SteamServerInfo& server_info, int master_server_port, const std::string& command_line);

	~SteamworksPlugin();

	// Called on every plug-in before any plug-ins are run
	void initialize() override;

	// runs the plugin on its own thread
	void run() override;

	void pumpCallbacks() ;

	//attempts to mark a steam achievement as completed
	// returns if successful
	bool achieve(const std::string& id);

	//Returns the user's current steam display name to the local user
	std::string getLocalName();

	// Returns the current user's Steasm ID
	uint64 getLocalSteamID();

	//Sets the object to be notifie when external Steam events come in (like joining a match or opening the overlay)
	void setSteamEventReceiver(SteamEventReceiver* receiver);

	// Create a Steam lobby only joinable by friends
	// and return a socket to communicate with those who join it
	std::shared_ptr<SteamSocket> hostPrivateLobby(const SteamServerInfo& info) ;

	//Returns the current list of servers
	std::vector<SteamServerInfo> getServerList();

	bool serverListReady() ;

	//Refreshes the server list that will be returned by get Server List
	void refreshServerList() ;

	void ServerResponded(HServerListRequest hRequest, int iServer) override;

	void ServerFailedToRespond(HServerListRequest, int) override ;
	void RefreshComplete(HServerListRequest, EMatchMakingServerResponse) override ;

	// Join a Steam lobby and create a socket to communicate with its host
	std::shared_ptr<SteamSocket> joinLobby(CSteamID lobby_id);

	// Join a Steam ganme by ip and port 
	std::shared_ptr<SteamSocket> joinAddress(SteamNetworkingIPAddr& addr) ;
	
	//Returns the activ socket if there is one or a nullptr if not
	std::shared_ptr<SteamSocket> getActiveSocket() ;

	//Closes any open socket and leaves any connected lobby
	void disconnect() ;

private:
	long steamapp_id;
	std::vector<SteamAchievement> achievements;
	bool initialized = false; 
	bool dedicated_server = false;
	int singal_to_wait_for = -1 ;
	static inline SteamEventReceiver* event_receiver = nullptr ;
	static inline bool server_connected_to_steam = false;
	static inline bool client_can_join_game = false;
	static inline bool client_waiting_on_ip_join = false;
	std::unordered_set <std::string> achieved_this_run; // so we can avoid excessive steamapi calls

	HServerListRequest server_list_request = nullptr ;
	bool requesting_server_list = false;
	std::vector<SteamServerInfo> servers ;
	std::vector<SteamServerInfo> last_servers;

	static inline std::shared_ptr<SteamSocket> steam_socket ;
	static inline SteamServerInfo lobby_info ;

	static inline bool join_lobby_pending = false ;
	static inline CSteamID pending_join_lobby_id ;
	static inline bool join_address_pending = false ;
	static inline SteamNetworkingIPAddr pending_join_address;
	static inline std::chrono::high_resolution_clock::time_point creation_time = now();
	static const inline std::string connect_param = "+connect ";
	static const inline std::string connect_lobby_param = "+connect_lobby ";
	
	//Process a command line or rich command line argument from Steam to extract join information
	static bool commandLineHasLobbyJoin(const char* command_line) ;
	static CSteamID getCommandLineLobbyJoin(const char* command_line);
	static bool commandLineHasAddressJoin(const char* command_line);
	static SteamNetworkingIPAddr getCommandLineAddressJoin(const char* command_line);
	static void catchCommandLineJoin(const char* command_line);



	STEAM_CALLBACK(SteamworksPlugin,onLobbyCreated,LobbyCreated_t);
	STEAM_CALLBACK(SteamworksPlugin, onLobbyEntered, LobbyEnter_t);
	STEAM_CALLBACK(SteamworksPlugin, OnUserStatsReceived, UserStatsReceived_t,m_CallbackUserStatsReceived);
	STEAM_CALLBACK(SteamworksPlugin, OnUserStatsStored, UserStatsStored_t,m_CallbackUserStatsStored);
	STEAM_CALLBACK(SteamworksPlugin, OnAchievementStored, UserAchievementStored_t, m_CallbackAchievementStored);
	STEAM_GAMESERVER_CALLBACK(SteamworksPlugin, OnSteamServersConnected, SteamServersConnected_t);
	STEAM_GAMESERVER_CALLBACK(SteamworksPlugin, OnSteamServersConnectFailure, SteamServerConnectFailure_t);
	STEAM_GAMESERVER_CALLBACK(SteamworksPlugin, OnSteamServersDisconnected, SteamServersDisconnected_t);
	STEAM_GAMESERVER_CALLBACK(SteamworksPlugin, OnPolicyResponse, GSPolicyResponse_t);
	STEAM_GAMESERVER_CALLBACK(SteamworksPlugin, OnValidateAuthTicketResponse, ValidateAuthTicketResponse_t);
	STEAM_GAMESERVER_CALLBACK(SteamworksPlugin, OnServerConnectionStatusChanged, SteamNetConnectionStatusChangedCallback_t);
	STEAM_GAMESERVER_CALLBACK(SteamworksPlugin, onServerLobbyCreated, LobbyCreated_t);
	STEAM_CALLBACK(SteamworksPlugin, OnLobbyGameCreated, LobbyGameCreated_t);
	STEAM_CALLBACK(SteamworksPlugin, OnGameJoinRequested, GameRichPresenceJoinRequested_t);
	STEAM_CALLBACK(SteamworksPlugin, OnGameLobbyJoinRequested, GameLobbyJoinRequested_t) ;
	STEAM_CALLBACK(SteamworksPlugin, OnGameServerChangeRequested, GameServerChangeRequested_t);
	STEAM_CALLBACK(SteamworksPlugin, OnAvatarImageLoaded, AvatarImageLoaded_t);
	STEAM_CALLBACK(SteamworksPlugin, OnNewUrlLaunchParameters, NewUrlLaunchParameters_t);
	STEAM_CALLBACK(SteamworksPlugin, OnGameOverlayActivated, GameOverlayActivated_t);
	STEAM_CALLBACK(SteamworksPlugin, OnGameWebCallback, GameWebCallback_t);
	STEAM_CALLBACK(SteamworksPlugin, OnClientConnectionStatusChanged, SteamNetConnectionStatusChangedCallback_t);
	STEAM_CALLBACK(SteamworksPlugin, OnIPCFailure, IPCFailure_t);
	STEAM_CALLBACK(SteamworksPlugin, OnSteamShutdown, SteamShutdown_t);





};






#endif // #ifndef _STEAMWORKS_H_
