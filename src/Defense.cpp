#include "Defense.h"
#include "GUI.cpp"
#include "Console.cpp"
#include "Parser.cpp"

static m4x4
IdentityTransform()
{
    m4x4 Result = {};
    
    Result.Values[0][0] = 1.0f;
    Result.Values[1][1] = 1.0f;
    Result.Values[2][2] = 1.0f;
    Result.Values[3][3] = 1.0f;
    
    return Result;
}

static m4x4
ScaleTransform(f32 X, f32 Y, f32 Z)
{
    m4x4 Result = {};
    Result.Values[0][0] = X;
    Result.Values[1][1] = Y;
    Result.Values[2][2] = Z;
    Result.Values[3][3] = 1.0f;
    return Result;
}

static m4x4
TranslateTransform(f32 X, f32 Y, f32 Z)
{
    m4x4 Result = {};
    Result.Values[0][0] = 1.0f;
    Result.Values[1][1] = 1.0f;
    Result.Values[2][2] = 1.0f;
    Result.Values[3][3] = 1.0f;
    Result.Values[3][0] = X;
    Result.Values[3][1] = Y;
    Result.Values[3][2] = Z;
    return Result;
}

static m4x4
RotateTransform()
{
    m4x4 Result = {};
    Result.Values[0][0] = 1.0f;
    Result.Values[2][1] = 1.0f;
    Result.Values[1][2] = -1.0f;
    Result.Values[3][3] = 1.0f;
    return Result;
}

/*
static m4x4
DefaultTransform()
{
    m4x4 Result = {};
    Result.Values[0][0] = 2.0f;
    Result.Values[1][1] = 2.0f / ScreenTop;
    Result.Values[3][0] = -1.0f;
    Result.Values[3][1] = -1.0f;
    Result.Values[3][3] = 1.0f;
    return Result;
}
*/

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

static v2
GetVertex(world_region* Region, i32 Index)
{
    u32 VertexIndex = (Region->VertexCount + Index) % (Region->VertexCount);
    v2 Result = Region->Vertices[VertexIndex];
    return Result;
}

static f32
DistanceInsideRegion(world_region* Region, v2 P)
{
    f32 MinDistance = 1000.0f;
    for (u32 VertexIndex = 0; VertexIndex < Region->VertexCount; VertexIndex++)
    {
        v2 A = GetVertex(Region, VertexIndex);
        v2 B = GetVertex(Region, VertexIndex + 1);
        
        v2 AP = P - A;
        v2 AB = B - A;
        
        f32 t = DotProduct(AP, AB) / DotProduct(AB, AB);
        
        //Check if closer to A then the edge, B will be checked at the next iteration
        v2 ClosestPoint;
        if (t < 0.0f)
        {
            ClosestPoint = A;
        }
        else if (t > 1.0f)
        {
            ClosestPoint = B;
        }
        else
        {
            ClosestPoint = A + t * AB;
        }
        
        f32 Distance = Length(P - ClosestPoint);
        
        if (Distance < MinDistance)
        {
            MinDistance = Distance;
        }
    }
    return MinDistance;
}

struct nearest_tower
{
    f32 Distance;
    tower* Tower;
};

static nearest_tower
NearestTowerTo(v2 P, game_state* Game, u32 RegionIndex)
{
    f32 NearestDistanceSq = 10000.0f;
    tower* Nearest = 0;
    for (u32 TowerIndex = 0; TowerIndex < Game->TowerCount; TowerIndex++)
    {
        tower* Tower = Game->Towers + TowerIndex;
        if (Tower->RegionIndex == RegionIndex)
        {
            f32 DistSq = LengthSq(Tower->P - P);
            if (DistSq < NearestDistanceSq)
            {
                NearestDistanceSq = DistSq;
                Nearest = Tower;
            }
        }
    }
    nearest_tower Result = {sqrtf(NearestDistanceSq), Nearest};
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

static void
SetName(world_region* Region, string Name)
{
    Assert(Name.Length < ArrayCount(Region->Name));
    memcpy(Region->Name, Name.Text, Name.Length);
    Region->NameLength = Name.Length;
}

static void
SetName(world_region* Region, char* Name)
{
    SetName(Region, String(Name));
}

static string
GetName(world_region* Region)
{
    string Result = {Region->Name, Region->NameLength};
    return Result;
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
    
    GameState->CastleTransform = RotateTransform() * ScaleTransform(0.03f, 0.03f, 0.03f);
    GameState->TurretTransform = RotateTransform() * ScaleTransform(0.06f, 0.06f, 0.06f);
    
    return GameState;
}

static void
DrawWorldRegion(game_state* Game, render_group* RenderGroup, world* World, world_region* Region, memory_arena* TArena, v4 Color)
{
    if (Region->VertexCount == 0)
    {
        return;
    }
    
    u32 TriangleCount = Region->VertexCount;
    span<triangle> Triangles = AllocSpan(TArena, triangle, TriangleCount);
    
    for (u32 TriangleIndex = 0; TriangleIndex < TriangleCount; TriangleIndex++)
    {
        triangle* Tri = Triangles + TriangleIndex;
        
        //TODO: Use functions
        Tri->Vertices[0].P = Region->Center;
        Tri->Vertices[0].Col = Color;
        
        Tri->Vertices[1].P = GetVertex(Region, TriangleIndex);
        Tri->Vertices[1].Col = Color;
        
        Tri->Vertices[2].P = GetVertex(Region, TriangleIndex + 1);
        Tri->Vertices[2].Col = Color;
    }
    
    DrawVertices((f32*)Triangles.Memory, TriangleCount * sizeof(triangle), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    //Draw Outline
    u32 VertexDrawCount = 6 * Region->VertexCount + 2;
    color_vertex* Vertices = AllocArray(TArena, color_vertex, VertexDrawCount);
    
    for (u32 VertexIndex = 0; VertexIndex < Region->VertexCount; VertexIndex++)
    {
        v2 Vertex = GetVertex(Region, VertexIndex);
        v2 PrevVertex = GetVertex(Region, VertexIndex - 1);
        v2 NextVertex = GetVertex(Region, VertexIndex + 1);
        
        v2 PerpA = UnitV(Perp(Vertex - PrevVertex));
        v2 PerpB = UnitV(Perp(NextVertex - Vertex));
        v2 Mid = UnitV(PerpA + PerpB);
        
        f32 SinAngle = Det(M2x2(UnitV(Vertex - PrevVertex), UnitV(NextVertex - Vertex)));
        
        v2 RectSize = V2(0.005f, 0.005f);
        
        f32 HalfBorderThickness = 0.0035f;
        
        //If concave
        if (SinAngle < 0.0f)
        {
            Vertices[VertexIndex * 6 + 0] = {(Vertex - HalfBorderThickness * Mid), V4(1.0f, 1.0f, 1.0f, 1.0f)};
            Vertices[VertexIndex * 6 + 1] = {(Vertex + HalfBorderThickness * PerpA), V4(1.0f, 1.0f, 1.0f, 1.0f)};
            Vertices[VertexIndex * 6 + 2] = {(Vertex - HalfBorderThickness * Mid), V4(1.0f, 1.0f, 1.0f, 1.0f)};
            Vertices[VertexIndex * 6 + 3] = {(Vertex + HalfBorderThickness * Mid), V4(1.0f, 1.0f, 1.0f, 1.0f)};
            Vertices[VertexIndex * 6 + 4] = {(Vertex - HalfBorderThickness * Mid), V4(1.0f, 1.0f, 1.0f, 1.0f)};
            Vertices[VertexIndex * 6 + 5] = {(Vertex + HalfBorderThickness * PerpB), V4(1.0f, 1.0f, 1.0f, 1.0f)};
        }
        else
        {
            Vertices[VertexIndex * 6 + 0] = {(Vertex - HalfBorderThickness * PerpA), V4(1.0f, 1.0f, 1.0f, 1.0f)};
            Vertices[VertexIndex * 6 + 1] = {(Vertex + HalfBorderThickness * Mid), V4(1.0f, 1.0f, 1.0f, 1.0f)};
            Vertices[VertexIndex * 6 + 2] = {(Vertex - HalfBorderThickness * Mid), V4(1.0f, 1.0f, 1.0f, 1.0f)};
            Vertices[VertexIndex * 6 + 3] = {(Vertex + HalfBorderThickness * Mid), V4(1.0f, 1.0f, 1.0f, 1.0f)};
            Vertices[VertexIndex * 6 + 4] = {(Vertex - HalfBorderThickness * PerpB), V4(1.0f, 1.0f, 1.0f, 1.0f)};
            Vertices[VertexIndex * 6 + 5] = {(Vertex + HalfBorderThickness * Mid), V4(1.0f, 1.0f, 1.0f, 1.0f)};
        }
        
        if (VertexIndex == 0)
        {
            Vertices[VertexDrawCount - 2] = Vertices[0];
            Vertices[VertexDrawCount - 1] = Vertices[1];
        }
    }
    
    DrawVertices((f32*)Vertices, VertexDrawCount * sizeof(color_vertex), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
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
    SetShader(ColorShader);
    DrawRectangle(V2(0.6f, 0.3f), V2(0.4f, 1.0f), V4(0.0f, 0.0f, 0.0f, 0.5f));
    
    gui_layout Layout = DefaultLayout(0.65f, 0.95f);
    
    SetShader(FontShader);
    Layout.Label("Tower Editor");
}

static void
SetTransform(m4x4 Transform)
{
    Transform = Transpose(Transform);
    SetVertexShaderConstant(0, &Transform, sizeof(Transform));
}

static void
SetShaderTime(f32 Time)
{
    f32 Constant[4] = {Time};
    SetVertexShaderConstant(1, Constant, sizeof(Constant));
}

static void
SetModelTransform(m4x4 Transform)
{
    Transform = Transpose(Transform);
    SetVertexShaderConstant(2, &Transform, sizeof(Transform));
}

static void
SetModelColor(v4 Color)
{
    SetVertexShaderConstant(3, &Color, sizeof(Color));
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
    GameState->PlacementMode = {};
    GameState->SelectedTower = {};
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
    
    SetShader(WaterShader);
    SetShaderTime((f32)GameState->Time);
    DrawTexture(V2(-1.0f, -1.0f), V2(1.0f, 1.0f), V2(0.0f, 0.0f), V2(2.0f, 2.0f));
    
    if (GameState->ShowBackground)
    {
        SetShader(TextureShader);
        SetTexture(BackgroundTexture);
        DrawTexture(V2(0.0f, 0.0f), V2(1.0f, 0.5625f));
    }
    
    //Get hovered region
    v2 CursorWorldPos = ScreenToWorld(GameState, Input->Cursor);
    world_region* HoveringRegion = 0;
    u32 HoveringRegionIndex = 0;
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
    
    //Draw regions
    for (u32 RegionIndex = 0; RegionIndex < GameState->World.RegionCount; RegionIndex++)
    {
        SetShader(ColorShader);
        world_region* Region = GameState->World.Regions + RegionIndex;
        
        bool Hovering = (Region == HoveringRegion);
        
        v4 Color = GameState->World.Colors[Region->ColorIndex];
        if (Hovering)
        {
            Color.RGB = 0.8f * Color.RGB;
        }
        
        DrawWorldRegion(GameState, RenderGroup, &GameState->World, Region, Allocator.Transient, Color);
        
        //Draw name
        if (Region->VertexCount > 0)
        {
            f32 TextSize = 0.04f;
            string Name = GetName(Region);
            
            f32 NameWidth = PlatformTextWidth(Name, TextSize);
            v2 P = Region->Center;
            P.X -= 0.5f * NameWidth;
            P.Y -= 0.25f * TextSize;
            
            SetShader(FontShader);
            DrawString(Name, P, V4(1.0f, 1.0f, 1.0f, 1.0f), TextSize);
        }
    }
    
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
    
    //Draw existing towers
    for (u32 TowerIndex = 0; TowerIndex < GameState->TowerCount; TowerIndex++)
    {
        tower* Tower = GameState->Towers + TowerIndex;
        world_region* Region = GameState->World.Regions + Tower->RegionIndex;
        
        v2 P = Tower->P;
        v4 RegionColor = GameState->World.Colors[Region->ColorIndex];
        
        v4 Color = V4(0.7f * RegionColor.RGB, RegionColor.A);
        if (Tower == GameState->SelectedTower)
        {
            f32 t = 0.5f + 0.25f * sinf(6.0f * (f32)GameState->Time);
            Color = t * RegionColor + (1.0f - t) * V4(1.0f, 1.0f, 1.0f, 1.0f);
        }
        
        span<model_vertex> ModelVertices = {};
        m4x4 Transform;
        
        if (Tower->Type == Tower_Castle)
        {
            ModelVertices = GameState->CubeVertices;
            Transform = GameState->CastleTransform;
        }
        else if (Tower->Type == Tower_Turret)
        {
            ModelVertices = GameState->TurretVertices;
            Transform = GameState->TurretTransform;
        }
        else
        {
            Assert(0);
        }
        
        SetModelColor(Color);
        SetModelTransform(Transform * TranslateTransform(P.X, P.Y, 0.0f));
        DrawVertices((f32*)ModelVertices.Memory, sizeof(model_vertex) * ModelVertices.Count, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, sizeof(model_vertex));
    }
    
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
        
        span<model_vertex> ModelVertices = {};
        tower_type Type = {};
        m4x4 Transform;
        
        if (GameState->PlacementMode == Place_Castle)
        {
            ModelVertices = GameState->CubeVertices;
            Transform = GameState->CastleTransform;
            Type = Tower_Castle;
        }
        else if (GameState->PlacementMode == Place_Turret)
        {
            ModelVertices = GameState->TurretVertices;
            Transform = GameState->TurretTransform;
            Type = Tower_Turret;
        }
        
        SetModelColor(Color);
        SetModelTransform(Transform * TranslateTransform(P.X, P.Y, 0.0f));
        
        DrawVertices((f32*)ModelVertices.Memory, sizeof(model_vertex) * ModelVertices.Count, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, sizeof(model_vertex));
        
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
        GameState->PlacementMode = Place_Castle;
    }
    
    if (Button(V2(-0.95f + (0.6f / GlobalAspectRatio), -0.8f), V2(0.5f / GlobalAspectRatio, 0.2f), String("Turret")))
    {
        SetMode(GameState, Mode_Place);
        GameState->PlacementMode = Place_Turret;
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