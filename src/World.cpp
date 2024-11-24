static v2
GetVertex(world_region* Region, i32 Index)
{
    u32 VertexIndex = (ArrayCount(Region->Vertices) + Index) % (ArrayCount(Region->Vertices));
    v2 Result = Region->Vertices[VertexIndex];
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

static bool
InRegion(world_region* Region, v2 WorldPos)
{
    v2 A0 = WorldPos;
    v2 A1 = Region->Center;
    
    u32 IntersectCount = 0;
    
    for (u32 TestIndex = 0; TestIndex < ArrayCount(Region->Vertices); TestIndex++)
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

static f32
DistanceInsideRegion(world_region* Region, v2 P)
{
    f32 MinDistance = 1000.0f;
    for (u32 VertexIndex = 0; VertexIndex < ArrayCount(Region->Vertices); VertexIndex++)
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
        V4(0.4f, 0.8f, 0.35f, 1.0f),
        V4(0.3f, 0.4f, 0.7f, 1.0f)
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
SetColor(tri* Tri, v4 Color)
{
    Tri->Vertices[0].Color = Color;
    Tri->Vertices[1].Color = Color;
    Tri->Vertices[2].Color = Color;
}

static void
CreateWorldVertexBuffer(game_assets* Assets, world* World, memory_arena* Arena)
{
    u64 TriangleCount = (World->RegionCount - 1) * 3 * ArrayCount(world_region::Vertices);
    static_array<tri> Triangles = AllocStaticArray(Arena, tri, TriangleCount);
    
    for (u64 RegionIndex = 1; RegionIndex < World->RegionCount; RegionIndex++)
    {
        world_region* Region = World->Regions + RegionIndex;
        f32 Z = Region->Z;
        v4 Color = Region->Color;
        
        //Top
        for (u64 VertexIndex = 0; VertexIndex < ArrayCount(Region->Vertices); VertexIndex++)
        {
            tri Tri = {};
            
            Tri.Vertices[0].P = V3(Region->Center, Z);
            Tri.Vertices[1].P = V3(GetVertex(Region, VertexIndex), Z);
            Tri.Vertices[2].P = V3(GetVertex(Region, VertexIndex + 1), Z);
            
            SetColor(&Tri, Color);
            Add(&Triangles, Tri);
        }
        
        //Sides
        for (u64 VertexIndex = 0; VertexIndex < ArrayCount(Region->Vertices); VertexIndex++)
        {
            tri Tri0 = {};
            tri Tri1 = {};
            
            Tri0.Vertices[0].P = V3(GetVertex(Region, VertexIndex), Z);
            Tri0.Vertices[1].P = V3(GetVertex(Region, VertexIndex), 2.0f);
            Tri0.Vertices[2].P = V3(GetVertex(Region, VertexIndex + 1), 2.0f);
            
            Tri1.Vertices[0].P = V3(GetVertex(Region, VertexIndex + 1), 2.0f);
            Tri1.Vertices[1].P = V3(GetVertex(Region, VertexIndex + 1), Z);
            Tri1.Vertices[2].P = V3(GetVertex(Region, VertexIndex), Z);
            
            SetColor(&Tri0, Color);
            SetColor(&Tri1, Color);
            Add(&Triangles, Tri0);
            Add(&Triangles, Tri1);
        }
    }
    
    Assert(Triangles.Count == Triangles.Capacity);
    
    CalculateModelVertexNormals(Triangles.Memory, Triangles.Count);
    
    Assets->VertexBuffers[VertexBuffer_World] = CreateVertexBuffer(Triangles.Memory, Triangles.Count * sizeof(tri), 
                                                                   D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, sizeof(vertex));
}

static void
CreateWorld(world* World, u64 PlayerCount)
{
    Assert(PlayerCount <= 4);
    
    static u32 Seed = 2001;
    Seed++;
    
    v4 Colors[4] = {
        V4(0.4f, 0.8f, 0.35f, 1.0f),
        V4(0.3f, 0.4f, 0.7f, 1.0f)
    };
    
    World->X0 = -1.0f;
    World->Y0 = -1.0f;
    World->Width  = 2.0f;
    World->Height = 2.0f;
    
    World->Rows = 13;
    World->Cols = 13;
    
    f32 HexagonRadius = 0.5f * World->Height / World->Rows;
    f32 HexagonSideLength = 1.118034f * HexagonRadius;
    
    //Region 0 is invalid
    int RegionIndex = 1;
    
    for (int Y = 0; Y < World->Rows; Y++)
    {
        for (int X = 0; X < World->Cols; X++)
        {
            Assert(RegionIndex < ArrayCount(World->Regions));
            world_region* Region = World->Regions + RegionIndex;
            
            Assert(ArrayCount(Region->Vertices) == 6);
            Region->Center = WorldPosFromGridPos(World, X, Y);
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
                Region->OwnerIndex = RandomBetween(0, PlayerCount - 1);
                Region->Color = Colors[Region->OwnerIndex];
            }
            
            RegionIndex++;
        }
    }
    
    World->RegionCount = RegionIndex;
}