enum region_edge
{
    RegionEdge_None,
    RegionEdge_Right,
    RegionEdge_BottomRight,
    RegionEdge_BottomLeft,
    RegionEdge_Left,
    RegionEdge_TopLeft,
    RegionEdge_TopRight
};

enum foliage_type : u8
{
    Foliage_Null,
    Foliage_Seaweed,
    Foliage_PinkFlower,
    Foliage_Bush,
    Foliage_RibbonPlant,
    Foliage_Grass,
    Foliage_Rock,
    Foliage_Paving
};

struct local_entity_info
{
    bool IsValid;
    v3 P;
};

enum structure_type : u8
{
    Structure_Null,
    Structure_ModularWood,
    Structure_House
};

//TODO: Why is this an i32?
struct entity_handle
{
    i32 Index;
    
    operator bool()
    {
        return (Index > 0);
    }
};

enum entity_type : u8
{
    Entity_Null,
    Entity_WorldHex,
    Entity_Foliage,
    Entity_Farm,
    Entity_Structure,
    Entity_Fence
};

enum hex_type : u8
{
    Hex_Empty,
    Hex_Settlement,
    Hex_UnitStack
};

struct entity
{
    entity_type Type;
    
    //hex_type HexType;
    v3 P; //Can be overriden locally
    i32 Owner;
    i32 Region; //TODO: Change to u32
    v4 Color;
    i8 Level;
    v2i GridP;
    
    //Wanting to get rid of:
    foliage_type FoliageType;
    structure_type StructureType;
    f32 Angle;
    entity_handle Parent;
    f32 Size;
};

struct world_region
{
    v2 Center;
    int HexCount;
};

struct world
{
    f32 X0, Y0;
    f32 Width, Height;
    
    f32 HexSize;
    
    int Rows, Cols;
    
    entity Entities[2048];
    u32 EntityCount;
    
    world_region Regions[16];
    int RegionCount;
    
    bool Darkness;
    int DarknessPlayer;
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
    Mode_Place,
    Mode_EditTower,
    Mode_TowerPOV,
    Mode_CellUpgrade,
    Mode_BuildFarm,
    Mode_WallUpgrade,
    Mode_Attack,
    Mode_LaunchStrike
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
    u32 HexIndex;
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
    Request_Hello,
    Request_StartGame,
    Request_Reset,
    Request_PlaceTower,
    Request_EndTurn,
    Request_TargetTower,
    Request_Throw,
    Request_UpgradeHex,
    Request_BuildFarm,
    Request_UpgradeWall,
    Request_Attack,
    Request_Darkness,
    Request_LaunchStrike
};

struct player_request
{
    player_request_type Type;
    
    //Type == PlaceTower
    tower_type TowerType;
    u32        TowerHexIndex;
    v2         TowerP;
    
    //Type == AimTower
    u32 TowerIndex;
    v2 TargetP;
    
    //Type == Throw ( also uses TowerIndex )
    v3 P;
    v3 Direction;
    
    //Type == UpgradeRegion, Type == Attack
    entity_handle Hex;
};

enum animation_type : u32
{
    Animation_Null,
    Animation_Projectile,
    Animation_Entity,
};

struct animation
{
    animation_type Type;
    v3 P0;
    v3 P1;
    f32 Duration;
    f32 t; // [0, 1]
    u32 EntityIndex;
};

enum server_message_type
{
    Message_Null,
    Message_Initialise,
    Message_PlayAnimation,
    Message_NewWorld,
    Message_ResetLocalEntityInfo, //Reset local entity info
};

struct server_packet_message
{
    channel Channel;
    
    server_message_type Type;
    
    //Message type = Initialise
    u32 InitialiseClientID;
    
    //Message type = PlayAnimation
    animation Animation;
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
    
    //
    v3 HoveringWorldP;
    entity* HoveringHex;
    entity_handle HoveringHexIndex;
    
    v3 HexOutlineP;
    v3 HexOutlineTargetP;
    v4 HexOutlineColor;
    
    //Placing tower
    v3 TowerPlaceIndicatorP;
    
    global_game_state GlobalState;
    u32 MyClientID;
    
    local_entity_info LocalEntityInfo[ ArrayCount(world::Entities) ];
    
    animation Animations[16];
    u32 AnimationCount;
    
    bool Dragging;
    v2 CursorWorldPos;
    
    f64 Time;
    
    m4x4 CastleTransform;
    m4x4 TurretTransform;
    
    //These are constants
    f32 ApproxTowerZ;
    
    //
    v3 SkyColor;
    v3 LightP;
    v3 LightDirection;
    f32 WaterZ;
    f32 ShadowIntensity;
};

struct defense_assets
{
    render_output_handle WaterReflection;
    render_output_handle WaterRefraction;
    texture_handle WaterDuDv;
    texture_handle WaterNormal;
    texture_handle WaterFlow;
    
    render_output_handle ShadowMap;
    render_output_handle Output1;
    
    render_output_handle BloomMipmaps[8];
    render_output_handle BloomAccum;
    
    texture_handle Button, Panel, Crystal, Target;
    
    model_textures ModelTextures;
    cube_map Skybox;
    
    model_handle WorldHex, WorldHexLowPoly, WorldHexSkirt;
    model_handle PinkFlower, Bush, RibbonPlant, Grass, Rock, Paving;
    model_handle ModularWood, House, House07;
    model_handle Castle, Turret, Mine, Tower, Fence05;
    
    model_handle Settlement1, Settlement2, Settlement3, Settlement4, Settlement5;
    model_handle Boat;
    model_handle Arrow;
    model_handle Question;
    
    vertex_buffer_handle GUIWholeScreen;
    
    vertex_buffer_handle HexOutline;
    
    dynamic_array<vertex_buffer_handle> Region;
    dynamic_array<vertex_buffer_handle> AttackHexes;
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
    Screen_GameOver,
    Screen_Settings
};

struct game_settings
{
    app_screen PreviousScreen;
    
    
};

struct app_state
{
    defense_assets AssetHandles;
    
    app_screen CurrentScreen;
    
    game_state* GameState;
    game_settings GameSettings;
};

struct ray_collision
{
    bool DidHit;
    f32 T;
    v3 P;
    v3 Normal;
};

void InitialiseServerState(global_game_state* Game);

void CreateWaterFlowMap(world* World, game_assets* Assets, memory_arena* Arena);
v3 ScreenToWorld(game_state* Game, v2 ScreenPos, f32 WorldZ = 0.0f);
entity* GetEntity(world* World, entity_handle Entity);
v3 GetEntityP(game_state* Game, entity_handle Entity);

model_handle GetModel(defense_assets* Assets, entity* Entity, bool LowPoly = false);
enum
{
    Model_Normal = 0,
    Model_LowPoly = 1
};

ray_collision WorldCollision(world* World, game_assets* Assets, defense_assets* GameAssets, v3 Ray0, v3 RayDirection);
vertex_buffer_handle CreateHexOutlineMesh(game_assets* Assets);