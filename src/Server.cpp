const u32 ServerMaxPlayers = 4;

static void
InitialiseServerState(global_game_state* Game)
{
    Game->MaxPlayers = ServerMaxPlayers;
    Game->World.EntityCount = 1; //Entity index 0 is invalid
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
            
            /*
            server_packet_message Message = {Channel_Message, Message_PlayAnimation};
            Message.AnimationP = P;
            Message.AnimationRadius = Radius;
            
            Append(MessageQueue, Message);
*/
        }
        
        if (Tower->Type == Tower_Mine)
        {
            Player->Credits += 5;
        }
    }
}

static span<server_packet_message>
ServerHandleRequest(global_game_state* Game, game_assets* Assets, u32 SenderIndex, player_request* Request, memory_arena* Arena, bool* FlushWorld)
{
    dynamic_array<server_packet_message> ServerPackets = {.Arena = Arena};
    
    Log("Server: Request!\n");
    
    switch (Request->Type)
    {
        case Request_Hello:
        {
            //Hello!
        } break;
        case Request_StartGame:
        {
            Assert(SenderIndex == Game->PlayerTurnIndex);
            CreateWorld(&Game->World, Game->PlayerCount);
            Game->GameStarted = true;
            *FlushWorld = true;
        } break;
        case Request_PlaceTower:
        {
            Assert(SenderIndex == Game->PlayerTurnIndex);
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
            CreateWorld(&Game->World, Game->PlayerCount);
            *FlushWorld = true;
        } break;
        case Request_EndTurn:
        {
            Assert(SenderIndex == Game->PlayerTurnIndex);
            PlayRound(Game, &ServerPackets);
            Game->PlayerTurnIndex = (Game->PlayerTurnIndex + 1) % Game->PlayerCount;
            *FlushWorld = true;
        } break;
        case Request_TargetTower:
        {
            Assert(SenderIndex == Game->PlayerTurnIndex);
            //TODO: Check for permission
            tower* Tower = Game->Towers + Request->TowerIndex;
            Tower->Target = Request->TargetP;
            Tower->Rotation = VectorAngle(Tower->Target - Tower->P);
            *FlushWorld = true;
        } break;
        case Request_Throw:
        {
            Assert(SenderIndex == Game->PlayerTurnIndex);
            
            ray_collision WorldCollision(world* World, game_assets* Assets, v3 Ray0, v3 RayDirection);
            ray_collision Collision = WorldCollision(&Game->World, Assets, Request->P, Request->Direction);
            
            if (Collision.T != 0.0f)
            {
                animation Animation = {
                    .Type = Animation_Projectile,
                    .P0 = Request->P, //Note: This is vulnerable to a client side cheat (lol as if)
                    .P1 = Collision.P,
                    .Duration = Length(Collision.P - Request->P)
                };
                
                server_packet_message Packet = {
                    .Channel = Channel_Message,
                    .Type = Message_PlayAnimation,
                    .Animation = Animation
                };
                
                Append(&ServerPackets, Packet);
            }
            
            Log("T = %f\n", Collision.T);
        } break;
        case Request_UpgradeRegion:
        {
            Assert(SenderIndex == Game->PlayerTurnIndex);
            
            //TODO: Verify index is valid and entity type is correct
            entity* Region = Game->World.Entities + Request->RegionIndex;
            v3 NewP = Region->P + V3(0.0f, 0.0f, -0.05f);
            
            //Play animation
            animation Animation = {
                .Type = Animation_Region,
                .P0 = Region->P,
                .P1 = NewP,
                .EntityIndex = Request->RegionIndex
            };
            
            server_packet_message Packet = {
                .Channel = Channel_Message,
                .Type = Message_PlayAnimation,
                .Animation = Animation
            };
            
            Append(&ServerPackets, Packet);
            
            Region->P = NewP;
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
    memory_arena TArena = Win32CreateMemoryArena(Megabytes(64), TRANSIENT);
    memory_arena PArena = Win32CreateMemoryArena(Megabytes(64), PERMANENT);
    
    allocator Allocator = {.Permanent = &PArena, .Transient = &TArena};
    
    CreateThread(0, 0, (LPTHREAD_START_ROUTINE)ServerNetworkThread, 0, 0, 0);
    
    game_assets* LoadServerAssets(allocator Allocator);
    game_assets* Assets = LoadServerAssets(Allocator);
    
    global_game_state Game = {};
    
    InitialiseServerState(&Game);
    
    server_message_queue MessageQueue = {};
    
    while (true)
    {
        span<packet> Packets = PollServerConnection(&TArena);
        
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
                
                span<server_packet_message> ServerPackets = ServerHandleRequest(&Game, Assets, Packet.SenderIndex, 
                                                                                Request, &TArena, &FlushWorld);
                
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
        
        ResetArena(&TArena);
    }
}