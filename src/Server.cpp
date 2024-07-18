static global_game_state ServerState;

static void
ServerHandleRequest(global_game_state* Game, u32 SenderIndex, player_request* Request)
{
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
                
                GameState->Towers[GameState->TowerCount++] = Tower;
            }
            else
            {
                //TODO: Handle this case
            }
        } break;
    }
}

static void
SendPacket(player_request* Request)
{
    //TODO: Send packet
    ServerHandleRequest(&ServerState, 0, Request);
}

static void
UpdateNewState(global_game_state* LocalState)
{
    //TODO: Check for received packet
    LocalState = ServerState;
}