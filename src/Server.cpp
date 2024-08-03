static void
InitialiseServerState(global_game_state* Game)
{
    CreateWorld(&Game->World);
    
    /*
    world_region Region = {};
    
    Region.VertexCount = 4;
    Region.Center = {0.0f, 0.0f};
    Region.Vertices[0] = {0.0f, 0.5f};
    Region.Vertices[1] = {0.5f, 0.0f};
    Region.Vertices[2] = {0.0f, -0.5f};
    Region.Vertices[3] = {-0.5f, 0.0f};
    SetName(&Region, "Niaga");
    
    Game->World.Regions[0] = Region;
    Game->World.RegionCount = 1;
    
    Game->World.Colors[0] = V4(0.3f, 0.7f, 0.25f, 1.0f);
    Game->World.Colors[1] = V4(0.2f, 0.4f, 0.5f, 1.0f);
*/
}

static inline void
AddMessage(server_message_queue* Queue, server_message Message)
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
            
            server_message Message = {Message_PlayAnimation};
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
        case Request_PlaceTower:
        {
            if (Game->TowerCount < ArrayCount(Game->Towers))
            {
                tower Tower = {Request->TowerType};
                
                //TODO: Assert position and region is valid!
                
                Tower.P = Request->TowerP;
                Tower.RegionIndex = Request->TowerRegionIndex;
                Tower.Health = 1.0f;
                
                Game->Towers[Game->TowerCount++] = Tower;
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
