struct world_region
{
    //Defined relative to world
    v2 Center;
    v2 Vertices[63];
    u32 VertexCount;
    
    u32 ColorIndex;
    
    char Name[128];
    u32 NameLength;
};

struct world
{
    v4 Colors[2];
    world_region Regions[32];
    u32 RegionCount;
};

struct editor
{
    bool DraggingVertex;
    u32 SelectedRegionIndex;
    u32 SelectedVertexIndex;
};

struct console;

enum game_mode
{
    Mode_Normal,
    Mode_Edit,
    Mode_Place
};

struct tower
{
    v2 P;
    u32 RegionIndex;
};

struct model_vertex
{
    v3 Position;
    v3 Normal;
};

struct game_state
{
    console* Console;
    
    v3 CameraP;
    v3 CameraDirection;
    f32 FOV;
    
    m4x4 WorldTransform;
    
    world World;
    
    game_mode Mode;
    editor Editor;
    
    tower Towers[64];
    u32 TowerCount;
    
    bool ShowBackground;
    
    bool Dragging;
    v2 CursorWorldPos;
    
    f64 Time;
    
    span<model_vertex> CubeVertices;
};

struct map_file_header
{
    u32 RegionCount;
};

struct map_file_region
{
    v2 Center;
    v2 Vertices[63];
    u32 VertexCount;
};
