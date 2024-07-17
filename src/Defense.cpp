#include "Defense.h"
#include "GUI.cpp"
#include "Console.cpp"
#include "Parser.cpp"

#include "World.cpp"
#include "Renderer.cpp"
#include "Render.cpp"

static v2
ScreenToWorld(game_state* Game, v2 ScreenPos)
{
    //TODO: Cache inverse matrix?
    
    v4 P = V4(ScreenPos, 1.0f, 1.0f) * Inverse(Game->WorldTransform);
    
    v3 P0 = Game->CameraP;
    v3 P1 = {P.X / P.W, P.Y / P.W, P.Z / P.W};
    
    f32 Z = P0.Z;
    f32 DeltaZ = P0.Z - P1.Z;
    
    f32 T = Z / DeltaZ;
    
    v3 ResultXYZ = P0 + T * (P1 - P0);
    
    return V2(ResultXYZ.X, ResultXYZ.Y);
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

static bool
LinesIntersect(v2 A0, v2 A1, v2 B0, v2 B1)
{
    f32 Epsilon = 0.00001f;
    
    v2 st = Inverse(M2x2(B1 - B0, A0 - A1)) * (A0 - B0);
    f32 s = st.X;
    f32 t = st.Y;
    
    bool Result = (s >= 0.0f && s <= 1.0f) && (t >= 0.0f && t <= 1.0f);
    return Result;
}


static void
SaveWorld(world* World, char* Path, memory_arena* Arena)
{
    Assert(Arena->Type == TRANSIENT);
    
    u32 Bytes = sizeof(map_file_header) + World->RegionCount * sizeof(map_file_region);
    u8* Memory = Alloc(Arena, Bytes);
    u8* At = Memory;
    
    map_file_header* Header = (map_file_header*) At;
    Header->RegionCount = World->RegionCount;
    At += sizeof(map_file_header);
    
    map_file_region* Regions = (map_file_region*) At;
    
    for (u32 RegionIndex = 0; RegionIndex < World->RegionCount; RegionIndex++)
    {
        Regions[RegionIndex].Center = World->Regions[RegionIndex].Center;
        Assert(sizeof(Regions[RegionIndex].Vertices) == sizeof(World->Regions[RegionIndex].Vertices));
        memcpy(Regions[RegionIndex].Vertices, World->Regions[RegionIndex].Vertices, sizeof(Regions[RegionIndex].Vertices));
        Regions[RegionIndex].VertexCount = World->Regions[RegionIndex].VertexCount;
    }
    
    span<u8> Data = {Memory, Bytes};
    PlatformSaveFile(Path, Data);
}

static void
LoadWorld(world* World, char* Path, memory_arena* Arena)
{
    Assert(Arena->Type == TRANSIENT);
    
    span<u8> FileContents = PlatformLoadFile(Arena, Path);
    
    u8* Memory = FileContents.Memory;
    u32 Bytes = FileContents.Count;
    
    map_file_header* Header = (map_file_header*) Memory;
    Memory += sizeof(map_file_header);
    
    Assert(Bytes == sizeof(map_file_header) + Header->RegionCount * sizeof(map_file_region));
    
    map_file_region* Regions = (map_file_region*) Memory;
    
    World->RegionCount = Header->RegionCount;
    
    for (u32 RegionIndex = 0; RegionIndex < Header->RegionCount; RegionIndex++)
    {
        World->Regions[RegionIndex].Center = Regions[RegionIndex].Center;
        Assert(sizeof(Regions[RegionIndex].Vertices) == sizeof(World->Regions[RegionIndex].Vertices));
        memcpy(World->Regions[RegionIndex].Vertices, Regions[RegionIndex].Vertices, sizeof(Regions[RegionIndex].Vertices));
        World->Regions[RegionIndex].VertexCount = Regions[RegionIndex].VertexCount;
    }
}

static game_state* 
GameInitialise(allocator Allocator)
{
    game_state* GameState = AllocStruct(Allocator.Permanent, game_state);
    GameState->Console = AllocStruct(Allocator.Permanent, console);
    
    world_region Region = {};
    
    Region.VertexCount = 4;
    Region.Center = {0.0f, 0.0f};
    Region.Vertices[0] = {0.0f, 0.5f};
    Region.Vertices[1] = {0.5f, 0.0f};
    Region.Vertices[2] = {0.0f, -0.5f};
    Region.Vertices[3] = {-0.5f, 0.0f};
    SetName(&Region, "Niaga");
    
    GameState->World.Regions[0] = Region;
    GameState->World.RegionCount = 1;
    
    GameState->World.Colors[0] = V4(0.3f, 0.7f, 0.25f, 1.0f);
    GameState->World.Colors[1] = V4(0.2f, 0.4f, 0.5f, 1.0f);
    
    GameState->CameraP = {0.0f, -1.0f, -0.5f};
    GameState->CameraDirection = {0.0f, 1.0f, 5.5f};
    GameState->FOV = 50.0f;
    
    GameState->CubeVertices   = LoadModel(Allocator, "assets/models/castle.obj", false);
    GameState->TurretVertices = LoadModel(Allocator, "assets/models/turret.obj", true);
    
    GameState->CastleTransform = ModelRotateTransform() * ScaleTransform(0.03f, 0.03f, 0.03f);
    GameState->TurretTransform = ModelRotateTransform() * ScaleTransform(0.06f, 0.06f, 0.06f);
    
    return GameState;
}

static bool
InRegion(world_region* Region, v2 WorldPos, game_input* Input)
{
    if (Region->VertexCount < 3)
    {
        return false;
    }
    
    v2 A0 = WorldPos;
    v2 A1 = Region->Center;
    
    u32 IntersectCount = 0;
    
    for (u32 TestIndex = 0; TestIndex < Region->VertexCount; TestIndex++)
    {
        v2 B0 = GetVertex(Region, TestIndex);
        v2 B1 = GetVertex(Region, TestIndex + 1);
        
        if (LinesIntersect(A0, A1, B0, B1))
        {
            IntersectCount++;
        }
    }
    
    return (IntersectCount % 2 == 0);
}

static void
RunEditor(render_group* Render, game_state* Game, game_input* Input, memory_arena* Arena)
{
    Assert(Arena->Type == TRANSIENT);
    
    PushRectangle(Render, V2(0.6f, 0.3f), V2(0.4f, 1.0f), V4(0.0f, 0.0f, 0.0f, 0.5f));
    Game->Dragging = false;
    
    gui_layout Layout = DefaultLayout(0.6f, 1.0f);
    
    SetShader(FontShader);
    Layout.Label("Regions");
    
    if (Layout.Button("Add"))
    {
        Game->Editor.SelectedRegionIndex = (Game->World.RegionCount++);
        Assert(Game->World.RegionCount < ArrayCount(Game->World.Regions));
    }
    
    Layout.NextRow();
    
    Layout.Label(ArenaPrint(Arena, "Region: %u", Game->Editor.SelectedRegionIndex));
    Layout.NextRow();
    Layout.Label(ArenaPrint(Arena, "Position: %u", Game->Editor.SelectedVertexIndex));
    Layout.NextRow();
    
    Layout.Label("Vertices");
    
    if (Layout.Button("Add"))
    {
        world_region* Region = Game->World.Regions + Game->Editor.SelectedRegionIndex;
        
        v2 NewVertex = {};
        
        //New vertex is average of first and last vertex
        if (Region->VertexCount >= 2)
        { 
            NewVertex = 0.5f * (GetVertex(Region, -1) + GetVertex(Region, 0));
        }
        
        if (Region->VertexCount + 1 < ArrayCount(Region->Vertices))
        {
            Region->Vertices[Region->VertexCount++] = NewVertex;
        }
    }
    
    if (Layout.Button("Remove"))
    {
        world_region* Region = Game->World.Regions + Game->Editor.SelectedRegionIndex;
        
        if (Region->VertexCount > 0)
        {
            Region->VertexCount--;
        }
    }
    
    f32 VertexDisplaySize = 0.01f;
    
    SetShader(ColorShader);
    for (u32 RegionIndex = 0; RegionIndex < Game->World.RegionCount; RegionIndex++)
    {
        world_region* Region = Game->World.Regions + RegionIndex;
        
        for (u32 PositionIndex = 0; PositionIndex < Region->VertexCount + 1; PositionIndex++)
        {
            v2 Vertex = Region->Positions[PositionIndex];
            v2 VertexScreen = WorldToScreen(Game, Vertex);
            
            v2 SquareSize = V2(VertexDisplaySize, VertexDisplaySize);
            
            v4 Color = V4(1.0f, 0.0f, 0.0f, 1.0f);
            
            DrawRectangle(VertexScreen - 0.5f * SquareSize, SquareSize, Color);
        }
    }
    
    if ((Input->Button & Button_LMouse) == 0)
    {
        Game->Editor.DraggingVertex = false;
    }
    
    if ((Input->ButtonDown & Button_LMouse) && !GUIInputIsBeingHandled())
    {
        for (u32 RegionIndex = 0; RegionIndex < Game->World.RegionCount; RegionIndex++)
        {
            world_region* Region = Game->World.Regions + RegionIndex;
            
            for (u32 PositionIndex = 0; PositionIndex < Region->VertexCount + 1; PositionIndex++)
            {
                v2 Vertex = Region->Positions[PositionIndex];
                v2 VertexScreen = WorldToScreen(Game, Vertex);
                
                if (Abs(VertexScreen.X - Input->Cursor.X) < 0.5f * VertexDisplaySize &&
                    Abs(VertexScreen.Y - Input->Cursor.Y) < 0.5f * VertexDisplaySize)
                {
                    Game->Editor.DraggingVertex = true;
                    Game->Editor.SelectedRegionIndex = RegionIndex;
                    Game->Editor.SelectedVertexIndex = PositionIndex;
                }
            }
        }
    }
    
    if (Game->Editor.DraggingVertex)
    {
        world_region* Region = Game->World.Regions + Game->Editor.SelectedRegionIndex;
        Region->Positions[Game->Editor.SelectedVertexIndex] = ScreenToWorld(Game, Input->Cursor);
    }
}

static void 
RunTowerEditor(game_state* Game, tower* Tower, game_input* Input, memory_arena* Arena)
{
    //Draw target
    if (Tower->Type == Tower_Turret)
    {
        SetShader(TextureShader);
        SetTexture(TargetTexture);
        SetTransform(Game->WorldTransform);
        v2 TargetSize = {0.1f, 0.1f};
        DrawTexture(Tower->Target - 0.5f * TargetSize, Tower->Target + 0.5f * TargetSize);
    }
    
    if (Game->TowerEditMode == TowerEdit_SetTarget)
    {
        v2 CursorP = ScreenToWorld(Game, Input->Cursor);
        
        SetShader(TextureShader);
        SetTexture(TargetTexture);
        SetTransform(Game->WorldTransform);
        v2 TargetSize = {0.1f, 0.1f};
        DrawTexture(CursorP - 0.5f * TargetSize, CursorP + 0.5f * TargetSize);
        
        if (Input->ButtonUp & Button_LMouse)
        {
            Tower->Target = CursorP;
            Tower->Rotation = VectorAngle(Tower->Target - Tower->P);
        }
        
        SetTransform(IdentityTransform());
    }
    
    SetTransform(IdentityTransform());
    SetShader(ColorShader);
    DrawRectangle(V2(0.6f, 0.3f), V2(0.4f, 1.0f), V4(0.0f, 0.0f, 0.0f, 0.5f));
    
    gui_layout Layout = DefaultLayout(0.65f, 0.95f);
    
    Layout.NextRow();
    
    if ((Tower->Type == Tower_Turret) && Layout.Button("Set Target"))
    {
        Game->TowerEditMode = TowerEdit_SetTarget;
    }
    
    
    SetShader(FontShader);
    Layout.Label("Tower Editor");
}

//From: 
//https://learn.microsoft.com/en-us/windows/win32/direct3d9/projection-transform
static m4x4
PerspectiveTransform(f32 FOV, f32 Near, f32 Far)
{
    f32 FOVRadians = 3.14159f / 180.0f * FOV;
    f32 W = 1.0f / (GlobalAspectRatio * tanf(0.5f * FOVRadians));
    f32 H = 1.0f / tanf(0.5f * FOVRadians);
    f32 Q = Far / (Far - Near);
    
    m4x4 Result = {};
    Result.Values[0][0] = W;
    Result.Values[1][1] = H;
    Result.Values[2][2] = Q;
    Result.Values[2][3] = 1.0f;
    Result.Values[3][2] = -Near*Q;
    
    return Result;
}

//From:
//https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dxmatrixlookatlh
static m4x4
ViewTransform(v3 Eye, v3 At)
{
    v3 Up = V3(0.0f, 0.0f, -1.0f);
    
    v3 ZAxis = UnitV(At - Eye);
    v3 XAxis = UnitV(CrossProduct(Up, ZAxis));
    v3 YAxis = CrossProduct(ZAxis, XAxis);
    
    m4x4 Result = {};
    
    Result.Vectors[0] = V4(XAxis, -DotProduct(XAxis, Eye));
    Result.Vectors[1] = V4(YAxis, -DotProduct(YAxis, Eye));;
    Result.Vectors[2] = V4(ZAxis, -DotProduct(ZAxis, Eye));;
    Result.Vectors[3] = V4(0.0f, 0.0f, 0.0f, 1.0f);
    
    return Transpose(Result);
}

static void
SetMode(game_state* GameState, game_mode NewMode)
{
    GameState->Mode = NewMode;
    
    //Reset mode-specific variables
    GameState->PlacementType = {};
    GameState->SelectedTower = {};
    GameState->TowerEditMode = {};
}

static void 
GameUpdateAndRender(render_group* RenderGroup, game_state* GameState, f32 SecondsPerFrame, game_input* Input, allocator Allocator)
{
    UpdateConsole(GameState, GameState->Console, Input, Allocator.Transient, SecondsPerFrame);
    
    GameState->Time += SecondsPerFrame;
    
    if (Input->ButtonDown & Button_Interact)
    {
        if (GameState->Mode == Mode_Edit)
        {
            SetMode(GameState, Mode_Normal);
        }
        else if (GameState->Mode == Mode_Normal)
        {
            SetMode(GameState, Mode_Edit);
        }
    }
    
    if (Input->ButtonDown & Button_Escape)
    {
        SetMode(GameState, Mode_Normal);
    }
    
    if (GameState->Mode == Mode_Normal)
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
    GameState->CameraP.Z += 0.1f * Input->ScrollDelta;
    
    v3 LookAt = GameState->CameraP + GameState->CameraDirection;
    GameState->WorldTransform = ViewTransform(GameState->CameraP, LookAt) * PerspectiveTransform(GameState->FOV, 0.01f, 10.0f);
    SetTransform(GameState->WorldTransform);
    
    //Get hovered region
    v2 CursorWorldPos = ScreenToWorld(GameState, Input->Cursor);
    u32 HoveringRegionIndex = 0;
    world_region* HoveringRegion = 0;
    for (u32 RegionIndex = 0; RegionIndex < GameState->World.RegionCount; RegionIndex++)
    {
        world_region* Region = GameState->World.Regions + RegionIndex;
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
    
    DrawWorld(GameState, &RenderContext);
    
    f32 TowerRadius = 0.03f;
    //Select tower
    if (GameState->Mode == Mode_Normal)
    {
        if (Input->ButtonDown & Button_LMouse)
        {
            nearest_tower NearestTower = NearestTowerTo(CursorWorldPos, GameState, HoveringRegionIndex);
            if (NearestTower.Distance < TowerRadius)
            {
                SetMode(GameState, Mode_EditTower);
                GameState->SelectedTower = NearestTower.Tower;
            }
        }
    }
    
    //Draw Towers
    SetDepthTest(true);
    SetShader(ModelShader);
    
    //Draw new tower
    if (GameState->Mode == Mode_Place)
    {
        v2 P = CursorWorldPos;
        
        bool Placeable = (HoveringRegion &&
                          DistanceInsideRegion(HoveringRegion, P) > TowerRadius &&
                          NearestTowerTo(P, GameState, HoveringRegionIndex).Distance > 2.0f * TowerRadius);
        
        v4 Color = V4(1.0f, 0.0f, 0.0f, 1.0f);
        
        if (Placeable)
        {
            v4 RegionColor = GameState->World.Colors[HoveringRegion->ColorIndex];
            
            f32 t = 0.5f + 0.25f * sinf(6.0f * (f32)GameState->Time);
            Color = t * RegionColor + (1.0f - t) * V4(1.0f, 1.0f, 1.0f, 1.0f);
        }
        
        Color = V4(0.7f * Color.RGB, 1.0f);
        
        tower_type Type = GameState->PlacementType;
        
        DrawTower(GameState, Type, P, Color);
        
        if (Placeable && (Input->ButtonDown & Button_LMouse) && !GUIInputIsBeingHandled())
        {
            tower Tower = {Type};
            
            Tower.P = P;
            Tower.RegionIndex = HoveringRegionIndex;
            Tower.Health = 1.0f;
            
            GameState->Towers[GameState->TowerCount++] = Tower;
            Assert(GameState->TowerCount < ArrayCount(GameState->Towers));
        }
    }
    
    SetDepthTest(false);
    
    SetTransform(IdentityTransform());
    
    //Draw health bars
    if (GameState->CameraP.Z > -0.5f)
    {
        SetShader(ColorShader);
        for (u32 TowerIndex = 0; TowerIndex < GameState->TowerCount; TowerIndex++)
        {
            tower* Tower = GameState->Towers + TowerIndex;
            v3 DrawP = V3(Tower->P, 0.0f) + V3(0.0f, 0.0f, -0.07f);
            v2 ScreenP = WorldToScreen(GameState, DrawP);
            
            f32 HealthBarEdge = 0.02f;
            v2 HealthBarSize = {0.4f / GlobalAspectRatio, 0.08f};
            v2 HealthBarInnerSize = HealthBarSize - V2(HealthBarEdge / GlobalAspectRatio, HealthBarEdge);
            DrawRectangle(ScreenP - 0.5f * HealthBarSize, HealthBarSize, V4(0.5f, 0.5f, 0.5f, 1.0f));
            DrawRectangle(ScreenP - 0.5f * HealthBarInnerSize, HealthBarInnerSize, V4(1.0f, 0.0f, 0.0f, 1.0f));
        }
    }
    
    BeginGUI(Input, RenderGroup);
    //Draw GUI
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
    
    
    if (GameState->Mode == Mode_Edit)
    {
        RunEditor(RenderGroup, GameState, Input, Allocator.Transient);
    }
    
    if (GameState->Mode == Mode_EditTower)
    {
        RunTowerEditor(GameState, GameState->SelectedTower, Input, Allocator.Transient);
    }
    
    string String = ArenaPrint(Allocator.Transient, "X: %f, Y: %f, Z: %f", 
                               GameState->CameraP.X, GameState->CameraP.Y, GameState->CameraP.Z);
    SetShader(FontShader);
    DrawGUIString(String, V2(-0.95f, -0.95f));
    
    DrawConsole(GameState->Console, RenderGroup, Allocator.Transient);
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
        
        world_region* Region = GameState->World.Regions + RegionIndex;
        SetName(Region, Name);
    }
}

void Command_reset(int ArgCount, string* Args, console* Console, game_state* GameState, memory_arena* Arena)
{
    GameState->TowerCount = 0;
}

void Command_color(int ArgCount, string* Args, console* Console, game_state* Game, memory_arena* Arena)
{
    u32 ColorCount = ArrayCount(Game->World.Colors);
    
    for (u32 RegionIndex = 0; RegionIndex < Game->World.RegionCount; RegionIndex++)
    {
        Game->World.Regions[RegionIndex].ColorIndex = rand() % ColorCount;
    }
}