static void
DrawOceanFloor(render_group* RenderGroup)
{
    PushRectBetter(RenderGroup, V3(-100.0f, -100.0f, 0.5f), V3(100.0f, 100.0f, 0.5f), V3(0, 0, -1));
    PushColor(RenderGroup, V4(0.1f, 0.15f, 0.3f, 1.0f));
}

static void
DrawWater(render_group* RenderGroup, f32 WaterZ)
{
    PushRectBetter(RenderGroup, V3(-100.0f, -100.0f, WaterZ), V3(100.0f, 100.0f, WaterZ), V3(0, 0, -1));
    PushShader(RenderGroup, Shader_Water);
}

static void
DrawSkybox(render_group* RenderGroup, game_assets* Assets)
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
DrawRegionOutline(render_group* RenderGroup, game_state* Game, u64 RegionIndex)
{
    v3 P = GetEntityP(Game, RegionIndex);
    
    v4 Color = V4(1.1f, 1.1f, 1.1f, 1.0f); 
    v3 Normal = V3(0.0f, 0.0f, -1.0f);
    f32 Z = P.Z - 0.001f;
    
    u32 VertexDrawCount = 6 * 6 + 2;
    vertex* Vertices = AllocArray(RenderGroup->Arena, vertex, VertexDrawCount);
    
    for (int VertexIndex = 0; VertexIndex < 6; VertexIndex++)
    {
        v2 Vertex = GetLocalRegionVertex(Game, RegionIndex, VertexIndex);
        v2 PrevVertex = GetLocalRegionVertex(Game, RegionIndex, VertexIndex - 1);
        v2 NextVertex = GetLocalRegionVertex(Game, RegionIndex,  VertexIndex + 1);
        
        v2 PerpA = UnitV(Perp(Vertex - PrevVertex));
        v2 PerpB = UnitV(Perp(NextVertex - Vertex));
        v2 Mid = UnitV(PerpA + PerpB);
        
        f32 SinAngle = Det(M2x2(UnitV(Vertex - PrevVertex), UnitV(NextVertex - Vertex)));
        
        f32 HalfBorderThickness = 0.0035f;
        
        //If concave
        if (SinAngle < 0.0f)
        {
            Vertices[VertexIndex * 6 + 0] = {V3(Vertex - HalfBorderThickness * Mid,   Z), Normal, Color};
            Vertices[VertexIndex * 6 + 1] = {V3(Vertex + HalfBorderThickness * PerpA, Z), Normal, Color};
            Vertices[VertexIndex * 6 + 2] = {V3(Vertex - HalfBorderThickness * Mid,   Z), Normal, Color};
            Vertices[VertexIndex * 6 + 3] = {V3(Vertex + HalfBorderThickness * Mid,   Z), Normal, Color};
            Vertices[VertexIndex * 6 + 4] = {V3(Vertex - HalfBorderThickness * Mid,   Z), Normal, Color};
            Vertices[VertexIndex * 6 + 5] = {V3(Vertex + HalfBorderThickness * PerpB, Z), Normal, Color};
        }
        else
        {
            Vertices[VertexIndex * 6 + 0] = {V3(Vertex - HalfBorderThickness * PerpA, Z), Normal, Color};
            Vertices[VertexIndex * 6 + 1] = {V3(Vertex + HalfBorderThickness * Mid,   Z), Normal, Color};
            Vertices[VertexIndex * 6 + 2] = {V3(Vertex - HalfBorderThickness * Mid,   Z), Normal, Color};
            Vertices[VertexIndex * 6 + 3] = {V3(Vertex + HalfBorderThickness * Mid,   Z), Normal, Color};
            Vertices[VertexIndex * 6 + 4] = {V3(Vertex - HalfBorderThickness * PerpB, Z), Normal, Color};
            
            Vertices[VertexIndex * 6 + 5] = {V3(Vertex + HalfBorderThickness * Mid,   Z), Normal, Color};
        }
        
        if (VertexIndex == 0)
        {
            Vertices[VertexDrawCount - 2] = Vertices[0];
            Vertices[VertexDrawCount - 1] = Vertices[1];
        }
    }
    
    PushVertices(RenderGroup, Vertices, VertexDrawCount * sizeof(vertex), sizeof(vertex),
                 D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, Shader_Model);
    PushNoShadow(RenderGroup);
    PushShader(RenderGroup, Shader_Color);
}

static void
DrawTower(render_group* RenderGroup, game_state* Game, game_assets* Assets, tower_type Type, v3 P, v4 Color, f32 Angle = 0.0f)
{
    m4x4 ModelTransform = TranslateTransform(P.X, P.Y, P.Z);
    
    renderer_vertex_buffer* VertexBuffer = 0;
    m4x4 Transform;
    
    if (Type == Tower_Castle)
    {
        span<render_command*> Commands = PushModelNew(RenderGroup, Assets, "Castle", ModelTransform);
        Commands[0]->Color = Color;
    }
    else if (Type == Tower_Turret)
    {
        span<render_command*> Commands = PushModelNew(RenderGroup, Assets, "Turret", ModelTransform);
        Commands[0]->Color = Color;
    }
    else if (Type == Tower_Mine)
    {
        VertexBuffer = FindVertexBuffer(Assets, "Mine");
        Transform = TranslateTransform(0.0f, 0.0f, -0.2f) * ScaleTransform(0.1f, 0.1f, 0.1f);
    }
    else
    {
        Assert(0);
    }
    
    
    
    PushModel(RenderGroup, VertexBuffer);
    PushColor(RenderGroup, Color);
    PushModelTransform(RenderGroup, ModelTransform);
    
    if (Type == Tower_Mine)
    {
        PushShader(RenderGroup, Shader_TexturedModel);
    }
}

static void
DrawTowers(render_group* RenderGroup, game_state* Game, game_assets* Assets)
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

static void RenderWorld(render_group* RenderGroup, game_state* Game, game_assets* Assets)
{
    world* World = &Game->GlobalState.World;
    
    shader_constants Constants = {};
    
    DrawSkybox(RenderGroup, Assets);
    DrawOceanFloor(RenderGroup);
    
    //PushModel(RenderGroup, FindVertexBuffer(Assets, "World"));
    for (u64 EntityIndex = 1; EntityIndex < World->EntityCount; EntityIndex++)
    {
        entity* Entity = World->Entities + EntityIndex;
        switch (Entity->Type)
        {
            case Entity_WorldRegion:
            {
                v3 P = GetEntityP(Game, EntityIndex);
                m4x4 Transform = TranslateTransform(P);
                span<render_command*> Commands = PushModelNew(RenderGroup, Assets, "Hexagon", Transform);
                render_command* Command = Commands[0];
                Command->Color = Entity->Color;
            } break;
            case Entity_Foliage:
            {
                Assert(Entity->Parent);
                
                v3 RegionP = GetEntityP(Game, Entity->Parent);
                
                m4x4 Transform = ScaleTransform(Entity->Size) * TranslateTransform(Entity->P.X, Entity->P.Y, RegionP.Z + Entity->P.Z);
                char* Model = GetFoliageAssetName(Entity->FoliageType);
                PushModelNew(RenderGroup, Assets, Model, Transform);
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
                    v3 P = RegionP + FarmP + V3(1.1f * World->Entities[Entity->Owner].Size * Offset, 0.0f);
                    
                    m4x4 Transform = ScaleTransform(0.01f) * TranslateTransform(P);
                    char* Model = "GrassPatch101_Plane.040";
                    PushModelNew(RenderGroup, Assets, Model, Transform);
                    PushWind(RenderGroup);
                }
            } break;
            case Entity_Structure:
            {
                v3 P = Entity->P;
                m4x4 Transform = RotateTransform(-Entity->Angle) * TranslateTransform(P); //idk why angle is negativo
                char* Model = GetStructureAssetName(Entity->StructureType);
                PushTexturedModel(RenderGroup, Assets, Model, Transform);
                //PushModelNew(RenderGroup, Assets, Model, Transform);
            } break;
            default: Assert(0);
        }
    }
    
    if (Game->HoveringRegion)
    {
        DrawRegionOutline(RenderGroup, Game, Game->HoveringRegionIndex);
    }
    
    DrawTowers(RenderGroup, Game, Assets);
    
    //Make constants
    m4x4 WorldTransform = Game->WorldTransform;
    m4x4 LightTransform = MakeLightTransform(Game, Game->LightP, Game->LightDirection);
    
    Constants.WorldToClipTransform = LightTransform;
    Constants.WorldToLightTransform = LightTransform;
    Constants.Time = Game->Time; //TODO: Maybe make this periodic?
    Constants.CameraPos = Game->CameraP;
    Constants.LightDirection = Game->LightDirection;
    Constants.LightColor = Game->SkyColor;
    
    SetBlendMode(BlendMode_Blend);
    
    //Draw shadow map
    SetDepthTest(true);
    SetFrontCullMode(true); //INCORRECTLY NAMED FUNCTION
    UnsetShadowMap();
    ClearOutput(Assets->ShadowMaps[0]);
    SetOutput(Assets->ShadowMaps[0]);
    DrawRenderGroup(RenderGroup, Assets, Constants, (render_draw_type)(Draw_OnlyDepth|Draw_Shadow));
    SetFrontCullMode(false);
    SetOutput({});
    SetShadowMap(Assets->ShadowMaps[0]);
    
    Constants.WorldToClipTransform = WorldTransform;
    
    //Draw reflection texture
    f32 WaterFrequency = 0.2f;
    f32 WaterZ = 0.125f + 0.002f * sinf(WaterFrequency * 2 * Pi * Game->Time);
    
    v3 ReflectionDirection = Game->CameraDirection;
    ReflectionDirection.Z *= -1.0f;
    v3 ReflectionCameraP = Game->CameraP;
    ReflectionCameraP.Z -= 2.0f * (ReflectionCameraP.Z - WaterZ);
    v3 ReflectionLookAt = ReflectionCameraP + ReflectionDirection;
    
    m4x4 ReflectionWorldTransform = ViewTransform(ReflectionCameraP, ReflectionLookAt) * PerspectiveTransform(Game->FOV, 0.01f, 150.0f);
    
    Constants.ClipPlane = V4(0.0f, 0.0f, -1.0f, WaterZ);
    Constants.WorldToClipTransform = ReflectionWorldTransform;
    ClearOutput(Assets->WaterReflection);
    SetOutput(Assets->WaterReflection);
    DrawRenderGroup(RenderGroup, Assets, Constants, Draw_Regular);
    
    //Refraction texture
    //The clip plane could probably be removed but having it might mean a few less calculations of the pixel shader
    Constants.ClipPlane = V4(0.0f, 0.0f, 1.0f, -(WaterZ - 0.05f));
    Constants.WorldToClipTransform = WorldTransform;
    ClearOutput(Assets->WaterRefraction);
    SetOutput(Assets->WaterRefraction);
    DrawRenderGroup(RenderGroup, Assets, Constants, Draw_Regular);
    
    //Draw normal world
    Constants.ClipPlane = {};
    
    DrawWater(RenderGroup, WaterZ);
    
    //Set reflection and refraction textures
    ClearOutput(Assets->Output1);
    SetOutput(Assets->Output1);
    SetShadowMap(Assets->ShadowMaps[0]);
    SetTexture(Assets->WaterReflection.Texture, 2);
    SetTexture(Assets->WaterRefraction.Texture, 3);
    SetTexture(Assets->WaterDuDv, 4);
    SetTexture(Assets->WaterFlow, 5);
    SetTexture(Assets->WaterNormal, 6);
    
    SetTexture(Assets->ModelTextures.Ambient, 7);
    SetTexture(Assets->ModelTextures.Diffuse, 8);
    SetTexture(Assets->ModelTextures.Normal, 9);
    SetTexture(Assets->ModelTextures.Specular, 10);
    
    DrawRenderGroup(RenderGroup, Assets, Constants, Draw_Regular);
    
    UnsetShadowMap();
    UnsetTexture(2);
    UnsetTexture(3);
    UnsetTexture(4);
    UnsetTexture(5);
    UnsetTexture(6);
    
    SetGUIShaderConstant(IdentityTransform());
    SetDepthTest(false);
    
    ClearOutput(Assets->BloomMipmaps[0]);
    SetOutput(Assets->BloomMipmaps[0]);
    
    SetTexture(Assets->Output1.Texture, 11);
    
    SetShader(Assets->Shaders[Shader_Bloom_Filter]);
    
    renderer_vertex_buffer WholeScreen = *FindVertexBuffer(Assets, "GUIWholeScreen");
    DrawVertexBuffer(WholeScreen);
    
    //Downsampling
    SetShader(Assets->Shaders[Shader_Bloom_Downsample]);
    for (u64 MipmapIndex = 1; MipmapIndex < ArrayCount(Assets->BloomMipmaps); MipmapIndex++)
    {
        ClearOutput(Assets->BloomMipmaps[MipmapIndex]);
        SetOutput(Assets->BloomMipmaps[MipmapIndex]);
        
        SetTexture(Assets->BloomMipmaps[MipmapIndex - 1].Texture, 11);
        DrawVertexBuffer(WholeScreen);
    }
    
    //Upsampling
    SetShader(Assets->Shaders[Shader_Bloom_Upsample]);
    SetBlendMode(BlendMode_Add);
    ClearOutput(Assets->BloomAccum, V4(0, 0, 0, 0));
    SetOutput(Assets->BloomAccum);
    for (u64 MipmapIndex = 0; MipmapIndex < ArrayCount(Assets->BloomMipmaps); MipmapIndex++)
    {
        SetTexture(Assets->BloomMipmaps[MipmapIndex].Texture, 11);
        DrawVertexBuffer(WholeScreen);
    }
    
    //Composing
    SetFrameBufferAsOutput();
    
    SetShader(Assets->Shaders[Shader_GUI_HDR_To_SDR]);
    
    SetBlendMode(BlendMode_Blend);
    SetTexture(Assets->Output1.Texture, 0);
    DrawVertexBuffer(WholeScreen);
    
    SetShader(Assets->Shaders[Shader_GUI_Texture]);
    
    SetBlendMode(BlendMode_Add);
    SetTexture(Assets->BloomAccum.Texture, 0);
    DrawVertexBuffer(WholeScreen);
    
    SetBlendMode(BlendMode_Blend);
}