u16   const DefaultPort = 22333;
char* const DefaultPortString = "22333";

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
void         ClientNetworkThread(char* Hostname);

span<packet> PollClientConnection(memory_arena* Arena);
void         SendToServer(packet Packet);
bool         IsConnectedToServer();

//Server API
void         ServerNetworkThread();

span<packet> PollServerConnection(memory_arena* Arena);
void         ServerSendPacket(packet Packet, u64  ClientIndex);
void         ServerBroadcastPacket(packet Packet);