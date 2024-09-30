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
    u32 Index;
    tower* Tower;
};

static nearest_tower
NearestTowerTo(v2 P, global_game_state* Game, u32 RegionIndex)
{
    f32 NearestDistanceSq = 10000.0f;
    tower* Nearest = 0;
    u32 Index;
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
                Index = TowerIndex;
            }
        }
    }
    nearest_tower Result = {sqrtf(NearestDistanceSq), Index, Nearest};
    return Result;
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

#define STB_PERLIN_IMPLEMENTATION
#include "stb_perlin.h"

enum triangulation_type : u8
{
    Null,
    TopLeftToBottomRight,
    BottomLeftToTopRight
};

struct world_grid
{
    u32 Rows;
    u32 Cols;
    
    f32 Width;
    f32 Height;
    f32 X0;
    f32 Y0;
    
    v2* Positions;
    
    //2D array of dimension (Rows - 1) x (Cols - 1)
    triangulation_type* TriTypes;
};

static world_grid
CreateWorldGrid()
{
    world_grid Result = {};
    
    f32 X0 = -1.0f;
    f32 Y0 = -1.0f;
    f32 Width  = 2.0f;
    f32 Height = 2.0f;
    
    u32 Rows = 13;
    u32 Cols = 13;
    
    Result.Rows = Rows;
    Result.Cols = Cols;
    Result.Width = Width;
    Result.Height = Height;
    Result.X0 = X0;
    Result.Y0 = Y0;
    
    Result.Positions = (v2*)malloc(sizeof(v2) * Rows * Cols);
    Result.TriTypes = (triangulation_type*) malloc(sizeof(triangulation_type) * (Rows - 1) * (Cols - 1));
    
    f32 Jitter = 0.05f;
    for (u32 Y = 0; Y < Rows; Y++)
    {
        for (u32 X = 0; X < Cols; X++)
        {
            f32 dX = Jitter * (Random() - 0.5f);
            f32 dY = Jitter * (Random() - 0.5f);
            
            Result.Positions[Cols * Y + X] = {
                Width * (X + 0.5f) / Cols + X0 + dX, 
                Height * (Y + 0.5f) / Rows + Y0 + dY
            };
        }
    }
    
    //Get triangulation types
    for (u32 Y = 0; Y < Rows - 1; Y++)
    {
        for (u32 X = 0; X < Cols - 1; X++)
        {
            triangulation_type Type = {};
            if (rand() % 2 == 0)
            {
                Type = TopLeftToBottomRight;
            }
            else
            {
                Type = BottomLeftToTopRight;
            }
            
            Result.TriTypes[Y * (Cols - 1) + X] = Type;
        }
    }
    return Result;
}

static v2
GetGeneratedPos(world_grid* Grid, u32 X, u32 Y)
{
    Assert(Y < Grid->Rows);
    Assert(X < Grid->Cols);
    
    v2 Result = Grid->Positions[Grid->Cols * Y + X];
    return Result;
}

static v2
GetActualPos(world_grid* Grid, u32 X, u32 Y)
{
    Assert(Y < Grid->Rows);
    Assert(X < Grid->Cols);
    
    v2 Result = {Grid->Width * (X + 0.5f) / Grid->Cols + Grid->X0, Grid->Height * (Y + 0.5f) / Grid->Rows + Grid->Y0};
    return Result;
}

static triangulation_type
GetTriType(world_grid* Grid, u32 X, u32 Y)
{
    Assert(Y < Grid->Rows - 1);
    Assert(X < Grid->Cols - 1);
    
    triangulation_type Result = Grid->TriTypes[(Grid->Cols - 1) * Y + X];
    return Result;
}

static v2
CenterOf(v2 P0, v2 P1, v2 P2)
{
    v2 Result = (1.0f / 3.0f) * (P0 + P1 + P2);
    return Result;
}

static void
AddVertex(world_region* Region, v2 Vertex)
{
    Region->Vertices[Region->VertexCount++] = Vertex;
    Assert(Region->VertexCount <= ArrayCount(Region->Vertices));
}

static f32
GetWorldHeight(v2 P, u32 Seed)
{
    f32 WaveLength = 0.02f;
    
    f32 NoiseValue = 0.5f + 0.5f * stb_perlin_noise3_seed(P.X / WaveLength , P.Y / WaveLength, 0.0f, 0, 0, 0, Seed++);
    f32 SquareDistance = Max(Abs(P.X), Abs(P.Y));
    
    f32 Mix = 0.5f;
    f32 Result = LinearInterpolate(NoiseValue, 1.8f - expf(0.8f * SquareDistance), Mix);
    
    return Result;
}

static v4
GetRandomPlayerColor()
{
    v4 PlayerColors[] = {
        V4(0.5f, 0.9f, 0.45f, 1.0f),
        V4(0.4f, 0.6f, 0.7f, 1.0f)
    };
    
    v4 Result = PlayerColors[rand() % ArrayCount(PlayerColors)];
    return Result;
}

static v4
GetWaterColor(f32 Height)
{
    v3 WaterColor = V3(0.17f, 0.23f, 0.46f);
    v3 Color = WaterColor + 2.0f * (Height - 0.5f) * WaterColor;
    //return V4(Color, 1.0f);
    return V4(0.0f, 0.0f, 0.0f, 0.0f);
}

static v2
CalculateRegionCenter(world_region* Region)
{
    v2 Result = {};
    
    if (Region->VertexCount > 0)
    {
        
        for (u32 VertexIndex = 0; VertexIndex < Region->VertexCount; VertexIndex++)
        {
            Result += Region->Vertices[VertexIndex];
        }
        
        Result = Result * (1.0f / Region->VertexCount);
    }
    return Result;
}

static void
CreateWorld(world* World)
{
    static u32 Seed = 1;
    Seed++;
    
    World->Colors[0] = V4(0.3f, 0.7f, 0.25f, 1.0f);
    World->Colors[1] = V4(0.2f, 0.4f, 0.5f, 1.0f);
    
    world_grid Grid = CreateWorldGrid();
    
    //TODO: Get rid of manual memory management here
    triangulation_type* TriangleTypes = (triangulation_type*) malloc(sizeof(triangulation_type) * (Grid.Rows - 1) * (Grid.Cols - 1));
    
    for (u32 GridY = 1; GridY < Grid.Rows - 1; GridY++)
    {
        for (u32 GridX = 1; GridX < Grid.Cols - 1; GridX++)
        {
            /*
P6 P7 P8
P3 P4 P5
P0 P1 P2
    */
            
            v2 P0 = GetGeneratedPos(&Grid, GridX - 1, GridY - 1);
            v2 P1 = GetGeneratedPos(&Grid, GridX    , GridY - 1);
            v2 P2 = GetGeneratedPos(&Grid, GridX + 1, GridY - 1);
            v2 P3 = GetGeneratedPos(&Grid, GridX - 1, GridY);
            v2 P4 = GetGeneratedPos(&Grid, GridX    , GridY);
            v2 P5 = GetGeneratedPos(&Grid, GridX + 1, GridY);
            v2 P6 = GetGeneratedPos(&Grid, GridX - 1, GridY + 1);
            v2 P7 = GetGeneratedPos(&Grid, GridX    , GridY + 1);
            v2 P8 = GetGeneratedPos(&Grid, GridX + 1, GridY + 1);
            
            char Name[128];
            sprintf_s(Name, "Region %u", World->RegionCount);
            
            world_region* Region = World->Regions + (World->RegionCount++);
            
            f32 RegionHeight = GetWorldHeight(P4, Seed);
            
            if (RegionHeight < 0.5f)
            {
                Region->IsWater = true;
                Region->Color = GetWaterColor(RegionHeight);
            }
            else
            {
                Region->Color = GetRandomPlayerColor();
            }
            
            if (GetTriType(&Grid, GridX - 1, GridY - 1) == TopLeftToBottomRight)
            {
                AddVertex(Region, CenterOf(P1, P3, P4));
            }
            else
            {
                AddVertex(Region, CenterOf(P0, P1, P4));
                AddVertex(Region, CenterOf(P0, P3, P4));
            }
            
            if (GetTriType(&Grid, GridX - 1, GridY) == TopLeftToBottomRight)
            {
                AddVertex(Region, CenterOf(P3, P4, P6));
                AddVertex(Region, CenterOf(P4, P6, P7));
            }
            else
            {
                AddVertex(Region, CenterOf(P3, P4, P7));
            }
            
            if (GetTriType(&Grid, GridX, GridY) == TopLeftToBottomRight)
            {
                AddVertex(Region, CenterOf(P4, P5, P7));
            }
            else
            {
                AddVertex(Region, CenterOf(P4, P7, P8));
                AddVertex(Region, CenterOf(P4, P5, P8));
            }
            
            if (GetTriType(&Grid, GridX, GridY - 1) == TopLeftToBottomRight)
            {
                AddVertex(Region, CenterOf(P2, P4, P5));
                AddVertex(Region, CenterOf(P1, P2, P4));
            }
            else
            {
                AddVertex(Region, CenterOf(P1, P4, P5));
            }
            
            SetName(Region, Name);
            
            Region->Center = CalculateRegionCenter(Region);
        }
    }
    
    //TODO: Get rid of this manual memory management
    free(Grid.Positions);
    free(Grid.TriTypes);
}

struct continent
{
    span<v2> Vertices;
};

static span<continent>
GetContinents(world* World)
{
    
}