static void
DrawWater(render_group* RenderGroup, game_state* Game)
{
    PushRect(RenderGroup, V3(-1.0f, -1.0f, 0.125f), V3(1.0f, 1.0f, 0.125f));
    PushShader(RenderGroup, Shader_Water);
    PushNoShadow(RenderGroup);
}

static void
DrawSkybox(render_group* RenderGroup, game_assets* Assets)
{
    PushTexturedRect(RenderGroup, Assets->Skybox.Textures[5], V3(-1000.0f, -1000.0f, 1000.0f), V3(1000.0f, 1000.0f, 1000.0f));
    PushNoDepthTest(RenderGroup);
}

static void
DrawBackground(render_group* RenderGroup)
{
    //TODO: Fix this
    //PushTexturedRect(RenderGroup, BackgroundTexture, V3(0.0f, 0.0f, 0.0f), V3(1.0f, 0.5625f, 0.0f));
}

static void
DrawWorldRegion(render_group* RenderGroup, game_state* Game, world* World, world_region* Region, v4 Color, bool Hovering)
{
    if (Region->VertexCount == 0)
    {
        return;
    }
    if (Hovering)
    {
        Color.RGB = 0.8f * Color.RGB;
    }
    
    u32 TriangleCount = Region->VertexCount + 2 * Region->VertexCount; // Top + sides
    span<tri> Triangles = AllocSpan(RenderGroup->Arena, tri, TriangleCount);
    
    f32 Z = Region->Z;
    
    //Top
    for (u32 VertexIndex = 0; VertexIndex < Region->VertexCount; VertexIndex++)
    {
        tri* Tri = Triangles + VertexIndex;
        
        Tri->Vertices[0].P = V3(Region->Center, Z);
        Tri->Vertices[1].P = V3(GetVertex(Region, VertexIndex), Z);
        Tri->Vertices[2].P = V3(GetVertex(Region, VertexIndex + 1), Z);
    }
    
    //Sides
    for (u32 VertexIndex = 0; VertexIndex < Region->VertexCount; VertexIndex++)
    {
        tri* Tri0 = Triangles + Region->VertexCount + 2 * VertexIndex + 0;
        tri* Tri1 = Triangles + Region->VertexCount + 2 * VertexIndex + 1;
        
        Tri0->Vertices[0].P = V3(GetVertex(Region, VertexIndex), Z);
        Tri0->Vertices[1].P = V3(GetVertex(Region, VertexIndex), 2.0f);
        Tri0->Vertices[2].P = V3(GetVertex(Region, VertexIndex + 1), 2.0f);
        
        Tri1->Vertices[0].P = V3(GetVertex(Region, VertexIndex + 1), 2.0f);
        Tri1->Vertices[1].P = V3(GetVertex(Region, VertexIndex + 1), Z);
        Tri1->Vertices[2].P = V3(GetVertex(Region, VertexIndex), Z);
    }
    
    CalculateModelVertexNormals(Triangles.Memory, Triangles.Count);
    PushVertices(RenderGroup, Triangles.Memory, TriangleCount * sizeof(tri), sizeof(vertex), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, Shader_Model);
    PushColor(RenderGroup, Region->Color);
    PushModelTransform(RenderGroup, IdentityTransform());
    
    Z -= 0.001f; //Prevent z-fighting
    
    bool DrawOutline = Hovering;
    if (DrawOutline)
    {
        u32 VertexDrawCount = 6 * Region->VertexCount + 2;
        vertex* Vertices = AllocArray(RenderGroup->Arena, vertex, VertexDrawCount);
        
        for (u32 VertexIndex = 0; VertexIndex < Region->VertexCount; VertexIndex++)
        {
            v2 Vertex = GetVertex(Region, VertexIndex);
            v2 PrevVertex = GetVertex(Region, VertexIndex - 1);
            v2 NextVertex = GetVertex(Region, VertexIndex + 1);
            
            v2 PerpA = UnitV(Perp(Vertex - PrevVertex));
            v2 PerpB = UnitV(Perp(NextVertex - Vertex));
            v2 Mid = UnitV(PerpA + PerpB);
            
            f32 SinAngle = Det(M2x2(UnitV(Vertex - PrevVertex), UnitV(NextVertex - Vertex)));
            
            v2 RectSize = V2(0.005f, 0.005f);
            
            f32 HalfBorderThickness = 0.0035f;
            
            //If concave
            if (SinAngle < 0.0f)
            {
                Vertices[VertexIndex * 6 + 0] = {V3(Vertex - HalfBorderThickness * Mid,   Z), {}, V4(1.0f, 1.0f, 1.0f, 1.0f)};
                Vertices[VertexIndex * 6 + 1] = {V3(Vertex + HalfBorderThickness * PerpA, Z), {}, V4(1.0f, 1.0f, 1.0f, 1.0f)};
                Vertices[VertexIndex * 6 + 2] = {V3(Vertex - HalfBorderThickness * Mid,   Z), {}, V4(1.0f, 1.0f, 1.0f, 1.0f)};
                Vertices[VertexIndex * 6 + 3] = {V3(Vertex + HalfBorderThickness * Mid,   Z), {}, V4(1.0f, 1.0f, 1.0f, 1.0f)};
                Vertices[VertexIndex * 6 + 4] = {V3(Vertex - HalfBorderThickness * Mid,   Z), {}, V4(1.0f, 1.0f, 1.0f, 1.0f)};
                Vertices[VertexIndex * 6 + 5] = {V3(Vertex + HalfBorderThickness * PerpB, Z), {}, V4(1.0f, 1.0f, 1.0f, 1.0f)};
            }
            else
            {
                Vertices[VertexIndex * 6 + 0] = {V3(Vertex - HalfBorderThickness * PerpA, Z), {}, V4(1.0f, 1.0f, 1.0f, 1.0f)};
                Vertices[VertexIndex * 6 + 1] = {V3(Vertex + HalfBorderThickness * Mid,   Z), {}, V4(1.0f, 1.0f, 1.0f, 1.0f)};
                Vertices[VertexIndex * 6 + 2] = {V3(Vertex - HalfBorderThickness * Mid,   Z), {}, V4(1.0f, 1.0f, 1.0f, 1.0f)};
                Vertices[VertexIndex * 6 + 3] = {V3(Vertex + HalfBorderThickness * Mid,   Z), {}, V4(1.0f, 1.0f, 1.0f, 1.0f)};
                Vertices[VertexIndex * 6 + 4] = {V3(Vertex - HalfBorderThickness * PerpB, Z), {}, V4(1.0f, 1.0f, 1.0f, 1.0f)};
                Vertices[VertexIndex * 6 + 5] = {V3(Vertex + HalfBorderThickness * Mid,   Z), {}, V4(1.0f, 1.0f, 1.0f, 1.0f)};
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
}


static void
DrawRegions(render_group* RenderGroup, game_state* Game, render_context* Context)
{
    SetShader(ColorShader);
    for (u32 RegionIndex = 0; RegionIndex < Game->GlobalState.World.RegionCount; RegionIndex++)
    {
        
        world_region* Region = Game->GlobalState.World.Regions + RegionIndex;
        
        bool Hovering = (Region == Context->HoveringRegion);
        DrawWorldRegion(RenderGroup, Game, &Game->GlobalState.World, Region, Region->Color, Hovering);
        
        //Draw name
        /*
        if (Region->VertexCount > 0)
        {
            f32 TextSize = 0.04f;
            string Name = GetName(Region);
            
            f32 NameWidth = PlatformTextWidth(Name, TextSize);
            v2 P = Region->Center;
            P.X -= 0.5f * NameWidth;
            P.Y -= 0.25f * TextSize;
            
            SetShader(FontShader);
            DrawString(Name, P, V4(1.0f, 1.0f, 1.0f, 1.0f), TextSize);
        }
*/
    }
}

static void
DrawTower(render_group* RenderGroup, game_state* Game, tower_type Type, v3 P, v4 Color, f32 Angle = 0.0f)
{
    //TODO: Use game assets
    span<vertex> ModelVertices = {};
    m4x4 Transform;
    
    if (Type == Tower_Castle)
    {
        ModelVertices = Game->CubeVertices;
        Transform = Game->CastleTransform;
    }
    else if (Type == Tower_Turret)
    {
        ModelVertices = Game->TurretVertices;
        Transform = Game->TurretTransform * RotateTransform(-1.0f * Angle); //idk why it is -1.0f
    }
    else
    {
        Assert(0);
    }
    
    m4x4 ModelTransform = Transform * TranslateTransform(P.X, P.Y, P.Z);
    
    PushVertices(RenderGroup, ModelVertices.Memory, sizeof(vertex) * ModelVertices.Count, sizeof(vertex), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, Shader_Model);
    PushColor(RenderGroup, Color);
    PushModelTransform(RenderGroup, ModelTransform);
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
    //SetDepthTest(false);
    
    
    //SetDepthTest(true);
    /*
    if (Game->ShowBackground)
    {
        DrawBackground(RenderGroup);
    }
*/
    DrawSkybox(RenderGroup, Assets);
    
    //SetDepthTest(false);
    DrawRegions(RenderGroup, Game, Context);
    DrawTowers(RenderGroup, Game, Context);
    DrawWater(RenderGroup, Game);
}

static void
RenderWorld(game_state* Game, game_assets* Assets, render_context* Context)
{
    m4x4 WorldTransform = Game->WorldTransform;
    
    v3 LightP = V3(-1.0f, -1.0f, -1.0f);
    v3 LightDirection = V3(1.0f, 1.0f, 1.0f);
    m4x4 Transform = ViewTransform(LightP, LightP + LightDirection) * OrthographicTransform(-1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 3.0f);
    
    render_group RenderGroup = {};
    RenderGroup.Arena = Context->Arena; //TODO: Fix this
    
    DrawWorld(&RenderGroup, Game, Assets, Context);
    
    shader_constants Constants = {};
    Constants.WorldToClipTransform = Transform;
    Constants.WorldToLightTransform = Transform;
    
    SetDepthTest(true);
    ClearOutput(Game->ShadowMap);
    SetOutput(Game->ShadowMap);
    SetTransform(Transform);
    DrawRenderGroup(&RenderGroup, Assets, Constants, (render_draw_type)(Draw_OnlyDepth|Draw_Shadow));
    UnsetShadowMap();
    
    Constants.WorldToClipTransform = WorldTransform;
    
    //Draw reflection texture
    f32 WaterZ = 0.125f;
    
    v3 ReflectionDirection = Game->CameraDirection;
    ReflectionDirection.Z *= -1.0f;
    v3 ReflectionCameraP = Game->CameraP;
    ReflectionCameraP.Z -= 2.0f * (ReflectionCameraP.Z - WaterZ);
    v3 ReflectionLookAt = ReflectionCameraP + ReflectionDirection;
    
    m4x4 ReflectionWorldTransform = ViewTransform(ReflectionCameraP, ReflectionLookAt) * PerspectiveTransform(Game->FOV, 0.01f, 1500.0f);
    
    //Constants.ClipPlane = V4(0.0f, 0.0f, -1.0f, -WaterZ);
    Constants.WorldToClipTransform = ReflectionWorldTransform;
    ClearOutput(Assets->WaterReflection);
    SetOutput(Assets->WaterReflection);
    DrawRenderGroup(&RenderGroup, Assets, Constants, Draw_Regular);
    
    //Refraction texture
    //Constants.ClipPlane = V4(0.0f, 0.0f, 1.0f, -WaterZ);
    Constants.WorldToClipTransform = WorldTransform;
    ClearOutput(Assets->WaterRefraction);
    SetOutput(Assets->WaterRefraction);
    DrawRenderGroup(&RenderGroup, Assets, Constants, Draw_Regular);
    
    //Draw normal world
    Constants.ClipPlane = {};
    
    PushTexturedRect(&RenderGroup, Assets->WaterReflection.Texture, V3(-1.5f, -1.5f, 0.0f), V3(-1.0f, -1.0f, 0.0f));
    
    //Set reflection and refraction textures
    SetTexture(Assets->WaterReflection.Texture, 2);
    SetTexture(Assets->WaterRefraction.Texture, 3);
    
    SetShadowMap(Game->ShadowMap);
    SetFrameBufferAsOutput();
    SetTransform(WorldTransform);
    SetLightTransform(Transform);
    DrawRenderGroup(&RenderGroup, Assets, Constants, Draw_Regular);
}