
static vertex_buffer
CreateModelVertexBuffer(allocator Allocator, char* Path, bool SwitchOrder)
{
    span<model_vertex> Vertices = LoadModel(Allocator, Path, SwitchOrder);
    
    vertex_buffer Result = {Vertices.Memory, Vertices.Count * sizeof(model_vertex)};
    
    return Result;
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
    
    //TODO: Don't do this
    Assets->Shaders[Shader_Color] = ColorShader;
    Assets->Shaders[Shader_Font] = FontShader;
    Assets->Shaders[Shader_Texture] = TextureShader;
    Assets->Shaders[Shader_Water] = WaterShader;
    Assets->Shaders[Shader_Model] = ModelShader;
    //Assets->Shaders[Shader_Background] = BackgroundShader;
    
    LoadSkybox(Assets);
    
    return Assets;
}

