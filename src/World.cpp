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
    return V4(Color, 1.0f);
    //return V4(0.0f, 0.0f, 0.0f, 0.0f);
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

static v2
WorldPosFromGridPos(world* World, int X, int Y)
{
    f32 dX = World->Width / World->Cols;
    f32 dY = 0.875f * World->Height / World->Rows;
    
    v2 Result = {World->X0 + X * dX, World->Y0 + Y * dY};
    if (Y % 2 == 1)
    {
        Result.X += 0.5f * dX;
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
    
    World->X0 = -1.0f;
    World->Y0 = -1.0f;
    World->Width  = 2.0f;
    World->Height = 2.0f;
    
    World->Rows = 13;
    World->Cols = 13;
    
    f32 HexagonRadius = 0.5f * World->Height / World->Rows;
    f32 HexagonSideLength = 1.118034f * HexagonRadius;
    
    int RegionIndex = 0;
    
    
    for (int Y = 0; Y < World->Rows; Y++)
    {
        for (int X = 0; X < World->Cols; X++)
        {
            Assert(RegionIndex < ArrayCount(World->Regions));
            world_region* Region = World->Regions + RegionIndex;
            
            Region->Center = WorldPosFromGridPos(World, X, Y);
            Region->VertexCount = 6;
            Region->Vertices[0] = Region->Center + HexagonSideLength * V2(0.0f, 1.0f);
            Region->Vertices[1] = Region->Center + HexagonSideLength * V2(0.86603f, 0.5f);
            Region->Vertices[2] = Region->Center + HexagonSideLength * V2(0.86603f, -0.5f);
            Region->Vertices[3] = Region->Center + HexagonSideLength * V2(0.0f, -1.0f);
            Region->Vertices[4] = Region->Center + HexagonSideLength * V2(-0.86603f, -0.5f);
            Region->Vertices[5] = Region->Center + HexagonSideLength * V2(-0.86603f, 0.5f);
            
            f32 RegionHeight = GetWorldHeight(Region->Center, Seed);
            Region->Z = 0.25f * (1.0f - RegionHeight);
            
            if (RegionHeight < 0.5f)
            {
                Region->IsWater = true;
                Region->Color = GetWaterColor(RegionHeight);
            }
            else
            {
                Region->Color = GetRandomPlayerColor();
            }
            
            RegionIndex++;
        }
    }
    
    World->RegionCount = RegionIndex; 
}
