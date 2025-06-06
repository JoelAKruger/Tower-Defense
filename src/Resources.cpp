void LoadMaterialsFromFile(game_assets* Assets, allocator Allocator, char* Path, char* Library);
void LoadObjectsFromFile(game_assets* Assets, allocator Allocator, char* Path);
void LoadObjectsFromFileForServer(game_assets* Assets, allocator Allocator, char* Path);

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
    
    span<u8> File = LoadFile(Arena, Path);
    u8* TempTexture = Alloc(Arena, TextureWidth * TextureHeight * sizeof(u8));
    
    stbtt_BakeFontBitmap(File.Memory, 0, Size, TempTexture, TextureWidth, TextureHeight, 0, 
                         ArrayCount(Result.BakedChars), Result.BakedChars);
    
    Result.Size = 0.45f * Size;
    Result.Texture = CreateTexture((u32*)TempTexture, TextureWidth, TextureHeight, 1);
    
    return Result;
}

static renderer_vertex_buffer*
FindVertexBuffer(game_assets* Assets, char* Description_)
{
    string Description = String(Description_);
    renderer_vertex_buffer* Result = 0;
    
    for (u64 VertexBufferIndex = 0; VertexBufferIndex < Assets->VertexBufferCount; VertexBufferIndex++)
    {
        renderer_vertex_buffer* VertexBuffer = Assets->VertexBuffersNew + VertexBufferIndex;
        
        if (StringsAreEqual(VertexBuffer->Description, Description))
        {
            Result = VertexBuffer;
            break;
        }
    }
    
    return Result;
}

static model* 
FindModel(game_assets* Assets, char* Description_)
{
    string Description = String(Description_);
    model* Result = 0;
    
    for (u64 ModelIndex = 0; ModelIndex < Assets->ModelCount; ModelIndex++)
    {
        model* Model = Assets->Models + ModelIndex;
        
        if (StringsAreEqual(Model->Name, Description))
        {
            Result = Model;
            break;
        }
    }
    
    return Result;
}

//TODO: Should this be called SetVertexBuffer
static void
LoadVertexBuffer(game_assets* Assets, char* Description, renderer_vertex_buffer Buffer)
{
    renderer_vertex_buffer* VertexBuffer = FindVertexBuffer(Assets, Description);
    
    if (!VertexBuffer)
    {
        VertexBuffer = Assets->VertexBuffersNew + (Assets->VertexBufferCount++);
        Assert(Assets->VertexBufferCount <= ArrayCount(Assets->VertexBuffersNew));
    }
    
    *VertexBuffer = Buffer;
    VertexBuffer->Description = String(Description);
}

static game_assets*
LoadAssets(allocator Allocator)
{
    game_assets* Assets = AllocStruct(Allocator.Permanent, game_assets);
    
    LoadShaders(Assets);
    
    Assets->ShadowMaps[0] = CreateShadowDepthTexture(4096, 4096);
    
    LoadVertexBuffer(Assets, "Castle", CreateModelVertexBuffer(Allocator, "assets/models/castle.obj", true));
    LoadVertexBuffer(Assets, "Turret", CreateModelVertexBuffer(Allocator, "assets/models/turret.obj", false));
    LoadVertexBuffer(Assets, "Mine",   CreateModelVertexBuffer(Allocator, "assets/models/crate.obj", false));
    
    LoadSkybox(Assets);
    
    Assets->WaterReflection = CreateRenderOutput(2048, 2048);
    Assets->WaterRefraction = CreateRenderOutput(2048, 2048);
    Assets->WaterDuDv   = CreateTexture("assets/textures/water_dudv.png");
    Assets->WaterNormal = CreateTexture("assets/textures/water_normal.png");
    
    Assets->Output1 = CreateRenderOutput(GlobalOutputWidth, GlobalOutputHeight);
    
    Assets->Button = CreateTexture("assets/textures/wenrexa_gui/Btn_TEST.png");
    Assets->Panel = CreateTexture("assets/textures/wenrexa_gui/Panel1_NoOpacity592x975px.png");
    Assets->Crystal = CreateTexture("assets/textures/crystal.png");
    Assets->Target = CreateTexture("assets/target.png");
    
    Assets->Font = LoadFont("assets/fonts/TitilliumWeb-Regular.ttf", 75.0f, Allocator.Transient);
    
    Assets->ModelTextures.Ambient = CreateTexture("assets/textures/Carvalho-Munique_ambient.jpg");
    Assets->ModelTextures.Diffuse = CreateTexture("assets/textures/Carvalho-Munique_Diffuse.jpg");
    Assets->ModelTextures.Normal = CreateTexture("assets/textures/Carvalho-Munique_normal.jpg");
    Assets->ModelTextures.Specular = CreateTexture("assets/textures/Carvalho-Munique_specular.jpg");
    
    Assets->BloomMipmaps[0] = CreateRenderOutput(1024, 1024);
    Assets->BloomMipmaps[1] = CreateRenderOutput(512, 512);
    Assets->BloomMipmaps[2] = CreateRenderOutput(256, 256);
    Assets->BloomMipmaps[3] = CreateRenderOutput(128, 128);
    Assets->BloomMipmaps[4] = CreateRenderOutput(64, 64);
    Assets->BloomMipmaps[5] = CreateRenderOutput(32, 32);
    Assets->BloomMipmaps[6] = CreateRenderOutput(16, 16);
    Assets->BloomMipmaps[7] = CreateRenderOutput(8, 8);
    
    Assets->BloomAccum = CreateRenderOutput(1024, 1024);
    
    material DefaultMaterial = {};
    DefaultMaterial.DiffuseColor = V3(0, 0, 0);
    DefaultMaterial.SpecularFocus = 100.0f;
    DefaultMaterial.SpecularColor = V3(1, 1, 1);
    
    Assert(Assets->MaterialCount == 0);
    Assets->Materials[Assets->MaterialCount++] = DefaultMaterial;
    
    LoadObjectsFromFile(Assets, Allocator, "assets/models/castle.obj");
    SetModelLocalTransform(Assets, "Castle", ModelRotateTransform() * ScaleTransform(0.03f, 0.03f, 0.03f));
    
    LoadObjectsFromFile(Assets, Allocator, "assets/models/turret.obj");
    SetModelLocalTransform(Assets, "Turret", ModelRotateTransform() * ScaleTransform(0.03f, 0.03f, 0.03f));
    
    LoadMaterialsFromFile(Assets, Allocator, "assets/models/Environment.mtl", "Environment.mtl");
    LoadObjectsFromFile(Assets, Allocator, "assets/models/Environment.obj");
    LoadObjectsFromFile(Assets, Allocator, "assets/models/hexagon.obj");
    
    SetModelLocalTransform(Assets, "2FPinkPlant_Plane.084", TranslateTransform(-5.872f, 0.0f, -23.1f) * 
                           ModelRotateTransform() * ScaleTransform(0.01f, 0.01f, 0.01f));
    
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
    
    gui_vertex Vertices[6] = {
        {V2(-1, -1), {}, V2(0, 1)},
        {V2(-1, 1), {}, V2(0, 0)},
        {V2(1, 1), {}, V2(1, 0)},
        {V2(1, 1), {}, V2(1, 0)},
        {V2(1, -1), {}, V2(1, 1)},
        {V2(-1, -1), {}, V2(0, 1)}
    };
    
    renderer_vertex_buffer WholeScreen = CreateVertexBuffer((f32*)Vertices, ArrayCount(Vertices) * sizeof(gui_vertex),
                                                            D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, sizeof(gui_vertex));
    LoadVertexBuffer(Assets, "GUIWholeScreen", WholeScreen);
    
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
    
    return Assets;
}

static game_assets*
LoadServerAssets(allocator Allocator)
{
    game_assets* Assets = AllocStruct(Allocator.Permanent, game_assets);
    
    LoadObjectsFromFile(Assets, Allocator, "assets/models/Environment.obj");
    LoadObjectsFromFile(Assets, Allocator, "assets/models/hexagon.obj");
    
    SetModelLocalTransform(Assets, "2FPinkPlant_Plane.084", TranslateTransform(-5.872f, 0.0f, -23.1f) * 
                           ModelRotateTransform() * ScaleTransform(0.01f, 0.01f, 0.01f));
    
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
    if (Assets)
    {
        Delete(&Assets->Output1);
        Assets->Output1 = CreateRenderOutput(GlobalOutputWidth, GlobalOutputHeight);
    }
}

static renderer_vertex_buffer
CreateMeshVertexBuffer(dynamic_array<v3> Positions, dynamic_array<v3> Normals, dynamic_array<v2> TexCoords, dynamic_array<obj_file_face> Faces, allocator Allocator)
{
    u64 VertexCount = Faces.Count * 3 * 2;
    vertex* Vertices = AllocArray(Allocator.Transient, vertex, VertexCount);
    
    for (u32 FaceIndex = 0; FaceIndex < Faces.Count; FaceIndex++)
    {
        obj_file_face* Face = Faces + FaceIndex;
        
        Vertices[FaceIndex * 6 + 0].P = Positions[Face->Vertices[0].PositionIndex - 1];
        Vertices[FaceIndex * 6 + 1].P = Positions[Face->Vertices[1].PositionIndex - 1];
        Vertices[FaceIndex * 6 + 2].P = Positions[Face->Vertices[2].PositionIndex - 1];
        
        Vertices[FaceIndex * 6 + 0].Normal = Normals[Face->Vertices[0].NormalIndex - 1];
        Vertices[FaceIndex * 6 + 1].Normal = Normals[Face->Vertices[1].NormalIndex - 1];
        Vertices[FaceIndex * 6 + 2].Normal = Normals[Face->Vertices[2].NormalIndex - 1];
        
        Vertices[FaceIndex * 6 + 3].P = Positions[Face->Vertices[0].PositionIndex - 1];
        Vertices[FaceIndex * 6 + 5].P = Positions[Face->Vertices[1].PositionIndex - 1];
        Vertices[FaceIndex * 6 + 4].P = Positions[Face->Vertices[2].PositionIndex - 1];
        
        Vertices[FaceIndex * 6 + 3].Normal = -1.0f * Normals[Face->Vertices[0].NormalIndex - 1];
        Vertices[FaceIndex * 6 + 5].Normal = -1.0f * Normals[Face->Vertices[1].NormalIndex - 1];
        Vertices[FaceIndex * 6 + 4].Normal = -1.0f * Normals[Face->Vertices[2].NormalIndex - 1];
        
        if (Face->Vertices[0].TexCoordIndex && Face->Vertices[1].TexCoordIndex && Face->Vertices[2].TexCoordIndex)
        {
            Vertices[FaceIndex * 6 + 0].UV = TexCoords[Face->Vertices[0].TexCoordIndex - 1];
            Vertices[FaceIndex * 6 + 1].UV = TexCoords[Face->Vertices[1].TexCoordIndex - 1];
            Vertices[FaceIndex * 6 + 2].UV = TexCoords[Face->Vertices[2].TexCoordIndex - 1];
            
            Vertices[FaceIndex * 6 + 3].UV = TexCoords[Face->Vertices[0].TexCoordIndex - 1];
            Vertices[FaceIndex * 6 + 5].UV = TexCoords[Face->Vertices[1].TexCoordIndex - 1];
            Vertices[FaceIndex * 6 + 4].UV = TexCoords[Face->Vertices[2].TexCoordIndex - 1];
        }
    }
    
    renderer_vertex_buffer Result = CreateVertexBuffer(Vertices, VertexCount * sizeof(vertex), 
                                                       D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, sizeof(vertex));
    return Result;
}

static span<triangle>
CreateMeshTriangles(dynamic_array<v3> Positions, dynamic_array<obj_file_face> Faces, allocator Allocator)
{
    u64 TriCount = Faces.Count * 2;
    triangle* Triangles = AllocArray(Allocator.Permanent, triangle, TriCount);
    
    int TriIndex = 0;
    for (u32 FaceIndex = 0; FaceIndex < Faces.Count; FaceIndex++)
    {
        obj_file_face* Face = Faces + FaceIndex;
        
        Triangles[TriIndex++] = {
            Positions[Face->Vertices[0].PositionIndex - 1],
            Positions[Face->Vertices[1].PositionIndex - 1],
            Positions[Face->Vertices[2].PositionIndex - 1]
        };
        
        Triangles[TriIndex++] = {
            Positions[Face->Vertices[0].PositionIndex - 1],
            Positions[Face->Vertices[1].PositionIndex - 1],
            Positions[Face->Vertices[2].PositionIndex - 1]
        };
    }
    
    Assert(TriIndex == TriCount);
    
    return {.Memory = Triangles, .Count = TriCount};
}

static void
LoadObjectsFromFile(game_assets* Assets, allocator Allocator, char* Path)
{
    span<u8> File = LoadFile(Allocator.Transient, Path);
    file_reader Reader = {(char*)File.Memory, (char*)File.Memory + File.Count};
    
    string CurrentMaterialLibrary = {};
    string CurrentMaterialName = {};
    
    model* CurrentModel = 0;
    string CurrentObjectName = {};
    
    //For file
    dynamic_array<v3> Positions = {};
    Positions.Arena = Allocator.Transient;
    
    dynamic_array<v3> Normals = {};
    Normals.Arena = Allocator.Transient;
    
    dynamic_array<v2> TexCoords = {};
    TexCoords.Arena = Allocator.Transient;
    
    //For current mesh
    dynamic_array<obj_file_face> Faces = {};
    Faces.Arena = Allocator.Transient;
    
    while (!AtEnd(&Reader))
    {
        if (Consume(&Reader, "#")) //Comment
        {
        }
        
        else if (Consume(&Reader, "mtllib ")) //New material
        {
            string Library = ParseString(&Reader);
            CurrentMaterialLibrary = CopyString(Allocator.Permanent, Library);
        }
        
        else if (Consume(&Reader, "o ")) //Object name -> new mesh then new model?
        {
            //TODO: This code is copied from below (fix)
            if (Faces.Count > 0)
            {
                mesh_index MeshIndex = (Assets->MeshCount++);
                mesh* Mesh = Assets->Meshes + MeshIndex;
                Assert(Assets->MeshCount < ArrayCount(Assets->Meshes));
                
                Mesh->MaterialLibrary = CurrentMaterialLibrary;
                Mesh->MaterialName = CurrentMaterialName;
                Mesh->VertexBuffer = CreateMeshVertexBuffer(Positions, Normals, TexCoords, Faces, Allocator);
                Mesh->Triangles    = CreateMeshTriangles(Positions, Faces, Allocator);
                
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
        
        else if (Consume(&Reader, "vn ")) //Vertex normal
        {
            v3 N = ParseVector3(&Reader);
            Append(&Normals, N);
        }
        
        else if (Consume(&Reader, "vt ")) //Vertex texcoord
        {
            v2 T = ParseVector2(&Reader);
            Append(&TexCoords, T);
        }
        
        else if (Consume(&Reader, "s ")) //Smooth shading (ignored)
        {
        }
        
        else if (Consume(&Reader, "usemtl ")) //Material name -> New mesh?
        {
            if (Faces.Count > 0)
            {
                mesh_index MeshIndex = (Assets->MeshCount++);
                mesh* Mesh = Assets->Meshes + MeshIndex;
                Assert(Assets->MeshCount < ArrayCount(Assets->Meshes));
                
                Mesh->MaterialLibrary = CurrentMaterialLibrary;
                Mesh->MaterialName = CurrentMaterialName;
                Mesh->VertexBuffer = CreateMeshVertexBuffer(Positions, Normals, TexCoords, Faces, Allocator);
                Mesh->Triangles    = CreateMeshTriangles(Positions, Faces, Allocator);
                
                CurrentModel->Meshes[CurrentModel->MeshCount++] = MeshIndex;
                Assert(CurrentModel->MeshCount < ArrayCount(CurrentModel->Meshes));
                
                Faces.Count = 0;
            }
            
            string MaterialName = ParseString(&Reader);
            CurrentMaterialName = CopyString(Allocator.Permanent, MaterialName);
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
        
        Mesh->MaterialLibrary = CurrentMaterialLibrary;
        Mesh->MaterialName = CurrentMaterialName;
        Mesh->VertexBuffer = CreateMeshVertexBuffer(Positions, Normals, TexCoords, Faces, Allocator);
        Mesh->Triangles    = CreateMeshTriangles(Positions, Faces, Allocator);
        
        CurrentModel->Meshes[CurrentModel->MeshCount++] = MeshIndex;
        Assert(CurrentModel->MeshCount < ArrayCount(CurrentModel->Meshes));
        
        Faces.Count = 0;
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
                
                Mesh->Triangles    = CreateMeshTriangles(Positions, Faces, Allocator);
                
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
                
                Mesh->Triangles    = CreateMeshTriangles(Positions, Faces, Allocator);
                
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
        
        Mesh->Triangles    = CreateMeshTriangles(Positions, Faces, Allocator);
        
        CurrentModel->Meshes[CurrentModel->MeshCount++] = MeshIndex;
        Assert(CurrentModel->MeshCount < ArrayCount(CurrentModel->Meshes));
        
        Faces.Count = 0;
    }
}


static void
LoadMaterialsFromFile(game_assets* Assets, allocator Allocator, char* Path, char* Library)
{
    span<u8> File = LoadFile(Allocator.Transient, Path);
    file_reader Reader = {(char*)File.Memory, (char*)File.Memory + File.Count};
    
    material* Material = 0;
    
    while (!AtEnd(&Reader))
    {
        if (Consume(&Reader, "#")) //Comment
        {
        }
        
        if (Consume(&Reader, "newmtl ")) //New material
        {
            Material = Assets->Materials + (Assets->MaterialCount++);
            Assert(Assets->MaterialCount <= ArrayCount(Assets->Materials));
            
            string Name = ParseString(&Reader);
            
            Material->Name = CopyString(Allocator.Permanent, Name);
            Material->Library = CopyString(Allocator.Permanent, String(Library));
        }
        
        if (Consume(&Reader, "Ns "))
        {
            Material->SpecularFocus = ParseFloat(&Reader);
        }
        
        //TODO: Fix this
        
        if (Consume(&Reader, "Ka "))
        {
            Material->AmbientColor.X = ParseFloat(&Reader);
            Consume(&Reader, " ");
            Material->AmbientColor.Y = ParseFloat(&Reader);
            Consume(&Reader, " ");
            Material->AmbientColor.Z = ParseFloat(&Reader);
        }
        
        if (Consume(&Reader, "Kd "))
        {
            Material->DiffuseColor.X = ParseFloat(&Reader);
            Consume(&Reader, " ");
            Material->DiffuseColor.Y = ParseFloat(&Reader);
            Consume(&Reader, " ");
            Material->DiffuseColor.Z = ParseFloat(&Reader);
        }
        
        if (Consume(&Reader, "Ks "))
        {
            Material->SpecularColor.X = ParseFloat(&Reader);
            Consume(&Reader, " ");
            Material->SpecularColor.Y = ParseFloat(&Reader);
            Consume(&Reader, " ");
            Material->SpecularColor.Z = ParseFloat(&Reader);
        }
        
        if (Consume(&Reader, "Ke "))
        {
            Material->EmissiveColor.X = ParseFloat(&Reader);
            Consume(&Reader, " ");
            Material->EmissiveColor.Y = ParseFloat(&Reader);
            Consume(&Reader, " ");
            Material->EmissiveColor.Z = ParseFloat(&Reader);
        }
        
        if (Consume(&Reader, "Ni "))
        {
            Material->OpticalDensity = ParseFloat(&Reader);
        }
        
        if (Consume(&Reader, "d "))
        {
            Material->Dissolve = ParseFloat(&Reader);
        }
        
        if (Consume(&Reader, "illum"))
        {
            Material->IlluminationMode = ParseInt(&Reader);
        }
        
        NextLine(&Reader);
    }
}

static void
SetModelLocalTransform(game_assets* Assets, char* ModelName_, m4x4 Transform)
{
    string ModelName = String(ModelName_);
    
    for (u64 ModelIndex = 0; ModelIndex < Assets->ModelCount; ModelIndex++)
    {
        model* Model = Assets->Models + ModelIndex;
        
        if (StringsAreEqual(Model->Name, ModelName))
        {
            Model->LocalTransform = Transform;
            break;
        }
    }
}

