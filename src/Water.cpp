/* 
       File:     Water.cpp
Date:     4/11/2024
Revision: 1.0
Creator:  Joel Kruger
*/

static bool
PointOnLand(world* World, v2 P)
{
    bool Result = false;
    for (u64 RegionIndex = 1; RegionIndex < World->RegionCount; RegionIndex++)
    {
        world_region* Region = World->Regions + RegionIndex;
        if (!Region->IsWater && InRegion(Region, P))
        {
            Result = true;
            break;
        }
    }
    return Result;
}

static v2
GetWaterVector(array_2d<v2> VectorField, int X, int Y, world* World)
{
    //Boundary (and non-boundary) conditions
    v2 BoundaryDirection = UnitV(V2(1.0f, 1.0f));
    
    if (X < 0 || X >= VectorField.Cols || Y < 0 || Y >= VectorField.Rows)
    {
        return BoundaryDirection;
    }
    
    v2 dP = {World->Width / VectorField.Cols, World->Height / VectorField.Rows};
    v2 P0 = V2(World->X0, World->Y0) + 0.5f * dP;
    v2 P = {P0.X + dP.X * X, P0.Y + dP.Y * Y};
    if (PointOnLand(World, P))
    {
        return {};
    }
    
    return VectorField.Get(X, Y);
}

static void
RunWaterIteration(array_2d<v2> Dest, array_2d<v2> Source, world* World)
{
    Assert(Dest.Cols == Source.Cols);
    Assert(Dest.Rows == Source.Rows);
    
    u64 Rows = Source.Rows;
    u64 Cols = Source.Cols;
    
    for (u64 Y = 0; Y < Rows; Y++)
    {
        for (u64 X = 0; X < Cols; X++)
        {
            v2 Flow = 0.25f * (GetWaterVector(Source, X - 1, Y, World) +
                               GetWaterVector(Source, X + 1, Y, World) + 
                               GetWaterVector(Source, X, Y - 1, World) + 
                               GetWaterVector(Source, X, Y + 1, World));
            
            Dest.Set(X, Y, Flow);
        }
    }
}

static array_2d<v2>
CreateWaterVectorField(memory_arena* Arena, world* World)
{
    u32 GridRows = 16;
    u32 GridCols = 16;
    
    array_2d<v2> VectorFieldA = AllocArray2D<v2>(Arena, GridRows, GridCols);
    array_2d<v2> VectorFieldB = AllocArray2D<v2>(Arena, GridRows, GridCols);
    
    v2 DefaultValue = UnitV(V2(1.0f, 1.0f));
    for (int Y = 0; Y < GridRows; Y++)
    {
        for (int X = 0; X < GridCols; X++)
        {
            VectorFieldA.Set(X, Y, DefaultValue);
        }
    }
    
    for (int Iter = 0; Iter < 4; Iter++)
    {
        array_2d<v2> Source, Dest;
        if (Iter % 2 == 0)
        {
            Source = VectorFieldA;
            Dest = VectorFieldB;
        }
        else
        {
            Source = VectorFieldB;
            Dest = VectorFieldA;
        }
        RunWaterIteration(Dest, Source, World);
    }
    
    return VectorFieldA;
}

static void
CreateWaterFlowMap(world* World, game_assets* Assets, memory_arena* Arena)
{
    u64 Start = ReadCPUTimer();
    
    array_2d<v2> VectorField = CreateWaterVectorField(Arena, World);
    
    array_2d<u32> TextureData = AllocArray2D<u32>(Arena, VectorField.Cols, VectorField.Rows);
    
    for (u64 Y = 0; Y < VectorField.Rows; Y++)
    {
        for (u64 X = 0; X < VectorField.Cols; X++)
        {
            v2 Vector = VectorField.Get(X, Y);
            u8 R = (u8)((Vector.X + 1.0f) * 128);
            u8 G = (u8)((Vector.Y + 1.0f) * 128);
            u32 Texel = R | (G << 8);
            TextureData.Set(X, Y, Texel);
        }
    }
    
    DeleteTexture(&Assets->WaterFlow);
    Assets->WaterFlow = CreateTexture(TextureData.Memory, TextureData.Cols, TextureData.Rows);
    
    u64 Ticks = ReadCPUTimer() - Start;
    f32 Seconds = CPUTimeToSeconds(Ticks, CPUFrequency);
    Log("Took %f seconds\n", Seconds);
}