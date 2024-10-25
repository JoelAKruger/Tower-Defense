static void
DrawWater(render_group* RenderGroup, game_state* Game)
{
    PushRect(RenderGroup, V3(-1.0f, -1.0f, 0.125f), V3(1.0f, 1.0f, 0.125f));
    PushShader(RenderGroup, Shader_Water);
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
    span<model_triangle> Triangles = AllocSpan(RenderGroup->Arena, model_triangle, TriangleCount);
    
    f32 Z = Region->Z;
    
    //Top
    for (u32 VertexIndex = 0; VertexIndex < Region->VertexCount; VertexIndex++)
    {
        model_triangle* Tri = Triangles + VertexIndex;
        
        Tri->Vertices[0].P = V3(Region->Center, Z);
        Tri->Vertices[1].P = V3(GetVertex(Region, VertexIndex), Z);
        Tri->Vertices[2].P = V3(GetVertex(Region, VertexIndex + 1), Z);
    }
    
    //Sides
    for (u32 VertexIndex = 0; VertexIndex < Region->VertexCount; VertexIndex++)
    {
        model_triangle* Tri0 = Triangles + Region->VertexCount + 2 * VertexIndex + 0;
        model_triangle* Tri1 = Triangles + Region->VertexCount + 2 * VertexIndex + 1;
        
        Tri0->Vertices[0].P = V3(GetVertex(Region, VertexIndex), Z);
        Tri0->Vertices[1].P = V3(GetVertex(Region, VertexIndex), 2.0f);
        Tri0->Vertices[2].P = V3(GetVertex(Region, VertexIndex + 1), 2.0f);
        
        Tri1->Vertices[0].P = V3(GetVertex(Region, VertexIndex + 1), 2.0f);
        Tri1->Vertices[1].P = V3(GetVertex(Region, VertexIndex + 1), Z);
        Tri1->Vertices[2].P = V3(GetVertex(Region, VertexIndex), Z);
    }
    
    CalculateModelVertexNormals(Triangles.Memory, Triangles.Count);
    PushVertices(RenderGroup, Triangles.Memory, TriangleCount * sizeof(model_triangle), sizeof(model_vertex), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, Shader_Model);
    PushColor(RenderGroup, Region->Color);
    PushModelTransform(RenderGroup, IdentityTransform());
    
    Z -= 0.001f; //Prevent z-fighting
    
    bool DrawOutline = Hovering;
    if (DrawOutline)
    {
        u32 VertexDrawCount = 6 * Region->VertexCount + 2;
        color_vertex* Vertices = AllocArray(RenderGroup->Arena, color_vertex, VertexDrawCount);
        
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
                Vertices[VertexIndex * 6 + 0] = {V3(Vertex - HalfBorderThickness * Mid,   Z), V4(1.0f, 1.0f, 1.0f, 1.0f)};
                Vertices[VertexIndex * 6 + 1] = {V3(Vertex + HalfBorderThickness * PerpA, Z), V4(1.0f, 1.0f, 1.0f, 1.0f)};
                Vertices[VertexIndex * 6 + 2] = {V3(Vertex - HalfBorderThickness * Mid,   Z), V4(1.0f, 1.0f, 1.0f, 1.0f)};
                Vertices[VertexIndex * 6 + 3] = {V3(Vertex + HalfBorderThickness * Mid,   Z), V4(1.0f, 1.0f, 1.0f, 1.0f)};
                Vertices[VertexIndex * 6 + 4] = {V3(Vertex - HalfBorderThickness * Mid,   Z), V4(1.0f, 1.0f, 1.0f, 1.0f)};
                Vertices[VertexIndex * 6 + 5] = {V3(Vertex + HalfBorderThickness * PerpB, Z), V4(1.0f, 1.0f, 1.0f, 1.0f)};
            }
            else
            {
                Vertices[VertexIndex * 6 + 0] = {V3(Vertex - HalfBorderThickness * PerpA, Z), V4(1.0f, 1.0f, 1.0f, 1.0f)};
                Vertices[VertexIndex * 6 + 1] = {V3(Vertex + HalfBorderThickness * Mid,   Z), V4(1.0f, 1.0f, 1.0f, 1.0f)};
                Vertices[VertexIndex * 6 + 2] = {V3(Vertex - HalfBorderThickness * Mid,   Z), V4(1.0f, 1.0f, 1.0f, 1.0f)};
                Vertices[VertexIndex * 6 + 3] = {V3(Vertex + HalfBorderThickness * Mid,   Z), V4(1.0f, 1.0f, 1.0f, 1.0f)};
                Vertices[VertexIndex * 6 + 4] = {V3(Vertex - HalfBorderThickness * PerpB, Z), V4(1.0f, 1.0f, 1.0f, 1.0f)};
                Vertices[VertexIndex * 6 + 5] = {V3(Vertex + HalfBorderThickness * Mid,   Z), V4(1.0f, 1.0f, 1.0f, 1.0f)};
            }
            
            if (VertexIndex == 0)
            {
                Vertices[VertexDrawCount - 2] = Vertices[0];
                Vertices[VertexDrawCount - 1] = Vertices[1];
            }
        }
        
        PushVertices(RenderGroup, Vertices, VertexDrawCount * sizeof(color_vertex), sizeof(color_vertex),
                     D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, Shader_Color);
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
    span<model_vertex> ModelVertices = {};
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
    
    PushVertices(RenderGroup, ModelVertices.Memory, sizeof(model_vertex) * ModelVertices.Count, sizeof(model_vertex), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, Shader_Model);
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
    v3 LightP = V3(-1.0f, -1.0f, -1.0f);
    v3 LightDirection = V3(1.0f, 1.0f, 1.0f);
    m4x4 Transform = ViewTransform(LightP, LightP + LightDirection) * OrthographicTransform(-1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 3.0f);
    
    render_group RenderGroup = {};
    RenderGroup.Arena = Context->Arena; //TODO: Fix this
    
    DrawWorld(&RenderGroup, Game, Assets, Context);
    
    SetDepthTest(true);
    ClearOutput(Game->ShadowMap);
    SetOutput(Game->ShadowMap);
    SetTransform(Transform);
    DrawRenderGroup(&RenderGroup, Assets);
    UnsetShadowMap();
    
    
    SetFrameBufferAsOutput();
    SetTransform(Game->WorldTransform);
    SetLightTransform(Transform);
    SetShadowMap(Game->ShadowMap);
    DrawRenderGroup(&RenderGroup, Assets);
}