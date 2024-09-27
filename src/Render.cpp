/*
static void
DrawWater(render_group* RenderGroup, game_state* Game)
{
    SetShader(WaterShader);
    //SetShaderTime((f32)Game->Time);
    DrawTexture(V3(-0.8f, -0.8f, 0.0f), V3(0.8f, 0.8f, 0.0f));
}
*/

/*
static void
PushWater(render_command_group* Group, game_state* Game)
{
    Push(Group, CommandSetShader(WaterShader));
    
    v3 P0 = V3(-0.8f, -0.8f, 0.0f);
    v3 P1 = V3(0.8f, 0.8f, 0.0f);
    
    texture_vertex VertexData[4] = {
        {V3(P0.X, P0.Y, P0.Z), V2(UV0.X, UV0.Y)},
        {V3(P0.X, P1.Y, P0.Z), V2(UV0.X, UV1.Y)},
        {V3(P1.X, P0.Y, P0.Z), V2(UV1.X, UV0.Y)},
        {V3(P1.X, P1.Y, P0.Z), V2(UV1.X, UV1.Y)}
    };
    
    Push(Group, CommandDraw((f32*)VertexData, sizeof(VertexData), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, sizeof(texture_vertex)));
}
*/

static void
DrawBackground(render_group* RenderGroup)
{
    PushTexturedRect(RenderGroup, BackgroundTexture, V3(0.0f, 0.0f, 0.0f), V3(1.0f, 0.5625f, 0.0f));
}

static void
DrawWorldRegion(render_group* RenderGroup, game_state* Game, world* World, world_region* Region, v4 Color)
{
    if (Region->VertexCount == 0)
    {
        return;
    }
    
    u32 TriangleCount = Region->VertexCount;
    span<triangle> Triangles = AllocSpan(RenderGroup->Arena, triangle, TriangleCount);
    
    f32 Z = Region->IsWater ? 0.001f : 0.0f;
    
    for (u32 TriangleIndex = 0; TriangleIndex < TriangleCount; TriangleIndex++)
    {
        triangle* Tri = Triangles + TriangleIndex;
        
        //TODO: Use functions
        Tri->Vertices[0].P = V3(Region->Center, Z);
        Tri->Vertices[0].Col = Color;
        
        Tri->Vertices[1].P = V3(GetVertex(Region, TriangleIndex), Z);
        Tri->Vertices[1].Col = Color;
        
        Tri->Vertices[2].P = V3(GetVertex(Region, TriangleIndex + 1), Z);
        Tri->Vertices[2].Col = Color;
    }
    
    PushVertices(RenderGroup, Triangles.Memory, TriangleCount * sizeof(triangle), sizeof(tri_vertex), 
                 D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, Shader_Color);
    
    //Outline Z (slightly higher)
    Z = -0.001f;
    
    bool DrawOutline = (Region->IsWater == false);
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
        
        v4 Color = Region->Color;
        if (Hovering)
        {
            Color.RGB = 0.8f * Color.RGB;
        }
        
        DrawWorldRegion(RenderGroup, Game, &Game->GlobalState.World, Region, Color);
        
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
        
        DrawTower(RenderGroup, Game, Tower->Type, V3(Tower->P, 0.0f), Color, Tower->Rotation);
    }
}

static void
DrawWorld(render_group* RenderGroup, game_state* Game, render_context* Context)
{
    //SetDepthTest(false);
    //DrawWater(Game);
    
    //SetDepthTest(true);
    
    if (Game->ShowBackground)
    {
        DrawBackground(RenderGroup);
    }
    
    //SetDepthTest(false);
    DrawRegions(RenderGroup, Game, Context);
    
    DrawTowers(RenderGroup, Game, Context);
}

static void
RenderWorld(game_state* Game, game_assets* Assets, render_context* Context)
{
    v3 LightP = V3(-1.0f, -1.0f, -1.0f);
    v3 LightDirection = V3(1.0f, 1.0f, 1.0f);
    m4x4 Transform = ViewTransform(LightP, LightP + LightDirection) * OrthographicTransform(-1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 3.0f);
    
    render_group RenderGroup = {};
    RenderGroup.Arena = Context->Arena; //TODO: Fix this
    
    DrawWorld(&RenderGroup, Game, Context);
    
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