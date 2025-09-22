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

// Unused
static span<entity*>
GetRegionNeighbours(world* World, entity* Region, memory_arena* Arena)
{
    Assert(Region->Type == Entity_WorldRegion);
    
    dynamic_array<entity*> Result = {.Arena = Arena};
    
    for (u64 EntityIndex = 0; EntityIndex < World->EntityCount; EntityIndex++)
    {
        entity* Entity = World->Entities + EntityIndex;
        if (Entity != Region && 
            Entity->Type == Entity_WorldRegion && 
            Length(Entity->P - Region->P) < 2.5f * Region->Size)
        {
            Append(&Result, Entity);
        }
    }
    
    return ToSpan(Result);
}

static bool
RegionsAreNeighbours(entity* A, entity* B)
{
    Assert(A->Type == Entity_WorldRegion && B->Type == Entity_WorldRegion);
    Assert(A->Size == B->Size);
    
    return (Length(A->P - B->P) < 2.5f * A->Size);
}

static v2
GetLocalRegionVertex(game_state* Game, u64 EntityIndex, i64 Index)
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

static u64 
AddEntity(world* World, entity Entity)
{
    Assert(World->EntityCount < ArrayCount(World->Entities));
    
    u64 Result = World->EntityCount;
    
    World->Entities[World->EntityCount++] = Entity;
    
    return Result;
}

static v4
GetPlayerColor(u64 PlayerIndex)
{
    v4 Colors[4] = {
        V4(0.40f, 0.79f, 0.39f, 1.0f),
        V4(0.70f, 0.41f, 0.74f, 1.0f),
        V4(0.99f, 0.20f, 0.21f, 1.0f),
        V4(1.00f, 0.50f, 0.00f, 1.0f)
    };
    Assert(PlayerIndex < ArrayCount(Colors));
    return Colors[PlayerIndex];
}

static u64
GetWorldRegionForTilePosition(world* World, tile_position P)
{
    u64 Result = 0;
    for (u64 EntityIndex = 0; EntityIndex < World->EntityCount; EntityIndex++)
    {
        entity* Entity = World->Entities + EntityIndex;
        if (Entity->Type == Entity_WorldRegion)
        {
            Assert(Entity->TilePositionIsValid);
            if (Entity->TilePosition.GridX == P.GridX &&
                Entity->TilePosition.GridY == P.GridY)
            {
                Result = EntityIndex;
                break;
            }
        }
    }
    return Result;
}

static bool
IsValidTilePosition(tile_position P)
{
    int N = 3;
    bool Result = (P.TileX >= -N && P.TileX < N && P.TileY >= -N && P.TileY < N &&
                   ((P.Top && (P.TileX + P.TileY < N-1) && (P.TileX + P.TileY > -N-2)) ||
                    (!P.Top && (P.TileX + P.TileY < N) && (P.TileX + P.TileY > -N-1))));
    return Result;
}

static bool
TileIsOnEdge(tile_position P)
{
    int N = 3;
    bool Result = (P.TileX == -N || P.TileX == N-1 || P.TileY == -N || P.TileY == N-1 ||
                   (P.Top && ((P.TileX + P.TileY == N-2) || (P.TileX + P.TileY == -N-1))) ||
                   (!P.Top && ((P.TileX + P.TileY == N-1) || (P.TileX + P.TileY == -N))));
    return Result;
}

static entity*
GetEntityAtTilePosition(world* World, tile_position P)
{
    Assert(IsValidTilePosition(P));
    
    entity* Result = 0;
    
    for (u64 EntityIndex = 0; EntityIndex < World->EntityCount; EntityIndex++)
    {
        entity* Entity = World->Entities + EntityIndex;
        tile_position TestP = Entity->TilePosition;
        
        if (TestP.GridX == P.GridX && TestP.GridY == P.GridY && TestP.Top == P.Top &&
            TestP.TileX == P.TileX && TestP.TileY == P.TileY)
        {
            Result = Entity;
            break;
        }
    }
    
    return Result;
}

struct tile_neighbour
{
    tile_position P;
    entity* Entity;
};

static span<tile_neighbour>
GetLocalNeighbours(memory_arena* Arena, world* World, tile_position P)
{
    dynamic_array<tile_neighbour> Result = {.Arena = Arena};
    
    tile_position TestPositions[3];
    
    if (P.Top == true)
    {
        TestPositions[0] = {.GridX = P.GridX, .GridY = P.GridY, .TileX = P.TileX + 1, .TileY = P.TileY,     .Top = false};
        TestPositions[1] = {.GridX = P.GridX, .GridY = P.GridY, .TileX = P.TileX,     .TileY = P.TileY + 1, .Top = false};
        TestPositions[2] = {.GridX = P.GridX, .GridY = P.GridY, .TileX = P.TileX,     .TileY = P.TileY,     .Top = false};
    }
    else
    {
        TestPositions[0] = {.GridX = P.GridX, .GridY = P.GridY, .TileX = P.TileX - 1, .TileY = P.TileY,     .Top = true};
        TestPositions[1] = {.GridX = P.GridX, .GridY = P.GridY, .TileX = P.TileX,     .TileY = P.TileY - 1, .Top = true};
        TestPositions[2] = {.GridX = P.GridX, .GridY = P.GridY, .TileX = P.TileX,     .TileY = P.TileY,     .Top = true};
    }
    
    for (tile_position TestP : TestPositions)
    {
        if (IsValidTilePosition(TestP))
        {
            tile_neighbour Neighbour = {
                .P = TestP,
                .Entity = GetEntityAtTilePosition(World, TestP)
            };
            Append(&Result, Neighbour);
        }
    }
    
    return ToSpan(Result);
}

static v2
TilePositionToRegionPosition(world* World, tile_position P)
{
    int N = 3;
    
    f32 dX = (World->Width / World->Cols);
    f32 dY = 0.866f * World->Height / World->Rows;
    
    f32 RegionX = World->X0 + P.GridX * dX;
    f32 RegionY = World->Y0 + P.GridY * dY;
    
    if (P.GridY % 2 == 1)
    {
        RegionX += 0.5f * dX;
    }
    
    v2 Result = V2(RegionX, RegionY);
    return Result;
}

static v2
TilePositionToWorldPosition(world* World, tile_position P)
{
    int N = 3;
    
    v2 dP = {};
    
    if (P.Top)
    {
        dP = (World->RegionSize / N) * (M2x2(V2(0.866f, 0.5f), V2(0, 1)) * V2(P.TileX + 0.66f, P.TileY + 0.66f));
    }
    else
    {
        dP = (World->RegionSize / N) * (M2x2(V2(0.866f, 0.5f), V2(0, 1)) * V2(P.TileX + 0.33f, P.TileY + 0.33f));
    }
    
    v2 Result = TilePositionToRegionPosition(World, P) + dP;
    return Result;
}

static bool
IsValidPathPosition(world* World, u64 RegionIndex, tile_position P, memory_arena* Arena, u32 PathOrigin)
{
    span<tile_neighbour> Neighbours = GetLocalNeighbours(Arena, World, P);
    
    u64 PathNeighbourCount = 0;
    for (tile_neighbour Neighbour : Neighbours)
    {
        if (Neighbour.Entity && Neighbour.Entity->PathOrigin == PathOrigin)
        {
            PathNeighbourCount++;
        }
    }
    
    return (PathNeighbourCount < 2);
}

static region_edge
GetTileEdgeType(tile_position P)
{
    int N = 3;
    Assert(TileIsOnEdge(P));
    
    region_edge Result = {};
    
    // Right side
    if (P.TileX == N-1 && P.TileX + P.TileY != N-1 && !(P.Top == false && P.TileY == -N))
    {
        Result = RegionEdge_Right;
    }
    // Bottom right
    else if (P.TileY == -N && (P.TileX + P.TileY != -N-1))
    {
        Result = RegionEdge_BottomRight;
    }
    // Bottom left, bottom tiles
    else if (P.Top == false && P.TileX + P.TileY == -N && P.TileX != -N)
    {
        Result = RegionEdge_BottomLeft;
    }
    //Bottom left, top tiles
    else if (P.Top == true && P.TileX + P.TileY == -N-1)
    {
        Result = RegionEdge_BottomLeft;
    }
    // Left side
    else if (P.TileX == -N && !(P.Top && P.TileY == N-1))
    {
        Result = RegionEdge_Left;
    }
    
    // Top left
    else if (P.TileY == N-1 && P.TileX + P.TileY != N-1)
    {
        Result = RegionEdge_TopLeft;
    }
    // Top right, bottom tiles
    else if (P.Top == false && P.TileX + P.TileY == N-1)
    {
        Result = RegionEdge_TopRight;
    }
    // Top right, top tiles
    else
    {
        Result = RegionEdge_TopRight;
    }
    
    return Result;
}

static entity*
CreatePathForRegion(world* World, tile_position Start, memory_arena* Arena, span<region_edge> DisallowedEdges, u32 PathOrigin)
{
    entity* End = 0;
    
    Assert(IsValidTilePosition(Start));
    u64 RegionIndex = GetWorldRegionForTilePosition(World, Start);
    Assert(RegionIndex > 0);
    
    entity* Region = World->Entities + RegionIndex;
    
    entity Pebble = {
        .Type = Entity_Foliage,
        .Size = 0.01f,
        .P = V3(TilePositionToWorldPosition(World, Start), 0.01f),
        .Angle = 0,
        .Owner = Region->Owner,
        .Parent = (i32) RegionIndex,
        .FoliageType = Foliage_Paving,
        .TilePositionIsValid = true,
        .TilePosition = Start,
        .PathOrigin = PathOrigin
    };
    
    AddEntity(World, Pebble);
    
    for (int I = 0; I < 100; I++)
    {
        span<tile_neighbour> Neighbours = GetLocalNeighbours(Arena, World, Pebble.TilePosition);
        
        int Index = RandomBetween(0, Neighbours.Count - 1);
        if (!Neighbours[Index].Entity &&
            IsValidPathPosition(World, RegionIndex, Neighbours[Index].P, Arena, PathOrigin))
        {
            tile_position P = Neighbours[Index].P;
            bool IsOnEdge = TileIsOnEdge(P);
            
            if (IsOnEdge)
            {
                region_edge EdgeType = GetTileEdgeType(P);
                if (Contains(EdgeType, DisallowedEdges))
                {
                    continue;
                }
            }
            
            Pebble.P = V3(TilePositionToWorldPosition(World, Neighbours[Index].P), 0.01f);
            Pebble.TilePosition = Neighbours[Index].P;
            u64 EntityIndex = AddEntity(World, Pebble);
            End = World->Entities + EntityIndex;
            
            if (IsOnEdge)
            {
                break;
            }
        }
    }
    
    return End;
}

static region_edge
GetOppositeEdgeType(region_edge Edge)
{
    region_edge Result = {};
    switch (Edge)
    {
        case RegionEdge_None: Result = RegionEdge_None; break;
        case RegionEdge_Right: Result = RegionEdge_Left; break;
        case RegionEdge_BottomRight: Result = RegionEdge_TopLeft; break;
        case RegionEdge_BottomLeft: Result = RegionEdge_TopRight; break;
        case RegionEdge_Left: Result = RegionEdge_Right; break;
        case RegionEdge_TopLeft: Result = RegionEdge_BottomRight; break;
        case RegionEdge_TopRight: Result = RegionEdge_BottomLeft; break;
    }
    return Result;
}

//TODO: Write this in terms of the function a bit above
static tile_position
GetOppositeTileFromEdge(tile_position P)
{
    int N = 3;
    Assert(TileIsOnEdge(P));
    
    tile_position Result = {};
    
    // Right side
    if (P.TileX == N-1 && P.TileX + P.TileY != N-1 && !(P.Top == false && P.TileY == -N))
    {
        Result.GridX = P.GridX + 1;
        Result.GridY = P.GridY;
        
        if (P.Top)
        {
            Result.TileX = P.TileX - (2*N-1);
            Result.TileY = P.TileY + N;
            Result.Top = false;
        }
        else
        {
            Result.TileX = P.TileX - (2*N-1);
            Result.TileY = P.TileY + (N-1);
            Result.Top = true;
        }
    }
    // Bottom right
    else if (P.TileY == -N)
    {
        Result.GridY = P.GridY - 1;
        if (P.GridY % 2 == 0)
        {
            Result.GridX = P.GridX;
        }
        else
        {
            Result.GridX = P.GridX + 1;
        }
        
        if (P.Top)
        {
            Result.TileX = P.TileX - (N-1);
            Result.TileY = P.TileY + (2*N-1);
            Result.Top = false;
        }
        else
        {
            Result.TileX = P.TileX - N;
            Result.TileY = P.TileY + (2*N-1);
        }
    }
    
    // Bottom left, bottom tiles
    else if (P.Top == false && P.TileX + P.TileY == -N && P.TileX != -N)
    {
        Result.GridY = P.GridY - 1;
        if (P.GridY % 2 == 0)
        {
            Result.GridX = P.GridX - 1;
        }
        else
        {
            Result.GridX = P.GridX;
        }
        
        Result.Top = true;
        Result.TileX = P.TileX + N-1;
        Result.TileY = P.TileY + N-1;
    }
    
    //Bottom left, top tiles
    else if (P.Top == true && P.TileX + P.TileY == -N-1)
    {
        Result.GridY = P.GridY - 1;
        if (P.GridY % 2 == 0)
        {
            Result.GridX = P.GridX - 1;
        }
        else
        {
            Result.GridX = P.GridX;
        }
        
        Result.Top = false;
        Result.TileX = P.TileX + N;
        Result.TileY = P.TileY + N;
    }
    
    // Left side
    else if (P.TileX == -N && !(P.Top && P.TileY == N-1))
    {
        Result.GridX = P.GridX - 1;
        Result.GridY = P.GridY;
        
        if (P.Top)
        {
            Result.Top = false;
            Result.TileX = P.TileX + (2*N-1);
            Result.TileY = P.TileY - (N-1);
        }
        else
        {
            Result.Top = true;
            Result.TileX = P.TileX + (2*N-1);
            Result.TileY = P.TileY - N;
        }
    }
    
    // Top left
    else if (P.TileY == N-1 && P.TileX + P.TileY != N-1)
    {
        Result.GridY = P.GridY + 1;
        if (P.GridY % 2 == 0)
        {
            Result.GridX = P.GridX - 1;
        }
        else
        {
            Result.GridX = P.GridX;
        }
        
        if (P.Top)
        {
            Result.Top = false;
            Result.TileX = P.TileX + N;
            Result.TileY = P.TileY - (2*N-1);
        }
        else
        {
            Result.Top = true;
            Result.TileX = P.TileX + (N-1);
            Result.TileY = P.TileY - (2*N-1);
        }
    }
    
    // Top right, bottom tiles
    else if (P.Top == false && P.TileX + P.TileY == N-1)
    {
        Result.GridY = P.GridY + 1;
        if (P.GridY % 2 == 0)
        {
            Result.GridX = P.GridX;
        }
        else
        {
            Result.GridX = P.GridX + 1;
        }
        
        Result.Top = true;
        Result.TileX = P.TileX + N;
        Result.TileY = P.TileY - N;
    }
    
    // Top right, top tiles
    else
    {
        Result.GridY = P.GridY + 1;
        if (P.GridY % 2 == 0)
        {
            Result.GridX = P.GridX;
        }
        else
        {
            Result.GridX = P.GridX + 1;
        }
        
        Result.Top = false;
        Result.TileX = P.TileX - (N-1);
        Result.TileY = P.TileY - (N-1);
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
    
    /*
    v4 Colors[4] = {
        V4(0.4f, 0.8f, 0.35f, 1.0f),
        V4(0.3f, 0.4f, 0.7f, 1.0f)
    };
*/
    
    World->X0 = -1.0f;
    World->Y0 = -1.0f;
    World->Width  = 2.0f;
    World->Height = 2.0f;
    
    World->Rows = 13;
    World->Cols = 13;
    
    World->RegionSize = 0.5f * World->Height / World->Rows;
    
    for (int Y = 0; Y < World->Rows; Y++)
    {
        for (int X = 0; X < World->Cols; X++)
        {
            entity Region = {.Type = Entity_WorldRegion};
            
            Region.Size = 0.95f * World->RegionSize / 0.866f;
            Region.TilePositionIsValid = true;
            Region.TilePosition = {.GridX = X, .GridY = Y};
            Region.P.XY = TilePositionToRegionPosition(World, Region.TilePosition);
            
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
                Region.Color = GetPlayerColor(Region.Owner);
                Region.P.Z = 0.1f;
            }
            
            AddEntity(World, Region);
        }
    }
    
    //GenerateFoliage(World, &Arena);
    
    //Add structures
    /*
    entity* A = World->Entities + 0x60;
    if (A->Type == Entity_WorldRegion && !IsWater(A))
    {
        int N = 4;
        for (int U = -N; U < N; U++)
        {
            for (int V = -N; V < N; V++)
            {
                bool DoBottom = V == N-1;
                bool DoTop = V == N-1;
                
                if (DoBottom)
                {
                    tile_position P = {.GridX = A->TilePosition.GridX, .GridY = A->TilePosition.GridY, 
                        .TileX = U, .TileY = V, .Top = false};
                    
                    entity Pebble = {
                        .Type = Entity_Foliage,
                        .Size = 0.01f,
                        .P = V3(TilePositionToWorldPosition(World, P), A->P.Z),
                        .Angle = 0,
                        .Owner = A->Owner,
                        .Parent = (i32) 0x60,
                        .FoliageType = Foliage_Rock,
                        .TilePositionIsValid = true,
                        .TilePosition = P
                    };
                    
                    AddEntity(World, Pebble);
                }
                
                if (DoTop)
                {
                    tile_position P = {.GridX = A->TilePosition.GridX, .GridY = A->TilePosition.GridY, 
                        .TileX = U, .TileY = V, .Top = true};
                    
                    entity Pebble = {
                        .Type = Entity_Foliage,
                        .Size = 0.01f,
                        .P = V3(TilePositionToWorldPosition(World, P), A->P.Z),
                        .Angle = 0,
                        .Owner = A->Owner,
                        .Parent = (i32) 0x60,
                        .FoliageType = Foliage_Rock,
                        .TilePositionIsValid = true,
                        .TilePosition = P
                    };
                    
                    AddEntity(World, Pebble);
                }
            }
        }
    }
*/
    
    /*
    dynamic_array<entity*> PathEnds = {.Arena = &Arena};
    
    for (u64 RegionIndex = 0; RegionIndex < World->EntityCount; RegionIndex++)
    {
        entity* Region = World->Entities + RegionIndex;
        if (Region->Type == Entity_WorldRegion &&
            !IsWater(Region))
        {
            tile_position Start = {.GridX = Region->TilePosition.GridX, .GridY = Region->TilePosition.GridY};
            entity* PathEnd = CreatePathForRegion(World, Start, &Arena);
            Append(&PathEnds, PathEnd);
        }
    }
    */
    
    struct bridge
    {
        tile_position P0, P1;
    };
    
    dynamic_array<bridge> Bridges = {.Arena = &Arena};
    span<region_edge> DisallowedEdges = AllocSpan(&Arena, region_edge, 1);
    
    for (int RegionIndex = 0; RegionIndex < World->EntityCount; RegionIndex++)
    {
        entity* Region = World->Entities + RegionIndex;
        tile_position P = Region->TilePosition;
        
        if (Region->Type == Entity_WorldRegion && !IsWater(Region))
        {
            int MaxRegionCount = 5;
            for (int RegionCount = 0; RegionCount < MaxRegionCount; RegionCount++)
            {
                entity* PathEnd = CreatePathForRegion(World, P, &Arena, DisallowedEdges, RegionIndex);
                if (!PathEnd)
                {
                    break;
                }
                
                tile_position End = PathEnd->TilePosition;
                
                if (!TileIsOnEdge(End))
                {
                    break;
                }
                
                DisallowedEdges[0] = GetOppositeEdgeType(GetTileEdgeType(End));
                tile_position NewP = GetOppositeTileFromEdge(End);
                
                u64 EndRegionIndex = GetWorldRegionForTilePosition(World, NewP);
                if (EndRegionIndex == 0 ||
                    Region->Owner != World->Entities[EndRegionIndex].Owner)
                {
                    break;
                }
                
                entity* EndRegion = World->Entities + EndRegionIndex;
                if (IsWater(EndRegion))
                {
                    break;
                }
                
                if (RegionCount != MaxRegionCount - 1)
                {
                    Append(&Bridges, {.P0 = End, .P1 = NewP});
                }
                
                P = NewP;
            }
        }
    }
    
    
    for (bridge Bridge : Bridges)
    {
        v2 P0 = TilePositionToWorldPosition(World, Bridge.P0);
        v2 P1 = TilePositionToWorldPosition(World, Bridge.P1);
        
        entity BridgeEntity = {
            .Type = Entity_Structure,
            .P = V3(0.5f * (P0 + P1), 0.11f),
            .Angle = VectorAngle(P1 - P0),
            .Owner = 0,
            .StructureType = Structure_ModularWood
        };
        
        AddEntity(World, BridgeEntity);
    }
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
        if (Entity->Type == Entity_Foliage && Distance < Result.Distance && Entity->FoliageType != Foliage_Paving)
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
    
    for (int Index = 0; Index < 512;)
    {
        v2 TestP = V2(World->X0 + Random() * World->Width, World->Y0 + Random() * World->Height);
        
        u64 RegionIndex = GetWorldRegionIndex(World, TestP);
        
        if (RegionIndex)
        {
            entity* Region = World->Entities + RegionIndex;
            
            v3 P = V3(TestP, 0);
            foliage_type FoliageType = RandomFoliage(IsWater(Region));
            f32 Size = GetDefaultSizeOf(FoliageType) * RandomBetween(0.5f, 1.5f);
            f32 MinDistanceInsideRegion = 0.5f * Size;
            nearest_foliage NearestFoliage = GetNearestFoliage(World, P);
            
            if ((!IsValid(NearestFoliage) || NearestFoliage.Distance >= Size + NearestFoliage.Foliage->Size) &&
                DistanceInsideRegion(Region, P.XY) >= MinDistanceInsideRegion)
            {
                entity Foliage = {.Type = Entity_Foliage};
                Foliage.FoliageType = FoliageType;
                Foliage.P = P;
                Foliage.Parent = RegionIndex;
                Foliage.Size = Size;
                
                AddEntity(World, Foliage);
                Index++;
            }
        }
    }
}