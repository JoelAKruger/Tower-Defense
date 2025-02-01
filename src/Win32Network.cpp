#include "Network/GameNetworkingSockets.cpp"

#if 0
static void
DisconnectFromServer()
{
}

static void AddMessage(server_message_queue* Queue, server_packet_message Message);

static void
CheckForServerUpdate(global_game_state* Game, multiplayer_context* Context, memory_arena* Arena)
{
    Assert(Arena->Type == TRANSIENT);
    
    span<packet> Packets = PollClientConnection(Arena);
    
    for (packet RawPacket : Packets)
    {
        Assert(RawPacket.Length >= sizeof(channel));
        
        channel Channel = *(channel*)RawPacket.Data;
        switch (Channel)
        {
            case Channel_GameState:
            {
                Log("Client: received game state\n");
                Assert(RawPacket.Length == sizeof(server_packet_game_state));
                server_packet_game_state* Packet = (server_packet_game_state*)RawPacket.Data;
                
                *Game = Packet->ServerGameState;
                
                server_packet_message Message = {Channel_Message, Message_NewWorld};
                AddMessage(&Context->MessageQueue, Message);
            } break;
            case Channel_Message:
            {
                Log("Client: received message\n");
                Assert(RawPacket.Length == sizeof(server_packet_message));
                server_packet_message* Message = (server_packet_message*)RawPacket.Data;
                AddMessage(&Context->MessageQueue, *Message);
            } break;
            default: Assert(0);
        }
    }
}

static bool
PlatformSend(multiplayer_context* Context, u8* Data, u64 Length)
{
    ISteamNetworkingSockets* Interface = SteamNetworkingSockets();
    Interface->SendMessageToConnection(GlobalClientContext.Connection, Data, Length, k_nSteamNetworkingSend_Reliable, 0);
    
    return true;
}

void ServerHandleRequest(global_game_state* Game, u32 SenderIndex, player_request* Request, server_message_queue* MessageQueue);
void InitialisePlayer(global_game_state* Game, u32 PlayerIndex);

server_state ServerState = {};

global_game_state* GlobalServerGameState_;

#if 0
DWORD WINAPI
ServerMain(LPVOID)
{
    SteamNetworkingIPAddr ServerAddress;
    ServerAddress.Clear();
    ServerAddress.m_port = DefaultPort;
    
    SteamNetworkingConfigValue_t Config;
    Config.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)Server_SteamConnectionStatusChanged);
    
    ServerState.Interface = SteamNetworkingSockets();
    
    ServerState.ListenSocket = ServerState.Interface->CreateListenSocketIP(ServerAddress, 1, &Config);
    Assert(ServerState.ListenSocket != k_HSteamListenSocket_Invalid);
    
    ServerState.PollGroup = ServerState.Interface->CreatePollGroup();
    Assert(ServerState.PollGroup != k_HSteamNetPollGroup_Invalid);
    
    Log("Created server\n");
    
    global_game_state Game = {};
    GlobalServerGameState_ = &Game;
    
    InitialiseServerState(&Game);
    
    server_message_queue MessageQueue = {};
    
    while (true)
    {
        ISteamNetworkingMessage* IncomingMessage = 0;
        int MessageCount = ServerState.Interface->ReceiveMessagesOnPollGroup(ServerState.PollGroup, &IncomingMessage, 1);
        
        Assert(MessageCount >= 0);
        
        if (MessageCount > 0)
        {
            Assert(MessageCount == 1 && IncomingMessage);
            
            Log("Server: Received data from client\n");
            
            Assert(IncomingMessage->m_cbSize == sizeof(player_request));
            player_request* Request = (player_request*)IncomingMessage->m_pData;
            
            u64 ClientIndex = GetClientIndex(IncomingMessage->m_conn);
            ServerHandleRequest(&Game, ClientIndex, Request, &MessageQueue);
            
            IncomingMessage->Release();
            
            ServerState.SendFlag = true;
        }
        else
        {
            Sleep(10);
        }
        
        if (ServerState.SendFlag)
        {
            //Send new state to clients
            server_packet_game_state GameStatePacket = {Channel_GameState};
            GameStatePacket.ServerGameState = Game;
            for (u32 ClientIndex = 0; ClientIndex < ServerState.ClientCount; ClientIndex++)
            {
                ServerState.Interface->SendMessageToConnection(ServerState.Clients[ClientIndex], (void*)&GameStatePacket, sizeof(GameStatePacket), k_nSteamNetworkingSend_Reliable, 0);
            }
            
            //Send messages to clients
            for (u32 MessageIndex = 0; MessageIndex < MessageQueue.MessageCount; MessageIndex++)
            {
                server_packet_message* Message = MessageQueue.Messages + MessageIndex;
                
                for (u32 ClientIndex = 0; ClientIndex < ServerState.ClientCount; ClientIndex++)
                {
                    ServerState.Interface->SendMessageToConnection(ServerState.Clients[ClientIndex], (void*)Message, sizeof(*Message), k_nSteamNetworkingSend_Reliable, 0);
                }
            }
            
            MessageQueue.MessageCount = 0;
            
            ServerState.SendFlag = false;
        }
        
        ServerState.Interface->RunCallbacks();
    }
}

static void
Host()
{
    LPVOID Param = 0;
    CreateThread(0, 0, (LPTHREAD_START_ROUTINE)ServerMain, Param, 0, 0);
}
#endif

#endif