#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment (lib, "Ws2_32.lib")

struct client
{
    bool Connected;
    SOCKET Socket;
    
    mutex Mutex;
    memory_arena ReceivedPacketArena;
    dynamic_array<packet> ReceivedPackets;
};

static client* Client;

//Client API
static void 
ClientNetworkThread(char* Hostname)
{
    memory_arena PermArena = Win32CreateMemoryArena(Megabytes(1), PERMANENT);
    Assert(!Client);
    Client = AllocStruct(&PermArena, client);
    Client->ReceivedPacketArena = Win32CreateMemoryArena(Megabytes(1), TRANSIENT);
    Client->ReceivedPackets = {.Arena = &Client->ReceivedPacketArena};
    
    WSADATA WsaData = {};
    
    WSAStartup(MAKEWORD(2,2), &WsaData);
    
    addrinfo Hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP
    };
    
    addrinfo* Result = 0;
    getaddrinfo(Hostname, DefaultPortString, &Hints, &Result);
    
    for (addrinfo* At = Result; At; At = At->ai_next)
    {
        Client->Socket = socket(At->ai_family, At->ai_socktype, At->ai_protocol);
        
        int ConnectResult = connect(Client->Socket, At->ai_addr, (int)At->ai_addrlen);
        if (ConnectResult != SOCKET_ERROR)
        {
            break;
        }
    }
    
    freeaddrinfo(Result);
    
    if (Client->Socket != INVALID_SOCKET)
    {
        Client->Connected = true;
    }
    
    u_long Mode = 1; // 1 = non-blocking, 0 = blocking
    ioctlsocket(Client->Socket, FIONBIO, &Mode);
}

static span<packet> 
PollClientConnection(memory_arena* Arena)
{
    dynamic_array<packet> Result = {.Arena = Arena};
    
    if (Client && Client->Connected)
    {
        while (true)
        {
            int RecvResult = recv(Client->Socket, (char*)ArenaAt(Arena), ArenaFreeSpace(Arena), 0);
            
            if (RecvResult > 0)
            {
                u64 Bytes = (u64)RecvResult;
                packet Packet = {.Data = ArenaAt(Arena), .Length = Bytes, .SenderIndex = 0};
                Arena->Used += Bytes;
                Append(&Result, Packet);
                
                Log("Client received data\n");
            }
            else if (RecvResult == 0)
            {
                //Connection closed
                Log("Closed\n");
                break;
            }
            else
            {
                //Would block (or another error)
                break;
            }
        }
    }
    
    return ToSpan(Result);
}
void SendToServer(packet Packet)
{
    if (Client && Client->Connected)
    {
        send(Client->Socket, (char*)Packet.Data, Packet.Length, 0);
    }
}
bool IsConnectedToServer()
{
    return (Client && Client->Connected);
}

struct server
{
    bool Initialised;
    mutex Mutex;
    memory_arena ReceivedPacketArena;
    dynamic_array<packet> ReceivedPackets;
    
    SOCKET Clients[8];
    u64 ClientCount;
};

server* Server;

//-------Server API--------

//Server network thread
void ServerNetworkThread()
{
    memory_arena PermArena = Win32CreateMemoryArena(Megabytes(1), PERMANENT);
    
    Assert(!Server);
    Server = AllocStruct(&PermArena, server);
    Server->ReceivedPacketArena = Win32CreateMemoryArena(Megabytes(1), TRANSIENT);
    Server->ReceivedPackets = {.Arena = &Server->ReceivedPacketArena};
    
    WSADATA WsaData = {};
    WSAStartup(MAKEWORD(2,2), &WsaData);
    
    addrinfo Hints = {
        .ai_flags = AI_PASSIVE,
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP
    };
    
    addrinfo* Result = 0;
    int GetAddrInfoResult = getaddrinfo(0, DefaultPortString, &Hints, &Result);
    
    SOCKET ListenSocket = socket(Result->ai_family, Result->ai_socktype, Result->ai_protocol);
    
    int BindResult = bind(ListenSocket, Result->ai_addr, (int)Result->ai_addrlen);
    
    freeaddrinfo(Result);
    
    int ListenResult = listen(ListenSocket, SOMAXCONN);
    
    FD_SET Set = {};
    FD_SET(ListenSocket, &Set);
    
    Server->Initialised = true;
    
    while (true)
    {
        FD_SET TempSet = Set;
        
        int SelectResult = select(0, &TempSet, 0, 0, 0);
        Assert(SelectResult >= 0);
        
        for (int I = 0; I < TempSet.fd_count; I++)
        {
            SOCKET Socket = TempSet.fd_array[I];
            
            if (Socket == ListenSocket)
            {
                SOCKET NewClient = accept(Socket, 0, 0);
                Assert(Server->ClientCount < ArrayCount(Server->Clients));
                Server->Clients[Server->ClientCount++] = NewClient;
                FD_SET(NewClient, &Set);
            }
            else
            {
                Lock(&Server->Mutex);
                int Bytes = recv(Socket, (char*)Server->ReceivedPacketArena.Buffer + Server->ReceivedPacketArena.Used, 
                                 Server->ReceivedPacketArena.Size - Server->ReceivedPacketArena.Used, 0);
                
                packet Packet = {
                    .Data = Server->ReceivedPacketArena.Buffer + Server->ReceivedPacketArena.Used, 
                    .Length = (u64)Bytes, 
                    .SenderIndex = 0
                };
                
                Server->ReceivedPacketArena.Used += Bytes;
                
                Append(&Server->ReceivedPackets, Packet);
                
                Unlock(&Server->Mutex);
                
                Log("Received data\n");
            }
        }
    }
}

//Called from server logic thread
span<packet> PollServerConnection(memory_arena* Arena)
{
    dynamic_array<packet> Result = {.Arena = Arena};
    //Log("Polled\n");
    
    if (Server && Server->Initialised)
    {
        Lock(&Server->Mutex);
        
        for (u64 PacketIndex = 0; PacketIndex < Server->ReceivedPackets.Count; PacketIndex++)
        {
            //Copy data from network thread to logic thread
            packet Packet = Server->ReceivedPackets[PacketIndex];
            Packet.Data = Copy(Arena, Packet.Data, Packet.Length);
            Append(&Result, Packet);
            
            Log("Copying data\n");
        }
        
        Clear(&Server->ReceivedPackets);
        
        ResetArena(&Server->ReceivedPacketArena);
        
        Unlock(&Server->Mutex);
    }
    
    return ToSpan(Result);
}

void ServerSendPacket(packet Packet, u64 ClientIndex)
{
    Assert(ClientIndex < Server->ClientCount);
    send(Server->Clients[ClientIndex], (char*)Packet.Data, Packet.Length, 0);
    
    Log("Server sending data of length %d\n", Packet.Length);
}

void ServerBroadcastPacket(packet Packet)
{
    for (u64 ClientIndex = 0; ClientIndex < Server->ClientCount; ClientIndex++)
    {
        send(Server->Clients[ClientIndex], (char*)Packet.Data, Packet.Length, 0);
    }
    Log("Server broadcasting data of length %d\n", Packet.Length);
}