#include "GUI.cpp"
#include "Console.cpp"
#include "Parser.cpp"

#include "World.cpp"
#include "Render.cpp"
#include "Server.cpp"
#include "Game.cpp"

static void UpdateAndRender(app_state* App, f32 DeltaTime, game_input* Input, allocator Allocator)
{
    switch (App->CurrentScreen)
    {
        case Screen_MainMenu:
        {
            
        } break;
        case Screen_Game:
        {
            
        } break;
        
        default: Assert(0);
    }
}

static void 
GameUpdateAndRender(game_state* GameState, f32 SecondsPerFrame, game_input* Input, allocator Allocator)
{
    UpdateConsole(GameState, GameState->Console, Input, Allocator.Transient, SecondsPerFrame);
    
    GameState->Time += SecondsPerFrame;
    
    server_message_queue MessageQueue = {};
    CheckForServerUpdate(&GameState->GlobalState, &GameState->MultiplayerContext);
    
    for (u32 MessageIndex = 0; MessageIndex < MessageQueue.MessageCount; MessageIndex++)
    {
        HandleServerMessage(MessageQueue.Messages + MessageIndex, GameState);
    }
    
    //Update modes based on server state
    if ((GameState->GlobalState.PlayerTurnIndex == GameState->MultiplayerContext.MyClientID) &&
        (GameState->Mode == Mode_Waiting))
    {
        SetMode(GameState, Mode_MyTurn);
    }
    
    if ((GameState->GlobalState.PlayerTurnIndex != GameState->MultiplayerContext.MyClientID) &&
        (GameState->Mode != Mode_Waiting))
    {
        SetMode(GameState, Mode_Waiting);
    }
    
    if (Input->ButtonDown & Button_Interact)
    {
        if (GameState->Mode == Mode_Edit)
        {
            SetMode(GameState, Mode_MyTurn);
        }
        else if (GameState->Mode == Mode_MyTurn)
        {
            SetMode(GameState, Mode_Edit);
        }
    }
    
    if (Input->ButtonDown & Button_Escape)
    {
        SetMode(GameState, Mode_MyTurn);
    }
    
    if (GameState->Mode == Mode_MyTurn || GameState->Mode == Mode_Waiting)
    {
        //Handle moving the camera
        if ((Input->Button & Button_LMouse) == 0)
        {
            GameState->Dragging = false;
        }
        
        if ((Input->ButtonDown & Button_LMouse) && !GUIInputIsBeingHandled())
        {
            GameState->CursorWorldPos = ScreenToWorld(GameState, Input->Cursor);
            GameState->Dragging = true;
        }
        
        //v2 ScreenMiddle = V2(0.5f, 0.5f * ScreenTop);
        //v2 ScreenMiddleWorldPos = ScreenToWorld(GameState, ScreenMiddle);
        
        if (GameState->Dragging)
        {
            v2 CurrentCursorWorldPos = ScreenToWorld(GameState, Input->Cursor);
            v2 DeltaP = CurrentCursorWorldPos - GameState->CursorWorldPos;
            GameState->CameraP.X -= DeltaP.X;
            GameState->CameraP.Y -= DeltaP.Y;
        }
    }
    
    GameState->CameraP.X += SecondsPerFrame * Input->Movement.X;
    GameState->CameraP.Y += SecondsPerFrame * Input->Movement.Y;
    
    GameState->CameraTargetZ = Clamp(GameState->CameraTargetZ + 0.25f * Input->ScrollDelta, -1.25f, -0.25f);
    
    f32 CameraSpeed = 15.0f;
    GameState->CameraP.Z = LinearInterpolate(GameState->CameraP.Z, GameState->CameraTargetZ, CameraSpeed * SecondsPerFrame);
    
    v3 LookAt = GameState->CameraP + GameState->CameraDirection;
    GameState->WorldTransform = ViewTransform(GameState->CameraP, LookAt) * PerspectiveTransform(GameState->FOV, 0.01f, 10.0f);
    SetTransform(GameState->WorldTransform);
    
    //Get hovered region
    v2 CursorWorldPos = ScreenToWorld(GameState, Input->Cursor);
    u32 HoveringRegionIndex = 0;
    world_region* HoveringRegion = 0;
    for (u32 RegionIndex = 0; RegionIndex < GameState->GlobalState.World.RegionCount; RegionIndex++)
    {
        world_region* Region = GameState->GlobalState.World.Regions + RegionIndex;
        bool Hovering = InRegion(Region, CursorWorldPos, Input);
        if (Hovering)
        {
            HoveringRegion = Region;
            HoveringRegionIndex = RegionIndex;
            break;
        }
    }
    
    render_context RenderContext = {};
    RenderContext.Arena = Allocator.Transient;
    RenderContext.HoveringRegion = HoveringRegion;
    RenderContext.SelectedTower = GameState->SelectedTower;
    
    DrawWorld(GameState, &RenderContext);
    
    //Draw animations
    TickAnimations(GameState, SecondsPerFrame);
    
    f32 TowerRadius = 0.03f;
    //Select tower
    if (GameState->Mode == Mode_MyTurn)
    {
        if (Input->ButtonDown & Button_LMouse)
        {
            nearest_tower NearestTower = NearestTowerTo(CursorWorldPos, &GameState->GlobalState, HoveringRegionIndex);
            if (NearestTower.Distance < TowerRadius)
            {
                SetMode(GameState, Mode_EditTower);
                GameState->SelectedTower = NearestTower.Tower;
                GameState->SelectedTowerIndex = NearestTower.Index;
            }
        }
    }
    
    SetDepthTest(true);
    SetShader(ModelShader);
    
    //Draw new tower
    if (GameState->Mode == Mode_Place)
    {
        v2 P = CursorWorldPos;
        
        bool Placeable = (HoveringRegion &&
                          DistanceInsideRegion(HoveringRegion, P) > TowerRadius &&
                          NearestTowerTo(P, &GameState->GlobalState, HoveringRegionIndex).Distance > 2.0f * TowerRadius);
        
        v4 Color = V4(1.0f, 0.0f, 0.0f, 1.0f);
        
        if (Placeable)
        {
            v4 RegionColor = HoveringRegion->Color;
            
            f32 t = 0.5f + 0.25f * sinf(6.0f * (f32)GameState->Time);
            Color = t * RegionColor + (1.0f - t) * V4(1.0f, 1.0f, 1.0f, 1.0f);
        }
        
        Color = V4(0.7f * Color.RGB, 1.0f);
        
        tower_type Type = GameState->PlacementType;
        
        //Draw slightly above a normal tower to prevent z-fighting
        DrawTower(GameState, Type, V3(P, -0.001f), Color);
        
        if (Placeable && (Input->ButtonDown & Button_LMouse) && !GUIInputIsBeingHandled())
        {
            player_request Request = {Request_PlaceTower};
            
            Request.TowerP = P;
            Request.TowerRegionIndex = HoveringRegionIndex;
            Request.TowerType = Type;
            
            SendPacket(&GameState->MultiplayerContext, &Request);
        }
    }
    
    SetDepthTest(false);
    SetTransform(IdentityTransform());
    
    f32 WorldInfoZThreshold = -0.51f;
    
    //Draw health bars
    if (GameState->CameraP.Z > WorldInfoZThreshold)
    {
        SetShader(ColorShader);
        for (u32 TowerIndex = 0; TowerIndex < GameState->GlobalState.TowerCount; TowerIndex++)
        {
            tower* Tower = GameState->GlobalState.Towers + TowerIndex;
            v3 DrawP = V3(Tower->P, 0.0f) + V3(0.0f, 0.0f, -0.07f);
            v2 ScreenP = WorldToScreen(GameState, DrawP);
            
            f32 HealthBarEdge = 0.02f;
            v2 HealthBarSize = {0.4f / GlobalAspectRatio, 0.08f};
            v2 HealthBarInnerSize = HealthBarSize - V2(HealthBarEdge / GlobalAspectRatio, HealthBarEdge);
            
            v2 HealthBarInner = V2(HealthBarInnerSize.X * Tower->Health, HealthBarInnerSize.Y);
            
            DrawRectangle(ScreenP - 0.5f * HealthBarSize, HealthBarSize, V4(0.5f, 0.5f, 0.5f, 1.0f));
            DrawRectangle(ScreenP - 0.5f * HealthBarInnerSize, HealthBarInner, V4(1.0f, 0.0f, 0.0f, 1.0f));
        }
    }
    
    //Draw region names
    SetShader(FontShader);
    if (GameState->CameraP.Z > WorldInfoZThreshold)
    {
        for (u32 RegionIndex = 0; RegionIndex < GameState->GlobalState.World.RegionCount; RegionIndex++)
        {
            world_region* Region = GameState->GlobalState.World.Regions + RegionIndex;
            
            if (!Region->IsWaterTile)
            {
                f32 TextSize = 0.1f;
                string Name = GetName(Region);
                
                f32 NameWidth = GUIStringWidth(Name, TextSize);
                v2 P = WorldToScreen(GameState, Region->Center);
                P.X -= 0.5f * NameWidth;
                P.Y -= 0.25f * TextSize;
                
                DrawGUIString(Name, P, V4(1.0f, 1.0f, 1.0f, 1.0f), TextSize);
            }
        }
        
    }
    
    BeginGUI(Input);
    //Draw GUI
    if (GameState->Mode == Mode_MyTurn)
    {
        if (Button(V2(-0.95f, -0.8f), V2(0.5f / GlobalAspectRatio, 0.2f), String("Castle")))
        {
            SetMode(GameState, Mode_Place);
            GameState->PlacementType = Tower_Castle;
        }
        
        if (Button(V2(-0.95f + (0.6f / GlobalAspectRatio), -0.8f), V2(0.5f / GlobalAspectRatio, 0.2f), String("Turret")))
        {
            SetMode(GameState, Mode_Place);
            GameState->PlacementType = Tower_Turret;
        }
        
        if (Button(V2(-0.95f + (1.2f / GlobalAspectRatio), -0.8f), V2(0.5f / GlobalAspectRatio, 0.2f), String("End Turn")))
        {
            player_request Request = {Request_EndTurn};
            SendPacket(&GameState->MultiplayerContext, &Request);
        }
    }
    
    if (GameState->Mode == Mode_Edit)
    {
        //Use actual world, not local world
        RunEditor(GameState, &ServerState_->World, Input, Allocator.Transient);
    }
    
    if (GameState->Mode == Mode_EditTower)
    {
        RunTowerEditor(GameState, GameState->SelectedTowerIndex, Input, Allocator.Transient);
    }
    
    string PosString = ArenaPrint(Allocator.Transient, "X: %f, Y: %f, Z: %f", 
                                  GameState->CameraP.X, GameState->CameraP.Y, GameState->CameraP.Z);
    SetShader(FontShader);
    DrawGUIString(PosString, V2(-0.95f, -0.95f));
    
    DrawGUIString(String(GameState->MultiplayerContext.Connected ? "Connected" : "Not connected"), V2(-0.95f, 0.0f));
    DrawGUIString(ArenaPrint(Allocator.Transient, "My client ID: %u", GameState->MultiplayerContext.MyClientID), V2(-0.95f, -0.05f));
    DrawGUIString(ArenaPrint(Allocator.Transient, "Current Turn: %u", GameState->GlobalState.PlayerTurnIndex), V2(-0.95f, -0.10f));
    
    DrawConsole(GameState->Console, Allocator.Transient);
}

void Command_p(int, string*, console* Console, game_state* GameState, memory_arena* Arena)
{
    string String = ArenaPrint(Arena, "X: %f, Y: %f, Z: %f", GameState->CameraP.X, GameState->CameraP.Y, GameState->CameraP.Z);
    AddLine(Console, String);
}

void Command_background(int ArgCount, string* Args, console* Console, game_state* GameState, memory_arena* Arena)
{
    if (ArgCount == 2)
    {
        if (StringsAreEqual(Args[1], String("off")))
        {
            GameState->ShowBackground = false;
        }
        else if (StringsAreEqual(Args[1], String("on")))
        {
            GameState->ShowBackground = true;
        }
    }
}

void Command_name(int ArgCount, string* Args, console* Console, game_state* GameState, memory_arena* Arena)
{
    if (ArgCount == 3)
    {
        u32 RegionIndex = StringToU32(Args[1]);
        string Name = Args[2];
        
        world_region* Region = ServerState_->World.Regions + RegionIndex;
        SetName(Region, Name);
    }
}

void Command_reset(int ArgCount, string* Args, console* Console, game_state* GameState, memory_arena* Arena)
{
    player_request Request = {Request_Reset};
    SendPacket(&GameState->MultiplayerContext, &Request);
}

void Command_color(int ArgCount, string* Args, console* Console, game_state* Game, memory_arena* Arena)
{
    u32 ColorCount = ArrayCount(ServerState_->World.Colors);
    
    for (u32 RegionIndex = 0; RegionIndex < ServerState_->World.RegionCount; RegionIndex++)
    {
        //ServerState_->World.Regions[RegionIndex].ColorIndex = rand() % ColorCount;
    }
}

void Command_create_server(int ArgCount, string* Args, console* Console, game_state* Game, memory_arena* Arena)
{
    Host();
}

void Command_connect(int ArgCount, string* Args, console* Console, game_state* Game, memory_arena* Arena)
{
    ConnectToServer(&Game->MultiplayerContext, "localhost");
}

void Command_new_world(int ArgCount, string* Args, console* Console, game_state* Game, memory_arena* Arena)
{
}