static renderer_vertex_buffer
CreateModelVertexBuffer(allocator Allocator, char* Path, bool SwitchOrder)
{
    span<vertex> Vertices = LoadModel(Allocator, Path, SwitchOrder);
    
    CalculateModelVertexNormals((tri*)Vertices.Memory, Vertices.Count / 3);
    
    renderer_vertex_buffer VertexBuffer = CreateVertexBuffer(Vertices.Memory, Vertices.Count * sizeof(vertex),
                                                             D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, sizeof(vertex));
    return VertexBuffer;
}

static void
LoadSkybox(game_assets* Assets)
{
    char* Paths[6] = {
        "assets/textures/skybox/clouds1_up.bmp",
        "assets/textures/skybox/clouds1_north.bmp",
        "assets/textures/skybox/clouds1_east.bmp",
        "assets/textures/skybox/clouds1_south.bmp",
        "assets/textures/skybox/clouds1_west.bmp",
        "assets/textures/skybox/clouds1_down.bmp"
    };
    
    for (int TextureIndex = 0; TextureIndex < 6; TextureIndex++)
    {
        Assets->Skybox.Textures[TextureIndex] = CreateTexture(Paths[TextureIndex]);
    }
}

static game_assets*
LoadAssets(allocator Allocator)
{
    game_assets* Assets = AllocStruct(Allocator.Permanent, game_assets);
    
    LoadShaders(Assets);
    
    Assets->ShadowMaps[0] = CreateShadowDepthTexture(4096, 4096);
    
    Assets->VertexBuffers[VertexBuffer_Castle] = CreateModelVertexBuffer(Allocator, "assets/models/castle.obj", false);
    Assets->VertexBuffers[VertexBuffer_Turret] = CreateModelVertexBuffer(Allocator, "assets/models/turret.obj", true);
    Assets->VertexBuffers[VertexBuffer_Cube]   = CreateModelVertexBuffer(Allocator, "assets/models/cube.obj", true);
    
    LoadSkybox(Assets);
    
    Assets->WaterReflection = CreateRenderOutput(2048, 2048);
    Assets->WaterRefraction = CreateRenderOutput(2048, 2048);
    Assets->WaterDuDv   = CreateTexture("assets/textures/water_dudv.png");
    Assets->WaterNormal = CreateTexture("assets/textures/water_normal.png");
    
    return Assets;
}

