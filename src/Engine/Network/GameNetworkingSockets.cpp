#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>

struct client_state
{
    HSteamNetConnection Connection;
    bool Connected;
};

static client_state GlobalClientContext;

static void
SteamDebugOutput(ESteamNetworkingSocketsDebugOutputType Type, const char* Message)
{
    Log("(DEBUG) Steam networking message: %s\n", Message);
}

static void
InitialiseGameNetworkingSockets()
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
            Log("Could not initialise steam networking library: %s\n", Error);
        }
    }
}

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
                Log("(CLIENT) Disconnected: Connection %s, reason %d: %s\n",
                    Info->m_info.m_szConnectionDescription,
                    Info->m_info.m_eEndReason,
                    Info->m_info.m_szEndDebug);
                
                GlobalClientContext.Connected = false;
            }
        } break;
        case k_ESteamNetworkingConnectionState_Connected:
        {
            GlobalClientContext.Connected = true;
            Log("(CLIENT) Connected\n");
        } break;
        case k_ESteamNetworkingConnectionState_Connecting:
        {
            Log("(CLIENT) Connecting\n");
        }break;
        default: Assert(0);
    }
}

static void
ConnectToServer(char* Hostname)
{
    InitialiseGameNetworkingSockets();
    
    Assert(GlobalClientContext.Connection == 0);
    
    ISteamNetworkingSockets* Interface = SteamNetworkingSockets();
    
    SteamNetworkingIPAddr Address;
    Address.Clear();
    Address.ParseString(Hostname);
    Address.m_port = DefaultPort;
    
    SteamNetworkingConfigValue_t Config;
    Config.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)Client_SteamConnectionStatusChanged);
    
    GlobalClientContext.Connection = Interface->ConnectByIPAddress(Address, 1, &Config);
    
    Assert(GlobalClientContext.Connection != k_HSteamNetConnection_Invalid);
    
    packet EmptyPacket = {};
    SendToServer(EmptyPacket);
}

static span<packet> 
PollClientConnection(memory_arena* Arena)
{
    if (!GlobalClientContext.Connection)
    {
        return {};
    }
    
    dynamic_array<packet> Result = {.Arena = Arena};
    
    ISteamNetworkingSockets* Interface = SteamNetworkingSockets();
    
    while (true)
    {
        ISteamNetworkingMessage* IncomingMessage = 0;
        int MessageCount = Interface->ReceiveMessagesOnConnection(GlobalClientContext.Connection, &IncomingMessage, 1);
        
        Assert(MessageCount >= 0);
        
        if (MessageCount > 0)
        {
            Assert(MessageCount == 1 && IncomingMessage);
            
            u64 Bytes = IncomingMessage->m_cbSize;
            u8* Dest = Alloc(Arena, Bytes);
            
            memcpy(Dest, IncomingMessage->m_pData, Bytes);
            
            packet Packet = {.Data = Dest, .Length = Bytes};
            
            Append(&Result, Packet);
            
            IncomingMessage->Release();
        }
        else
        {
            break;
        }
    }
    
    Interface->RunCallbacks();
    
    return ToSpan(Result);
}

void SendToServer(packet Packet)
{
    ISteamNetworkingSockets* Interface = SteamNetworkingSockets();
    Interface->SendMessageToConnection(GlobalClientContext.Connection, Packet.Data, Packet.Length, k_nSteamNetworkingSend_Reliable, 0);
}

static bool
IsConnectedToServer()
{
    return GlobalClientContext.Connected;
}

struct server_state
{
    ISteamNetworkingSockets* Interface;
    HSteamListenSocket ListenSocket;
    HSteamNetPollGroup PollGroup;
    
    HSteamNetConnection Clients[8];
    u64 ClientCount = 0;
};

static server_state GlobalServerState;

static u64 
GetClientIndex(HSteamNetConnection Connection)
{
    for (u64 CheckClientIndex = 0; CheckClientIndex < GlobalServerState.ClientCount; CheckClientIndex++)
    {
        if (GlobalServerState.Clients[CheckClientIndex] == Connection)
        {
            return CheckClientIndex;
        }
    }
    
    if (GlobalServerState.ClientCount < ArrayCount(GlobalServerState.Clients))
    {
        u64 ClientIndex = GlobalServerState.ClientCount++;
        GlobalServerState.Clients[ClientIndex] = Connection;
        return ClientIndex;
    }
    
    Assert(0);
    return 0;
}

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
                Log("(SERVER) Disconnected: Connection %s, reason %d: %s\n",
                    Info->m_info.m_szConnectionDescription,
                    Info->m_info.m_eEndReason,
                    Info->m_info.m_szEndDebug);
                
                u64 ClientIndex = GetClientIndex(Info->m_hConn);
                GlobalServerState.Clients[ClientIndex] = {};
                
                GlobalServerState.Interface->CloseConnection(Info->m_hConn, 0, 0, false);
            }
        } break;
        
        case k_ESteamNetworkingConnectionState_Connected:
        {
            Log("(SERVER) Connected: Connection %s\n", Info->m_info.m_szConnectionDescription);
        } break;
        
        case k_ESteamNetworkingConnectionState_Connecting:
        {
            Assert(GlobalServerState.ClientCount < ArrayCount(GlobalServerState.Clients));
            
            //TODO: Check return values
            GlobalServerState.Interface->AcceptConnection(Info->m_hConn);
            GlobalServerState.Interface->SetConnectionPollGroup(Info->m_hConn, GlobalServerState.PollGroup);
            
            u64 PlayerIndex = (GlobalServerState.ClientCount++); //TODO: Bounds checking
            GlobalServerState.Clients[PlayerIndex] = Info->m_hConn;
            
            Log("(SERVER) Connecting: Connection %s\n", Info->m_info.m_szConnectionDescription);
        } break;
        default: Assert(0);
    }
}

static span<packet>
PollServerConnection(memory_arena* Arena)
{
    if (!GlobalServerState.ListenSocket)
    {
        return {};
    }
    
    dynamic_array<packet> Result = {.Arena = Arena};
    
    ISteamNetworkingSockets* Interface = GlobalServerState.Interface;
    
    while (true)
    {
        ISteamNetworkingMessage* IncomingMessage = 0;
        int MessageCount = Interface->ReceiveMessagesOnPollGroup(GlobalServerState.PollGroup, &IncomingMessage, 1);
        
        Assert(MessageCount >= 0);
        
        if (MessageCount > 0)
        {
            Assert(MessageCount == 1 && IncomingMessage);
            
            u64 Bytes = IncomingMessage->m_cbSize;
            u8* Dest = Alloc(Arena, Bytes);
            u64 ClientIndex = GetClientIndex(IncomingMessage->m_conn);
            
            memcpy(Dest, IncomingMessage->m_pData, Bytes);
            
            packet Packet = {.Data = Dest, .Length = Bytes, .SenderIndex = ClientIndex};
            
            Append(&Result, Packet);
            
            IncomingMessage->Release();
        }
        else
        {
            break;
        }
        
        GlobalServerState.Interface->RunCallbacks();
    }
    
    return ToSpan(Result);
}

static void
HostServer()
{
    Assert(GlobalServerState.ListenSocket == 0);
    
    InitialiseGameNetworkingSockets();
    
    SteamNetworkingIPAddr ServerAddress;
    ServerAddress.Clear();
    ServerAddress.m_port = DefaultPort;
    
    SteamNetworkingConfigValue_t Config;
    Config.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)Server_SteamConnectionStatusChanged);
    
    GlobalServerState.Interface = SteamNetworkingSockets();
    
    GlobalServerState.ListenSocket = GlobalServerState.Interface->CreateListenSocketIP(ServerAddress, 1, &Config);
    Assert(GlobalServerState.ListenSocket != k_HSteamListenSocket_Invalid);
    
    GlobalServerState.PollGroup = GlobalServerState.Interface->CreatePollGroup();
    Assert(GlobalServerState.PollGroup != k_HSteamNetPollGroup_Invalid);
    
    Log("(SERVER) Created server\n");
}

static void
ServerSendPacket(packet Packet, u64 ClientIndex)
{
    Log("(SERVER) Sending packet to %d\n", ClientIndex);
    
    Assert(ClientIndex < GlobalServerState.ClientCount);
    
    GlobalServerState.Interface->SendMessageToConnection(GlobalServerState.Clients[ClientIndex],
                                                         (u8*)Packet.Data, Packet.Length,
                                                         k_nSteamNetworkingSend_Reliable, 0);
    GlobalServerState.Interface->RunCallbacks();
}

static void
ServerBroadcastPacket(packet Packet)
{
    Log("(SERVER) Broadcasting packet\n");
    
    for (u64 ClientIndex = 0; ClientIndex < GlobalServerState.ClientCount; ClientIndex++)
    {
        GlobalServerState.Interface->SendMessageToConnection(GlobalServerState.Clients[ClientIndex],
                                                             (u8*)Packet.Data, Packet.Length,
                                                             k_nSteamNetworkingSend_Reliable, 0);
    }
    GlobalServerState.Interface->RunCallbacks();
}