static renderer_vertex_buffer
CreateModelVertexBuffer(allocator Allocator, char* Path, bool SwitchOrder)
{
    span<vertex> Vertices = LoadModel(Allocator, Path, SwitchOrder);
    
    //CalculateModelVertexNormals((tri*)Vertices.Memory, Vertices.Count / 3);
    
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

static font_asset
LoadFont(char* Path, f32 Size, memory_arena* Arena)
{
    Assert(Arena->Type == TRANSIENT);
    
    font_asset Result = {};
    
    int TextureWidth = 512;
    int TextureHeight = 512;
    
    span<u8> File = PlatformLoadFile(Arena, Path);
    u8* TempTexture = Alloc(Arena, TextureWidth * TextureHeight * sizeof(u8));
    
    stbtt_BakeFontBitmap(File.Memory, 0, Size, TempTexture, TextureWidth, TextureHeight, 0, 
                         ArrayCount(Result.BakedChars), Result.BakedChars);
    
    Result.Size = 0.45f * Size;
    Result.Texture = CreateTexture((u32*)TempTexture, TextureWidth, TextureHeight, 1);
    
    return Result;
}

static game_assets*
LoadAssets(allocator Allocator)
{
    game_assets* Assets = AllocStruct(Allocator.Permanent, game_assets);
    
    LoadShaders(Assets);
    
    Assets->ShadowMaps[0] = CreateShadowDepthTexture(4096, 4096);
    
    Assets->VertexBuffers[VertexBuffer_Castle] = CreateModelVertexBuffer(Allocator, "assets/models/castle.obj", true);
    Assets->VertexBuffers[VertexBuffer_Turret] = CreateModelVertexBuffer(Allocator, "assets/models/turret.obj", false);
    Assets->VertexBuffers[VertexBuffer_Mine]   = CreateModelVertexBuffer(Allocator, "assets/models/crate.obj", false);
    
    LoadSkybox(Assets);
    
    Assets->WaterReflection = CreateRenderOutput(2048, 2048);
    Assets->WaterRefraction = CreateRenderOutput(2048, 2048);
    Assets->WaterDuDv   = CreateTexture("assets/textures/water_dudv.png");
    Assets->WaterNormal = CreateTexture("assets/textures/water_normal.png");
    
    Assets->Button = CreateTexture("assets/textures/wenrexa_gui/Btn_TEST.png");
    Assets->Panel = CreateTexture("assets/textures/wenrexa_gui/Panel1_NoOpacity592x975px.png");
    Assets->Crystal = CreateTexture("assets/textures/crystal.png");
    
    Assets->Font = LoadFont("assets/fonts/TitilliumWeb-Regular.ttf", 75.0f, Allocator.Transient);
    
    Assets->ModelTextures.Ambient = CreateTexture("assets/textures/Carvalho-Munique_ambient.jpg");
    Assets->ModelTextures.Diffuse = CreateTexture("assets/textures/Carvalho-Munique_Diffuse.jpg");
    Assets->ModelTextures.Normal = CreateTexture("assets/textures/Carvalho-Munique_normal.jpg");
    Assets->ModelTextures.Specular = CreateTexture("assets/textures/Carvalho-Munique_specular.jpg");
    
    return Assets;
}