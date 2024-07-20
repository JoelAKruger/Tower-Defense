#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "lib/enet64.lib")

#include <enet/enet.h>

static u16 Port = 22333;

ENetHost* ServerHost;
ENetPeer* ServerPeer;
bool Connected;

static bool
ConnectToServer(char* Hostname)
{
    bool Result = false;
    
    int ChannelCount = 2;
    int ConnectionCount = 1;
    
    ServerHost = enet_host_create(0, ConnectionCount, ChannelCount, 0, 0);
    
    ENetAddress Address = {};
    enet_address_set_host(&Address, Hostname);
    Address.port = Port;
    
    ServerPeer = enet_host_connect(ServerHost, &Address, ChannelCount, 0);
    
    return Result;
}

static bool
CheckForServerUpdate(global_game_state* Game)
{
    if (ServerHost)
    {
        ENetEvent Event = {};
        while (enet_host_service(ServerHost, &Event, 0) > 0)
        {
            switch (Event.type)
            {
                case ENET_EVENT_TYPE_CONNECT:
                {
                    Connected = true;
                } break;
                case ENET_EVENT_TYPE_RECEIVE:
                {
                    ENetPacket* Packet = Event.packet;
                    
                    Assert(Packet->dataLength == sizeof(global_game_state));
                    *Game = *(global_game_state*)Packet->data;
                    
                    enet_packet_destroy(Packet);
                } break;
                case ENET_EVENT_TYPE_DISCONNECT:
                {
                    Connected = false;
                } break;
                default:
                {
                    Assert(0);
                }
            }
        }
    }
    
    return Connected;
}

static bool
PlatformSend(u8* Data, u32 Length)
{
    ENetPacket* Packet = enet_packet_create((void*)Data, Length, ENET_PACKET_FLAG_RELIABLE);
    
    enet_peer_send(ServerPeer, 0, Packet);
    
    enet_host_flush(ServerHost);
    enet_packet_destroy(Packet); 
    
    return true;
}

struct client_info
{
    u32 ClientIndex;
};

void ServerHandleRequest(global_game_state* Game, u32 SenderIndex, player_request* Request);

global_game_state* ServerState_;

DWORD WINAPI
ServerMain(LPVOID)
{
    enet_initialize();
    
    ENetAddress Address = {};
    Address.host = ENET_HOST_ANY;
    Address.port = Port;
    
    ENetHost* Server = enet_host_create(&Address, 8, 2, 0, 0);
    LOG("Created server\n");
    
    global_game_state Game = {};
    InitialiseServerState(&Game);
    
    ServerState_ = &Game;
    
    ENetPeer* Clients[8];
    u32 ClientCount = 0;
    
    while (true)
    {
        ENetEvent Event = {};
        int ENetResult = enet_host_service(Server, &Event, 0);
        
        if (ENetResult > 0)
        {
            switch (Event.type)
            {
                case ENET_EVENT_TYPE_CONNECT:
                {
                    ENetPeer* Peer = Event.peer;
                    LOG("New connection from %x:%u\n", Peer->address.host, Peer->address.port);
                    
                    u32 ClientIndex = ClientCount++;
                    Assert(ClientIndex < ArrayCount(Clients));
                    
                    Clients[ClientIndex] = Peer;
                    
                    client_info* ClientInfo = new client_info{};
                    ClientInfo->ClientIndex = ClientIndex;
                    Peer->data = ClientInfo;
                } break;
                case ENET_EVENT_TYPE_RECEIVE:
                {
                    LOG("Received data");
                    ENetPacket* Packet = Event.packet;
                    client_info* ClientInfo = (client_info*)Event.peer->data;
                    
                    Assert(Packet->dataLength == sizeof(player_request));
                    player_request* Request = (player_request*)Packet->data;
                    
                    ServerHandleRequest(&Game, ClientInfo->ClientIndex, Request);
                    
                    enet_packet_destroy(Packet);
                } break;
                case ENET_EVENT_TYPE_DISCONNECT:
                {
                    LOG("Client disconnected");
                    
                    client_info* ClientInfo = (client_info*)Event.peer->data;
                    delete ClientInfo;
                } break;
                default:
                {
                    Assert(0);
                }
            }
            
            //Send new state to clients
            //TODO: Check if this is necessary
            
            ENetPacket* Packet = enet_packet_create((void*)&Game, sizeof(Game), ENET_PACKET_FLAG_RELIABLE);
            
            for (u32 ClientIndex = 0; ClientIndex < ClientCount; ClientIndex++)
            {
                enet_peer_send(Clients[ClientIndex], 0, Packet);
            }
            
            enet_host_flush(Server);
            enet_packet_destroy(Packet); 
        }
        else
        {
            Sleep(10);
        }
    }
}

static void
Host()
{
    LPVOID Param = 0;
    CreateThread(0, 0, (LPTHREAD_START_ROUTINE)ServerMain, Param, 0, 0);
}