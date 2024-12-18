static void
DrawWater(render_group* RenderGroup, game_state* Game)
{
    PushRect(RenderGroup, V3(-1.0f, -1.0f, 0.125f), V3(1.0f, 1.0f, 0.125f));
    PushShader(RenderGroup, Shader_Water);
}

static void
DrawSkybox(render_group* RenderGroup, game_assets* Assets)
{
    
    PushTexturedRect(RenderGroup, Assets->Skybox.Textures[5], V3(-100.0f, -100.0f, 100.0f), V3(100.0f, 100.0f, 100.0f));
    PushNoDepthTest(RenderGroup);
    
    //PushTexturedRect(RenderGroup, Assets->Skybox.Textures[0], V3(100.0f, 100.0f, -100.0f), V3(-100.0f, -100.0f, -100.0f));
    //PushNoDepthTest(RenderGroup);
    
    PushTexturedRect(RenderGroup, Assets->Skybox.Textures[0], 
                     V3(-100.0f, 100.0f, -100.0f), V3(-100.0f, -100.0f, -100.0f),
                     V3(100.0f, -100.0f, -100.0f), V3(100.0f, 100.0f, -100.0f),
                     V2(1.0f, 1.0f), V2(1.0f, 0.0f), V2(0.0f, 0.0f), V2(0.0f, 1.0f));
}

static void
DrawRegionOutline(render_group* RenderGroup, world_region* Region)
{
    v4 Color = V4(1.0f, 1.0f, 1.0f, 1.0f);
    v3 Normal = V3(0.0f, 0.0f, -1.0f);
    f32 Z = Region->Z - 0.001f;
    
    u32 VertexDrawCount = 6 * ArrayCount(Region->Vertices) + 2;
    vertex* Vertices = AllocArray(RenderGroup->Arena, vertex, VertexDrawCount);
    
    for (u32 VertexIndex = 0; VertexIndex < ArrayCount(Region->Vertices); VertexIndex++)
    {
        v2 Vertex = GetVertex(Region, VertexIndex);
        v2 PrevVertex = GetVertex(Region, VertexIndex - 1);
        v2 NextVertex = GetVertex(Region, VertexIndex + 1);
        
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
}

static void
DrawTower(render_group* RenderGroup, game_state* Game, tower_type Type, v3 P, v4 Color, f32 Angle = 0.0f)
{
    vertex_buffer_index VertexBuffer = {};
    m4x4 Transform;
    
    if (Type == Tower_Castle)
    {
        VertexBuffer = VertexBuffer_Castle;
        Transform = Game->CastleTransform;
    }
    else if (Type == Tower_Turret)
    {
        VertexBuffer = VertexBuffer_Turret;
        Transform = Game->TurretTransform * RotateTransform(-1.0f * Angle); //idk why it is -1.0f
    }
    else if (Type == Tower_Mine)
    {
        VertexBuffer = VertexBuffer_Mine;
        Transform = TranslateTransform(0.0f, 0.0f, -0.2f) * ScaleTransform(0.1f, 0.1f, 0.1f);
    }
    else
    {
        Assert(0);
    }
    
    m4x4 ModelTransform = Transform * TranslateTransform(P.X, P.Y, P.Z);
    
    PushModel(RenderGroup, VertexBuffer);
    PushColor(RenderGroup, Color);
    PushModelTransform(RenderGroup, ModelTransform);
    
    if (Type == Tower_Mine)
    {
        PushShader(RenderGroup, Shader_TexturedModel);
    }
}

static void
DrawTowers(render_group* RenderGroup, game_state* Game, render_context* Context)
{
    for (u32 TowerIndex = 0; TowerIndex < Game->GlobalState.TowerCount; TowerIndex++)
    {
        tower* Tower = Game->GlobalState.Towers + TowerIndex;
        world_region* Region = Game->GlobalState.World.Regions + Tower->RegionIndex;
        
        v4 RegionColor = Region->Color;
        
        v4 Color = V4(0.7f * RegionColor.RGB, RegionColor.A);
        if (Tower == Context->SelectedTower)
        {
            f32 t = 0.5f + 0.25f * sinf(6.0f * (f32)Game->Time);
            Color = t * RegionColor + (1.0f - t) * V4(1.0f, 1.0f, 1.0f, 1.0f);
        }
        
        DrawTower(RenderGroup, Game, Tower->Type, V3(Tower->P, Region->Z), Color, Tower->Rotation);
    }
}

static void
DrawWorld(render_group* RenderGroup, game_state* Game, game_assets* Assets, render_context* Context)
{
    DrawSkybox(RenderGroup, Assets);
    
    PushModel(RenderGroup, VertexBuffer_World);
    if (Context->HoveringRegion)
    {
        DrawRegionOutline(RenderGroup, Context->HoveringRegion);
    }
    
    DrawTowers(RenderGroup, Game, Context);
}

static m4x4
MakeLightTransform(game_state* Game, v3 LightP, v3 LightDirection)
{
    m4x4 InvWorldTransform = Inverse(Game->WorldTransform);
    
    m4x4 LightViewTransform = ViewTransform(LightP, LightP + LightDirection);
    
    f32 MinWorldZ = -0.2f;
    f32 MaxWorldZ = 0.1f;
    
    v3 WorldPositions[8] = {
        V3(ScreenToWorld(Game, V2(-1, -1), MinWorldZ), MinWorldZ),
        V3(ScreenToWorld(Game, V2(-1, 1), MinWorldZ), MinWorldZ),
        V3(ScreenToWorld(Game, V2(1, -1), MinWorldZ), MinWorldZ),
        V3(ScreenToWorld(Game, V2(1, 1), MinWorldZ), MinWorldZ),
        V3(ScreenToWorld(Game, V2(-1, -1), MaxWorldZ), MaxWorldZ),
        V3(ScreenToWorld(Game, V2(-1, 1), MaxWorldZ), MaxWorldZ),
        V3(ScreenToWorld(Game, V2(1, -1), MaxWorldZ), MaxWorldZ),
        V3(ScreenToWorld(Game, V2(1, 1), MaxWorldZ), MaxWorldZ),
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

static void RenderWorld(game_state* Game, game_assets* Assets, render_context* Context, shader_constants* Constants)
{
    m4x4 WorldTransform = Game->WorldTransform;
    
    /*
    f32 Visibility = -Game->CameraP.Z;
    v2 Focus = ScreenToWorld(Game, V2(0.0f, 0.0f), 0.0f);
    v3 LightP = V3(Focus, 0.0f) + Visibility * V3(-1.0f, -1.0f, -1.0f);
    v3 LightDirection = V3(1.0f, 1.0f, 1.0f);
    m4x4 LightTransform = ViewTransform(LightP, LightP + LightDirection) * OrthographicTransform(-1.8f * Visibility, Visibility, -Visibility, 1.1f * Visibility, 0.0f, 3.0f);
    */
    
    v3 LightP = V3(-1.0f, -1.0f, -1.0f);
    v3 LightDirection = UnitV(V3(1.0f, 1.0f, 1.0f));
    //m4x4 LightTransform = ViewTransform(LightP, LightP + LightDirection) * OrthographicTransform(-0.7f, 0.7f, -0.7f, 0.3f, 0.0f, 3.0f);
    m4x4 LightTransform = MakeLightTransform(Game, LightP, LightDirection);
    
    render_group RenderGroup = {};
    RenderGroup.Arena = Context->Arena; //TODO: Fix this
    
    DrawWorld(&RenderGroup, Game, Assets, Context);
    
    Constants->WorldToClipTransform = LightTransform;
    Constants->WorldToLightTransform = LightTransform;
    Constants->Time = Game->Time; //TODO: Maybe make this periodic?
    Constants->CameraPos = Game->CameraP;
    Constants->LightDirection = LightDirection;
    
    //Draw shadow map
    SetDepthTest(true);
    SetFrontCullMode(true);
    UnsetShadowMap();
    ClearOutput(Assets->ShadowMaps[0]);
    SetOutput(Assets->ShadowMaps[0]);
    SetTransform(LightTransform);
    DrawRenderGroup(&RenderGroup, Assets, *Constants, (render_draw_type)(Draw_OnlyDepth|Draw_Shadow));
    SetFrontCullMode(false);
    
    Constants->WorldToClipTransform = WorldTransform;
    
    //Draw reflection texture
    f32 WaterZ = 0.125f;
    
    v3 ReflectionDirection = Game->CameraDirection;
    ReflectionDirection.Z *= -1.0f;
    v3 ReflectionCameraP = Game->CameraP;
    ReflectionCameraP.Z -= 2.0f * (ReflectionCameraP.Z - WaterZ);
    v3 ReflectionLookAt = ReflectionCameraP + ReflectionDirection;
    
    m4x4 ReflectionWorldTransform = ViewTransform(ReflectionCameraP, ReflectionLookAt) * PerspectiveTransform(Game->FOV, 0.01f, 1500.0f);
    
    Constants->ClipPlane = V4(0.0f, 0.0f, -1.0f, WaterZ);
    Constants->WorldToClipTransform = ReflectionWorldTransform;
    ClearOutput(Assets->WaterReflection);
    SetOutput(Assets->WaterReflection);
    DrawRenderGroup(&RenderGroup, Assets, *Constants, Draw_Regular);
    
    //Refraction texture
    Constants->ClipPlane = V4(0.0f, 0.0f, 1.0f, -WaterZ);
    Constants->WorldToClipTransform = WorldTransform;
    ClearOutput(Assets->WaterRefraction);
    SetOutput(Assets->WaterRefraction);
    DrawRenderGroup(&RenderGroup, Assets, *Constants, Draw_Regular);
    
    //Draw normal world
    Constants->ClipPlane = {};
    
    DrawWater(&RenderGroup, Game);
    
    //Set reflection and refraction textures
    SetFrameBufferAsOutput();
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
    
    
    SetTransform(WorldTransform);
    SetLightTransform(LightTransform);
    DrawRenderGroup(&RenderGroup, Assets, *Constants, Draw_Regular);
    
    UnsetShadowMap();
    UnsetTexture(2);
    UnsetTexture(3);
    UnsetTexture(4);
    UnsetTexture(5);
    UnsetTexture(6);
}