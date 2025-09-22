static renderer_vertex_buffer
CreateModelVertexBuffer(allocator Allocator, char* Path, bool SwitchOrder)
{
    span<vertex> Vertices = LoadModel(Allocator, Path, SwitchOrder);
    
    //CalculateModelVertexNormals((tri*)Vertices.Memory, Vertices.Count / 3);
    
    renderer_vertex_buffer VertexBuffer = PlatformCreateVertexBuffer(Vertices.Memory, Vertices.Count * sizeof(vertex),
                                                                     D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, sizeof(vertex));
    return VertexBuffer;
}

static void
LoadSkybox(game_assets* Assets, defense_assets* GameAssets)
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
        GameAssets->Skybox.Textures[TextureIndex] = LoadTexture(Assets, Paths[TextureIndex]);
    }
}

//TODO: Should this be called SetVertexBuffer


static void
LoadAssets(game_assets* Assets, allocator Allocator)
{
    Assert(Assets->Initialised == false);
    Assets->Initialised = true;
    
    Assets->MeshCount = 1;
    Assets->ModelCount = 1;
    Assets->TextureCount = 1;
    Assets->RenderOutputCount = 1;
    Assets->FontCount = 1;
    
    
    LoadShaders(Assets);
    
    if (!Assets->GameData)
    {
        Assets->GameData = AllocStruct(Allocator.Permanent, defense_assets);
    }
    
    defense_assets* GameAssets = (defense_assets*)Assets->GameData;
    
    GameAssets->ShadowMap = CreateShadowDepthTexture(Assets, 4096, 4096);
    
    LoadSkybox(Assets, GameAssets);
    
    GameAssets->WaterReflection = CreateRenderOutput(Assets, 2048, 2048);
    GameAssets->WaterRefraction = CreateRenderOutput(Assets, 2048, 2048);
    GameAssets->WaterDuDv   = LoadTexture(Assets, "assets/textures/water_dudv.png");
    GameAssets->WaterNormal = LoadTexture(Assets, "assets/textures/water_normal.png");
    
    GameAssets->Output1 = CreateRenderOutput(Assets, GlobalOutputWidth, GlobalOutputHeight);
    
    GameAssets->Button = LoadTexture(Assets, "assets/textures/wenrexa_gui/Btn_TEST.png");
    GameAssets->Panel = LoadTexture(Assets, "assets/textures/wenrexa_gui/Panel1_NoOpacity592x975px.png");
    GameAssets->Crystal = LoadTexture(Assets, "assets/textures/crystal.png");
    GameAssets->Target = LoadTexture(Assets, "assets/target.png");
    
    Assets->Font = LoadFont("assets/fonts/TitilliumWeb-Regular.ttf", 75.0f, Allocator.Transient);
    
    GameAssets->ModelTextures.Ambient = LoadTexture(Assets, "assets/textures/Carvalho-Munique_ambient.jpg");
    GameAssets->ModelTextures.Diffuse = LoadTexture(Assets, "assets/textures/Carvalho-Munique_Diffuse.jpg");
    GameAssets->ModelTextures.Diffuse = LoadTexture(Assets, "assets/textures/Texture_01.png");
    //Assets->ModelTextures.Diffuse = CreateTexture("assets/textures/Carvalho-Munique_Diffuse.jpg");
    //Assets->ModelTextures.Normal = CreateTexture("assets/textures/Carvalho-Munique_normal.jpg");
    //Assets->ModelTextures.Specular = CreateTexture("assets/textures/Carvalho-Munique_specular.jpg");
    
    GameAssets->BloomMipmaps[0] = CreateRenderOutput(Assets, 1024, 1024);
    GameAssets->BloomMipmaps[1] = CreateRenderOutput(Assets, 512, 512);
    GameAssets->BloomMipmaps[2] = CreateRenderOutput(Assets, 256, 256);
    GameAssets->BloomMipmaps[3] = CreateRenderOutput(Assets, 128, 128);
    GameAssets->BloomMipmaps[4] = CreateRenderOutput(Assets, 64, 64);
    GameAssets->BloomMipmaps[5] = CreateRenderOutput(Assets, 32, 32);
    GameAssets->BloomMipmaps[6] = CreateRenderOutput(Assets, 16, 16);
    GameAssets->BloomMipmaps[7] = CreateRenderOutput(Assets, 8, 8);
    
    GameAssets->BloomAccum = CreateRenderOutput(Assets, 1024, 1024);
    
    material DefaultMaterial = {};
    DefaultMaterial.DiffuseColor = V3(0, 0, 0);
    DefaultMaterial.SpecularFocus = 100.0f;
    DefaultMaterial.SpecularColor = V3(1, 1, 1);
    
    Assert(Assets->MaterialCount == 0);
    Assets->Materials[Assets->MaterialCount++] = DefaultMaterial;
    
    LoadObjects(Assets, "assets/models/castle.obj");
    SetModelLocalTransform(Assets, "Castle", ModelRotateTransform() * ScaleTransform(0.03f, 0.03f, 0.03f));
    
    LoadObjects(Assets, "assets/models/turret.obj");
    SetModelLocalTransform(Assets, "Turret", ModelRotateTransform() * ScaleTransform(0.03f, 0.03f, 0.03f));
    
    LoadMaterials(Assets, "assets/models/Environment.mtl", "Environment.mtl");
    LoadObjects(Assets, "assets/models/Environment.obj");
    LoadObjects(Assets, "assets/models/hexagon_new.obj");
    LoadObjects(Assets, "assets/models/wall.obj");
    LoadObjects(Assets, "assets/models/stairs.obj");
    LoadObjects(Assets, "assets/models/modular_wood.obj");
    LoadObjects(Assets, "assets/models/house.obj");
    
    SetModelLocalTransform(Assets, "2FPinkPlant_Plane.084", TranslateTransform(-5.872f, 0.0f, -23.1f) * 
                           ModelRotateTransform() * ScaleTransform(1.0f));
    
    SetModelLocalTransform(Assets, "Rock6_Cube.014", TranslateTransform(-25.553f, 0.0f, -21.882f) * 
                           ModelRotateTransform() * ScaleTransform(0.5f));
    
    SetModelLocalTransform(Assets, "Bush2_Cube.046", TranslateTransform(2.067f, 0.0f, 0.0f) * 
                           ModelRotateTransform() * ScaleTransform(0.15f));
    
    SetModelLocalTransform(Assets, "RibbonPlant2_Plane.079", TranslateTransform(1.82f, 0.0f, -13.517f) * 
                           ModelRotateTransform() * ScaleTransform(0.2f));
    
    SetModelLocalTransform(Assets, "GrassPatch101_Plane.040", TranslateTransform(-5.277f, 0.0f, -40.195f) * 
                           ModelRotateTransform() * ScaleTransform(0.6f));
    
    SetModelLocalTransform(Assets, "Rock2_Cube.009", TranslateTransform(-52.7, 0.0f, -9.509f) *
                           ModelRotateTransform() * ScaleTransform(0.1f));
    
    SetModelLocalTransform(Assets, "Circle", ModelRotateTransform());
    
    SetModelLocalTransform(Assets, "Wall_01", ScaleTransform(0.02f) * ModelRotateTransform());
    SetModelLocalTransform(Assets, "Stairs_01", ScaleTransform(0.01f) * ModelRotateTransform());
    SetModelLocalTransform(Assets, "ModularWood_01", ScaleTransform(0.01f) * ModelRotateTransform() * RotateTransform(0.5f * Pi));
    SetModelLocalTransform(Assets, "House_01", ScaleTransform(0.01f) * ModelRotateTransform());
    
    GameAssets->WorldRegion = GetModelHandle(Assets, "Circle");
    GameAssets->PinkFlower = GetModelHandle(Assets, "2FPinkPlant_Plane.084");
    GameAssets->Bush = GetModelHandle(Assets, "Bush2_Cube.046");
    GameAssets->RibbonPlant = GetModelHandle(Assets, "RibbonPlant2_Plane.079");
    GameAssets->Grass = GetModelHandle(Assets, "GrassPatch101_Plane.040");
    GameAssets->Rock = GetModelHandle(Assets, "Rock2_Cube.009");
    GameAssets->Paving = GetModelHandle(Assets, "Rock2_Cube.009");
    GameAssets->ModularWood = GetModelHandle(Assets, "ModularWood_01");
    GameAssets->House = GetModelHandle(Assets, "House_01");
    GameAssets->Castle = GetModelHandle(Assets, "Castle");
    GameAssets->Turret = GetModelHandle(Assets, "Turret");
    
    gui_vertex Vertices[6] = {
        {V2(-1, -1), {}, V2(0, 1)},
        {V2(-1, 1), {}, V2(0, 0)},
        {V2(1, 1), {}, V2(1, 0)},
        {V2(1, 1), {}, V2(1, 0)},
        {V2(1, -1), {}, V2(1, 1)},
        {V2(-1, -1), {}, V2(0, 1)}
    };
    
    GameAssets->GUIWholeScreen = CreateVertexBuffer(Assets, Vertices, ArrayCount(Vertices) * sizeof(gui_vertex),
                                                    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, sizeof(gui_vertex));
    
    GameAssets->RegionOutline = CreateRegionOutlineMesh(Assets);
    
#define LOG 1
#if LOG == 1
    for (u64 ModelIndex = 0; ModelIndex < Assets->ModelCount; ModelIndex++)
    {
        Log("Loaded model %s\n", Assets->Models[ModelIndex].Name.Text);
    }
#endif
    
    // Internal materials
    material DirtMaterial = {};
    DirtMaterial.Library = String("TowerDefense");
    DirtMaterial.Name = String("Dirt");
    DirtMaterial.DiffuseColor = 0.5f * V3(0.38f, 0.31f, 0.22f);
    DirtMaterial.SpecularFocus = 100.0f;
    DirtMaterial.SpecularColor = V3(1, 1, 1);
    
    Assets->Materials[Assets->MaterialCount++] = DirtMaterial;
    Assert(Assets->MaterialCount < ArrayCount(Assets->Materials));
}

static game_assets*
LoadServerAssets(allocator Allocator)
{
    game_assets* Assets = AllocStruct(Allocator.Permanent, game_assets);
    
    Assets->Allocator = Allocator;
    
    LoadObjects(Assets, "assets/models/Environment.obj");
    LoadObjects(Assets, "assets/models/hexagon.obj");
    
    //TODO: Remove duplicated code
    SetModelLocalTransform(Assets, "2FPinkPlant_Plane.084", TranslateTransform(-5.872f, 0.0f, -23.1f) * 
                           ModelRotateTransform() * ScaleTransform(0.5f));
    
    SetModelLocalTransform(Assets, "Rock6_Cube.014", TranslateTransform(-25.553f, 0.0f, -21.882f) * 
                           ModelRotateTransform() * ScaleTransform(0.002f, 0.002f, 0.002f));
    
    SetModelLocalTransform(Assets, "Bush2_Cube.046", TranslateTransform(2.067f, 0.0f, 0.0f) * 
                           ModelRotateTransform() * ScaleTransform(0.01f, 0.01f, 0.01f));
    
    SetModelLocalTransform(Assets, "RibbonPlant2_Plane.079", TranslateTransform(1.82f, 0.0f, -13.517f) * 
                           ModelRotateTransform() * ScaleTransform(0.01f, 0.01f, 0.01f));
    
    SetModelLocalTransform(Assets, "GrassPatch101_Plane.040", TranslateTransform(-5.277f, 0.0f, -40.195f) * 
                           ModelRotateTransform() * ScaleTransform(0.01f, 0.01f, 0.01f));
    
    SetModelLocalTransform(Assets, "Hexagon", TranslateTransform(0.0f, -5.0f, 0.0f) * 
                           ModelRotateTransform() * ScaleTransform(0.09f, 0.09f, 0.09f));
    
    return Assets;
}

static void
ResizeAssets(game_assets* Assets)
{
    if (Assets && Assets->GameData)
    {
        defense_assets* GameAssets = (defense_assets*) Assets->GameData;
        render_output* Output = Assets->RenderOutputs + GameAssets->Output1;
        Delete(Output);
        *Output = PlatformCreateRenderOutput(GlobalOutputWidth, GlobalOutputHeight);
    }
}

static void
LoadObjectsFromFileForServer(game_assets* Assets, allocator Allocator, char* Path)
{
    span<u8> File = LoadFile(Allocator.Transient, Path);
    file_reader Reader = {(char*)File.Memory, (char*)File.Memory + File.Count};
    
    model* CurrentModel = 0;
    string CurrentObjectName = {};
    
    //For file
    dynamic_array<v3> Positions = {};
    Positions.Arena = Allocator.Transient;
    
    //For current mesh
    dynamic_array<obj_file_face> Faces = {};
    Faces.Arena = Allocator.Transient;
    
    while (!AtEnd(&Reader))
    {
        if (Consume(&Reader, "#")) {} //Comment
        else if (Consume(&Reader, "mtllib ")) {} //New material
        else if (Consume(&Reader, "o ")) //Object name -> new mesh then new model?
        {
            //TODO: This code is copied from below (fix)
            if (Faces.Count > 0)
            {
                mesh_index MeshIndex = (Assets->MeshCount++);
                mesh* Mesh = Assets->Meshes + MeshIndex;
                Assert(Assets->MeshCount < ArrayCount(Assets->Meshes));
                
                Mesh->Triangles    = CreateMeshTriangles(Positions, Faces, Allocator.Permanent);
                
                CurrentModel->Meshes[CurrentModel->MeshCount++] = MeshIndex;
                Assert(CurrentModel->MeshCount < ArrayCount(CurrentModel->Meshes));
                
                Faces.Count = 0;
            }
            
            CurrentModel = Assets->Models + (Assets->ModelCount++);
            Assert(Assets->ModelCount <= ArrayCount(Assets->Models));
            
            string Name = ParseString(&Reader);
            CurrentModel->Name = CopyString(Allocator.Permanent, Name);
        }
        else if (Consume(&Reader, "v ")) //Vertex position
        {
            v3 P = ParseVector3(&Reader);
            Append(&Positions, P);
        }
        else if (Consume(&Reader, "vn ")) {} //Vertex normal
        else if (Consume(&Reader, "vt ")) {} //Vertex texcoord
        else if (Consume(&Reader, "s "))  {} //Smooth shading (ignored)
        else if (Consume(&Reader, "usemtl ")) //Material name -> New mesh?
        {
            if (Faces.Count > 0)
            {
                mesh_index MeshIndex = (Assets->MeshCount++);
                mesh* Mesh = Assets->Meshes + MeshIndex;
                Assert(Assets->MeshCount < ArrayCount(Assets->Meshes));
                
                Mesh->Triangles    = CreateMeshTriangles(Positions, Faces, Allocator.Permanent);
                
                CurrentModel->Meshes[CurrentModel->MeshCount++] = MeshIndex;
                Assert(CurrentModel->MeshCount < ArrayCount(CurrentModel->Meshes));
                
                Faces.Count = 0;
            }
        }
        else if (Consume(&Reader, "f "))
        {
            obj_file_face Face;
            Face.Vertices[0] = ParseVertex(&Reader);
            Consume(&Reader, " ");
            Face.Vertices[1] = ParseVertex(&Reader);
            Consume(&Reader, " ");
            Face.Vertices[2] = ParseVertex(&Reader);
            Append(&Faces, Face);
        }
        else
        {
            Assert(0);
        }
        
        NextLine(&Reader);
    }
    
    //TODO: This code is copied from above (fix)
    if (Faces.Count > 0)
    {
        mesh_index MeshIndex = (Assets->MeshCount++);
        mesh* Mesh = Assets->Meshes + MeshIndex;
        Assert(Assets->MeshCount < ArrayCount(Assets->Meshes));
        
        Mesh->Triangles    = CreateMeshTriangles(Positions, Faces, Allocator.Permanent);
        
        CurrentModel->Meshes[CurrentModel->MeshCount++] = MeshIndex;
        Assert(CurrentModel->MeshCount < ArrayCount(CurrentModel->Meshes));
        
        Faces.Count = 0;
    }
}

static span<v2>
GetHexagonVertexPositions(v2 P, f32 Radius, memory_arena* Arena)
{
    span<v2> Result = AllocSpan(Arena, v2, 6);
    
    v2 Offsets[6] = {
        {0.0f, 1.0f},
        {0.86603f, 0.5f},
        {0.86603f, -0.5f},
        {0.0f, -1.0f},
        {-0.86603f, -0.5f},
        {-0.86603f, 0.5f}
    };
    
    for (u64 I = 0; I < ArrayCount(Offsets); I++)
    {
        Result[I] = P + Radius * Offsets[I];
    }
    
    return Result;
}

static vertex_buffer_index
CreateRegionOutlineMesh(game_assets* Assets)
{
    span<v2> HexagonVertices = GetHexagonVertexPositions(V2(0, 0), 1.0f, Assets->Allocator.Transient);
    
    v4 Color = V4(1.1f, 1.1f, 1.1f, 1.0f); 
    v3 Normal = V3(0.0f, 0.0f, -1.0f);
    
    u64 VertexCount = 6 * 6 + 2;
    vertex* Vertices = AllocArray(Assets->Allocator.Transient, vertex, VertexCount);
    
    for (int HexagonVertexIndex = 0; HexagonVertexIndex < 6; HexagonVertexIndex++)
    {
        v2 Vertex = HexagonVertices[HexagonVertexIndex];
        v2 PrevVertex = HexagonVertices[Mod(HexagonVertexIndex - 1, 6)];
        v2 NextVertex = HexagonVertices[Mod(HexagonVertexIndex + 1, 6)];
        
        v2 PerpA = UnitV(Perp(Vertex - PrevVertex));
        v2 PerpB = UnitV(Perp(NextVertex - Vertex));
        v2 Mid = UnitV(PerpA + PerpB);
        
        f32 SinAngle = Det(M2x2(UnitV(Vertex - PrevVertex), UnitV(NextVertex - Vertex)));
        
        f32 HalfBorderThickness = 0.05f;
        
        //If concave
        if (SinAngle < 0.0f)
        {
            Vertices[HexagonVertexIndex * 6 + 0] = {V3(Vertex - HalfBorderThickness * Mid,   0), Normal, Color};
            Vertices[HexagonVertexIndex * 6 + 1] = {V3(Vertex + HalfBorderThickness * PerpA, 0), Normal, Color};
            Vertices[HexagonVertexIndex * 6 + 2] = {V3(Vertex - HalfBorderThickness * Mid,   0), Normal, Color};
            Vertices[HexagonVertexIndex * 6 + 3] = {V3(Vertex + HalfBorderThickness * Mid,   0), Normal, Color};
            Vertices[HexagonVertexIndex * 6 + 4] = {V3(Vertex - HalfBorderThickness * Mid,   0), Normal, Color};
            Vertices[HexagonVertexIndex * 6 + 5] = {V3(Vertex + HalfBorderThickness * PerpB, 0), Normal, Color};
        }
        else
        {
            Vertices[HexagonVertexIndex * 6 + 0] = {V3(Vertex - HalfBorderThickness * PerpA, 0), Normal, Color};
            Vertices[HexagonVertexIndex * 6 + 1] = {V3(Vertex + HalfBorderThickness * Mid,   0), Normal, Color};
            Vertices[HexagonVertexIndex * 6 + 2] = {V3(Vertex - HalfBorderThickness * Mid,   0), Normal, Color};
            Vertices[HexagonVertexIndex * 6 + 3] = {V3(Vertex + HalfBorderThickness * Mid,   0), Normal, Color};
            Vertices[HexagonVertexIndex * 6 + 4] = {V3(Vertex - HalfBorderThickness * PerpB, 0), Normal, Color};
            Vertices[HexagonVertexIndex * 6 + 5] = {V3(Vertex + HalfBorderThickness * Mid,   0), Normal, Color};
        }
        
        if (HexagonVertexIndex == 0)
        {
            Vertices[VertexCount - 2] = Vertices[0];
            Vertices[VertexCount - 1] = Vertices[1];
        }
    }
    
    vertex_buffer_index Result = CreateVertexBuffer(Assets, Vertices, VertexCount * sizeof(vertex), 
                                                    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, sizeof(vertex));
    
    return Result;
}