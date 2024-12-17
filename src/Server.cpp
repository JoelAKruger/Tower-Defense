const u32 ServerMaxPlayers = 4;

static void
InitialiseServerState(global_game_state* Game)
{
    Game->MaxPlayers = ServerMaxPlayers;
}

static void
InitialisePlayer(global_game_state* Game, u32 PlayerIndex)
{
    Assert(PlayerIndex < ArrayCount(Game->Players));
    Assert(PlayerIndex < Game->PlayerCount);
    
    player* Player = Game->Players + PlayerIndex;
    *Player = {};
    
    Player->Credits = 10;
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
PlayRound(global_game_state* Game, server_message_queue* MessageQueue)
{
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
            AddMessage(MessageQueue, Message);
        }
    }
}

static void
ServerHandleRequest(global_game_state* Game, u32 SenderIndex, player_request* Request, server_message_queue* MessageQueue)
{
    LOG("Server: Request!\n");
    
    //TODO: This is not strictly necessary (e.g. a player typing in chat)
    Assert(SenderIndex == Game->PlayerTurnIndex);
    
    switch (Request->Type)
    {
        case Request_StartGame:
        {
            CreateWorld(&Game->World, Game->PlayerCount);
            Game->GameStarted = true;
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
        } break;
        case Request_Reset:
        {
            Game->TowerCount = 0;
        } break;
        case Request_EndTurn:
        {
            PlayRound(Game, MessageQueue);
            Game->PlayerTurnIndex = (Game->PlayerTurnIndex + 1) % Game->PlayerCount;
        } break;
        case Request_TargetTower:
        {
            //TODO: Use getters that check for permission
            tower* Tower = Game->Towers + Request->TowerIndex;
            Tower->Target = Request->TargetP;
            Tower->Rotation = VectorAngle(Tower->Target - Tower->P);
        } break;
        default:
        {
            Assert(0);
        }
    }
}

static void
SendPacket(multiplayer_context* Context, player_request* Request)
{
    PlatformSend(Context, (u8*)Request, sizeof(*Request));
}
