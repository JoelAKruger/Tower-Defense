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
DrawRegionOutline(render_group* RenderGroup, game_assets* Assets, defense_assets* GameAssets, game_state* Game)
{
    v3 P = Game->RegionOutlineP;
    
    f32 Z = P.Z - 0.001f;
    m4x4 Transform = ScaleTransform(0.9f * Game->GlobalState.World.RegionSize) * TranslateTransform(V3(P.XY, Z));
    
    PushVertexBuffer(RenderGroup, GameAssets->RegionOutline, Transform);
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
        entity* Region = Game->GlobalState.World.Entities + Tower->RegionIndex;
        v3 RegionP = GetEntityP(Game, Tower->RegionIndex);
        
        v4 RegionColor = Region->Color;
        
        v4 Color = V4(0.7f * RegionColor.RGB, RegionColor.A);
        if (Game->SelectedTower && TowerIndex == Game->SelectedTowerIndex)
        {
            f32 t = 0.5f + 0.25f * sinf(6.0f * (f32)Game->Time);
            Color = t * RegionColor + (1.0f - t) * V4(1.0f, 1.0f, 1.0f, 1.0f);
        }
        
        DrawTower(RenderGroup, Game, Assets, Tower->Type, V3(Tower->P, RegionP.Z), Color, Tower->Rotation);
    }
}

static m4x4
MakeLightTransform(game_state* Game, v3 LightP, v3 LightDirection)
{
    m4x4 InvWorldTransform = Inverse(Game->WorldTransform);
    
    m4x4 LightViewTransform = ViewTransform(LightP, LightP + LightDirection);
    
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
    
    m4x4 LightTransform = ViewTransform(LightP, LightP + LightDirection) * OrthographicTransform(Left, Right, Bottom, Top, Near, Far);
    
    return LightTransform;
}

static void
DrawDirt(render_group* RenderGroup, game_state* Game, u64 RegionIndex)
{
#if 0
    v3 P = GetEntityP(Game, RegionIndex);
    v4 Color = {}; //V4(0.5f * V3(0.44f, 0.31f, 0.22f), 1.0f); 
    v3 Normal = V3(0.0f, 0.0f, -1.0f);
    f32 Z = P.Z - 0.001f;
    
    u32 VertexDrawCount = 3 * 6;
    vertex* Vertices = AllocArray(RenderGroup->Arena, vertex, VertexDrawCount);
    
    for (int TriangleIndex = 0; TriangleIndex < 6; TriangleIndex++)
    {
        v2 VertexA = GetLocalRegionVertex(Game, RegionIndex, TriangleIndex);
        v2 VertexB = GetLocalRegionVertex(Game, RegionIndex, TriangleIndex - 1);
        
        
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
                case Entity_WorldRegion:
                {
                    v3 P = GetEntityP(Game, EntityIndex);
                    m4x4 Transform = ScaleTransform(Entity->Size) * TranslateTransform(P);
                    span<render_command*> Commands = PushModelNew(RenderGroup, GameAssets->WorldRegion, Transform);
                    Commands[0]->Color = Entity->Color;
                    Transform = ScaleTransform(Entity->Size) * TranslateTransform(P + V3(0.0f, 0.0f, 0.1f));
                    Commands = PushModelNew(RenderGroup, GameAssets->WorldRegionSkirt, Transform);
                    Commands[0]->Color = V4(0.15f, 0.25f, 0.5f, 1.0f);
                } break;
                case Entity_Foliage:
                {
                    Assert(Entity->Parent);
                    
                    v3 RegionP = GetEntityP(Game, Entity->Parent);
                    
                    m4x4 Transform = ScaleTransform(Entity->Size) * TranslateTransform(Entity->P.X, Entity->P.Y, RegionP.Z + Entity->P.Z);
                    model_handle Model = GetModel(GameAssets, Entity);
                    PushModelNew(RenderGroup, Model, Transform);
                } break;
                case Entity_Farm:
                {
                    Assert(Entity->Parent);
                    
                    v3 RegionP = GetEntityP(Game, Entity->Parent);
                    v3 FarmP = GetEntityP(Game, EntityIndex);
                    
                    DrawDirt(RenderGroup, Game, Entity->Parent);
                    span<v2> Offsets = GetGrassRandomOffsets();
                    
                    for (v2 Offset : Offsets)
                    {
                        v3 P = RegionP + FarmP + V3(World->Entities[Entity->Parent].Size * Offset, 0.0f);
                        
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
                    v3 RegionP = GetEntityP(Game, Entity->Parent);
                    
                    span<v2> Vertices = GetHexagonVertexPositions(RegionP.XY, 0.89f * World->RegionSize, Assets->Allocator.Transient);
                    
                    for (u64 VertexIndex = 0; VertexIndex < Vertices.Count; VertexIndex++)
                    {
                        v2 A = Vertices[VertexIndex];
                        v2 B = Vertices[(VertexIndex + 1) % Vertices.Count];
                        
                        f32 Angle = VectorAngle(A - B);
                        
                        v3 P = V3(A, RegionP.Z);
                        
                        m4x4 Transform = RotateTransform(-Angle - 0.01f) * ScaleTransform(0.9f * World->RegionSize) * TranslateTransform(P);
                        PushTexturedModel(RenderGroup, Model, Transform);
                    }
                    
                    
                } break;
                default: Assert(0);
            }
        }
    }
    
    {
        TimeBlock("DrawRegionOutline");
        DrawRegionOutline(RenderGroup, Assets, GameAssets, Game);
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
    ClearOutput(Assets->RenderOutputs[GameAssets->ShadowMap]);
    SetOutput(Assets->RenderOutputs[GameAssets->ShadowMap]);
    DrawRenderGroup(RenderGroup, Constants, (render_draw_type)(Draw_OnlyDepth|Draw_Shadow));
    SetFrontCullMode(false);
    SetOutput({});
    SetShadowMap(Assets->RenderOutputs[GameAssets->ShadowMap]);
    
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
    ClearOutput(Assets->RenderOutputs[GameAssets->WaterReflection]);
    SetOutput(Assets->RenderOutputs[GameAssets->WaterReflection]);
    DrawRenderGroup(RenderGroup, Constants, Draw_Regular);
    
    //Refraction texture
    //The clip plane could probably be removed but having it might mean a few less calculations of the pixel shader
    Constants.ClipPlane = V4(0.0f, 0.0f, 1.0f, -(Game->WaterZ - 0.05f));
    Constants.WorldToClipTransform = WorldTransform;
    ClearOutput(Assets->RenderOutputs[GameAssets->WaterRefraction]);
    SetOutput(Assets->RenderOutputs[GameAssets->WaterRefraction]);
    DrawRenderGroup(RenderGroup, Constants, Draw_Regular);
    
    //Draw normal world
    Constants.ClipPlane = {};
    
    DrawWater(RenderGroup, Game->WaterZ);
    
    //Set reflection and refraction textures
    ClearOutput(Assets->RenderOutputs[GameAssets->Output1]);
    SetOutput(Assets->RenderOutputs[GameAssets->Output1]);
    SetShadowMap(Assets->RenderOutputs[GameAssets->ShadowMap]);
    SetTexture(Assets->RenderOutputs[GameAssets->WaterReflection].Texture, 2);
    SetTexture(Assets->RenderOutputs[GameAssets->WaterRefraction].Texture, 3);
    SetTexture(Assets->Textures[GameAssets->WaterDuDv], 4);
    SetTexture(Assets->Textures[GameAssets->WaterFlow], 5);
    SetTexture(Assets->Textures[GameAssets->WaterNormal], 6);
    
    SetTexture(Assets->Textures[GameAssets->ModelTextures.Ambient], 7);
    SetTexture(Assets->Textures[GameAssets->ModelTextures.Diffuse], 8);
    SetTexture(Assets->Textures[GameAssets->ModelTextures.Normal], 9);
    SetTexture(Assets->Textures[GameAssets->ModelTextures.Specular], 10);
    
    DrawRenderGroup(RenderGroup, Constants, Draw_Regular);
    
    UnsetShadowMap();
    UnsetTexture(2);
    UnsetTexture(3);
    UnsetTexture(4);
    UnsetTexture(5);
    UnsetTexture(6);
    
    SetGUIShaderConstant(IdentityTransform());
    SetDepthTest(false);
    
    ClearOutput(Assets->RenderOutputs[GameAssets->BloomMipmaps[0]]);
    SetOutput(Assets->RenderOutputs[GameAssets->BloomMipmaps[0]]);
    
    SetTexture(Assets->RenderOutputs[GameAssets->Output1].Texture, 11);
    
    SetShader(Shader_Bloom_Filter);
    
    renderer_vertex_buffer WholeScreen = Assets->VertexBuffers[GameAssets->GUIWholeScreen];
    DrawVertexBuffer(WholeScreen);
    
    //Downsampling
    SetShader(Shader_Bloom_Downsample);
    for (u64 MipmapIndex = 1; MipmapIndex < ArrayCount(GameAssets->BloomMipmaps); MipmapIndex++)
    {
        ClearOutput(Assets->RenderOutputs[GameAssets->BloomMipmaps[MipmapIndex]]);
        SetOutput(Assets->RenderOutputs[GameAssets->BloomMipmaps[MipmapIndex]]);
        
        SetTexture(Assets->RenderOutputs[GameAssets->BloomMipmaps[MipmapIndex - 1]].Texture, 11);
        DrawVertexBuffer(WholeScreen);
    }
    
    //Upsampling
    SetShader(Shader_Bloom_Upsample);
    SetBlendMode(BlendMode_Add);
    ClearOutput(Assets->RenderOutputs[GameAssets->BloomAccum], V4(0, 0, 0, 0));
    SetOutput(Assets->RenderOutputs[GameAssets->BloomAccum]);
    for (u64 MipmapIndex = 0; MipmapIndex < ArrayCount(GameAssets->BloomMipmaps); MipmapIndex++)
    {
        SetTexture(Assets->RenderOutputs[GameAssets->BloomMipmaps[MipmapIndex]].Texture, 11);
        DrawVertexBuffer(WholeScreen);
    }
    
    //Composing
    SetFrameBufferAsOutput();
    
    SetShader(Shader_GUI_HDR_To_SDR);
    
    SetBlendMode(BlendMode_Blend);
    SetTexture(Assets->RenderOutputs[GameAssets->Output1].Texture, 0);
    DrawVertexBuffer(WholeScreen);
    
    SetShader(Shader_GUI_Texture);
    
    SetBlendMode(BlendMode_Add);
    SetTexture(Assets->RenderOutputs[GameAssets->BloomAccum].Texture, 0);
    DrawVertexBuffer(WholeScreen);
    
    SetBlendMode(BlendMode_Blend);
}