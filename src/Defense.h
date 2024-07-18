struct world_region
{
    //Defined relative to world
    union
    {
        struct
        {
            v2 Center;
            v2 Vertices[63];
        };
        v2 Positions[64];
    };
    
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
    Mode_Null,
    Mode_MyTurn,
    Mode_GamePlay,
    Mode_Edit,
    Mode_Place,
    Mode_EditTower
};

enum tower_type
{
    Tower_Null,
    Tower_Castle,
    Tower_Turret
};

struct tower
{
    tower_type Type;
    
    v2 P;
    u32 RegionIndex;
    f32 Health;
    
    v2 Target;
    f32 Rotation;
};

struct model_vertex
{
    v3 Position;
    v3 Normal;
};

enum tower_edit_mode
{
    TowerEdit_Null,
    TowerEdit_SetTarget
};

struct global_game_state
{
    u32 PlayerTurnIndex;
    
    world World;
    
    tower Towers[64];
    u32 TowerCount;
};

enum player_request_type
{
    Request_Null,
    Request_PlaceTower
};

struct player_request
{
    player_request_type Type;
    
    //Type == PlaceTower
    tower_type TowerType;
    u32        TowerRegionIndex;
    v2         TowerP;
};

struct game_state
{
    console* Console;
    editor Editor;
    
    v3 CameraP;
    v3 CameraDirection;
    f32 FOV;
    
    m4x4 WorldTransform;
    
    world World;
    
    game_mode Mode;
    
    //Valid when mode is Mode_Place
    tower_type PlacementType; 
    
    //Valid when mode is Mode_EditTower
    tower* SelectedTower;     
    tower_edit_mode TowerEditMode;
    
    global_game_state GlobalState;
    
    u32 MyColorIndex;
    
    tower Towers[64];
    u32 TowerCount;
    
    bool ShowBackground;
    
    bool Dragging;
    v2 CursorWorldPos;
    
    f64 Time;
    
    //TODO: Do not draw models in immediate mode
    span<model_vertex> CubeVertices;
    span<model_vertex> TurretVertices;
    
    m4x4 CastleTransform;
    m4x4 TurretTransform;
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

struct render_context
{
    memory_arena* Arena;
    world_region* HoveringRegion;
};