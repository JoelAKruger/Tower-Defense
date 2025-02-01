u16 const DefaultPort = 22333;

//TODO: Delete this
struct multiplayer_context;
struct platform_multiplayer_context;

struct packet
{
    u8* Data;
    u64 Length;
    u64 SenderIndex;
};

//Client API
void         ConnectToServer(char* Hostname);
span<packet> PollClientConnection(memory_arena* Arena);
void         SendToServer(packet Packet);
bool         IsConnectedToServer();

//Server API
void         HostServer();
span<packet> PollServerConnection(memory_arena* Arena);
void         ServerSendPacket(packet Packet, u64  ClientIndex);
void         ServerBroadcastPacket(packet Packet);

struct global_game_state;
bool PlatformSend(multiplayer_context* Context, u8* Data, u64 Length);
void CheckForServerUpdate(global_game_state* Game, multiplayer_context* Context, memory_arena* Arena);
void Host();
void ConnectToServer(char* Hostname);