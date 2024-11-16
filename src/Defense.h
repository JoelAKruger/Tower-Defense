#include <enet/enet.h>

struct world_region
{
    //Defined relative to world
    v2 Center;
    v2 Vertices[6];
    
    f32 Z;
    
    u32 OwnerIndex;
    
    bool IsWater;
    v4 Color;
    
    char Name[64];
    u32 NameLength;
};

struct world
{
    v4 Colors[2];
    
    f32 X0, Y0;
    f32 Width, Height;
    
    int Rows, Cols;
    
    world_region Regions[256];
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
    Mode_Waiting,
    Mode_MyTurn,
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

enum tower_edit_mode
{
    TowerEdit_Null,
    TowerEdit_SetTarget
};

struct global_game_state
{
    u32 PlayerCount;
    u32 PlayerTurnIndex;
    
    world World;
    
    tower Towers[64];
    u32 TowerCount;
};

enum player_request_type
{
    Request_Null,
    Request_Reset,
    Request_PlaceTower,
    Request_EndTurn,
    Request_TargetTower
};

struct player_request
{
    player_request_type Type;
    
    //Type == PlaceTower
    tower_type TowerType;
    u32        TowerRegionIndex;
    v2         TowerP;
    
    //Type == AimTower
    u32 TowerIndex;
    v2 TargetP;
};

enum server_message_type
{
    Message_Null,
    Message_Initialise,
    Message_PlayAnimation,
    Message_NewWorld
};

struct server_message
{
    server_message_type Type;
    
    //Message type = Initialise
    u32 InitialiseClientID;
    
    //Message type = PlayAnimation
    v2 AnimationP;
    f32 AnimationRadius;
};

struct server_message_queue
{
    server_message Messages[32];
    u32 MessageCount;
};

struct platform_multiplayer_context
{
    ENetHost* ServerHost;
    ENetPeer* ServerPeer;
};

struct multiplayer_context
{
    bool Connected;
    platform_multiplayer_context Platform;
    
    server_message_queue MessageQueue;
    u32 MyClientID;
};

struct animation
{
    v2 P;
    f32 Radius;
    f32 Duration;
    f32 t; // [0, 1]
};

struct game_state
{
    console* Console;
    editor Editor;
    
    v3 CameraP;
    f32 CameraTargetZ;
    v3 CameraDirection;
    f32 FOV;
    
    m4x4 WorldTransform;
    
    render_output ShadowMap;
    
    game_mode Mode;
    
    //Valid when mode is Mode_Place
    tower_type PlacementType; 
    
    //Valid when mode is Mode_EditTower
    tower* SelectedTower;     
    u32    SelectedTowerIndex;
    tower_edit_mode TowerEditMode;
    
    global_game_state GlobalState;
    multiplayer_context MultiplayerContext;
    
    animation Animations[16];
    u32 AnimationCount;
    
    u32 MyColorIndex;
    
    bool ShowBackground;
    
    bool Dragging;
    v2 CursorWorldPos;
    
    f64 Time;
    
    m4x4 CastleTransform;
    m4x4 TurretTransform;
    
    //These are constants
    f32 ApproxTowerZ;
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
    tower* SelectedTower;
};

enum app_screen
{
    Screen_MainMenu,
    Screen_Game
};

struct game_assets;

struct app_state
{
    app_screen CurrentScreen;
    
    game_state* GameState;
    game_assets* Assets;
};

void InitialiseServerState(global_game_state* Game);

void CreateWaterFlowMap(world* World, game_assets* Assets, memory_arena* Arena);