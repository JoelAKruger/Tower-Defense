#include "Engine.h"

#include "Renderer.h"
#include "Defense.h"

#include "Maths.cpp"
#include "Utilities.cpp"
#include "GUI.cpp"
#include "Console.cpp"
#include "Parser.cpp"
#include "Graphics.cpp"

#include "World.cpp"
#include "Render.cpp"
#include "Server.cpp"
#include "Resources.cpp"
#include "Water.cpp"

struct ray_collision
{
    bool DidHit;
    f32 T;
    v3 P;
    v3 Normal;
};

static ray_collision
RayTriangleIntersect(triangle Tri, v3 Ray0, v3 RayDir)
{
    ray_collision Result = {};
    
    f32 Epsilon = 0.000001f;
    v3 Edge1 = Tri.Positions[1] - Tri.Positions[0];
    v3 Edge2 = Tri.Positions[2] - Tri.Positions[0];
    v3 H = CrossProduct(RayDir, Edge2);
    f32 A = DotProduct(Edge1, H);
    if (A > -Epsilon && A < Epsilon)
    {
        return Result;
    }
    
    f32 F = 1.0f / A;
    v3 S = Ray0 - Tri.Positions[0];
    f32 U = F * DotProduct(S, H);
    if (U < 0.0f || U > 1.0f)
    {
        return Result;
    }
    
    v3 Q = CrossProduct(S, Edge1);
    f32 V = F * DotProduct(RayDir, Q);
    if (V < 0.0f || U + V > 1.0f)
    {
        return Result;
    }
    
    Result.T = F * DotProduct(Edge2, Q);
    if (Result.T > Epsilon)
    {
        Result.DidHit = true;
        Result.P = Ray0 + RayDir * Result.T;
        Result.Normal = CrossProduct(Edge1, Edge2);
    }
    
    return Result;
}

static v3
TransformPoint(v3 P, m4x4 Transform)
{
    v4 Transformed_ = V4(P, 1.0f) * Transform;
    v3 Transformed = Transformed_.XYZ * (1.0f / Transformed_.W);
    return Transformed;
}

static ray_collision
RayModelIntersection(game_assets* Assets, model* Model, m4x4 WorldTransform, v3 Ray0, v3 RayDir)
{
    ray_collision Result = {};
    
    m4x4 Transform = Model->LocalTransform * WorldTransform;
    
    for (u64 MeshIndex = 0; MeshIndex < Model->MeshCount; MeshIndex++)
    {
        mesh* Mesh = Assets->Meshes + Model->Meshes[MeshIndex];
        for (triangle Tri : Mesh->Triangles)
        {
            Tri = {
                TransformPoint(Tri.Positions[0], Transform),
                TransformPoint(Tri.Positions[1], Transform),
                TransformPoint(Tri.Positions[2], Transform)
            };
            
            ray_collision Collision = RayTriangleIntersect(Tri, Ray0, RayDir);
            if ((Collision.DidHit == true && Collision.T < Result.T) || Result.DidHit == false)
            {
                Result = Collision;
            }
        }
    }
    return Result;
}

static ray_collision
WorldCollision(game_state* Game, game_assets* Assets)
{
    ray_collision Result = {};
    world* World = &Game->GlobalState.World;
    model* Model = FindModel(Assets, "Hexagon");
    
    for (u64 RegionIndex = 1; RegionIndex < World->RegionCount; RegionIndex++)
    {
        world_region* Region = World->Regions + RegionIndex;
        m4x4 Transform = TranslateTransform(Region->Center.X, Region->Center.Y, Region->Z);
        ray_collision Collision = RayModelIntersection(Assets, Model, Transform, Game->CameraP, Game->CameraDirection);
        
        if ((Collision.DidHit == true && Collision.T < Result.T) || Result.DidHit == false)
        {
            Result = Collision;
        }
    }
    
    return Result;
}

static v3 
GetSkyColor(int Hour, int Minute) 
{
    int TimeOfDay = 60 * (Hour % 24) + Minute;
    
    int Hours[] = {0, 6, 9, 12, 17, 20, 24};
    v3 Colors[] = {
        {0.10f, 0.10f, 0.44f},
        {1.00f, 0.55f, 0.00f},
        {0.53f, 0.81f, 0.98f},
        {0.27f, 0.51f, 0.71f},
        {1.00f, 0.55f, 0.00f},
        {0.10f, 0.10f, 0.44f},
        {0.10f, 0.10f, 0.44f}
    };
    
    Assert(ArrayCount(Hours) == ArrayCount(Colors));
    
    v3 Result = {};
    for (size_t I = 0; I < ArrayCount(Hours) - 1; I++) 
    {
        int Time = 60 * Hours[I];
        int NextTime = 60 * Hours[(I + 1) % ArrayCount(Hours)];
        
        if (TimeOfDay >= Time && TimeOfDay < NextTime)
        {
            f32 t = (f32)(TimeOfDay - Time) / (f32)(NextTime - Time);
            
            v3 A = Colors[I];
            v3 B = Colors[(I + 1) % ArrayCount(Colors)];
            
            Result = LinearInterpolate(A, B, t);
        }
        
    }
    
    return Result;
}

static v3
ScreenToWorld(game_state* Game, v2 ScreenPos /*Clip Space */, f32 WorldZ)
{
    //TODO: Cache inverse matrix?
    
    v4 P = V4(ScreenPos, 1.0f, 1.0f) * Inverse(Game->WorldTransform);
    
    v3 P0 = Game->CameraP;
    v3 P1 = {P.X / P.W, P.Y / P.W, P.Z / P.W};
    
    f32 Z = P0.Z;
    f32 DeltaZ = P0.Z - P1.Z;
    
    f32 T = (Z - WorldZ) / DeltaZ;
    
    v3 Result = P0 + T * (P1 - P0);
    
    return Result;
}

static v2
WorldToScreen(game_state* GameState, v3 WorldPos)
{
    v4 P = V4(WorldPos, 1.0f);
    v4 ClipSpace = P * GameState->WorldTransform;
    v2 Result = {ClipSpace.X, ClipSpace.Y};
    Result = Result* (1.0f / ClipSpace.W);
    return Result;
}

static v2
WorldToScreen(game_state* GameState, v2 WorldPos)
{
    v2 Result = WorldToScreen(GameState, V3(WorldPos, 0.0f));
    return Result;
}

static game_state* 
GameInitialise(allocator Allocator)
{
    game_state* GameState = AllocStruct(Allocator.Permanent, game_state);
    GameState->Console = AllocStruct(Allocator.Permanent, console);
    
    GameState->Mode = Mode_Waiting;
    
    GameState->CameraP = {0.0f, -0.25, 0.0f};
    GameState->CameraTargetP = {0.0f, -1.25f, 0.0f};
    GameState->CameraDirection = UnitV(V3(0.0f, 2.0f, 5.5f));
    GameState->FOV = 50.0f;
    
    GameState->CastleTransform = ModelRotateTransform() * ScaleTransform(0.03f, 0.03f, 0.03f);
    GameState->TurretTransform = ModelRotateTransform() * ScaleTransform(0.06f, 0.06f, 0.06f);
    
    GameState->ApproxTowerZ = -0.06f;
    
    return GameState;
}

static void
DrawExplosion(v2 P, f32 Z, f32 ExplosionRadius, u32 Frame)
{
    u32 FrameCount = 22;
    Frame = Frame % FrameCount;
    
    u32 TexturesAcross = 6;
    u32 TexturesHigh = 4;
    
    u32 I = Frame % TexturesAcross;
    u32 J = Frame / TexturesAcross;
    
    v2 ExplosionTextureSize = ExplosionRadius * V2(16.0f / 9.0f, 1.0f); //This matches the texture
    
    //TODO: Fix this
    //SetTexture(ExplosionTexture);
    
    v2 P0 = P - 0.5f * ExplosionTextureSize;
    v2 P1 = P + 0.5f * ExplosionTextureSize;
    
    v2 UV0 = {(f32)I / TexturesAcross, (f32)(TexturesHigh - 1 - J) / TexturesHigh};
    v2 UV1 = UV0 + V2(1.0f / TexturesAcross, 1.0f / TexturesHigh);
    
    DrawTexture(V3(P0, Z), V3(P1, Z), UV0, UV1);
}

static void
SetMode(game_state* GameState, game_mode NewMode)
{
    GameState->Mode = NewMode;
    
    //Reset mode-specific variables
    GameState->PlacementType = {};
    GameState->SelectedTower = {};
    GameState->SelectedTowerIndex = {};
    GameState->TowerEditMode = {};
    GameState->TowerPerspective = {};
}

static v3
GetTowerP(game_state* Game, tower* Tower)
{
    f32 TowerZ = Game->GlobalState.World.Regions[Tower->RegionIndex].Z;
    v3 Result = V3(Tower->P, TowerZ);
    return Result;
}

static void 
RunTowerEditor(game_state* Game, u32 TowerIndex, game_input* Input, memory_arena* Arena)
{
    tower* Tower = Game->GlobalState.Towers + TowerIndex;
    f32 TowerZ = Game->GlobalState.World.Regions[Tower->RegionIndex].Z;
    
    //Draw new target
    if (Game->TowerEditMode == TowerEdit_SetTarget)
    {
        v3 CursorP = ScreenToWorld(Game, Input->Cursor);
        
        if ((Input->ButtonUp & Button_LMouse) && !GUIInputIsBeingHandled())
        {
            player_request Request = {Request_TargetTower};
            Request.TowerIndex = TowerIndex;
            Request.TargetP = CursorP.XY;
            SendPacket(&Request);
        }
        
    }
    
    GUI_DrawRectangle(V2(0.6f, 0.3f), V2(0.4f, 1.0f), V4(0.0f, 0.0f, 0.0f, 0.5f));
    
    gui_layout Layout = DefaultLayout(0.65f, 0.95f);
    
    Layout.NextRow();
    
    if ((Tower->Type == Tower_Turret) && Layout.Button(String("Set Target")))
    {
        Game->TowerEditMode = TowerEdit_SetTarget;
    }
    
    SetShader(GUIFontShader); //TODO: Remove
    Layout.Label("Tower Editor");
    
    Layout.NextRow();
    if (Layout.Button("Enter Tower POV"))
    {
        SetMode(Game, Mode_TowerPOV);
        Game->TowerPerspective = Tower;
        Game->CameraTargetP = GetTowerP(Game, Tower) + V3(0.0f, 0.0f, -0.07f); //0.3
        Game->CameraTargetDirection = UnitV(V3(cosf(Tower->Rotation), sinf(Tower->Rotation), 0.0f));
    }
}

//TODO: This should not take game_input
static void
DrawCrosshairForTowerEditor(game_state* Game, u32 TowerIndex, game_input* Input, render_group* RenderGroup)
{
    tower* Tower = Game->GlobalState.Towers + TowerIndex;
    
    //Draw target
    v2 TargetSize = {0.075f, 0.075f};
    f32 TargetZ = Game->ApproxTowerZ;
    
    if (Tower->Type == Tower_Turret)
    {
        PushTexturedRect(RenderGroup, RenderGroup->Assets->Target, V3(Tower->Target - 0.5f * TargetSize, TargetZ), V3(Tower->Target + 0.5f * TargetSize, TargetZ));
    }
    
    //Draw new target
    if (Game->TowerEditMode == TowerEdit_SetTarget)
    {
        v3 CursorP = ScreenToWorld(Game, Input->Cursor);
        
        PushTexturedRect(RenderGroup, RenderGroup->Assets->Target, V3(CursorP.XY - 0.5f * TargetSize, TargetZ), V3(CursorP.XY + 0.5f * TargetSize, TargetZ));
    }
}

static void
BeginAnimation(game_state* Game, v2 P, f32 Radius)
{
    if (Game->AnimationCount < ArrayCount(Game->Animations))
    {
        animation* Animation = Game->Animations + (Game->AnimationCount++);
        Animation->P = P;
        Animation->Radius = Radius;
        Animation->Duration = 0.6f;
        Animation->t = 0.0f;
    }
    else
    {
        Log("Animation could not be started\n");
    }
}

static void
TickAnimations(game_state* Game, f32 DeltaTime)
{
    SetDepthTest(false);
    //TODO: Fix this
    //SetShader(TextureShader);
    
    for (u32 AnimationIndex = 0; AnimationIndex < Game->AnimationCount; AnimationIndex++)
    {
        animation* Animation = Game->Animations + AnimationIndex;
        
        u32 FrameCount = 22;
        u32 Frame = (u32)(Animation->t * FrameCount);
        
        DrawExplosion(Animation->P, Game->ApproxTowerZ, Animation->Radius, Frame);
        Animation->t += (DeltaTime / Animation->Duration);
        
        if (Animation->t >= 1.0f)
        {
            //Delete animation (move last to current)
            *Animation = Game->Animations[Game->AnimationCount - 1];
            Game->AnimationCount--;
            AnimationIndex--;
        }
    }
}

static void
HandleMessageFromServer(server_packet_message* Message, game_state* GameState, game_assets* Assets, memory_arena* TArena)
{
    switch (Message->Type)
    {
        case Message_Initialise:
        {
            GameState->MyClientID = Message->InitialiseClientID;
        } break;
        case Message_PlayAnimation:
        {
            BeginAnimation(GameState, Message->AnimationP, Message->AnimationRadius);
        } break;
        case Message_NewWorld:
        {
            CreateWorldVertexBuffer(Assets, &GameState->GlobalState.World, TArena);
            CreateWaterFlowMap(&GameState->GlobalState.World, Assets, TArena);
        } break;
        default:
        {
            Assert(0);
        }
    }
}

static player*
GetPlayer(game_state* Game)
{
    player* Result = &Game->GlobalState.Players[Game->MyClientID];
    return Result;
}

static void
DoTowerMenu(game_state* Game, game_assets* Assets, memory_arena* Arena)
{
    Assert(Arena->Type == TRANSIENT);
    
    player* Player = GetPlayer(Game);
    
    panel_layout Panel = DefaultPanelLayout(-1.0f, 1.0f, 1.0f);
    Panel.DoBackground();
    
    Panel.Image(Assets->Crystal);
    Panel.Text(ArenaPrint(Arena, "%u", Player->Credits));
    Panel.NextRow();
    
    if (Panel.Button("Castle"))
    {
        SetMode(Game, Mode_Place);
        Game->PlacementType = Tower_Castle;
    }
    Panel.NextRow();
    if (Panel.Button("Sniper"))
    {
        SetMode(Game, Mode_Place);
        Game->PlacementType = Tower_Turret;
    }
    Panel.NextRow();
    if (Panel.Button("Mine"))
    {
        SetMode(Game, Mode_Place);
        Game->PlacementType = Tower_Mine;
    }
    Panel.NextRow();
    Panel.NextRow();
    if (Panel.Button("End Turn"))
    {
        player_request Request = {.Type = Request_EndTurn};
        SendPacket(&Request);
    }
}


static void
NewWorldData(global_game_state* NewGameState)
{
    
}

static void
RunLobby(game_state* GameState, game_assets* Assets, f32 SecondsPerFrame, game_input* Input, allocator Allocator)
{
    global_game_state* Server = &GameState->GlobalState;
    
    BeginGUI(Input, Assets);
    
    panel_layout Layout = DefaultPanelLayout(-0.9f, 0.9f);
    Layout.Text(ArenaPrint(Allocator.Transient, "In Lobby (%u / %u)", Server->PlayerCount, Server->MaxPlayers));
    Layout.NextRow();
    if (Layout.Button("Start Game"))
    {
        player_request Request = {.Type = Request_StartGame};
        SendPacket(&Request);
    }
}

struct cursor_target
{
    v2 WorldP;
    u64 HoveringRegionIndex;
    world_region* HoveringRegion;
};

static cursor_target
GetCursorTarget(game_state* Game, game_input* Input)

{
    cursor_target Result = {};
    
    v2 MouseP = Input->Cursor;
    
    if (Game->Mode == Mode_TowerPOV)
    {
        MouseP = {0.0f, 0.0f};
    }
    
    //TODO: This is rather inefficient
    u64 HoveringRegionIndex = 0;
    f32 HoveringRegionDistanceSq = FLT_MAX;
    
    world* World = &Game->GlobalState.World;
    
    for (u64 RegionIndex = 1; RegionIndex < World->RegionCount; RegionIndex++)
    {
        world_region* Region = World->Regions + RegionIndex;
        
        v3 WorldP = ScreenToWorld(Game, MouseP, Region->Z);
        
        f32 DistanceToRegionSq = LengthSq(V3(Region->Center, Region->Z) - Game->CameraP);
        
        if ((DistanceToRegionSq < HoveringRegionDistanceSq) && InRegion(Region, WorldP.XY))
        {
            HoveringRegionIndex= RegionIndex;
            HoveringRegionDistanceSq = DistanceToRegionSq;
        }
    }
    
    Game->HoveringRegionIndex = HoveringRegionIndex;
    Result.HoveringRegionIndex = HoveringRegionIndex;
    if (Game->HoveringRegionIndex)
    {
        Result.HoveringRegion = Game->GlobalState.World.Regions + Game->HoveringRegionIndex;
        Result.WorldP = ScreenToWorld(Game, MouseP, Result.HoveringRegion->Z).XY;
    }
    else
    {
        Result.WorldP = ScreenToWorld(Game, MouseP, 0.25f).XY;
    }
    
    return Result;
}

static void
RunGame(game_state* GameState, game_assets* Assets, f32 SecondsPerFrame, game_input* Input, allocator Allocator)
{
    //Update modes based on server state
    if ((GameState->GlobalState.PlayerTurnIndex == GameState->MyClientID) &&
        (GameState->Mode == Mode_Waiting))
    {
        SetMode(GameState, Mode_MyTurn);
    }
    
    if ((GameState->GlobalState.PlayerTurnIndex != GameState->MyClientID) &&
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
    
    if ((Input->Button & Button_LMouse) == 0)
    {
        GameState->Dragging = false;
    }
    
    GameState->CameraP.X += SecondsPerFrame * Input->Movement.X;
    GameState->CameraP.Y += SecondsPerFrame * Input->Movement.Y;
    
    bool ShouldDrawHealthBars = true;
    
    //Do camera
    f32 CameraSpeed = 10.0f;
    
    switch (GameState->Mode)
    {
        case Mode_Waiting: case Mode_MyTurn:  case Mode_Place: case Mode_EditTower: //Top down perspective
        {
            SetCursorState(true);
            
            if ((Input->ButtonDown & Button_LMouse) && !GUIInputIsBeingHandled())
            {
                GameState->CursorWorldPos = ScreenToWorld(GameState, Input->Cursor).XY;
                GameState->Dragging = true;
            }
            
            f32 ZoomLevels[] = {-0.1f, -0.2f, -0.4f, -0.8f, -1.6f};
            int DeltaZoom = -(int)Input->ScrollDelta;
            GameState->TopDownCameraZoomLevel = Clamp(GameState->TopDownCameraZoomLevel + DeltaZoom, 0, ArrayCount(ZoomLevels) - 1);
            
            GameState->CameraTargetP.Z = ZoomLevels[GameState->TopDownCameraZoomLevel];
            
            GameState->CameraP = LinearInterpolate(GameState->CameraP, GameState->CameraTargetP, CameraSpeed * SecondsPerFrame);
            
            if (GameState->Dragging)
            {
                v2 CurrentCursorWorldPos = ScreenToWorld(GameState, Input->Cursor).XY;
                v2 DeltaP = CurrentCursorWorldPos - GameState->CursorWorldPos;
                GameState->CameraP.X -= DeltaP.X;
                GameState->CameraP.Y -= DeltaP.Y;
                GameState->CameraTargetP.XY = GameState->CameraP.XY;
            }
            
            GameState->CameraDirection = UnitV(V3(0.0f, 2.0f, 5.5f));
            GameState->FOV = 50.0f;
        } break;
        case Mode_TowerPOV:
        {
            SetCursorState(false);
            
            ShouldDrawHealthBars = false;
            
            Input->Cursor = V2(0.5f, 0.5f);
            
            f32 ThetaOld = acosf(GameState->CameraDirection.Z);
            f32 PhiOld   = atan2(GameState->CameraDirection.Y, GameState->CameraDirection.X);
            
            f32 Sensitivity = 0.001f;
            f32 ThetaNew = Clamp(ThetaOld - Input->CursorDelta.Y * Sensitivity, 0.0001f, Pi - 0.0001f);
            f32 PhiNew = PhiOld - Input->CursorDelta.X * Sensitivity;
            
            GameState->CameraTargetDirection = V3(sinf(ThetaNew) * cosf(PhiNew), sinf(ThetaNew) * sinf(PhiNew), cosf(ThetaNew));
            //GameState->CameraDirection = GameState->CameraTargetDirection;
            
            GameState->CameraP = LinearInterpolate(GameState->CameraP, GameState->CameraTargetP, CameraSpeed * SecondsPerFrame);
            GameState->CameraDirection = UnitV(LinearInterpolate(GameState->CameraDirection, GameState->CameraTargetDirection, CameraSpeed * SecondsPerFrame));
            GameState->FOV = LinearInterpolate(GameState->FOV, 90.0f, CameraSpeed * SecondsPerFrame);
        } break;
        default: Assert(0);
    }
    
    v3 LookAt = GameState->CameraP + GameState->CameraDirection;
    GameState->WorldTransform = ViewTransform(GameState->CameraP, LookAt) * PerspectiveTransform(GameState->FOV, 0.01f, 150.0f);
    
    cursor_target Cursor = GetCursorTarget(GameState, Input);
    
    render_group RenderGroup = {};
    RenderGroup.Arena = Allocator.Transient;
    RenderGroup.Assets = Assets;
    
    //Draw animations
    TickAnimations(GameState, SecondsPerFrame);
    
    f32 Time = GameState->Time / 1.0f;
    int CurrentHour = (int)Time % 24;
    int CurrentMinute = (int)(60.0f * (Time - (int)Time));
    
    GameState->SkyColor = 3.0f *V3(1,1,1); //GetSkyColor(CurrentHour, CurrentMinute);
    f32 t = (CurrentHour * 60 + CurrentMinute) / (24.0f * 60.0f);
    
    //GameState->LightP = V3(4.0f * sinf(t * 2 * Pi), -1.0f, 4.0f * cosf(t * 2 * Pi));
    //GameState->LightDirection = UnitV(V3(0, 0, 0) - GameState->LightP);
    
    GameState->LightDirection = UnitV(V3(1.0f, -0.35f, 0.6f));
    GameState->LightP = -1.0f * GameState->LightDirection;
    
    f32 TowerRadius = 0.03f;
    //Select tower
    if (GameState->Mode == Mode_MyTurn)
    {
        if (Input->ButtonDown & Button_LMouse)
        {
            nearest_tower NearestTower = NearestTowerTo(Cursor.WorldP, &GameState->GlobalState, GameState->HoveringRegionIndex);
            if (NearestTower.Distance < TowerRadius)
            {
                SetMode(GameState, Mode_EditTower);
                GameState->SelectedTower = NearestTower.Tower;
                GameState->SelectedTowerIndex = NearestTower.Index;
            }
        }
    }
    
    SetDepthTest(true);
    SetShader(Assets->Shaders[Shader_Model]);
    
    //Draw new tower
    if (GameState->Mode == Mode_Place)
    {
        v2 P = Cursor.WorldP;
        
        bool Placeable = (Cursor.HoveringRegion &&
                          !Cursor.HoveringRegion->IsWater &&
                          Cursor.HoveringRegion->OwnerIndex == GameState->MyClientID && 
                          DistanceInsideRegion(Cursor.HoveringRegion, P) > TowerRadius &&
                          NearestTowerTo(P, &GameState->GlobalState, Cursor.HoveringRegionIndex).Distance > 2.0f * TowerRadius);
        
        f32 Z = 0.25f;
        if (Cursor.HoveringRegion)
        {
            Z = Cursor.HoveringRegion->Z;
        }
        
        v4 Color = V4(1.0f, 0.0f, 0.0f, 1.0f);
        
        if (Placeable)
        {
            v4 RegionColor = Cursor.HoveringRegion->Color;
            
            f32 t = 0.5f + 0.25f * sinf(6.0f * (f32)GameState->Time);
            Color = t * RegionColor + (1.0f - t) * V4(1.0f, 1.0f, 1.0f, 1.0f);
        }
        
        Color = V4(0.7f * Color.RGB, 1.0f);
        
        tower_type Type = GameState->PlacementType;
        
        //Draw slightly above a normal tower to prevent z-fighting
        DrawTower(&RenderGroup, GameState, Assets, Type, V3(P, Z - 0.001f), Color);
        
        if (Placeable && (Input->ButtonDown & Button_LMouse) && !GUIInputIsBeingHandled())
        {
            player_request Request = {.Type = Request_PlaceTower};
            
            Request.TowerP = P;
            Request.TowerRegionIndex = GameState->HoveringRegionIndex;
            Request.TowerType = Type;
            
            SendPacket(&Request);
        }
    }
    
    if (GameState->Mode == Mode_EditTower)
    {
        DrawCrosshairForTowerEditor(GameState, GameState->SelectedTowerIndex, Input, &RenderGroup);
    }
    
    ray_collision Collision = WorldCollision(GameState, Assets);
    Log("T = %f\n", Collision.T);
    
    PushModelNew(&RenderGroup, Assets, "Rock6_Cube.014", TranslateTransform(Collision.P.X, Collision.P.Y, Collision.P.Z));
    //PushModelNew(&RenderGroup, Assets, "Rock6_Cube.014", TranslateTransform(0.0f, 0.0f, -0.5f));
    
    RenderWorld(&RenderGroup, GameState, Assets);
    
    //Draw GUI
    SetDepthTest(false);
    SetGUIShaderConstant(IdentityTransform());
    
    f32 WorldInfoZThreshold = -0.51f;
    
    //Draw health bars
    if (ShouldDrawHealthBars && GameState->CameraP.Z > WorldInfoZThreshold)
    {
        SetShader(GUIColorShader);
        for (u32 TowerIndex = 0; TowerIndex < GameState->GlobalState.TowerCount; TowerIndex++)
        {
            tower* Tower = GameState->GlobalState.Towers + TowerIndex;
            f32 TowerBaseZ = GameState->GlobalState.World.Regions[Tower->RegionIndex].Z;
            
            v3 DrawP = V3(Tower->P, TowerBaseZ) + V3(0.0f, 0.0f, -0.07f);
            v2 ScreenP = WorldToScreen(GameState, DrawP);
            
            f32 HealthBarEdge = 0.02f;
            v2 HealthBarSize = {0.4f / GlobalAspectRatio, 0.08f};
            v2 HealthBarInnerSize = HealthBarSize - V2(HealthBarEdge / GlobalAspectRatio, HealthBarEdge);
            
            v2 HealthBarInner = V2(HealthBarInnerSize.X * Tower->Health, HealthBarInnerSize.Y);
            
            GUI_DrawRectangle(ScreenP - 0.5f * HealthBarSize, HealthBarSize, V4(0.5f, 0.5f, 0.5f, 1.0f));
            GUI_DrawRectangle(ScreenP - 0.5f * HealthBarInnerSize, HealthBarInner, V4(1.0f, 0.0f, 0.0f, 1.0f));
        }
    }
    
    if (Cursor.HoveringRegion)
    {
        string RegionText = {};
        if (Cursor.HoveringRegion->IsWater)
        {
            RegionText = String("Ocean");
        }
        else
        {
            RegionText = ArenaPrint(Allocator.Transient, "Owned by Player %d", Cursor.HoveringRegion->OwnerIndex);
        }
        
        f32 Width = GUIStringWidth(RegionText, 0.1f);
        GUI_DrawText(&Assets->Font, RegionText, V2(-0.5f * Width, 0.9f));
    }
    
    //Draw crosshair
    if (GameState->Mode == Mode_TowerPOV)
    {
        v2 CrosshairSize = 0.005f * V2(1.0f, GlobalAspectRatio);
        GUI_DrawRectangle(-0.5f * CrosshairSize, CrosshairSize, V4(0.5f, 0.5f, 0.5f, 1));
    }
    
    BeginGUI(Input, Assets);
    //Draw GUI
    
    //TODO: Create a proper layout system
    if (GameState->Mode == Mode_MyTurn)
    {
        DoTowerMenu(GameState, Assets, Allocator.Transient);
    }
    
    if (GameState->Mode == Mode_EditTower)
    {
        RunTowerEditor(GameState, GameState->SelectedTowerIndex, Input, Allocator.Transient);
    }
    
    string PosString = ArenaPrint(Allocator.Transient, "CameraP (%f, %f, %f), POV %f, CameraDir (%f, %f, %f)", 
                                  GameState->CameraP.X, GameState->CameraP.Y, GameState->CameraP.Z, GameState->FOV,
                                  GameState->CameraDirection.X, GameState->CameraDirection.Y, GameState->CameraDirection.Z);
    SetShader(GUIFontShader);
    DrawGUIString(PosString, V2(-0.95f, -0.95f));
    
    string TimeString = ArenaPrint(Allocator.Transient, "Time %02d:%02d", CurrentHour, CurrentMinute);
    DrawGUIString(TimeString, V2(-0.95f, -0.75f));
    
    //RunClayLayoutTest(Assets, Input, SecondsPerFrame);
}

static void
ReceiveServerPackets(game_state* Game, game_assets* Assets, memory_arena* Arena)
{
    Assert(Arena->Type == TRANSIENT);
    
    span<packet> Packets = PollClientConnection(Arena);
    
    for (packet RawPacket : Packets)
    {
        Assert(RawPacket.Length >= sizeof(channel));
        
        channel Channel = *(channel*)RawPacket.Data;
        switch (Channel)
        {
            case Channel_GameState:
            {
                Log("Client: received game state\n");
                Assert(RawPacket.Length == sizeof(server_packet_game_state));
                server_packet_game_state* Packet = (server_packet_game_state*)RawPacket.Data;
                
                Game->GlobalState = Packet->ServerGameState;
                
                server_packet_message Message = {.Channel = Channel_Message, .Type = Message_NewWorld};
                HandleMessageFromServer(&Message, Game, Assets, Arena);
            } break;
            case Channel_Message:
            {
                Log("Client: received message\n");
                Assert(RawPacket.Length == sizeof(server_packet_message));
                server_packet_message* Message = (server_packet_message*)RawPacket.Data;
                HandleMessageFromServer(Message, Game, Assets, Arena);
            } break;
            default: Assert(0);
        }
    }
}

static void 
GameUpdateAndRender(game_state* GameState, game_assets* Assets, f32 SecondsPerFrame, game_input* Input, allocator Allocator)
{
    UpdateConsole(GameState, GameState->Console, Input, Allocator.Transient, Assets, SecondsPerFrame);
    
    GameState->Time += SecondsPerFrame;
    
    ReceiveServerPackets(GameState, Assets, Allocator.Transient);
    
    if (GameState->GlobalState.GameStarted)
    {
        RunGame(GameState, Assets, SecondsPerFrame, Input, Allocator);
    }
    else
    {
        RunLobby(GameState, Assets, SecondsPerFrame, Input, Allocator);
    }
    
    DrawGUIString(String(IsConnectedToServer() ? "Connected" : "Not connected"), V2(-0.95f, 0.0f));
    DrawGUIString(ArenaPrint(Allocator.Transient, "My client ID: %u", GameState->MyClientID), V2(-0.95f, -0.05f));
    DrawGUIString(ArenaPrint(Allocator.Transient, "Current Turn: %u", GameState->GlobalState.PlayerTurnIndex), V2(-0.95f, -0.10f));
    
    DrawConsole(GameState->Console, Allocator.Transient);
}

void Command_p(int, string*, console* Console, game_state* GameState, game_assets*, memory_arena* Arena)
{
    string String = ArenaPrint(Arena, "X: %f, Y: %f, Z: %f", GameState->CameraP.X, GameState->CameraP.Y, GameState->CameraP.Z);
    AddLine(Console, String);
}

void Command_reset(int ArgCount, string* Args, console* Console, game_state* GameState, game_assets*, memory_arena* Arena)
{
    player_request Request = {.Type = Request_Reset};
    SendPacket(&Request);
}

void Command_create_server(int ArgCount, string* Args, console* Console, game_state* Game, game_assets*, memory_arena* Arena)
{
    void RunServer();
    CreateThread(0, 0, (LPTHREAD_START_ROUTINE)RunServer, 0, 0, 0);
}

void Command_connect(int ArgCount, string* Args, console* Console, game_state* Game, game_assets*, memory_arena* Arena)
{
    void ClientNetworkThread(char*);
    
    char* Address = "127.0.0.1";
    if (ArgCount > 1)
    {
        Address = (char*) malloc(Args[1].Length + 1);
        memcpy(Address, Args[1].Text, Args[1].Length);
        Address[Args[1].Length] = 0;
    }
    CreateThread(0, 0, (LPTHREAD_START_ROUTINE)ClientNetworkThread, (LPVOID)Address, 0, 0);
}

void Command_new_world(int ArgCount, string* Args, console* Console, game_state* Game, game_assets*, memory_arena* Arena)
{
}


static void UpdateAndRender(app_state* App, f32 DeltaTime, game_input* Input, allocator Allocator)
{
    if (!App->Assets)
    {
        App->Assets = LoadAssets(Allocator);
    }
    
    switch (App->CurrentScreen)
    {
        case Screen_MainMenu:
        {
            BeginGUI(Input, App->Assets);
            
            f32 ButtonWidth = 0.8f / GlobalAspectRatio;
            if (Button(V2(-0.5f * ButtonWidth, 0.0f), V2(ButtonWidth, 0.3f), String("Play")))
            {
                App->CurrentScreen = Screen_Game;
                App->GameState = GameInitialise(Allocator);
            }
        } break;
        case Screen_Game:
        {
            GameUpdateAndRender(App->GameState, App->Assets, DeltaTime, Input, Allocator);
        } break;
        
        default: Assert(0);
    }
}
