#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "lib/enet64.lib")

static u16 Port = 22333;

enum 
{
    Channel_Message,
    Channel_WorldData,
    
    Channel_Count
};

static void
ConnectToServer(multiplayer_context* Context, char* Hostname)
{
    int ConnectionCount = 1;
    
    Context->Platform.ServerHost = enet_host_create(0, ConnectionCount, Channel_Count, 0, 0);
    
    ENetAddress Address = {};
    enet_address_set_host(&Address, Hostname);
    Address.port = Port;
    
    Context->Platform.ServerPeer = enet_host_connect(Context->Platform.ServerHost, &Address, Channel_Count, 0);
    Context->Connected = true;
}


static void
DisconnectFromServer()
{
    
}

static void AddMessage(server_message_queue* Queue, server_message Message);

static void
CheckForServerUpdate(global_game_state* Game, multiplayer_context* Context)
{
    if (Context->Platform.ServerHost)
    {
        while (true)
        {
            ENetEvent Event = {};
            int ENetResult = enet_host_service(Context->Platform.ServerHost, &Event, 0);
            
            if (ENetResult == 0)
            {
                break;
            }
            
            Assert(ENetResult > 0);
            
            switch (Event.type)
            {
                case ENET_EVENT_TYPE_CONNECT:
                {
                    Context->Connected = true;
                } break;
                case ENET_EVENT_TYPE_RECEIVE:
                {
                    ENetPacket* Packet = Event.packet;
                    
                    switch (Event.channelID)
                    {
                        case Channel_WorldData:
                        {
                            Assert(Packet->dataLength == sizeof(global_game_state));
                            *Game = *(global_game_state*)Packet->data;
                            
                            enet_packet_destroy(Packet);
                        } break;
                        case Channel_Message:
                        {
                            Assert(Packet->dataLength == sizeof(server_message));
                            server_message* Message = (server_message*)Packet->data;
                            AddMessage(&Context->MessageQueue, *Message);
                        } break;
                        default:
                        {
                            Assert(0);
                        }
                    }
                } break;
                case ENET_EVENT_TYPE_DISCONNECT:
                {
                    Context->Connected = false;
                } break;
                default:
                {
                    Assert(0);
                }
            }
        }
    }
}

static bool
PlatformSend(multiplayer_context* Context, u8* Data, u32 Length)
{
    ENetPacket* Packet = enet_packet_create((void*)Data, Length, ENET_PACKET_FLAG_RELIABLE);
    
    enet_peer_send(Context->Platform.ServerPeer, 0, Packet);
    
    enet_host_flush(Context->Platform.ServerHost);
    enet_packet_destroy(Packet); 
    
    return true;
}

struct client_info
{
    u32 ClientIndex;
};

void ServerHandleRequest(global_game_state* Game, u32 SenderIndex, player_request* Request, server_message_queue* MessageQueue);

global_game_state* ServerState_;

DWORD WINAPI
ServerMain(LPVOID)
{
    enet_initialize();
    
    ENetAddress Address = {};
    Address.host = ENET_HOST_ANY;
    Address.port = Port;
    
    ENetHost* Server = enet_host_create(&Address, 8, Channel_Count, 0, 0);
    LOG("Created server\n");
    
    global_game_state Game = {};
    InitialiseServerState(&Game);
    
    ServerState_ = &Game;
    
    ENetPeer* Clients[8];
    u32 ClientCount = 0;
    
    server_message_queue MessageQueue = {};
    
    while (true)
    {
        ENetEvent Event = {};
        int ENetResult = enet_host_service(Server, &Event, 0);
        
        Assert(ENetResult >= 0);
        
        if (ENetResult > 0)
        {
            switch (Event.type)
            {
                case ENET_EVENT_TYPE_CONNECT:
                {
                    ENetPeer* Peer = Event.peer;
                    u32 Host = Peer->address.host;
                    LOG("New connection from %u.%u.%u.%u:%u\n", (u8)(Host), (u8)(Host >> 8), (u8)(Host >> 16), (u8)(Host >> 24) , Peer->address.port);
                    
                    u32 ClientIndex = ClientCount++;
                    Assert(ClientIndex < ArrayCount(Clients));
                    
                    Clients[ClientIndex] = Peer;
                    
                    client_info* ClientInfo = new client_info{};
                    ClientInfo->ClientIndex = ClientIndex;
                    Peer->data = ClientInfo;
                    
                    //Send client its client index
                    server_message Message = {};
                    Message.Type = Message_Initialise;
                    Message.InitialiseClientID = ClientIndex;
                    
                    ENetPacket* SendPacket = enet_packet_create((void*)&Message, sizeof(Message), ENET_PACKET_FLAG_RELIABLE);
                    
                    enet_peer_send(Peer, Channel_Message, SendPacket);
                    
                    Game.PlayerCount = ClientCount;
                } break;
                case ENET_EVENT_TYPE_RECEIVE:
                {
                    LOG("Received data\n");
                    ENetPacket* Packet = Event.packet;
                    client_info* ClientInfo = (client_info*)Event.peer->data;
                    
                    Assert(Packet->dataLength == sizeof(player_request));
                    player_request* Request = (player_request*)Packet->data;
                    
                    ServerHandleRequest(&Game, ClientInfo->ClientIndex, Request, &MessageQueue);
                    
                    enet_packet_destroy(Packet);
                } break;
                case ENET_EVENT_TYPE_DISCONNECT:
                {
                    LOG("Client disconnected\n");
                    
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
            ENetPacket* WorldPacket = enet_packet_create((void*)&Game, sizeof(Game), ENET_PACKET_FLAG_RELIABLE);
            
            for (u32 ClientIndex = 0; ClientIndex < ClientCount; ClientIndex++)
            {
                enet_peer_send(Clients[ClientIndex], Channel_WorldData, WorldPacket);
            }
            
            for (u32 MessageIndex = 0; MessageIndex < MessageQueue.MessageCount; MessageIndex++)
            {
                server_message* Message = MessageQueue.Messages + MessageIndex;
                ENetPacket* Packet = enet_packet_create((void*)Message, sizeof(*Message), ENET_PACKET_FLAG_RELIABLE);
                
                for (u32 ClientIndex = 0; ClientIndex < ClientCount; ClientIndex++)
                {
                    enet_peer_send(Clients[ClientIndex], Channel_Message, Packet);
                }
            }
            
            //Send messages to clients
            
            MessageQueue.MessageCount = 0;
            
            enet_host_flush(Server);
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