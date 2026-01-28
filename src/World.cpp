static v2
GetHexVertex(entity* Hex, i32 Index)
{
    Assert(Hex->Type == Entity_WorldHex);
    
    v2 Offsets[6] = {
        {0.0f, 1.0f},
        {0.86603f, 0.5f},
        {0.86603f, -0.5f},
        {0.0f, -1.0f},
        {-0.86603f, -0.5f},
        {-0.86603f, 0.5f}
    };
    
    Assert(Hex->Type == Entity_WorldHex);
    u32 VertexIndex = (ArrayCount(Offsets) + Index) % (ArrayCount(Offsets));
    
    v2 Result = Hex->P.XY + Hex->Size * Offsets[VertexIndex];
    return Result;
}

static span<entity*>
GetHexNeighbours(world* World, entity* Hex, memory_arena* Arena)
{
    Assert(Hex->Type == Entity_WorldHex);
    
    dynamic_array<entity*> Result = {.Arena = Arena};
    
    for (u64 EntityIndex = 0; EntityIndex < World->EntityCount; EntityIndex++)
    {
        entity* Entity = World->Entities + EntityIndex;
        if (Entity != Hex && 
            Entity->Type == Entity_WorldHex && 
            Length(Entity->P - Hex->P) < 2.5f * Hex->Size)
        {
            Append(&Result, Entity);
        }
    }
    
    return ToSpan(Result);
}

//TODO: Optimise this
static span<entity_handle>
GetHexNeighbourHandles(world* World, entity* Hex, memory_arena* Arena)
{
    Assert(Hex->Type == Entity_WorldHex);
    
    v2i TargetGridPositions[6] = {};
    
    if (Hex->GridP.Y % 2 == 1)
    {
        TargetGridPositions[0] = {Hex->GridP.X + 1, Hex->GridP.Y + 1};
        TargetGridPositions[1] = {Hex->GridP.X + 1, Hex->GridP.Y};
        TargetGridPositions[2] = {Hex->GridP.X + 1, Hex->GridP.Y - 1};
        TargetGridPositions[3] = {Hex->GridP.X, Hex->GridP.Y - 1};
        TargetGridPositions[4] = {Hex->GridP.X - 1, Hex->GridP.Y};
        TargetGridPositions[5] = {Hex->GridP.X, Hex->GridP.Y + 1};
    }
    else
    {
        TargetGridPositions[0] = {Hex->GridP.X, Hex->GridP.Y + 1};
        TargetGridPositions[1] = {Hex->GridP.X + 1, Hex->GridP.Y};
        TargetGridPositions[2] = {Hex->GridP.X, Hex->GridP.Y - 1};
        TargetGridPositions[3] = {Hex->GridP.X - 1, Hex->GridP.Y - 1};
        TargetGridPositions[4] = {Hex->GridP.X - 1, Hex->GridP.Y};
        TargetGridPositions[5] = {Hex->GridP.X - 1, Hex->GridP.Y + 1};
    }
    
    span<entity_handle> Result = AllocSpan(Arena, entity_handle, 6);
    
    for (u64 EntityIndex = 0; EntityIndex < World->EntityCount; EntityIndex++)
    {
        entity* Entity = World->Entities + EntityIndex;
        if (Entity->Type == Entity_WorldHex && Contains(Entity->GridP, TargetGridPositions, ArrayCount(TargetGridPositions)))
        {
            int Index = IndexOf(Entity->GridP, TargetGridPositions);
            Result[Index] = {(i32)EntityIndex};
        }
    }
    
    return Result;
}

static bool
HexesAreNeighbours(entity* A, entity* B)
{
    Assert(A->Type == Entity_WorldHex && B->Type == Entity_WorldHex);
    Assert(A->Size == B->Size);
    
    return (Length(A->P - B->P) < 2.5f * A->Size);
}

static bool
AreApproxEqual(v2 A, v2 B, f32 Tolerance)
{
    return (Distance(A, B) <= Tolerance);
}

static span<span<v2>>
GetEdgesOfHexGroup(world* World, span<entity_handle> Hexes, memory_arena* Arena)
{
    dynamic_array<line_segment> Lines = {.Arena = Arena};
    
    //Step 1. Get all edges
    for (u32 EntityIndex = 1; EntityIndex < World->EntityCount; EntityIndex++)
    {
        entity* Hex = World->Entities + EntityIndex;
        if (Hex->Type == Entity_WorldHex && Contains((entity_handle){(i32)EntityIndex}, Hexes))
        {
            span<entity_handle> Neighbours = GetHexNeighbourHandles(World, Hex, Arena);
            
            for (int NeighbourIndex = 0; NeighbourIndex < Neighbours.Count; NeighbourIndex++)
            {
                if (!Neighbours[NeighbourIndex] || !Contains(Neighbours[NeighbourIndex], Hexes))
                {
                    line_segment Line = {
                        GetHexVertex(Hex, NeighbourIndex),
                        GetHexVertex(Hex, NeighbourIndex + 1)
                    };
                    Append(&Lines, Line);
                }
            }
        }
    }
    
    //Step 2. Construct Areas
    f32 Tolerance = 0.02f;
    
    u64 Iter = 0;
    
    dynamic_array<span<v2>> Result = {.Arena = Arena};
    
    while (Lines.Count > 0)
    {
        v2 Start = Lines[0].Start;
        v2 Current = Lines[0].End;
        RemoveAt(0, &Lines);
        u64 CurrentIndex = 0;
        dynamic_array<v2> Vertices = {.Arena = Arena};
        Append(&Vertices, Start);
        Append(&Vertices, Current);
        
        while (!AreApproxEqual(Start, Current, Tolerance))
        {
            for (u64 LineIndex = 0; LineIndex < Lines.Count; LineIndex++)
            {
                line_segment Line = Lines[LineIndex];
                
                if (AreApproxEqual(Current, Line.Start, Tolerance))
                {
                    Append(&Vertices, Line.End);
                    Current = Line.End;
                    CurrentIndex = LineIndex;
                    
                    RemoveAt(LineIndex, &Lines);
                    LineIndex--;
                    break;
                }
                else if (AreApproxEqual(Current, Line.End, Tolerance))
                {
                    Append(&Vertices, Line.Start);
                    Current = Line.Start;
                    CurrentIndex = LineIndex;
                    
                    RemoveAt(LineIndex, &Lines);
                    LineIndex--;
                    break;
                }
            }
        }
        
        Append(&Result, ToSpan(Vertices));
        Clear(&Vertices);
        
        Iter++;
    }
    
    return ToSpan(Result);
}

static span<span<v2>>
GetRegionEdges(world* World, i32 Region, memory_arena* Arena)
{
    Assert(Arena->Type == TRANSIENT);
    Assert(Region > 0);
    
    dynamic_array<entity_handle> Hexes = {.Arena = Arena};
    
    for (u32 EntityIndex = 1; EntityIndex < World->EntityCount; EntityIndex++)
    {
        entity* Hex = World->Entities + EntityIndex;
        if (Hex->Type == Entity_WorldHex && Hex->Region == Region)
        {
            Append(&Hexes, (entity_handle){(i32)EntityIndex});
        }
    }
    
    span<span<v2>> Result = GetEdgesOfHexGroup(World, ToSpan(Hexes), Arena);
    
    return Result;
}

/*
static v2
GetLocalHexVertex(game_state* Game, u64 EntityIndex, i64 Index)
{
    v3 P = GetEntityP(Game, EntityIndex);
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
*/
static bool
IsWater(entity* Entity)
{
    Assert(Entity->Type == Entity_WorldHex);
    f32 LandZ = 0.1f;
    return (Entity->P.Z > LandZ);
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
InHex(entity* Hex, v2 WorldPos)
{
    Assert(Hex->Type == Entity_WorldHex);
    
    v2 A0 = WorldPos;
    
    v2 A1 = Hex->P.XY;
    
    u32 IntersectCount = 0;
    
    for (u32 TestIndex = 0; TestIndex < 6; TestIndex++)
    {
        v2 B0 = GetHexVertex(Hex, TestIndex);
        v2 B1 = GetHexVertex(Hex, TestIndex + 1);
        
        if (LinesIntersect(A0, A1, B0, B1))
        {
            IntersectCount++;
        }
    }
    
    return (IntersectCount % 2 == 0);
}

static f32
DistanceInsideHex(entity* Hex, v2 P)
{
    Assert(Hex->Type == Entity_WorldHex);
    
    f32 MinDistance = 1000.0f;
    for (u32 VertexIndex = 0; VertexIndex < 6; VertexIndex++)
    {
        v2 A = GetHexVertex(Hex, VertexIndex);
        v2 B = GetHexVertex(Hex, VertexIndex + 1);
        
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
NearestTowerTo(v2 P, global_game_state* Game, entity_handle Hex)
{
    f32 NearestDistanceSq = 10000.0f;
    tower* Nearest = 0;
    u32 Index;
    for (u32 TowerIndex = 0; TowerIndex < Game->TowerCount; TowerIndex++)
    {
        tower* Tower = Game->Towers + TowerIndex;
        if (Tower->HexIndex == Hex.Index)
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
GetWaterColor(f32 Z)
{
    f32 Height = 1.0f - 4 * Z;
    v3 WaterColor = V3(0.17f, 0.23f, 0.46f);
    v3 Color = WaterColor + 2.0f * (Height - 0.4f) * WaterColor;
    return V4(Color, 1.0f);
}

/*
static v2
WorldPosFromGridPos(world* World, world_grid_position P)
{
    f32 dX = World->Width / World->Cols;
    f32 dY = 0.866f * World->Height / World->Rows; //0.875f * World->Height / World->Rows;
    
    v2 Result = {World->X0 + P.X * dX, World->Y0 + P.Y * dY};
    if (P.Y % 2 == 1)
    {
        Result.X += 0.5f * dX;
    }
    
    return Result;
}
*/

static void
SetColor(tri* Tri, v4 Color)
{
    Tri->Vertices[0].Color = Color;
    Tri->Vertices[1].Color = Color;
    Tri->Vertices[2].Color = Color;
}

void GenerateFoliage(world* World, memory_arena* Arena);


//TODO: Should return entity_handle
static u64 
AddEntity(world* World, entity Entity)
{
    Assert(World->EntityCount < ArrayCount(World->Entities));
    
    u64 Result = World->EntityCount;
    
    World->Entities[World->EntityCount++] = Entity;
    
    return Result;
}

static v4
GetPlayerColor(i32 PlayerIndex)
{
    if (PlayerIndex == -1)
    {
        return V4(0.5f, 0.5f, 0.5f, 1.0f);
    }
    
    v4 Colors[4] = {
        V4(0.40f, 0.79f, 0.39f, 1.0f),
        V4(0.70f, 0.41f, 0.74f, 1.0f),
        V4(0.99f, 0.20f, 0.21f, 1.0f),
        V4(1.00f, 0.50f, 0.00f, 1.0f)
    };
    Assert(PlayerIndex < ArrayCount(Colors));
    return Colors[PlayerIndex];
}

static v2
GridPositionToHexPosition(world* World, int GridX, int GridY)
{
    int N = 3;
    
    f32 dX = (World->Width / World->Cols);
    f32 dY = 0.866f * World->Height / World->Rows;
    
    f32 HexX = World->X0 + GridX * dX;
    f32 HexY = World->Y0 + GridY * dY;
    
    if (GridY % 2 == 1)
    {
        HexX += 0.5f * dX;
    }
    
    v2 Result = V2(HexX, HexY);
    return Result;
}

struct nearest_region
{
    f32 Distance;
    int Region;
};

static nearest_region
NearestRegionTo(v2 P, world* World)
{
    nearest_region Result = {
        .Distance = FLT_MAX,
        .Region = -1
    };
    
    for (int I = 1; I < World->RegionCount; I++)
    {
        f32 Dist = Distance(P, World->Regions[I].Center);
        if (Dist < Result.Distance)
        {
            Result.Distance = Dist;
            Result.Region = I;
        }
    }
    
    return Result;
}

static void
CreateRegionCenters(world* World, int Max = 100)
{
    f32 MinDistance = 0.2f;
    int Count = 0;
    
    World->RegionCount = 1;
    
    while (Count < Max && World->RegionCount < ArrayCount(World->Regions))
    {
        v2 MinP = GridPositionToHexPosition(World, 7, 7);
        v2 MaxP = GridPositionToHexPosition(World, World->Cols - 7, World->Rows - 5);
        v2 RandomP = V2(RandomBetween(MinP.X, MaxP.X),
                        RandomBetween(MinP.Y, MaxP.Y));
        
        if (NearestRegionTo(RandomP, World).Distance >= MinDistance)
        {
            World->Regions[World->RegionCount++].Center = RandomP;
        }
        
        Count++;
    }
}

static entity*
GetRandomLandHex(world* World, random_series* RNG)
{
    entity* Result = 0;
    
    u64 LandHexCount = 0;
    for (u64 EntityIndex = 1; EntityIndex < World->EntityCount; EntityIndex++)
    {
        entity* Entity = World->Entities + EntityIndex;
        if (Entity->Type == Entity_WorldHex && !IsWater(Entity))
        {
            LandHexCount++;
        }
    }
    
    u64 Index = RandomBetween(0, LandHexCount - 1, RNG);
    
    u64 Count = 0;
    for (u64 EntityIndex = 1; EntityIndex < World->EntityCount; EntityIndex++)
    {
        entity* Entity = World->Entities + EntityIndex;
        if (Entity->Type == Entity_WorldHex && !IsWater(Entity))
        {
            if (Count == Index)
            {
                Result = Entity;
                break;
            }
            Count++;
        }
    }
    
    return Result;
}

static void
CreateWorld(world* World, u64 PlayerCount)
{
    *World = {};
    AddEntity(World, {});
    
    memory_arena Arena = Win32CreateMemoryArena(Megabytes(1), TRANSIENT);
    
    Assert(PlayerCount <= 4);
    
    static u32 Seed = 2001;
    Seed++;
    
    random_series RNG = BeginRandom(Seed);
    
    
    //v4 Colors[4] = {
    //V4(0.4f, 0.8f, 0.35f, 1.0f),
    //V4(0.3f, 0.4f, 0.7f, 1.0f)
    //};
    
    
    World->X0 = -1.0f;
    World->Y0 = -1.0f;
    World->Width  = 2.0f;
    World->Height = 2.0f;
    
    World->Rows = 20;
    World->Cols = 20;
    
    World->HexSize = 0.5f * World->Height / World->Rows;
    
    CreateRegionCenters(World);
    
    for (int Y = 0; Y < World->Rows; Y++)
    {
        for (int X = 0; X < World->Cols; X++)
        {
            entity Hex = {.Type = Entity_WorldHex};
            
            Hex.GridP = {X, Y};
            Hex.Size = 0.97f * World->HexSize / 0.866f;
            Hex.P.XY = GridPositionToHexPosition(World, X, Y);
            
            f32 HexHeight = GetWorldHeight(Hex.P.XY, Seed);
            Hex.P.Z = 0.1f;
            
            bool GenerateFoliage = false;
            bool HexIsWater = (HexHeight < 0.5f);
            if (HexIsWater)
            {
                Hex.Owner = -1;
                Hex.P.Z = 0.25f * (1.0f - HexHeight);
                Hex.Color = GetWaterColor(Hex.P.Z);
                
                //Prevent z-fighting with water
                if (Hex.P.Z < 0.13f)
                {
                    Hex.P.Z = 0.13f;
                }
            }
            else
            {
                Hex.P.Z = 0.1f;
                
                GenerateFoliage = true;
                Hex.Owner = -1;
                Hex.Color = GetPlayerColor(Hex.Owner);
            }
            
            if (!HexIsWater)
            {
                Hex.Region = NearestRegionTo(Hex.P.XY, World).Region;
                World->Regions[Hex.Region].HexCount++;
            }
            
            u64 HexIndex = AddEntity(World, Hex);
        };
    }
    
    //Give players their starting tile
    for (u64 PlayerIndex = 0; PlayerIndex < PlayerCount; PlayerIndex++)
    {
        while (true)
        {
            entity* StartingHex = GetRandomLandHex(World, &RNG);
            if (StartingHex->Owner == -1)
            {
                StartingHex->Owner = PlayerIndex;
                StartingHex->Color = GetPlayerColor(StartingHex->Owner);
                StartingHex->Level = 1;
                break;
            }
        }
    }
    
    
    GenerateFoliage(World, &Arena);
    
}



/*
static void
CreateWorld(world* World, u64 PlayerCount)
{
    *World = {};
    AddEntity(World, {});
    
    memory_arena Arena = Win32CreateMemoryArena(Megabytes(1), TRANSIENT);
    
    Assert(PlayerCount <= 4);
    
    static u32 Seed = 2001;
    Seed++;
    
    World->X0 = -1.0f;
    World->Y0 = -1.0f;
    World->Width  = 2.0f;
    World->Height = 2.0f;
    
    World->Rows = 20;
    World->Cols = 20;
    
    World->HexSize = 0.5f * World->Height / World->Rows;
    
    CreateRegionCenters(World, 1);
    
    v2i Land[] = {
        {7, 7},
        {8, 7},
        {8, 8},
        {9, 9},
        {9, 10}
    };
    
    for (int Y = 0; Y < World->Rows; Y++)
    {
        for (int X = 0; X < World->Cols; X++)
        {
            entity Hex = {.Type = Entity_WorldHex};
            
            Hex.GridP = {X, Y};
            Hex.Size = 0.97f * World->HexSize / 0.866f;
            Hex.P.XY = GridPositionToHexPosition(World, X, Y);
            
            bool Special = Contains(v2i{X, Y}, Land, ArrayCount(Land));
            
            f32 HexHeight = Special ? 1.0f : 0.4f;
            
            bool GenerateFoliage = false;
            bool HexIsWater = (HexHeight < 0.5f);
            if (HexIsWater)
            {
                Hex.Owner = -1;
                Hex.Color = GetWaterColor(HexHeight);
                Hex.P.Z = 0.25f * (1.0f - HexHeight);
                
                //Prevent z-fighting with water
                if (Hex.P.Z < 0.13f)
                {
                    Hex.P.Z = 0.13f;
                }
            }
            else
            {
                bool HexIsFoliage = Random() > 0.5f;
                Hex.P.Z = 0.1f;
                
                if (HexIsFoliage)
                {
                    GenerateFoliage = true;
                    Hex.Owner = -1;
                    Hex.Color = V4(0.5f, 0.5f, 0.5f, 1.0f);
                }
                else
                {
                    Hex.Owner = RandomBetween(0, PlayerCount - 1);
                    Hex.Color = GetPlayerColor(Hex.Owner);
                }
            }
            
            if (!HexIsWater)
            {
                Hex.Region = NearestRegionTo(Hex.P.XY, World).Region;
            }
            
            u64 HexIndex = AddEntity(World, Hex);
            
            if (!HexIsWater)
            {
                world_region* Region = World->Regions + Hex.Region;
                Region->Hexes[Region->HexCount++] = {(i32)HexIndex};
                Assert(Region->HexCount < ArrayCount(Region->Hexes));
            }
        };
    }
    
    GenerateFoliage(World, &Arena);
    
}
*/

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
        if (Entity->Type == Entity_Foliage && Distance < Result.Distance && Entity->FoliageType != Foliage_Paving)
        {
            Result.Distance = Distance;
            Result.Foliage = Entity;
        }
    }
    
    return Result;
}

static u64
GetWorldHexIndex(world* World, v2 P)
{
    u64 Result = 0;
    
    for (u64 EntityIndex = 1; EntityIndex < World->EntityCount; EntityIndex++)
    {
        entity* Entity = World->Entities + EntityIndex;
        
        if (Entity->Type == Entity_WorldHex && InHex(Entity, P))
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

static f32
GetDefaultSizeOf(foliage_type Type)
{
    f32 Result = 1.0f;
    
    switch (Type)
    {
        case Foliage_PinkFlower:  Result = 0.01f; break;
        case Foliage_Bush:        Result = 0.06f; break;
        case Foliage_RibbonPlant: Result = 0.05f; break;
        case Foliage_Grass:       Result = 0.02f; break;
        case Foliage_Rock:        Result = 0.005f; break;
        default: Assert(0);
    }
    
    return Result; 
}

static void
GenerateFoliage(world* World, memory_arena* Arena)
{
    f32 MinDistance = 0.05f;
    
    for (int Index = 0; Index < 1024;)
    {
        v2 TestP = V2(World->X0 + Random() * World->Width, World->Y0 + Random() * World->Height);
        
        u64 HexIndex = GetWorldHexIndex(World, TestP);
        entity* Hex = World->Entities + HexIndex;
        
        if (HexIndex && IsWater(Hex))
        {
            v3 P = V3(TestP, 0);
            foliage_type FoliageType = RandomFoliage(IsWater(Hex));
            f32 Size = GetDefaultSizeOf(FoliageType) * RandomBetween(0.5f, 1.5f);
            f32 MinDistanceInsideHex = 0.5f * Size;
            nearest_foliage NearestFoliage = GetNearestFoliage(World, P);
            
            if ((!IsValid(NearestFoliage) || NearestFoliage.Distance >= Size + NearestFoliage.Foliage->Size) &&
                DistanceInsideHex(Hex, P.XY) >= MinDistanceInsideHex)
            {
                entity Foliage = {.Type = Entity_Foliage};
                Foliage.FoliageType = FoliageType;
                Foliage.P = P;
                Foliage.Parent = (entity_handle){(i32)HexIndex};
                Foliage.Size = Size;
                
                AddEntity(World, Foliage);
                Index++;
            }
        }
    }
}

static void
DoAirAttack(entity* Defender, int TotalAttackPower)
{
    f32 SuccessProbability = 0.6f;
    
    int TotalDefendPower = Defender->Level;
    
    int AttackPower = TotalAttackPower;
    int DefendPower = TotalDefendPower;
    
    while (AttackPower > 0 && DefendPower > 0)
    {
        if (Random() < SuccessProbability)
        {
            Defender->Level--;
            DefendPower--;
        }
        else
        {
            AttackPower--;
        }
    }
    
    if (AttackPower == 0) //Defender won
    {
    }
    else //Attacker won
    {
        //Update defender
        Defender->Owner = -1;
        Defender->Level = 0;
        
        if (IsWater(Defender))
        {
            Defender->Color = GetWaterColor(Defender->P.Z);
        }
        else
        {
            Defender->Color = GetPlayerColor(-1);
        }
    }
}

static void
DoAttack(u64 AttackerPlayerIndex, span<entity*> Attackers, entity* Defender)
{
    f32 SuccessfulAttackProbabilities[6] = {
        0.5f,
        0.6f,
        0.7f,
        0.8f,
        0.9f,
        1.0f
    };
    Assert(Attackers.Count < ArrayCount(SuccessfulAttackProbabilities));
    
    f32 SuccessProbability = SuccessfulAttackProbabilities[Attackers.Count - 1];
    
    if (Defender->Owner == -1)
    {
        SuccessProbability = 1.0f;
    }
    
    int TotalAttackPower = 0;
    for (entity* Attacker : Attackers)
    {
        TotalAttackPower += Attacker->Level;
    }
    
    int TotalDefendPower = Defender->Level;
    
    int AttackPower = TotalAttackPower;
    int DefendPower = TotalDefendPower;
    
    while (AttackPower > 0 && DefendPower > 0)
    {
        entity* Attacker = GetRandomFrom(Attackers);
        
        if (Attacker->Level > 0)
        {
            if (Random() < SuccessProbability)
            {
                Defender->Level--;
                DefendPower--;
            }
            else
            {
                Attacker->Level--;
                AttackPower--;
            }
        }
    }
    
    if (AttackPower == 0) //Defender won
    {
        
    }
    else //Attacker won
    {
        //Subtract level from attacker with highest level
        int HighestLevel = 1;
        for (entity* Attacker : Attackers)
        {
            HighestLevel = Max(HighestLevel, Attacker->Level);
        }
        
        while (true)
        {
            entity* Attacker = GetRandomFrom(Attackers);
            if (Attacker->Level == HighestLevel)
            {
                Attacker->Level--;
                break;
            }
        }
        
        Defender->Owner = AttackerPlayerIndex;
        Defender->Color = GetPlayerColor(Defender->Owner); //TODO: Is this good?
        Defender->Level = 1;
    }
    
    for (entity* Attacker : Attackers)
    {
        if (Attacker->Level <= 0)
        {
            Attacker->Owner = -1;
            if (IsWater(Attacker))
            {
                Attacker->Color = GetWaterColor(Attacker->P.Z);
            }
            else
            {
                Attacker->Color = GetPlayerColor(-1);
            }
        }
    }
}