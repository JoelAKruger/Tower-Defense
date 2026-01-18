struct client
{
    bool Connected;
    SOCKET Socket;
    
    mutex Mutex;
    memory_arena ReceivedPacketArena;
    dynamic_array<packet> ReceivedPackets;
};

static client* Client_;

//Client API
static void 
ClientNetworkThread(client_network_thread_data* Data)
{
    memory_arena PermArena = Win32CreateMemoryArena(Megabytes(1), PERMANENT);
    Assert(!Client_);
    client* Client = AllocStruct(&PermArena, client);
    Client->ReceivedPacketArena = Win32CreateMemoryArena(Megabytes(1), TRANSIENT);
    Client->ReceivedPackets = {.Arena = &Client->ReceivedPacketArena};
    Client_ = Client;
    
    WSADATA WsaData = {};
    
    WSAStartup(MAKEWORD(2,2), &WsaData);
    
    addrinfo Hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP
    };
    
    addrinfo* Result = 0;
    getaddrinfo(Data->Hostname, DefaultPortString, &Hints, &Result);
    
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
    
    /*
    u_long Mode = 1; // 1 = non-blocking, 0 = blocking
    ioctlsocket(Client->Socket, FIONBIO, &Mode);
    */
    
    //INSANE HACK BELOW
    channel Init = Channel_Init;
    packet Packet = {.Data = (u8*) &Init, .Length = sizeof(Init)};
    SendToServer(Packet);
    
    while (true)
    {
        //Receive channel
        channel Channel = {};
        recv(Client->Socket, (char*) &Channel, sizeof(Channel), MSG_PEEK);
        
        int Bytes = 0;
        switch (Channel)
        {
            case Channel_Init:      Bytes = sizeof(channel); break;
            case Channel_Message:   Bytes = Data->MessagePacketSize; break;
            case Channel_GameState: Bytes = Data->GameStatePacketSize; break;
            default: Assert(0);
        }
        
        //Get buffer
        Lock(&Client->Mutex);
        
        u8* Buffer = Alloc(&Client->ReceivedPacketArena, Bytes);
        
        //Receive data
        int BytesReceived = recv(Client->Socket, (char*) Buffer, Bytes, MSG_WAITALL);
        
        Assert(BytesReceived == Bytes);
        
        //Add packet to the array
        Log("Packet: Data = %x, Length = %lu\n", Buffer, Bytes);
        packet Packet = {.Data = Buffer, .Length = (u64) Bytes};
        Append(&Client->ReceivedPackets, Packet);
        
        Unlock(&Client->Mutex);
    }
}

static span<packet> 
PollClientConnection(memory_arena* Arena)
{
    client* Client = Client_;
    
    dynamic_array<packet> Result = {.Arena = Arena};
    
    if (Client && Client->Connected)
    {
        Lock(&Client->Mutex);
        
        for (u64 PacketIndex = 0; PacketIndex < Client->ReceivedPackets.Count; PacketIndex++)
        {
            //Copy data from network thread to logic thread
            packet Packet = Client->ReceivedPackets[PacketIndex];
            Packet.Data = Copy(Arena, Packet.Data, Packet.Length);
            Append(&Result, Packet);
            
            Log("Client received packet\n");
        }
        
        Reset(&Client->ReceivedPackets);
        
        ResetArena(&Client->ReceivedPacketArena);
        
        Unlock(&Client->Mutex);
    }
    
    return ToSpan(Result);
}
bool SendToServer(packet Packet)
{
    client* Client = Client_;
    bool Result = false;
    if (Client && Client->Connected)
    {
        send(Client->Socket, (char*)Packet.Data, Packet.Length, 0);
        Result = true;
    }
    return Result;
}
bool IsConnectedToServer()
{
    client* Client = Client_;
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

server* Server_;

//-------Server API--------

//Server network thread
void ServerNetworkThread()
{
    memory_arena PermArena = Win32CreateMemoryArena(Megabytes(1), PERMANENT);
    
    Assert(!Server_);
    server* Server = AllocStruct(&PermArena, server);
    Server->ReceivedPacketArena = Win32CreateMemoryArena(Megabytes(1), TRANSIENT);
    Server->ReceivedPackets = {.Arena = &Server->ReceivedPacketArena};
    Server_ = Server;
    
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
                    .SenderIndex = IndexOf(Socket, Server->Clients)
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
    server* Server = Server_;
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
        
        Reset(&Server->ReceivedPackets);
        
        ResetArena(&Server->ReceivedPacketArena);
        
        Unlock(&Server->Mutex);
    }
    
    return ToSpan(Result);
}

void ServerSendPacket(packet Packet, u64 ClientIndex)
{
    server* Server = Server_;
    Assert(ClientIndex < Server->ClientCount);
    send(Server->Clients[ClientIndex], (char*)Packet.Data, Packet.Length, 0);
    
    Log("Server sending data of length %d\n", Packet.Length);
}

void ServerBroadcastPacket(packet Packet)
{
    server* Server = Server_;
    for (u64 ClientIndex = 0; ClientIndex < Server->ClientCount; ClientIndex++)
    {
        send(Server->Clients[ClientIndex], (char*)Packet.Data, Packet.Length, 0);
    }
    Log("Server broadcasting data of length %d\n", Packet.Length);
}