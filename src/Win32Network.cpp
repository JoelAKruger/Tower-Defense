const u16 DefaultPort = 22333;

static void
SteamDebugOutput(ESteamNetworkingSocketsDebugOutputType Type, const char* Message)
{
    LOG("Steam networking message: %s", Message);
}

static void
InitialiseSteamNetworking()
{
    static bool Initialised = false;
    if (!Initialised)
    {
        SteamNetworkingErrMsg Error;
        if (GameNetworkingSockets_Init(0, Error))
        {
            SteamNetworkingUtils()->SetDebugOutputFunction(k_ESteamNetworkingSocketsDebugOutputType_Msg, SteamDebugOutput);
            Initialised = true;
        }
        else
        {
            LOG("Could not initialise steam networking library: %s\n", Error);
        }
    }
}

static multiplayer_context* GlobalClientContext;

static void
Client_SteamConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* Info)
{
    switch (Info->m_info.m_eState)
    {
        case k_ESteamNetworkingConnectionState_ClosedByPeer:
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
        {
            if (Info->m_eOldState == k_ESteamNetworkingConnectionState_Connected)
            {
                //Disconnected
                LOG("(CLIENT) Disconnected: Connection %s, reason %d: %s\n",
                    Info->m_info.m_szConnectionDescription,
                    Info->m_info.m_eEndReason,
                    Info->m_info.m_szEndDebug);
                
                GlobalClientContext->Connected = false;
            }
        } break;
        case k_ESteamNetworkingConnectionState_Connected:
        {
            GlobalClientContext->Connected = true;
            LOG("(CLIENT) Connected");
        } break;
        case k_ESteamNetworkingConnectionState_Connecting:
        {
        }break;
        default: Assert(0);
    }
}

static void
ConnectToServer(multiplayer_context* Context, char* Hostname)
{
    GlobalClientContext = Context;
    
    ISteamNetworkingSockets* Interface = SteamNetworkingSockets();
    
    SteamNetworkingIPAddr Address;
    Address.Clear();
    Address.ParseString(Hostname);
    Address.m_port = DefaultPort;
    
    SteamNetworkingConfigValue_t Config;
    Config.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)Client_SteamConnectionStatusChanged);
    
    Context->Platform.Connection = Interface->ConnectByIPAddress(Address, 1, &Config);
    
    Assert(Context->Platform.Connection != k_HSteamNetConnection_Invalid);
}


static void
DisconnectFromServer()
{
}

static void AddMessage(server_message_queue* Queue, server_packet_message Message);

static void
CheckForServerUpdate(global_game_state* Game, multiplayer_context* Context)
{
    if (!Context->Platform.Connection)
    {
        return;
    }
    
    ISteamNetworkingSockets* Interface = SteamNetworkingSockets();
    while (true)
    {
        ISteamNetworkingMessage* IncomingMessage = 0;
        int MessageCount = Interface->ReceiveMessagesOnConnection(Context->Platform.Connection, 
                                                                  &IncomingMessage, 1);
        
        Assert(MessageCount >= 0);
        
        if (MessageCount > 0)
        {
            Assert(MessageCount == 1 && IncomingMessage);
            
            channel Channel = *(channel*)IncomingMessage->m_pData;
            switch (Channel)
            {
                case Channel_GameState:
                {
                    LOG("Client: received game state\n");
                    Assert(IncomingMessage->m_cbSize == sizeof(server_packet_game_state));
                    server_packet_game_state* Packet = (server_packet_game_state*)IncomingMessage->m_pData;
                    
                    *Game = Packet->ServerGameState;
                    
                    server_packet_message Message = {Channel_Message, Message_NewWorld};
                    AddMessage(&Context->MessageQueue, Message);
                } break;
                case Channel_Message:
                {
                    LOG("Client: received message\n");
                    Assert(IncomingMessage->m_cbSize == sizeof(server_packet_message));
                    server_packet_message* Message = (server_packet_message*)IncomingMessage->m_pData;
                    AddMessage(&Context->MessageQueue, *Message);
                } break;
                default: Assert(0);
            }
            
            IncomingMessage->Release();
        }
        else
        { 
            break;
        }
    }
    
    Interface->RunCallbacks();
}

static bool
PlatformSend(multiplayer_context* Context, u8* Data, u32 Length)
{
    ISteamNetworkingSockets* Interface = SteamNetworkingSockets();
    Interface->SendMessageToConnection(Context->Platform.Connection, Data, Length, k_nSteamNetworkingSend_Reliable, 0);
    
    return true;
}

void ServerHandleRequest(global_game_state* Game, u32 SenderIndex, player_request* Request, server_message_queue* MessageQueue);
void InitialisePlayer(global_game_state* Game, u32 PlayerIndex);

struct server_state
{
    ISteamNetworkingSockets* Interface;
    HSteamListenSocket ListenSocket;
    HSteamNetPollGroup PollGroup;
    
    bool SendFlag;
    HSteamNetConnection Clients[8];
    u32 ClientCount = 0;
};

server_state ServerState = {};

static u64 
GetClientIndex(HSteamNetConnection Connection)
{
    for (u32 CheckClientIndex = 0; CheckClientIndex < ArrayCount(ServerState.Clients); CheckClientIndex++)
    {
        if (ServerState.Clients[CheckClientIndex] == Connection)
        {
            return CheckClientIndex;
        }
    }
    Assert(0);
    return 0;
}

global_game_state* GlobalServerGameState_;

static void
Server_SteamConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* Info)
{
    switch (Info->m_info.m_eState)
    {
        case k_ESteamNetworkingConnectionState_ClosedByPeer:
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
        {
            if (Info->m_eOldState == k_ESteamNetworkingConnectionState_Connected)
            {
                //Disconnected
                LOG("(SERVER) Disconnected: Connection %s, reason %d: %s\n",
                    Info->m_info.m_szConnectionDescription,
                    Info->m_info.m_eEndReason,
                    Info->m_info.m_szEndDebug);
                
                u64 ClientIndex = GetClientIndex(Info->m_hConn);
                ServerState.Clients[ClientIndex] = {};
                
                ServerState.Interface->CloseConnection(Info->m_hConn, 0, 0, false);
            }
        } break;
        
        case k_ESteamNetworkingConnectionState_Connected:
        {
            LOG("(SERVER) Connected: Connection %s\n", Info->m_info.m_szConnectionDescription);
        } break;
        
        case k_ESteamNetworkingConnectionState_Connecting:
        {
            Assert(ServerState.ClientCount < ArrayCount(ServerState.Clients));
            
            //TODO: Check return values
            ServerState.Interface->AcceptConnection(Info->m_hConn);
            ServerState.Interface->SetConnectionPollGroup(Info->m_hConn, ServerState.PollGroup);
            
            u64 PlayerIndex = (ServerState.ClientCount++);
            ServerState.Clients[PlayerIndex] = Info->m_hConn;
            
            GlobalServerGameState_->PlayerCount = ServerState.ClientCount;
            InitialisePlayer(GlobalServerGameState_, PlayerIndex);
            
            LOG("(SERVER) Connecting: Connection %s\n", Info->m_info.m_szConnectionDescription);
            
            server_packet_message Message = {Channel_Message};
            Message.Type = Message_Initialise;
            Message.InitialiseClientID = PlayerIndex;
            ServerState.Interface->SendMessageToConnection(Info->m_hConn, (void*)&Message, sizeof(Message), k_nSteamNetworkingSend_Reliable, 0);
            
            ServerState.SendFlag = true;
        } break;
        default: Assert(0);
    }
}

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
    
    LOG("Created server\n");
    
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
            
            LOG("Server: Received data from client\n");
            
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