enum foliage_type
{
    Foliage_Null,
    Foliage_Seaweed,
    Foliage_PinkFlower,
    Foliage_Bush,
    Foliage_RibbonPlant,
    Foliage_Grass,
    Foliage_Rock,
};

enum entity_type : u32
{
    Entity_Null,
    Entity_WorldRegion,
    Entity_Foliage
};

struct entity
{
    entity_type Type;
    u32 ModelHandle;
    f32 Size;
    v3 P;
    i32 Owner;
    v4 Color;
    foliage_type FoliageType;
};

struct world
{
    f32 X0, Y0;
    f32 Width, Height;
    
    int Rows, Cols;
    
    entity Entities[1024];
    u32 EntityCount;
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
    Mode_EditTower,
    Mode_TowerPOV,
};

enum tower_type
{
    Tower_Null,
    Tower_Castle,
    Tower_Turret,
    Tower_Mine
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

struct player
{
    char Name[64];
    u32 NameLength;
    //u32 ColorIndex
    
    u32 Credits;
    bool Initialised;
};

struct global_game_state
{
    b32 GameStarted;
    
    u32 PlayerCount;
    u32 MaxPlayers;
    u32 PlayerTurnIndex;
    player Players[4];
    
    world World;
    
    tower Towers[64];
    u32 TowerCount;
};

enum player_request_type
{
    Request_Null,
    Request_StartGame,
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

enum channel : u32
{
    Channel_Null,
    Channel_Message,
    Channel_GameState,
    
    Channel_Count
};

enum server_message_type
{
    Message_Null,
    Message_Initialise,
    Message_PlayAnimation,
    Message_NewWorld
};

struct server_packet_message
{
    channel Channel;
    
    server_message_type Type;
    
    //Message type = Initialise
    u32 InitialiseClientID;
    
    //Message type = PlayAnimation
    v2 AnimationP;
    f32 AnimationRadius;
};

struct server_packet_game_state
{
    channel Channel;
    global_game_state ServerGameState;
};

struct server_message_queue
{
    server_packet_message Messages[32];
    u32 MessageCount;
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
    v3 CameraTargetP;
    v3 CameraDirection;
    v3 CameraTargetDirection;
    
    int TopDownCameraZoomLevel;
    v3 TopDownCameraP;
    
    f32 FOV;
    f32 TargetFOV;
    
    m4x4 WorldTransform;
    
    game_mode Mode;
    
    //Valid when mode is Mode_Place
    tower_type PlacementType; 
    
    //Valid when mode is Mode_EditTower
    tower* SelectedTower;     
    u32    SelectedTowerIndex;
    tower_edit_mode TowerEditMode;
    
    //Valid when mode is Mode_TowerPOV
    tower* TowerPerspective;
    
    entity* HoveringRegion;
    u32 HoveringRegionIndex;
    
    //Placing tower
    f32 TowerPlaceIndicatorZ;
    
    global_game_state GlobalState;
    u32 MyClientID;
    
    animation Animations[16];
    u32 AnimationCount;
    
    bool Dragging;
    v2 CursorWorldPos;
    
    f64 Time;
    
    m4x4 CastleTransform;
    m4x4 TurretTransform;
    
    //These are constants
    f32 ApproxTowerZ;
    v4 Colors[4];
    
    v3 SkyColor;
    v3 LightP;
    v3 LightDirection;
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

struct game_assets;

enum app_screen
{
    Screen_MainMenu,
    Screen_Game,
    Screen_GameOver
};


struct app_state
{
    app_screen CurrentScreen;
    
    game_state* GameState;
    game_assets* Assets;
};

void InitialiseServerState(global_game_state* Game);

void CreateWaterFlowMap(world* World, game_assets* Assets, memory_arena* Arena);
v3 ScreenToWorld(game_state* Game, v2 ScreenPos, f32 WorldZ = 0.0f);