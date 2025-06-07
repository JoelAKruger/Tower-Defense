static v2
GetRegionVertex(entity* Region, i32 Index)
{
    Assert(Region->Type == Entity_WorldRegion);
    
    v2 Offsets[6] = {
        {0.0f, 1.0f},
        {0.86603f, 0.5f},
        {0.86603f, -0.5f},
        {0.0f, -1.0f},
        {-0.86603f, -0.5f},
        {-0.86603f, 0.5f}
    };
    
    Assert(Region->Type == Entity_WorldRegion);
    u32 VertexIndex = (ArrayCount(Offsets) + Index) % (ArrayCount(Offsets));
    
    v2 Result = Region->P.XY + Region->Size * Offsets[VertexIndex];
    return Result;
}

static v2
GetLocalRegionVertex(game_state* Game, u64 EntityIndex, i64 Index)
{
    v3 P = GetRegionP(Game, EntityIndex);
    f32 Size = Game->GlobalState.World.Entities[EntityIndex].Size;
    
    v2 Offsets[6] = {
        {0.0f, 1.0f},
        {0.86603f, 0.5f},
        {0.86603f, -0.5f},
        {0.0f, -1.0f},
        {-0.86603f, -0.5f},
        {-0.86603f, 0.5f}
    };
    
    u32 VertexIndex = (ArrayCount(Offsets) + Index) % (ArrayCount(Offsets));
    
    v2 Result = P.XY + Size * Offsets[VertexIndex];
    return Result;
}

static bool
IsWater(entity* Entity)
{
    Assert(Entity->Type == Entity_WorldRegion);
    return (Entity->Owner == -1);
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
InRegion(entity* Region, v2 WorldPos)
{
    Assert(Region->Type == Entity_WorldRegion);
    
    v2 A0 = WorldPos;
    
    v2 A1 = Region->P.XY;
    
    u32 IntersectCount = 0;
    
    for (u32 TestIndex = 0; TestIndex < 6; TestIndex++)
    {
        v2 B0 = GetRegionVertex(Region, TestIndex);
        v2 B1 = GetRegionVertex(Region, TestIndex + 1);
        
        if (LinesIntersect(A0, A1, B0, B1))
        {
            IntersectCount++;
        }
    }
    
    return (IntersectCount % 2 == 0);
}

static f32
DistanceInsideRegion(entity* Region, v2 P)
{
    Assert(Region->Type == Entity_WorldRegion);
    
    f32 MinDistance = 1000.0f;
    for (u32 VertexIndex = 0; VertexIndex < 6; VertexIndex++)
    {
        v2 A = GetRegionVertex(Region, VertexIndex);
        v2 B = GetRegionVertex(Region, VertexIndex + 1);
        
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
GetWaterColor(f32 Height)
{
    v3 WaterColor = V3(0.17f, 0.23f, 0.46f);
    v3 Color = WaterColor + 2.0f * (Height - 0.4f) * WaterColor;
    return V4(Color, 1.0f);
    //return V4(0.0f, 0.0f, 0.0f, 0.0f);
}

static v2
WorldPosFromGridPos(world* World, int X, int Y)
{
    f32 dX = World->Width / World->Cols;
    f32 dY = 0.866f * World->Height / World->Rows; //0.875f * World->Height / World->Rows;
    
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

void GenerateFoliage(world* World, memory_arena* Arena);

static u64 
AddEntity(world* World, entity Entity)
{
    Assert(World->EntityCount < ArrayCount(World->Entities));
    
    u64 Result = World->EntityCount;
    
    World->Entities[World->EntityCount++] = Entity;
    
    return Result;
}

static void
CreateWorld(world* World, u64 PlayerCount)
{
    *World = {};
    
    memory_arena Arena = Win32CreateMemoryArena(Megabytes(1), TRANSIENT);
    
    Assert(PlayerCount <= 4);
    
    static u32 Seed = 2001;
    Seed++;
    
    /*
    v4 Colors[4] = {
        V4(0.4f, 0.8f, 0.35f, 1.0f),
        V4(0.3f, 0.4f, 0.7f, 1.0f)
    };
*/
    
    v4 Colors[4] = {
        V4(0.30f, 0.69f, 0.29f, 1.0f),
        V4(0.60f, 0.31f, 0.64f, 1.0f),
        V4(0.89f, 0.10f, 0.11f, 1.0f),
        V4(1.00f, 0.50f, 0.00f, 1.0f)
    };
    
    World->X0 = -1.0f;
    World->Y0 = -1.0f;
    World->Width  = 2.0f;
    World->Height = 2.0f;
    
    World->Rows = 13;
    World->Cols = 13;
    
    //Region 0 is invalid
    int RegionIndex = 1;
    
    for (int Y = 0; Y < World->Rows; Y++)
    {
        for (int X = 0; X < World->Cols; X++)
        {
            entity Region = {.Type = Entity_WorldRegion};
            
            Region.Size = 0.5f * World->Height / World->Rows;
            Region.P.XY = WorldPosFromGridPos(World, X, Y);
            
            f32 RegionHeight = GetWorldHeight(Region.P.XY, Seed);
            Region.P.Z = 0.1f;
            
            bool RegionIsWater = (RegionHeight < 0.5f);
            if (RegionIsWater)
            {
                Region.Owner = -1;
                Region.Color = GetWaterColor(RegionHeight);
                Region.P.Z = 0.25f * (1.0f - RegionHeight);
                
                //Prevent z-fighting with water
                if (Region.P.Z < 0.13f)
                {
                    Region.P.Z = 0.13f;
                }
            }
            else
            {
                Region.Owner = RandomBetween(0, PlayerCount - 1);
                Region.Color = Colors[Region.Owner];
                Region.P.Z = 0.1f;
            }
            
            u64 RegionIndex = AddEntity(World, Region);
            
            if (!RegionIsWater)
            {
                if (RandomBetween(0, 2) == 0)
                {
                    entity Farm = {.Type = Entity_Farm};
                    Farm.Owner = Region.Owner;
                    Farm.Parent = RegionIndex;
                    AddEntity(World, Farm);
                }
            }
        }
    }
    
    GenerateFoliage(World, &Arena);
}

struct nearest_foliage
{
    entity* Foliage;
    f32 Distance;
};

static bool
IsValid(nearest_foliage Foliage)
{
    return Foliage.Foliage != 0;
}

static nearest_foliage
GetNearestFoliage(world* World, v3 P)
{
    nearest_foliage Result = {
        .Foliage = 0,
        .Distance = 100000.0f
    };
    
    for (int EntityIndex = 0; EntityIndex < World->EntityCount; EntityIndex++)
    {
        entity* Entity = World->Entities + EntityIndex;
        f32 Distance = Length(P - Entity->P);
        if (Entity->Type == Entity_Foliage && Distance < Result.Distance)
        {
            Result.Distance = Distance;
            Result.Foliage = Entity;
        }
    }
    
    return Result;
}

static u64
GetWorldRegionIndex(world* World, v2 P)
{
    u64 Result = 0;
    
    for (u64 EntityIndex = 1; EntityIndex < World->EntityCount; EntityIndex++)
    {
        entity* Entity = World->Entities + EntityIndex;
        
        if (Entity->Type == Entity_WorldRegion && InRegion(Entity, P))
        {
            Result = EntityIndex;
            break;
        }
    }
    
    return Result;
}

static foliage_type
RandomFoliage(bool Water)
{
    if (Water)
    {
        foliage_type Types[] = {Foliage_RibbonPlant, Foliage_Grass, Foliage_Rock};
        int Index = RandomBetween(0, ArrayCount(Types) - 1);
        return Types[Index];
    }
    else
    {
        foliage_type Types[] = {Foliage_PinkFlower, Foliage_Bush, Foliage_RibbonPlant, Foliage_Grass, Foliage_Rock};
        int Index = RandomBetween(0, ArrayCount(Types) - 1);
        return Types[Index];
    }
}

static char*
GetFoliageAssetName(foliage_type Type)
{
    char* Result = {};
    
    switch (Type)
    {
        case Foliage_PinkFlower:  Result = "2FPinkPlant_Plane.084"; break;
        case Foliage_Bush:        Result = "Bush2_Cube.046"; break;
        case Foliage_RibbonPlant: Result = "RibbonPlant2_Plane.079"; break;
        case Foliage_Grass:       Result = "GrassPatch101_Plane.040"; break;
        case Foliage_Rock:        Result = "Rock6_Cube.014"; break;
        default: Assert(0);
    }
    
    return Result; 
}

static void
GenerateFoliage(world* World, memory_arena* Arena)
{
    f32 MinDistance = 0.05f;
    
    for (int Index = 0; Index < 256;)
    {
        v2 TestP = V2(World->X0 + Random() * World->Width, World->Y0 + Random() * World->Height);
        
        u64 RegionIndex = GetWorldRegionIndex(World, TestP);
        
        if (RegionIndex)
        {
            entity* Region = World->Entities + RegionIndex;
            
            v3 P = V3(TestP, Region->P.Z);
            f32 Size = RandomBetween(0.01f, 0.1f);
            f32 MinDistanceInsideRegion = 0.5f * Size;
            nearest_foliage NearestFoliage = GetNearestFoliage(World, P);
            
            if ((!IsValid(NearestFoliage) || NearestFoliage.Distance >= Size + NearestFoliage.Foliage->Size) &&
                DistanceInsideRegion(Region, P.XY) >= MinDistanceInsideRegion)
            {
                entity Foliage = {.Type = Entity_Foliage};
                Foliage.FoliageType = RandomFoliage(IsWater(Region));
                Foliage.P = P;
                Foliage.Parent = RegionIndex;
                Foliage.Size = Size;
                
                AddEntity(World, Foliage);
                Index++;
            }
        }
    }
}