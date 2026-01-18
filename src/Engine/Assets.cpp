
static texture_handle
LoadTexture(char* Path)
{
    int Width = 0, Height = 0, Channels = 0;
    stbi_set_flip_vertically_on_load(true);
    stbi_uc* TextureData = stbi_load(Path, &Width, &Height, &Channels, 4);
    
    texture_handle Result = PlatformCreateTexture((u32*)TextureData, Width, Height, 4);
    
    free(TextureData);
    
    return Result;
}

static font_handle
LoadFont(game_assets* Assets, char* Path, f32 Size)
{
    font_asset Font = {};
    
    int TextureWidth = 512;
    int TextureHeight = 512;
    
    span<u8> File = LoadFile(Assets->Allocator.Transient, Path);
    u8* TempTexture = Alloc(Assets->Allocator.Transient, TextureWidth * TextureHeight * sizeof(u8));
    
    stbtt_BakeFontBitmap(File.Memory, 0, Size, TempTexture, TextureWidth, TextureHeight, 0, 
                         ArrayCount(Font.BakedChars), Font.BakedChars);
    
    Font.Size = 0.45f * Size;
    Font.Texture = PlatformCreateTexture((u32*)TempTexture, TextureWidth, TextureHeight, 1);
    
    font_handle Result = Assets->FontCount++;
    Assert(Result < ArrayCount(Assets->Fonts));
    
    Assets->Fonts[Result] = Font;
    
    return Result;
}

static vertex_buffer_handle
CreateVertexBuffer(game_assets* Assets, void* Data, u64 Bytes, int Topology, u64 Stride)
{
    vertex_buffer_handle Result = Assets->VertexBuffers.AllocHandle();
    Assets->VertexBuffers[Result] = PlatformCreateVertexBuffer(Data, Bytes, Topology, Stride);
    return Result;
}

static void
FreeVertexBuffer(game_assets* Assets, vertex_buffer_handle Handle)
{
    FreeVertexBuffer(Assets->VertexBuffers[Handle]);
    Assets->VertexBuffers.Unallocate(Handle);
}

static span<triangle>
CreateMeshTriangles(dynamic_array<v3> Positions, dynamic_array<obj_file_face> Faces, memory_arena* Arena)
{
    u64 TriCount = Faces.Count * 2;
    triangle* Triangles = AllocArray(Arena, triangle, TriCount);
    
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

static vertex_buffer_handle
CreateMeshVertexBuffer(game_assets* Assets, dynamic_array<v3> Positions, dynamic_array<v3> Normals, dynamic_array<v2> TexCoords, dynamic_array<obj_file_face> Faces, allocator Allocator)
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
    
    vertex_buffer_handle Result = CreateVertexBuffer(Assets, Vertices, VertexCount * sizeof(vertex), 
                                                     D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, sizeof(vertex));
    return Result;
}

static span<mesh_handle>
LoadObjects(game_assets* Assets, char* Path)
{
    allocator Allocator = Assets->Allocator;
    dynamic_array<mesh_handle> Result = {.Arena = Allocator.Transient};
    
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
                mesh_handle MeshIndex = (Assets->MeshCount++);
                mesh* Mesh = Assets->Meshes + MeshIndex;
                Assert(Assets->MeshCount < ArrayCount(Assets->Meshes));
                
                Mesh->MaterialLibrary = CurrentMaterialLibrary;
                Mesh->MaterialName = CurrentMaterialName;
                Mesh->VertexBuffer = CreateMeshVertexBuffer(Assets, Positions, Normals, TexCoords, Faces, Allocator);
                Mesh->Triangles    = CreateMeshTriangles(Positions, Faces, Allocator.Permanent);
                
                Append(&Result, MeshIndex);
                
                CurrentModel->Meshes[CurrentModel->MeshCount++] = MeshIndex;
                Assert(CurrentModel->MeshCount < ArrayCount(CurrentModel->Meshes));
                
                Faces.Count = 0;
            }
            
            CurrentModel = Assets->Models + (Assets->ModelCount++);
            Assert(Assets->ModelCount <= ArrayCount(Assets->Models));
            
            string Name = ParseString(&Reader);
            CurrentModel->Name = CopyString(Allocator.Permanent, Name);
            CurrentModel->LocalTransform = IdentityTransform();
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

        else if (Consume(&Reader, "l "))
        {
        }
        
        else if (Consume(&Reader, "usemtl ")) //Material name -> New mesh?
        {
            if (Faces.Count > 0)
            {
                mesh_handle MeshIndex = (Assets->MeshCount++);
                mesh* Mesh = Assets->Meshes + MeshIndex;
                Assert(Assets->MeshCount < ArrayCount(Assets->Meshes));
                
                Mesh->MaterialLibrary = CurrentMaterialLibrary;
                Mesh->MaterialName = CurrentMaterialName;
                Mesh->VertexBuffer = CreateMeshVertexBuffer(Assets, Positions, Normals, TexCoords, Faces, Allocator);
                Mesh->Triangles    = CreateMeshTriangles(Positions, Faces, Allocator.Permanent);
                Append(&Result, MeshIndex);
                
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
        mesh_handle MeshIndex = (Assets->MeshCount++);
        mesh* Mesh = Assets->Meshes + MeshIndex;
        Assert(Assets->MeshCount < ArrayCount(Assets->Meshes));
        
        Mesh->MaterialLibrary = CurrentMaterialLibrary;
        Mesh->MaterialName = CurrentMaterialName;
        Mesh->VertexBuffer = CreateMeshVertexBuffer(Assets, Positions, Normals, TexCoords, Faces, Allocator);
        Mesh->Triangles    = CreateMeshTriangles(Positions, Faces, Allocator.Permanent);
        Append(&Result, MeshIndex);
        
        CurrentModel->Meshes[CurrentModel->MeshCount++] = MeshIndex;
        Assert(CurrentModel->MeshCount < ArrayCount(CurrentModel->Meshes));
        
        Faces.Count = 0;
    }
    
    return ToSpan(Result);
}

static void
LoadMaterials(game_assets* Assets, char* Path, char* Library)
{
    memory_arena* PArena = Assets->Allocator.Permanent;
    
    span<u8> File = LoadFile(Assets->Allocator.Transient, Path);
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
            
            Material->Name = CopyString(PArena, Name);
            Material->Library = CopyString(PArena, String(Library));
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
SetModelLocalTransform(game_assets* Assets, model_handle Model, m4x4 Transform)
{
    Assets->Models[Model].LocalTransform = Transform;
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
            return;
        }
    }
    Assert(0);
}

static model_handle
GetModelHandle(game_assets* Assets, char* ModelName_)
{
    string ModelName = String(ModelName_);
    
    u64 Result = 0;
    for (u64 ModelIndex = 0; ModelIndex < Assets->ModelCount; ModelIndex++)
    {
        if (StringsAreEqual(ModelName, Assets->Models[ModelIndex].Name))
        {
            Result = ModelIndex;
            break;
        }
    }
    return Result;
}

//DELETE EVERYTHING BELOW HERE

/*
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
    Result.Texture = PlatformCreateTexture((u32*)TempTexture, TextureWidth, TextureHeight, 1);
    
    return Result;
}
*/