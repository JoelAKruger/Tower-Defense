static void
InitialiseServerState(global_game_state* Game)
{
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
}

static void
ServerHandleRequest(global_game_state* Game, u32 SenderIndex, player_request* Request)
{
    LOG("Request!\n");
    
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
            Game->PlayerTurnIndex++;
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
SendPacket(player_request* Request)
{
    //TODO: Send packet
    //ServerHandleRequest(&ServerState, 0, Request);
    PlatformSend((u8*)Request, sizeof(*Request));
}
