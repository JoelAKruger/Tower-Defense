
static v2
ScreenToWorld(game_state* Game, v2 ScreenPos)
{
    //TODO: Cache inverse matrix?
    
    v4 P = V4(ScreenPos, 1.0f, 1.0f) * Inverse(Game->WorldTransform);
    
    v3 P0 = Game->CameraP;
    v3 P1 = {P.X / P.W, P.Y / P.W, P.Z / P.W};
    
    f32 Z = P0.Z;
    f32 DeltaZ = P0.Z - P1.Z;
    
    f32 T = Z / DeltaZ;
    
    v3 ResultXYZ = P0 + T * (P1 - P0);
    
    return V2(ResultXYZ.X, ResultXYZ.Y);
}

static v2
WorldToScreen(game_state* GameState, v3 WorldPos)
{
    v4 P = V4(WorldPos, 1.0f);
    v4 ClipSpace = P * GameState->WorldTransform;
    v2 Result = {ClipSpace.X, ClipSpace.Y};
    Result = Result* (1.0f / ClipSpace.W);
    return Result;
}

static v2
WorldToScreen(game_state* GameState, v2 WorldPos)
{
    v2 Result = WorldToScreen(GameState, V3(WorldPos, 0.0f));
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


static void
SaveWorld(world* World, char* Path, memory_arena* Arena)
{
    Assert(Arena->Type == TRANSIENT);
    
    u32 Bytes = sizeof(map_file_header) + World->RegionCount * sizeof(map_file_region);
    u8* Memory = Alloc(Arena, Bytes);
    u8* At = Memory;
    
    map_file_header* Header = (map_file_header*) At;
    Header->RegionCount = World->RegionCount;
    At += sizeof(map_file_header);
    
    map_file_region* Regions = (map_file_region*) At;
    
    for (u32 RegionIndex = 0; RegionIndex < World->RegionCount; RegionIndex++)
    {
        Regions[RegionIndex].Center = World->Regions[RegionIndex].Center;
        Assert(sizeof(Regions[RegionIndex].Vertices) == sizeof(World->Regions[RegionIndex].Vertices));
        memcpy(Regions[RegionIndex].Vertices, World->Regions[RegionIndex].Vertices, sizeof(Regions[RegionIndex].Vertices));
        Regions[RegionIndex].VertexCount = World->Regions[RegionIndex].VertexCount;
    }
    
    span<u8> Data = {Memory, Bytes};
    PlatformSaveFile(Path, Data);
}

static void
LoadWorld(world* World, char* Path, memory_arena* Arena)
{
    Assert(Arena->Type == TRANSIENT);
    
    span<u8> FileContents = PlatformLoadFile(Arena, Path);
    
    u8* Memory = FileContents.Memory;
    u32 Bytes = FileContents.Count;
    
    map_file_header* Header = (map_file_header*) Memory;
    Memory += sizeof(map_file_header);
    
    Assert(Bytes == sizeof(map_file_header) + Header->RegionCount * sizeof(map_file_region));
    
    map_file_region* Regions = (map_file_region*) Memory;
    
    World->RegionCount = Header->RegionCount;
    
    for (u32 RegionIndex = 0; RegionIndex < Header->RegionCount; RegionIndex++)
    {
        World->Regions[RegionIndex].Center = Regions[RegionIndex].Center;
        Assert(sizeof(Regions[RegionIndex].Vertices) == sizeof(World->Regions[RegionIndex].Vertices));
        memcpy(World->Regions[RegionIndex].Vertices, Regions[RegionIndex].Vertices, sizeof(Regions[RegionIndex].Vertices));
        World->Regions[RegionIndex].VertexCount = Regions[RegionIndex].VertexCount;
    }
}

static game_state* 
GameInitialise(allocator Allocator)
{
    game_state* GameState = AllocStruct(Allocator.Permanent, game_state);
    GameState->Console = AllocStruct(Allocator.Permanent, console);
    
    GameState->Mode = Mode_Waiting;
    
    GameState->CameraP = {0.0f, -0.25, 0.0f};
    GameState->CameraTargetZ = -1.25f;
    GameState->CameraDirection = {0.0f, 1.0f, 5.5f};
    GameState->FOV = 50.0f;
    
    GameState->ShadowMap = CreateShadowDepthTexture(8192, 8192);
    
    GameState->CubeVertices   = LoadModel(Allocator, "assets/models/castle.obj", false);
    GameState->TurretVertices = LoadModel(Allocator, "assets/models/turret.obj", true);
    
    GameState->CastleTransform = ModelRotateTransform() * ScaleTransform(0.03f, 0.03f, 0.03f);
    GameState->TurretTransform = ModelRotateTransform() * ScaleTransform(0.06f, 0.06f, 0.06f);
    
    GameState->ApproxTowerZ = -0.06f;
    
    return GameState;
}

static bool
InRegion(world_region* Region, v2 WorldPos, game_input* Input)
{
    if (Region->VertexCount < 3)
    {
        return false;
    }
    
    v2 A0 = WorldPos;
    v2 A1 = Region->Center;
    
    u32 IntersectCount = 0;
    
    for (u32 TestIndex = 0; TestIndex < Region->VertexCount; TestIndex++)
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

static void
RunEditor(game_state* Game, world* World, game_input* Input, memory_arena* Arena)
{
    Assert(Arena->Type == TRANSIENT);
    
    DrawRectangle(V2(0.6f, 0.3f), V2(0.4f, 1.0f), V4(0.0f, 0.0f, 0.0f, 0.5f));
    Game->Dragging = false;
    
    gui_layout Layout = DefaultLayout(0.6f, 1.0f);
    
    SetShader(FontShader);
    Layout.Label("Regions");
    
    if (Layout.Button("Add"))
    {
        Game->Editor.SelectedRegionIndex = (World->RegionCount++);
        Assert(World->RegionCount < ArrayCount(World->Regions));
    }
    
    Layout.NextRow();
    
    Layout.Label(ArenaPrint(Arena, "Region: %u", Game->Editor.SelectedRegionIndex));
    Layout.NextRow();
    Layout.Label(ArenaPrint(Arena, "Position: %u", Game->Editor.SelectedVertexIndex));
    Layout.NextRow();
    
    Layout.Label("Vertices");
    
    if (Layout.Button("Add"))
    {
        world_region* Region = World->Regions + Game->Editor.SelectedRegionIndex;
        
        v2 NewVertex = {};
        
        //New vertex is average of first and last vertex
        if (Region->VertexCount >= 2)
        { 
            NewVertex = 0.5f * (GetVertex(Region, -1) + GetVertex(Region, 0));
        }
        
        if (Region->VertexCount + 1 < ArrayCount(Region->Vertices))
        {
            Region->Vertices[Region->VertexCount++] = NewVertex;
        }
    }
    
    if (Layout.Button("Remove"))
    {
        world_region* Region = World->Regions + Game->Editor.SelectedRegionIndex;
        
        if (Region->VertexCount > 0)
        {
            Region->VertexCount--;
        }
    }
    
    f32 VertexDisplaySize = 0.01f;
    
    SetShader(ColorShader);
    for (u32 RegionIndex = 0; RegionIndex < World->RegionCount; RegionIndex++)
    {
        world_region* Region = World->Regions + RegionIndex;
        
        for (u32 PositionIndex = 0; PositionIndex < Region->VertexCount + 1; PositionIndex++)
        {
            v2 Vertex = Region->Positions[PositionIndex];
            v2 VertexScreen = WorldToScreen(Game, Vertex);
            
            v2 SquareSize = V2(VertexDisplaySize, VertexDisplaySize);
            
            v4 Color = V4(1.0f, 0.0f, 0.0f, 1.0f);
            
            DrawRectangle(VertexScreen - 0.5f * SquareSize, SquareSize, Color);
        }
    }
    
    if ((Input->Button & Button_LMouse) == 0)
    {
        Game->Editor.DraggingVertex = false;
    }
    
    if ((Input->ButtonDown & Button_LMouse) && !GUIInputIsBeingHandled())
    {
        for (u32 RegionIndex = 0; RegionIndex < World->RegionCount; RegionIndex++)
        {
            world_region* Region = World->Regions + RegionIndex;
            
            for (u32 PositionIndex = 0; PositionIndex < Region->VertexCount + 1; PositionIndex++)
            {
                v2 Vertex = Region->Positions[PositionIndex];
                v2 VertexScreen = WorldToScreen(Game, Vertex);
                
                if (Abs(VertexScreen.X - Input->Cursor.X) < 0.5f * VertexDisplaySize &&
                    Abs(VertexScreen.Y - Input->Cursor.Y) < 0.5f * VertexDisplaySize)
                {
                    Game->Editor.DraggingVertex = true;
                    Game->Editor.SelectedRegionIndex = RegionIndex;
                    Game->Editor.SelectedVertexIndex = PositionIndex;
                }
            }
        }
    }
    
    if (Game->Editor.DraggingVertex)
    {
        world_region* Region = World->Regions + Game->Editor.SelectedRegionIndex;
        Region->Positions[Game->Editor.SelectedVertexIndex] = ScreenToWorld(Game, Input->Cursor);
    }
}

static void
DrawExplosion(v2 P, f32 Z, f32 ExplosionRadius, u32 Frame)
{
    u32 FrameCount = 22;
    Frame = Frame % FrameCount;
    
    u32 TexturesAcross = 6;
    u32 TexturesHigh = 4;
    
    u32 I = Frame % TexturesAcross;
    u32 J = Frame / TexturesAcross;
    
    v2 ExplosionTextureSize = ExplosionRadius * V2(16.0f / 9.0f, 1.0f); //This matches the texture
    
    SetTexture(ExplosionTexture);
    
    v2 P0 = P - 0.5f * ExplosionTextureSize;
    v2 P1 = P + 0.5f * ExplosionTextureSize;
    
    v2 UV0 = {(f32)I / TexturesAcross, (f32)(TexturesHigh - 1 - J) / TexturesHigh};
    v2 UV1 = UV0 + V2(1.0f / TexturesAcross, 1.0f / TexturesHigh);
    
    DrawTexture(V3(P0, Z), V3(P1, Z), UV0, UV1);
}

static void 
RunTowerEditor(game_state* Game, u32 TowerIndex, game_input* Input, memory_arena* Arena)
{
    tower* Tower = Game->GlobalState.Towers + TowerIndex;
    
    //Draw target
    v2 TargetSize = {0.075f, 0.075f};
    f32 TargetZ = Game->ApproxTowerZ;
    
    if (Tower->Type == Tower_Turret)
    {
        SetShader(TextureShader);
        SetTexture(TargetTexture);
        SetTransform(Game->WorldTransform);
        DrawTexture(V3(Tower->Target - 0.5f * TargetSize, TargetZ), V3(Tower->Target + 0.5f * TargetSize, TargetZ));
    }
    
    //Draw new target
    if (Game->TowerEditMode == TowerEdit_SetTarget)
    {
        v2 CursorP = ScreenToWorld(Game, Input->Cursor);
        
        SetShader(TextureShader);
        SetTexture(TargetTexture);
        SetTransform(Game->WorldTransform);
        DrawTexture(V3(CursorP - 0.5f * TargetSize, TargetZ), V3(CursorP + 0.5f * TargetSize, TargetZ));
        
        if (Input->ButtonUp & Button_LMouse)
        {
            player_request Request = {Request_TargetTower};
            Request.TowerIndex = TowerIndex;
            Request.TargetP = CursorP;
            SendPacket(&Game->MultiplayerContext, &Request);
        }
        
        SetTransform(IdentityTransform());
    }
    
    SetTransform(IdentityTransform());
    SetShader(ColorShader);
    DrawRectangle(V2(0.6f, 0.3f), V2(0.4f, 1.0f), V4(0.0f, 0.0f, 0.0f, 0.5f));
    
    gui_layout Layout = DefaultLayout(0.65f, 0.95f);
    
    Layout.NextRow();
    
    if ((Tower->Type == Tower_Turret) && Layout.Button("Set Target"))
    {
        Game->TowerEditMode = TowerEdit_SetTarget;
    }
    
    
    SetShader(FontShader);
    Layout.Label("Tower Editor");
}

static void
SetMode(game_state* GameState, game_mode NewMode)
{
    GameState->Mode = NewMode;
    
    //Reset mode-specific variables
    GameState->PlacementType = {};
    GameState->SelectedTower = {};
    GameState->SelectedTowerIndex = {};
    GameState->TowerEditMode = {};
}

static void
BeginAnimation(game_state* Game, v2 P, f32 Radius)
{
    if (Game->AnimationCount < ArrayCount(Game->Animations))
    {
        animation* Animation = Game->Animations + (Game->AnimationCount++);
        Animation->P = P;
        Animation->Radius = Radius;
        Animation->Duration = 0.6f;
        Animation->t = 0.0f;
    }
    else
    {
        LOG("Animation could not be started\n");
    }
}

static void
TickAnimations(game_state* Game, f32 DeltaTime)
{
    SetDepthTest(false);
    SetShader(TextureShader);
    
    for (u32 AnimationIndex = 0; AnimationIndex < Game->AnimationCount; AnimationIndex++)
    {
        animation* Animation = Game->Animations + AnimationIndex;
        
        u32 FrameCount = 22;
        u32 Frame = (u32)(Animation->t * FrameCount);
        
        DrawExplosion(Animation->P, Game->ApproxTowerZ, Animation->Radius, Frame);
        Animation->t += (DeltaTime / Animation->Duration);
        
        if (Animation->t >= 1.0f)
        {
            //Delete animation (move last to current)
            *Animation = Game->Animations[Game->AnimationCount - 1];
            Game->AnimationCount--;
            AnimationIndex--;
        }
    }
}

static void
HandleServerMessage(server_message* Message, game_state* GameState)
{
    switch (Message->Type)
    {
        case Message_Initialise:
        {
            GameState->MultiplayerContext.MyClientID = Message->InitialiseClientID;
        } break;
        case Message_PlayAnimation:
        {
            BeginAnimation(GameState, Message->AnimationP, Message->AnimationRadius);
        } break;
        default:
        {
            Assert(0);
        }
    }
}


static void 
GameUpdateAndRender(game_state* GameState, game_assets* Assets, f32 SecondsPerFrame, game_input* Input, allocator Allocator)
{
    UpdateConsole(GameState, GameState->Console, Input, Allocator.Transient, SecondsPerFrame);
    
    GameState->Time += SecondsPerFrame;
    
    CheckForServerUpdate(&GameState->GlobalState, &GameState->MultiplayerContext);
    
    for (u32 MessageIndex = 0; MessageIndex < GameState->MultiplayerContext.MessageQueue.MessageCount; MessageIndex++)
    {
        HandleServerMessage(GameState->MultiplayerContext.MessageQueue.Messages + MessageIndex, GameState);
    }
    GameState->MultiplayerContext.MessageQueue.MessageCount = 0;
    
    //Update modes based on server state
    if ((GameState->GlobalState.PlayerTurnIndex == GameState->MultiplayerContext.MyClientID) &&
        (GameState->Mode == Mode_Waiting))
    {
        SetMode(GameState, Mode_MyTurn);
    }
    
    if ((GameState->GlobalState.PlayerTurnIndex != GameState->MultiplayerContext.MyClientID) &&
        (GameState->Mode != Mode_Waiting))
    {
        SetMode(GameState, Mode_Waiting);
    }
    
    if (Input->ButtonDown & Button_Interact)
    {
        if (GameState->Mode == Mode_Edit)
        {
            SetMode(GameState, Mode_MyTurn);
        }
        else if (GameState->Mode == Mode_MyTurn)
        {
            SetMode(GameState, Mode_Edit);
        }
    }
    
    if (Input->ButtonDown & Button_Escape)
    {
        SetMode(GameState, Mode_MyTurn);
    }
    
    if (GameState->Mode == Mode_MyTurn || GameState->Mode == Mode_Waiting)
    {
        //Handle moving the camera
        if ((Input->Button & Button_LMouse) == 0)
        {
            GameState->Dragging = false;
        }
        
        if ((Input->ButtonDown & Button_LMouse) && !GUIInputIsBeingHandled())
        {
            GameState->CursorWorldPos = ScreenToWorld(GameState, Input->Cursor);
            GameState->Dragging = true;
        }
        
        //v2 ScreenMiddle = V2(0.5f, 0.5f * ScreenTop);
        //v2 ScreenMiddleWorldPos = ScreenToWorld(GameState, ScreenMiddle);
        
        if (GameState->Dragging)
        {
            v2 CurrentCursorWorldPos = ScreenToWorld(GameState, Input->Cursor);
            v2 DeltaP = CurrentCursorWorldPos - GameState->CursorWorldPos;
            GameState->CameraP.X -= DeltaP.X;
            GameState->CameraP.Y -= DeltaP.Y;
        }
    }
    
    GameState->CameraP.X += SecondsPerFrame * Input->Movement.X;
    GameState->CameraP.Y += SecondsPerFrame * Input->Movement.Y;
    
    GameState->CameraTargetZ = Clamp(GameState->CameraTargetZ + 0.25f * Input->ScrollDelta, -1.25f, -0.25f);
    
    f32 CameraSpeed = 15.0f;
    GameState->CameraP.Z = LinearInterpolate(GameState->CameraP.Z, GameState->CameraTargetZ, CameraSpeed * SecondsPerFrame);
    
    v3 LookAt = GameState->CameraP + GameState->CameraDirection;
    GameState->WorldTransform = ViewTransform(GameState->CameraP, LookAt) * PerspectiveTransform(GameState->FOV, 0.01f, 10.0f);
    SetTransform(GameState->WorldTransform);
    
    //Get hovered region
    v2 CursorWorldPos = ScreenToWorld(GameState, Input->Cursor);
    u32 HoveringRegionIndex = 0;
    world_region* HoveringRegion = 0;
    for (u32 RegionIndex = 0; RegionIndex < GameState->GlobalState.World.RegionCount; RegionIndex++)
    {
        world_region* Region = GameState->GlobalState.World.Regions + RegionIndex;
        bool Hovering = InRegion(Region, CursorWorldPos, Input);
        if (Hovering)
        {
            HoveringRegion = Region;
            HoveringRegionIndex = RegionIndex;
            break;
        }
    }
    
    render_context RenderContext = {};
    RenderContext.Arena = Allocator.Transient;
    RenderContext.HoveringRegion = HoveringRegion;
    RenderContext.SelectedTower = GameState->SelectedTower;
    
    RenderWorld(GameState, Assets, &RenderContext);
    
    //Draw animations
    TickAnimations(GameState, SecondsPerFrame);
    
    f32 TowerRadius = 0.03f;
    //Select tower
    if (GameState->Mode == Mode_MyTurn)
    {
        if (Input->ButtonDown & Button_LMouse)
        {
            nearest_tower NearestTower = NearestTowerTo(CursorWorldPos, &GameState->GlobalState, HoveringRegionIndex);
            if (NearestTower.Distance < TowerRadius)
            {
                SetMode(GameState, Mode_EditTower);
                GameState->SelectedTower = NearestTower.Tower;
                GameState->SelectedTowerIndex = NearestTower.Index;
            }
        }
    }
    
    SetDepthTest(true);
    SetShader(ModelShader);
    
    //Draw new tower
    if (GameState->Mode == Mode_Place)
    {
        v2 P = CursorWorldPos;
        
        bool Placeable = (HoveringRegion &&
                          !HoveringRegion->IsWater &&
                          DistanceInsideRegion(HoveringRegion, P) > TowerRadius &&
                          NearestTowerTo(P, &GameState->GlobalState, HoveringRegionIndex).Distance > 2.0f * TowerRadius);
        
        LOG("Placeable: %u\n", Placeable);
        LOG("Hovering region index: %u\n", HoveringRegionIndex);
        
        v4 Color = V4(1.0f, 0.0f, 0.0f, 1.0f);
        
        if (Placeable)
        {
            v4 RegionColor = HoveringRegion->Color;
            
            f32 t = 0.5f + 0.25f * sinf(6.0f * (f32)GameState->Time);
            Color = t * RegionColor + (1.0f - t) * V4(1.0f, 1.0f, 1.0f, 1.0f);
        }
        
        Color = V4(0.7f * Color.RGB, 1.0f);
        
        tower_type Type = GameState->PlacementType;
        
        //Draw slightly above a normal tower to prevent z-fighting
        //DrawTower(GameState, Type, V3(P, -0.001f), Color);
        
        if (Placeable && (Input->ButtonDown & Button_LMouse) && !GUIInputIsBeingHandled())
        {
            player_request Request = {Request_PlaceTower};
            
            Request.TowerP = P;
            Request.TowerRegionIndex = HoveringRegionIndex;
            Request.TowerType = Type;
            
            SendPacket(&GameState->MultiplayerContext, &Request);
        }
    }
    
    SetDepthTest(false);
    SetTransform(IdentityTransform());
    
    f32 WorldInfoZThreshold = -0.51f;
    
    //Draw health bars
    if (GameState->CameraP.Z > WorldInfoZThreshold)
    {
        SetShader(ColorShader);
        for (u32 TowerIndex = 0; TowerIndex < GameState->GlobalState.TowerCount; TowerIndex++)
        {
            tower* Tower = GameState->GlobalState.Towers + TowerIndex;
            v3 DrawP = V3(Tower->P, 0.0f) + V3(0.0f, 0.0f, -0.07f);
            v2 ScreenP = WorldToScreen(GameState, DrawP);
            
            f32 HealthBarEdge = 0.02f;
            v2 HealthBarSize = {0.4f / GlobalAspectRatio, 0.08f};
            v2 HealthBarInnerSize = HealthBarSize - V2(HealthBarEdge / GlobalAspectRatio, HealthBarEdge);
            
            v2 HealthBarInner = V2(HealthBarInnerSize.X * Tower->Health, HealthBarInnerSize.Y);
            
            DrawRectangle(ScreenP - 0.5f * HealthBarSize, HealthBarSize, V4(0.5f, 0.5f, 0.5f, 1.0f));
            DrawRectangle(ScreenP - 0.5f * HealthBarInnerSize, HealthBarInner, V4(1.0f, 0.0f, 0.0f, 1.0f));
        }
    }
    
    //Draw region names
    SetShader(FontShader);
    if (GameState->CameraP.Z > WorldInfoZThreshold)
    {
        for (u32 RegionIndex = 0; RegionIndex < GameState->GlobalState.World.RegionCount; RegionIndex++)
        {
            world_region* Region = GameState->GlobalState.World.Regions + RegionIndex;
            
            if (!Region->IsWater)
            {
                f32 TextSize = 0.1f;
                string Name = GetName(Region);
                
                f32 NameWidth = GUIStringWidth(Name, TextSize);
                v2 P = WorldToScreen(GameState, Region->Center);
                P.X -= 0.5f * NameWidth;
                P.Y -= 0.25f * TextSize;
                
                DrawGUIString(Name, P, V4(1.0f, 1.0f, 1.0f, 1.0f), TextSize);
            }
        }
        
    }
    
    BeginGUI(Input);
    //Draw GUI
    if (GameState->Mode == Mode_MyTurn)
    {
        if (Button(V2(-0.95f, -0.8f), V2(0.5f / GlobalAspectRatio, 0.2f), String("Castle")))
        {
            SetMode(GameState, Mode_Place);
            GameState->PlacementType = Tower_Castle;
        }
        
        if (Button(V2(-0.95f + (0.6f / GlobalAspectRatio), -0.8f), V2(0.5f / GlobalAspectRatio, 0.2f), String("Turret")))
        {
            SetMode(GameState, Mode_Place);
            GameState->PlacementType = Tower_Turret;
        }
        
        if (Button(V2(-0.95f + (1.2f / GlobalAspectRatio), -0.8f), V2(0.5f / GlobalAspectRatio, 0.2f), String("End Turn")))
        {
            player_request Request = {Request_EndTurn};
            SendPacket(&GameState->MultiplayerContext, &Request);
        }
    }
    
    if (GameState->Mode == Mode_Edit)
    {
        //Use actual world, not local world
        RunEditor(GameState, &ServerState_->World, Input, Allocator.Transient);
    }
    
    if (GameState->Mode == Mode_EditTower)
    {
        RunTowerEditor(GameState, GameState->SelectedTowerIndex, Input, Allocator.Transient);
    }
    
    string PosString = ArenaPrint(Allocator.Transient, "X: %f, Y: %f, Z: %f", 
                                  GameState->CameraP.X, GameState->CameraP.Y, GameState->CameraP.Z);
    SetShader(FontShader);
    DrawGUIString(PosString, V2(-0.95f, -0.95f));
    
    DrawGUIString(String(GameState->MultiplayerContext.Connected ? "Connected" : "Not connected"), V2(-0.95f, 0.0f));
    DrawGUIString(ArenaPrint(Allocator.Transient, "My client ID: %u", GameState->MultiplayerContext.MyClientID), V2(-0.95f, -0.05f));
    DrawGUIString(ArenaPrint(Allocator.Transient, "Current Turn: %u", GameState->GlobalState.PlayerTurnIndex), V2(-0.95f, -0.10f));
    
    DrawConsole(GameState->Console, Allocator.Transient);
}

void Command_p(int, string*, console* Console, game_state* GameState, memory_arena* Arena)
{
    string String = ArenaPrint(Arena, "X: %f, Y: %f, Z: %f", GameState->CameraP.X, GameState->CameraP.Y, GameState->CameraP.Z);
    AddLine(Console, String);
}

void Command_background(int ArgCount, string* Args, console* Console, game_state* GameState, memory_arena* Arena)
{
    if (ArgCount == 2)
    {
        if (StringsAreEqual(Args[1], String("off")))
        {
            GameState->ShowBackground = false;
        }
        else if (StringsAreEqual(Args[1], String("on")))
        {
            GameState->ShowBackground = true;
        }
    }
}

void Command_name(int ArgCount, string* Args, console* Console, game_state* GameState, memory_arena* Arena)
{
    if (ArgCount == 3)
    {
        u32 RegionIndex = StringToU32(Args[1]);
        string Name = Args[2];
        
        world_region* Region = ServerState_->World.Regions + RegionIndex;
        SetName(Region, Name);
    }
}

void Command_reset(int ArgCount, string* Args, console* Console, game_state* GameState, memory_arena* Arena)
{
    player_request Request = {Request_Reset};
    SendPacket(&GameState->MultiplayerContext, &Request);
}

void Command_color(int ArgCount, string* Args, console* Console, game_state* Game, memory_arena* Arena)
{
    u32 ColorCount = ArrayCount(ServerState_->World.Colors);
    
    for (u32 RegionIndex = 0; RegionIndex < ServerState_->World.RegionCount; RegionIndex++)
    {
        //ServerState_->World.Regions[RegionIndex].ColorIndex = rand() % ColorCount;
    }
}

void Command_create_server(int ArgCount, string* Args, console* Console, game_state* Game, memory_arena* Arena)
{
    Host();
}

void Command_connect(int ArgCount, string* Args, console* Console, game_state* Game, memory_arena* Arena)
{
    ConnectToServer(&Game->MultiplayerContext, "localhost");
}

void Command_new_world(int ArgCount, string* Args, console* Console, game_state* Game, memory_arena* Arena)
{
}