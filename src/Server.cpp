const u32 ServerMaxPlayers = 4;

static void
InitialiseServerState(global_game_state* Game)
{
    Game->MaxPlayers = ServerMaxPlayers;
}

static void
InitialisePlayer(global_game_state* Game, u32 PlayerIndex)
{
    Log("(SERVER) Initialising player index %d\n", PlayerIndex);
    
    Assert(PlayerIndex < ArrayCount(Game->Players));
    Assert(PlayerIndex < Game->PlayerCount);
    
    player* Player = Game->Players + PlayerIndex;
    *Player = {};
    
    Player->Initialised = true;
    Player->Credits = 100;
    
    //Send player initialisation message
    server_packet_message Message = {.Channel = Channel_Message};
    Message.Type = Message_Initialise;
    Message.InitialiseClientID = PlayerIndex;
    
    packet Packet = {.Data = (u8*)&Message, .Length = sizeof(Message)};
    ServerSendPacket(Packet, PlayerIndex);
}

static inline void
AddMessage(server_message_queue* Queue, server_packet_message Message)
{
    if (Queue->MessageCount < ArrayCount(Queue->Messages))
    {
        Queue->Messages[Queue->MessageCount++] = Message;
    }
}

static void
DoExplosion(global_game_state* Game, v2 P, f32 Radius)
{
    for (u32 TowerIndex = 0; TowerIndex < Game->TowerCount; TowerIndex++)
    {
        tower* Tower = Game->Towers + TowerIndex;
        f32 Distance = Length(P - Tower->P);
        
        f32 NormalisedDistance = Distance / Radius;
        
        if (NormalisedDistance < 1.0f)
        {
            f32 Damage = (1.0f - NormalisedDistance);
            Tower->Health -= Damage;
        }
    }
}

static void
PlayRound(global_game_state* Game, dynamic_array<server_packet_message>* MessageQueue)
{
    player* Player = Game->Players + Game->PlayerTurnIndex;
    
    for (u32 TowerIndex = 0; TowerIndex < Game->TowerCount; TowerIndex++)
    {
        tower* Tower = Game->Towers + TowerIndex;
        
        if (Tower->Type == Tower_Turret)
        {
            v2 P = Tower->Target;
            f32 Radius = 0.1f;
            
            DoExplosion(Game, P, Radius);
            
            server_packet_message Message = {Channel_Message, Message_PlayAnimation};
            Message.AnimationP = P;
            Message.AnimationRadius = Radius;
            Append(MessageQueue, Message);
        }
        
        if (Tower->Type == Tower_Mine)
        {
            Player->Credits += 5;
        }
    }
}

static span<server_packet_message>
ServerHandleRequest(global_game_state* Game, u32 SenderIndex, player_request* Request, memory_arena* Arena, bool* FlushWorld)
{
    dynamic_array<server_packet_message> ServerPackets = {.Arena = Arena};
    
    Log("Server: Request!\n");
    
    //TODO: This is not strictly necessary (e.g. a player typing in chat)
    Assert(SenderIndex == Game->PlayerTurnIndex);
    
    switch (Request->Type)
    {
        case Request_StartGame:
        {
            CreateWorld(&Game->World, Game->PlayerCount);
            Game->GameStarted = true;
            *FlushWorld = true;
        } break;
        case Request_PlaceTower:
        {
            int Cost = 5;
            player* Player = Game->Players + Game->PlayerTurnIndex;
            bool CanPlace = (Player->Credits >= Cost);
            
            if (CanPlace && (Game->TowerCount < ArrayCount(Game->Towers)))
            {
                tower Tower = {Request->TowerType};
                
                //TODO: Assert position, region, tower type are valid!
                
                Tower.P = Request->TowerP;
                Tower.RegionIndex = Request->TowerRegionIndex;
                Tower.Health = 1.0f;
                
                Game->Towers[Game->TowerCount++] = Tower;
                
                Player->Credits -= Cost;
            }
            else
            {
                //TODO: Handle this case
            }
            *FlushWorld = true;
        } break;
        case Request_Reset:
        {
            Game->TowerCount = 0;
        } break;
        case Request_EndTurn:
        {
            PlayRound(Game, &ServerPackets);
            Game->PlayerTurnIndex = (Game->PlayerTurnIndex + 1) % Game->PlayerCount;
            *FlushWorld = true;
        } break;
        case Request_TargetTower:
        {
            //TODO: Use getters that check for permission
            tower* Tower = Game->Towers + Request->TowerIndex;
            Tower->Target = Request->TargetP;
            Tower->Rotation = VectorAngle(Tower->Target - Tower->P);
            *FlushWorld = true;
        } break;
        default:
        {
            Assert(0);
        }
    }
    
    return ToSpan(ServerPackets);
}

static void
SendPacket(player_request* Request)
{
    packet Packet = {.Data = (u8*)Request, .Length = sizeof(*Request)};
    SendToServer(Packet);
}

static void
RunServer()
{
    memory_arena Arena = Win32CreateMemoryArena(Megabytes(1), TRANSIENT);
    
    HostServer();
    
    global_game_state Game = {};
    
    InitialiseServerState(&Game);
    
    server_message_queue MessageQueue = {};
    
    while (true)
    {
        span<packet> Packets = PollServerConnection(&Arena);
        
        for (packet Packet : Packets)
        {
            player* Sender = Game.Players + Packet.SenderIndex;
            
            bool FlushWorld = false;
            
            if (!Sender->Initialised)
            {
                Game.PlayerCount = Max((i32)Game.PlayerCount, (i32)Packet.SenderIndex + 1); //Hack but realistically fine
                InitialisePlayer(&Game, Packet.SenderIndex);
                FlushWorld = true;
            }
            
            if (Packet.Length != 0)
            {
                Log("Server: Received data from client\n");
                Assert(Packet.Length == sizeof(player_request));
                
                player_request* Request = (player_request*)Packet.Data;
                
                span<server_packet_message> ServerPackets = ServerHandleRequest(&Game, Packet.SenderIndex, 
                                                                                Request, &Arena, &FlushWorld);
                
                for (server_packet_message& ServerPacket : ServerPackets)
                {
                    packet Packet = {.Data = (u8*)&ServerPacket, .Length = sizeof(ServerPacket)};
                    ServerBroadcastPacket(Packet);
                }
            }
            
            if (FlushWorld)
            {
                server_packet_game_state GameStatePacket = {.Channel = Channel_GameState};
                GameStatePacket.ServerGameState = Game;
                
                packet Packet = {.Data = (u8*)&GameStatePacket, .Length = sizeof(GameStatePacket)};
                ServerBroadcastPacket(Packet);
            }
        }
        
        ResetArena(&Arena);
    }
}