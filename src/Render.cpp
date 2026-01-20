static void
DrawOceanFloor(render_group* RenderGroup, f32 Z)
{
    PushRectBetter(RenderGroup, V3(-100.0f, -100.0f, Z), V3(100.0f, 100.0f, Z), V3(0, 0, -1));
    PushShader(RenderGroup, Shader_Color);
    PushColor(RenderGroup, V4(0.15f, 0.25f, 0.5f, 1.0f));
    PushNoShadows(RenderGroup);
}

static void
DrawWater(render_group* RenderGroup, f32 WaterZ)
{
    PushRectBetter(RenderGroup, V3(-100.0f, -100.0f, WaterZ), V3(100.0f, 100.0f, WaterZ), V3(0, 0, -1));
    PushShader(RenderGroup, Shader_Water);
}

static void
DrawSkybox(render_group* RenderGroup, defense_assets* Assets)
{
    int D = 80;
    
    //East
    PushTexturedRect(RenderGroup, Assets->Skybox.Textures[2], V3(D, D, -D), V3(D, -D, -D), V3(D, -D, D), V3(D, D, D), V2(0, 1), V2(1, 1), V2(1, 0), V2(0, 0));
    PushColor(RenderGroup, V4(1.05f, 1.05f, 1.05f, 1.0f));
    
    //North
    PushTexturedRect(RenderGroup, Assets->Skybox.Textures[1], V3(-D, D, -D), V3(D, D, -D), V3(D, D, D), V3(-D, D, D), V2(0, 1), V2(1, 1), V2(1, 0), V2(0, 0));
    PushColor(RenderGroup, V4(1.05f, 1.05f, 1.05f, 1.0f));
    
    //South
    PushTexturedRect(RenderGroup, Assets->Skybox.Textures[3], V3(D, -D, -D), V3(-D, -D, -D), V3(-D, -D, D), V3(D, -D, D), V2(0, 1), V2(1, 1), V2(1, 0), V2(0, 0));
    PushColor(RenderGroup, V4(1.05f, 1.05f, 1.05f, 1.0f));
    
    //West
    PushTexturedRect(RenderGroup, Assets->Skybox.Textures[4], V3(-D, -D, -D), V3(-D, D, -D), V3(-D, D, D), V3(-D, -D, D), V2(0, 1), V2(1, 1), V2(1, 0), V2(0, 0));
    PushColor(RenderGroup, V4(1.05f, 1.05f, 1.05f, 1.0f));
    
    //Up
    PushTexturedRect(RenderGroup, Assets->Skybox.Textures[0], V3(-D, D, -D), V3(-D, -D, -D), V3(D, -D, -D),  V3(D, D, -D),  V2(0, 0), V2(0, 1), V2(1, 1), V2(1, 0));
    PushColor(RenderGroup, V4(1.05f, 1.05f, 1.05f, 1.0f));
}

static void
DrawHexOutline(render_group* RenderGroup, defense_assets* GameAssets, game_state* Game, v3 P)
{
    f32 Z = P.Z - 0.001f;
    m4x4 Transform = ScaleTransform(0.9f * Game->GlobalState.World.HexSize) * TranslateTransform(V3(P.XY, Z));
    
    PushVertexBuffer(RenderGroup, GameAssets->HexOutline, Transform);
    PushColor(RenderGroup, Game->HexOutlineColor);
    PushDoesNotCastShadow(RenderGroup);
    PushShader(RenderGroup, Shader_Color);
}

static void
DrawTower(render_group* RenderGroup, game_state* Game, defense_assets* Assets, tower_type Type, v3 P, v4 Color, f32 Angle = 0.0f)
{
    m4x4 ModelTransform = TranslateTransform(P.X, P.Y, P.Z);
    
    renderer_vertex_buffer* VertexBuffer = 0;
    m4x4 Transform;
    
    if (Type == Tower_Castle)
    {
        //span<render_command*> Commands = PushModelNew(RenderGroup, Assets->Castle, ModelTransform);
        span<render_command*> Commands = PushTexturedModel(RenderGroup, Assets->House07, ModelTransform);
        Commands[0]->Color = Color;
    }
    else if (Type == Tower_Turret)
    {
        //span<render_command*> Commands = PushTexturedModel(RenderGroup,Assets->House07, ModelTransform);
        span<render_command*> Commands = PushTexturedModel(RenderGroup, Assets->Tower, ModelTransform);
        Commands[0]->Color = Color;
    }
    else
    {
        Assert(0);
    }
}

static void
DrawTowers(render_group* RenderGroup, game_state* Game, defense_assets* Assets)
{
    for (u32 TowerIndex = 0; TowerIndex < Game->GlobalState.TowerCount; TowerIndex++)
    {
        tower* Tower = Game->GlobalState.Towers + TowerIndex;
        entity* Hex = Game->GlobalState.World.Entities + Tower->HexIndex;
        v3 HexP = GetEntityP(Game, Tower->HexIndex);
        
        v4 HexColor = Hex->Color;
        
        v4 Color = V4(0.7f * HexColor.RGB, HexColor.A);
        if (Game->SelectedTower && TowerIndex == Game->SelectedTowerIndex)
        {
            f32 t = 0.5f + 0.25f * sinf(6.0f * (f32)Game->Time);
            Color = t * HexColor + (1.0f - t) * V4(1.0f, 1.0f, 1.0f, 1.0f);
        }
        
        DrawTower(RenderGroup, Game, Assets, Tower->Type, V3(Tower->P, HexP.Z), Color, Tower->Rotation);
    }
}

static m4x4
MakeLightTransform(game_state* Game, v3 LightP, v3 LightDirection)
{
    m4x4 InvWorldTransform = Inverse(Game->WorldTransform);
    
    m4x4 LightViewTransform = ViewTransform(LightP, LightP + LightDirection);
    
    /*
    f32 MinWorldZ = -0.4f;
    f32 MaxWorldZ = 0.2f;
    
    v3 WorldPositions[8] = {
        ScreenToWorld(Game, V2(-1, -1), MinWorldZ),
        ScreenToWorld(Game, V2(-1, 1), MinWorldZ),
        ScreenToWorld(Game, V2(1, -1), MinWorldZ),
        ScreenToWorld(Game, V2(1, 1), MinWorldZ),
        ScreenToWorld(Game, V2(-1, -1), MaxWorldZ),
        ScreenToWorld(Game, V2(-1, 1), MaxWorldZ),
        ScreenToWorld(Game, V2(1, -1), MaxWorldZ),
        ScreenToWorld(Game, V2(1, 1), MaxWorldZ),
    };

*/
    
    f32 WorldZ = 0.2f;
    f32 X = 1.0f;
    
    v3 WorldPositions[] = {
        ScreenToWorld(Game, V2(-X, -X), WorldZ),
        ScreenToWorld(Game, V2(-X, X), WorldZ),
        ScreenToWorld(Game, V2(X, -X), WorldZ),
        ScreenToWorld(Game, V2(X, X), WorldZ),
    };
    
    f32 Right = FLT_MIN;
    f32 Top = FLT_MIN;
    f32 Far = FLT_MIN;
    f32 Left = FLT_MAX;
    f32 Bottom = FLT_MAX;
    f32 Near = FLT_MAX;
    
    for (u64 PosIndex = 0; PosIndex < ArrayCount(WorldPositions); PosIndex++)
    {
        v3 WorldP = WorldPositions[PosIndex];
        
        v4 LightP_ = V4(WorldP, 1.0f) * LightViewTransform;
        v3 LightP =  (1.0f / LightP_.W) * LightP_.XYZ;
        
        Right  = Max(LightP.X, Right);
        Top    = Max(LightP.Y, Top);
        Far    = Max(LightP.Z, Far);
        Left = Min(LightP.X, Left);
        Bottom = Min(LightP.Y, Bottom);
        Near = Min(LightP.Z, Near);
    }
    
    Left = 0.5f * (Left + Right);
    Bottom = 0.5f * (Bottom + Top);
    
    
    m4x4 LightTransform = ViewTransform(LightP, LightP + LightDirection) * OrthographicTransform(Left, Right, Bottom, Top, Near, Far);
    
    return LightTransform;
}

static void
DrawDirt(render_group* RenderGroup, game_state* Game, u64 HexIndex)
{
#if 0
    v3 P = GetEntityP(Game, HexIndex);
    v4 Color = {}; //V4(0.5f * V3(0.44f, 0.31f, 0.22f), 1.0f); 
    v3 Normal = V3(0.0f, 0.0f, -1.0f);
    f32 Z = P.Z - 0.001f;
    
    u32 VertexDrawCount = 3 * 6;
    vertex* Vertices = AllocArray(RenderGroup->Arena, vertex, VertexDrawCount);
    
    for (int TriangleIndex = 0; TriangleIndex < 6; TriangleIndex++)
    {
        v2 VertexA = GetLocalHexVertex(Game, HexIndex, TriangleIndex);
        v2 VertexB = GetLocalHexVertex(Game, HexIndex, TriangleIndex - 1);
        
        Vertices[TriangleIndex * 3 + 0] = {P, Normal, Color};
        Vertices[TriangleIndex * 3 + 1] = {V3(VertexB, Z), Normal, Color};
        Vertices[TriangleIndex * 3 + 2] = {V3(VertexA, Z), Normal, Color};
    }
    
    PushVertices(RenderGroup, Vertices, VertexDrawCount * sizeof(vertex), sizeof(vertex),
                 D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, Shader_Model);
    PushNoShadow(RenderGroup);
    PushShader(RenderGroup, Shader_PBR);
    PushMaterial(RenderGroup, "TowerDefense", "Dirt");
#endif
}

static span<v2>
GetGrassRandomOffsets()
{
    int Seed = 213;
    
    static bool Initialised = false;
    static v2 Offsets[128];
    
    if (!Initialised)
    {
        random_series Series = BeginRandom(Seed);
        
        for (u64 I = 0; I < ArrayCount(Offsets); I++)
        {
            f32 Distance = 0.7f * sqrtf(Random(&Series));
            f32 Angle = 2 * Pi * Random(&Series);
            Offsets[I] = PolarToRectangular(Distance, Angle);
        }
        
        Initialised = true;
    }
    
    return {.Memory = Offsets, .Count = ArrayCount(Offsets)};
}

//TODO: This need not be computed on each frame
void DrawRegionOutline(render_group* RenderGroup, game_assets* Assets, defense_assets* Handles, game_state* Game)
{
    f32 Opacity = Map(Game->CameraP.Z, -0.2f, -1.6f, 0.0f, 1.0f);
    
    //Draw region outline
    if (Game->HoveringHex && !IsWater(Game->HoveringHex))
    {
        span<span<v2>> Edges = GetRegionEdges(&Game->GlobalState.World, Game->HoveringHex->Region, RenderGroup->Arena);
        
        for (vertex_buffer_handle Handle : Handles->Region)
        {
            FreeVertexBuffer(Assets, Handle);
        }
        Clear(&Handles->Region);
        
        for (span<v2> RegionPart : Edges)
        {
            vertex_buffer_handle VertexBuffer = CreateOutlineMesh(Assets, RegionPart, 0.01f, V4(1.0f, 1.0f, 1.0f, Opacity));
            Append(&Handles->Region, VertexBuffer);
        }
        
        for (vertex_buffer_handle Handle : Handles->Region)
        {
            PushVertexBuffer(RenderGroup, Handle, TranslateTransform(0.0f, 0.0f, 0.1f));
            PushShader(RenderGroup, Shader_Color);
            PushNoShadows(RenderGroup);
        }
    }
    
}

//TODO: This should ideally only take a defense assets
static void RenderWorld(render_group* RenderGroup, game_state* Game, game_assets* Assets, defense_assets* GameAssets)
{
    TimeFunction;
    world* World = &Game->GlobalState.World;
    
    shader_constants Constants = {};
    
    DrawSkybox(RenderGroup, GameAssets);
    DrawOceanFloor(RenderGroup, 0.35f);
    
    //TODO: These should use GetModelTransformOfEntity()
    {
        TimeBlock("Entity Render Loop");
        
        for (u64 EntityIndex = 1; EntityIndex < World->EntityCount; EntityIndex++)
        {
            entity* Entity = World->Entities + EntityIndex;
            switch (Entity->Type)
            {
                case Entity_WorldHex:
                {
                    bool Hidden = true;
                    
                    if (IsWater(Entity) || Entity->Owner == Game->MyClientID)
                    {
                        Hidden = false;
                    }
                    
                    //Optimisation: only check neighbours if we don't know if it's shown
                    if (Hidden)
                    {
                        span<entity*> Neighbours = GetHexNeighbours(World, Entity, RenderGroup->Arena);
                        
                        for (entity* Neighbour : Neighbours)
                        {
                            if (Neighbour->Owner == Game->MyClientID)
                            {
                                Hidden = false;
                                break;
                            }
                        }
                    }
                    
                    v4 Color = Hidden ? V4(1,1,1,1) : Entity->Color;
                    
                    //Draw hex
                    v3 P = GetEntityP(Game, EntityIndex);
                    m4x4 Transform = ScaleTransform(Entity->Size) * TranslateTransform(P);
                    span<render_command*> Commands = PushModelNew(RenderGroup, GameAssets->WorldHex, Transform);
                    Commands[0]->Color = Color;
                    
                    //Draw skirt
                    Transform = ScaleTransform(Entity->Size) * TranslateTransform(P + V3(0.0f, 0.0f, 0.1f));
                    Commands = PushModelNew(RenderGroup, GameAssets->WorldHexSkirt, Transform);
                    Commands[0]->Color = V4(0.15f, 0.25f, 0.5f, 1.0f);
                    
                    //Draw question mark
                    if (Hidden)
                    {
                        Transform = ScaleTransform(0.5f * Entity->Size) * TranslateTransform(P + V3(0.0f, 0.0f, -0.001f));
                        PushModelNew(RenderGroup, GameAssets->Question, Transform);
                        PushNoShadows(RenderGroup);
                    }
                    
                    model_handle Model = {};
                    
                    if (IsWater(Entity) && Entity->Level > 0)
                    {
                        Model = GameAssets->Boat;
                        PushTexturedModel(RenderGroup, Model, ScaleTransform(3.0f * Entity->Size) * TranslateTransform(V3(P.XY, Game->WaterZ)));
                    }
                    else
                    {
                        switch (Entity->Level)
                        {
                            case 1: Model = GameAssets->Settlement1; break;
                            case 2: Model = GameAssets->Settlement2; break;
                            case 3: Model = GameAssets->Settlement3; break;
                            case 4: Model = GameAssets->Settlement4; break;
                            case 5: Model = GameAssets->Settlement5; break;
                        }
                        PushTexturedModel(RenderGroup, Model, ScaleTransform(Entity->Size) * TranslateTransform(P));
                    }
                } break;
                case Entity_Foliage:
                {
                    Assert(Entity->Parent);
                    
                    v3 HexP = GetEntityP(Game, Entity->Parent);
                    
                    m4x4 Transform = ScaleTransform(Entity->Size) * TranslateTransform(Entity->P.X, Entity->P.Y, HexP.Z + Entity->P.Z);
                    model_handle Model = GetModel(GameAssets, Entity);
                    PushModelNew(RenderGroup, Model, Transform);
                } break;
                case Entity_Farm:
                {
                    Assert(Entity->Parent);
                    
                    v3 HexP = GetEntityP(Game, Entity->Parent);
                    v3 FarmP = GetEntityP(Game, EntityIndex);
                    
                    DrawDirt(RenderGroup, Game, Entity->Parent);
                    span<v2> Offsets = GetGrassRandomOffsets();
                    
                    for (v2 Offset : Offsets)
                    {
                        v3 P = HexP + FarmP + V3(World->Entities[Entity->Parent].Size * Offset, 0.0f);
                        
                        m4x4 Transform = ScaleTransform(0.01f) * TranslateTransform(P);
                        PushModelNew(RenderGroup, GameAssets->Grass, Transform);
                        PushWind(RenderGroup);
                    }
                } break;
                case Entity_Structure:
                {
                    v3 P = Entity->P;
                    m4x4 Transform = RotateTransform(-Entity->Angle) * TranslateTransform(P); //idk why angle is negativo
                    model_handle Model = GetModel(GameAssets, Entity);
                    PushTexturedModel(RenderGroup, Model, Transform);
                    //PushModelNew(RenderGroup, Assets, Model, Transform);
                } break;
                case Entity_Fence:
                {
                    model_handle Model = GetModel(GameAssets, Entity);
                    v3 HexP = GetEntityP(Game, Entity->Parent);
                    
                    span<v2> Vertices = GetHexagonVertexPositions(HexP.XY, 0.89f * World->HexSize, Assets->Allocator.Transient);
                    
                    for (u64 VertexIndex = 0; VertexIndex < Vertices.Count; VertexIndex++)
                    {
                        v2 A = Vertices[VertexIndex];
                        v2 B = Vertices[(VertexIndex + 1) % Vertices.Count];
                        
                        f32 Angle = VectorAngle(A - B);
                        
                        v3 P = V3(A, HexP.Z);
                        
                        m4x4 Transform = RotateTransform(-Angle - 0.01f) * ScaleTransform(0.9f * World->HexSize) * TranslateTransform(P);
                        PushTexturedModel(RenderGroup, Model, Transform);
                    }
                    
                    
                } break;
                default: Assert(0);
            }
        }
    }
    
    {
        TimeBlock("DrawHexOutline");
        v3 HexP = Game->HexOutlineP;
        DrawHexOutline(RenderGroup, GameAssets, Game, HexP);
    }
    
    {
        TimeBlock("DrawTowers");
        DrawTowers(RenderGroup, Game, GameAssets);
    }
    
    //Make constants
    m4x4 WorldTransform = Game->WorldTransform;
    m4x4 LightTransform = MakeLightTransform(Game, Game->LightP, Game->LightDirection);
    
    Constants.WorldToClipTransform = LightTransform;
    Constants.WorldToLightTransform = LightTransform;
    Constants.Time = Game->Time; //TODO: Maybe make this periodic?
    Constants.CameraPos = Game->CameraP;
    Constants.LightDirection = Game->LightDirection;
    Constants.LightColor = Game->SkyColor;
    Constants.ShadowIntensity = Game->ShadowIntensity;
    
    SetBlendMode(BlendMode_Blend);
    
    //Draw shadow map
    SetDepthTest(true);
    SetFrontCullMode(true); //INCORRECTLY NAMED FUNCTION
    UnsetShadowMap();
    ClearOutput(GameAssets->ShadowMap);
    SetOutput(GameAssets->ShadowMap);
    DrawRenderGroup(RenderGroup, Constants, (render_draw_type)(Draw_OnlyDepth|Draw_Shadow));
    SetFrontCullMode(false);
    SetOutput({});
    SetShadowMap(GameAssets->ShadowMap);
    
    Constants.WorldToClipTransform = WorldTransform;
    
    //Draw reflection texture
    
    
    v3 ReflectionDirection = Game->CameraDirection;
    ReflectionDirection.Z *= -1.0f;
    v3 ReflectionCameraP = Game->CameraP;
    ReflectionCameraP.Z -= 2.0f * (ReflectionCameraP.Z - Game->WaterZ);
    v3 ReflectionLookAt = ReflectionCameraP + ReflectionDirection;
    
    m4x4 ReflectionWorldTransform = ViewTransform(ReflectionCameraP, ReflectionLookAt) * PerspectiveTransform(Game->FOV, 0.01f, 150.0f);
    
    Constants.ClipPlane = V4(0.0f, 0.0f, -1.0f, Game->WaterZ);
    Constants.WorldToClipTransform = ReflectionWorldTransform;
    ClearOutput(GameAssets->WaterReflection);
    SetOutput(GameAssets->WaterReflection);
    DrawRenderGroup(RenderGroup, Constants, Draw_Regular);
    
    //Refraction texture
    //The clip plane could probably be removed but having it might mean a few less calculations of the pixel shader
    Constants.ClipPlane = V4(0.0f, 0.0f, 1.0f, -(Game->WaterZ - 0.05f));
    Constants.WorldToClipTransform = WorldTransform;
    ClearOutput(GameAssets->WaterRefraction);
    SetOutput(GameAssets->WaterRefraction);
    DrawRenderGroup(RenderGroup, Constants, Draw_Regular);
    
    //Draw normal world
    Constants.ClipPlane = {};
    
    DrawWater(RenderGroup, Game->WaterZ);
    DrawRegionOutline(RenderGroup, Assets, GameAssets, Game);
    
    //Set reflection and refraction textures
    ClearOutput(GameAssets->Output1);
    SetOutput(GameAssets->Output1);
    SetShadowMap(GameAssets->ShadowMap);
    SetTexture(GameAssets->WaterReflection, 2);
    SetTexture(GameAssets->WaterRefraction, 3);
    SetTexture(GameAssets->WaterDuDv, 4);
    SetTexture(GameAssets->WaterFlow, 5);
    SetTexture(GameAssets->WaterNormal, 6);
    
    SetTexture(GameAssets->ModelTextures.Ambient, 7);
    SetTexture(GameAssets->ModelTextures.Diffuse, 8);
    SetTexture(GameAssets->ModelTextures.Normal, 9);
    SetTexture(GameAssets->ModelTextures.Specular, 10);
    
    DrawRenderGroup(RenderGroup, Constants, Draw_Regular);
    
    UnsetShadowMap();
    UnsetTexture(2);
    UnsetTexture(3);
    UnsetTexture(4);
    UnsetTexture(5);
    UnsetTexture(6);
    
    SetGUIShaderConstant(IdentityTransform());
    SetDepthTest(false);
    
    ClearOutput(GameAssets->BloomMipmaps[0]);
    SetOutput(GameAssets->BloomMipmaps[0]);
    
    SetTexture(GameAssets->Output1, 11);
    
    SetShader(Shader_Bloom_Filter);
    
    renderer_vertex_buffer WholeScreen = Assets->VertexBuffers[GameAssets->GUIWholeScreen];
    DrawVertexBuffer(WholeScreen);
    
    //Downsampling
    SetShader(Shader_Bloom_Downsample);
    for (u64 MipmapIndex = 1; MipmapIndex < ArrayCount(GameAssets->BloomMipmaps); MipmapIndex++)
    {
        ClearOutput(GameAssets->BloomMipmaps[MipmapIndex]);
        SetOutput(GameAssets->BloomMipmaps[MipmapIndex]);
        
        SetTexture(GameAssets->BloomMipmaps[MipmapIndex - 1], 11);
        DrawVertexBuffer(WholeScreen);
    }
    
    //Upsampling
    SetShader(Shader_Bloom_Upsample);
    SetBlendMode(BlendMode_Add);
    ClearOutput(GameAssets->BloomAccum, V4(0, 0, 0, 0));
    SetOutput(GameAssets->BloomAccum);
    for (u64 MipmapIndex = 0; MipmapIndex < ArrayCount(GameAssets->BloomMipmaps); MipmapIndex++)
    {
        SetTexture(GameAssets->BloomMipmaps[MipmapIndex], 11);
        DrawVertexBuffer(WholeScreen);
    }
    
    //Composing
    SetFrameBufferAsOutput();
    
    SetShader(Shader_GUI_HDR_To_SDR);
    
    SetBlendMode(BlendMode_Blend);
    SetTexture(GameAssets->Output1, 0);
    DrawVertexBuffer(WholeScreen);
    
    SetShader(Shader_GUI_Texture);
    
    SetBlendMode(BlendMode_Add);
    SetTexture(GameAssets->BloomAccum, 0);
    DrawVertexBuffer(WholeScreen);
    
    SetBlendMode(BlendMode_Blend);
}