static render_command*
GetNextEntry(render_group* RenderGroup)
{
    Assert(RenderGroup->CommandCount < ArrayCount(RenderGroup->Commands));
    
    render_command* Result = RenderGroup->Commands + (RenderGroup->CommandCount++);
    return Result;
}

static render_command*
GetLastEntry(render_group* RenderGroup)
{
    Assert(RenderGroup->CommandCount > 0);
    
    render_command* Result = RenderGroup->Commands + (RenderGroup->CommandCount - 1);
    return Result;
}

static void*
CopyVertexData(memory_arena* Arena, void* Data, u64 Bytes)
{
    void* Result = Alloc(Arena, Bytes);
    memcpy(Result, Data, Bytes);
    return Result;
}

void PushRect(render_group* RenderGroup, v3 P0, v3 P1, v2 UV0, v2 UV1)
{
    render_command* Command = GetNextEntry(RenderGroup);
    
    vertex VertexData[4] = {
        {V3(P0.X, P0.Y, P0.Z), {}, {}, V2(UV0.X, UV0.Y)},
        {V3(P0.X, P1.Y, P0.Z), {}, {}, V2(UV0.X, UV1.Y)},
        {V3(P1.X, P0.Y, P0.Z), {}, {}, V2(UV1.X, UV0.Y)},
        {V3(P1.X, P1.Y, P0.Z), {}, {}, V2(UV1.X, UV1.Y)}
    };
    
    //CalculateModelVertexNormals((tri*)VertexData, 2);
    
    //Get rid of this
    u64 VertexDataBytes = sizeof(VertexData);
    
    Command->VertexData = CopyVertexData(RenderGroup->Arena, VertexData, VertexDataBytes);
    Command->VertexDataStride = sizeof(vertex);
    Command->VertexDataBytes = VertexDataBytes;
    
    Command->Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    Command->Shader = Shader_Model;
    Command->ModelTransform = IdentityTransform();
}

static void
PushTexturedRect(render_group* RenderGroup, texture Texture, v3 P0, v3 P1, v2 UV0, v2 UV1)
{
    render_command* Command = GetNextEntry(RenderGroup);
    
    vertex VertexData[4] = {
        {V3(P0.X, P0.Y, P0.Z), {}, {}, V2(UV0.X, UV0.Y)},
        {V3(P0.X, P1.Y, P0.Z), {}, {}, V2(UV0.X, UV1.Y)},
        {V3(P1.X, P0.Y, P0.Z), {}, {}, V2(UV1.X, UV0.Y)},
        {V3(P1.X, P1.Y, P0.Z), {}, {}, V2(UV1.X, UV1.Y)}
    };
    
    u64 VertexDataBytes = sizeof(VertexData);
    
    Command->VertexData = CopyVertexData(RenderGroup->Arena, VertexData, VertexDataBytes);
    Command->VertexDataStride = sizeof(vertex);
    Command->VertexDataBytes = VertexDataBytes;
    
    Command->Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    Command->Shader = Shader_Texture;
    Command->Texture = Texture;
    Command->ModelTransform = IdentityTransform();
}


static void
PushTexturedRect(render_group* RenderGroup, texture Texture, v3 P0, v3 P1, v3 P2, v3 P3, v2 UV0, v2 UV1, v2 UV2, v2 UV3)
{
    render_command* Command = GetNextEntry(RenderGroup);
    
    vertex VertexData[4] = {
        {V3(P0.X, P0.Y, P0.Z), {}, {}, V2(UV0.X, UV0.Y)},
        {V3(P1.X, P1.Y, P1.Z), {}, {}, V2(UV1.X, UV1.Y)},
        {V3(P3.X, P3.Y, P3.Z), {}, {}, V2(UV3.X, UV3.Y)},
        {V3(P2.X, P2.Y, P2.Z), {}, {}, V2(UV2.X, UV2.Y)}
    };
    
    u64 VertexDataBytes = sizeof(VertexData);
    
    Command->VertexData = CopyVertexData(RenderGroup->Arena, VertexData, VertexDataBytes);
    Command->VertexDataStride = sizeof(vertex);
    Command->VertexDataBytes = VertexDataBytes;
    
    Command->Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    Command->Shader = Shader_Texture;
    Command->Texture = Texture;
    Command->ModelTransform = IdentityTransform();
}

static void
PushVertices(render_group* RenderGroup, void* Data, u32 Bytes, u32 Stride, D3D11_PRIMITIVE_TOPOLOGY Topology, shader_index Shader)
{
    render_command* Command = GetNextEntry(RenderGroup);
    Command->VertexData = Data;
    Command->VertexDataBytes = Bytes;
    Command->VertexDataStride = Stride;
    Command->Topology = Topology;
    Command->Shader = Shader;
    Command->ModelTransform = IdentityTransform();
}

static void
PushModel(render_group* RenderGroup, renderer_vertex_buffer* VertexBuffer)
{
    render_command* Command = GetNextEntry(RenderGroup);
    Command->VertexBuffer = VertexBuffer;
    Command->Shader = Shader_Model;
    Command->ModelTransform = IdentityTransform();
}

static material*
FindMaterial(game_assets* Assets, string Library, string Name)
{
    material* Result = 0;
    
    for (u64 MaterialIndex = 0; MaterialIndex < Assets->MaterialCount; MaterialIndex++)
    {
        material* Material = Assets->Materials + MaterialIndex;
        
        if (StringsAreEqual(Library, Material->Library) && StringsAreEqual(Name, Material->Name))
        {
            Result = Material;
            break;
        }
    }
    
    return Result;
}

static void
PushModelNew(render_group* RenderGroup, game_assets* Assets, char* ModelName, m4x4 Transform)
{
    //TODO: Make more functions
    string Name = String(ModelName);
    
    for (u64 ModelIndex = 0; ModelIndex < Assets->ModelCount; ModelIndex++)
    {
        model* Model = Assets->Models + ModelIndex;
        if (StringsAreEqual(Model->Name, Name))
        {
            m4x4 ModelTransform = Model->LocalTransform * Transform;
            
            for (u64 MeshHandleIndex = 0; MeshHandleIndex < Model->MeshCount; MeshHandleIndex++)
            {
                mesh_index MeshHandle = Model->Meshes[MeshHandleIndex];
                
                mesh* Mesh = Assets->Meshes + MeshHandle;
                
                PushModel(RenderGroup, &Mesh->VertexBuffer);
                GetLastEntry(RenderGroup)->Shader = Shader_PBR;
                PushModelTransform(RenderGroup, ModelTransform);
                
                v4 Color = V4(1, 1, 1, 1);
                
                material* Material = FindMaterial(Assets, Mesh->MaterialLibrary, Mesh->MaterialName);
                if (Material)
                {
                    Color = V4(Material->DiffuseColor, 1.0f);
                }
                
                PushColor(RenderGroup, Color);
            }
            break;
        }
    }
}


static void
PushColor(render_group* RenderGroup, v4 Color)
{
    render_command* Command = GetLastEntry(RenderGroup);
    Command->Color = Color;
}

static void
PushModelTransform(render_group* RenderGroup, m4x4 Transform)
{
    render_command* Command = GetLastEntry(RenderGroup);
    Command->ModelTransform = Transform;
}

static void
PushNoDepthTest(render_group* RenderGroup)
{
    render_command* Command = GetLastEntry(RenderGroup);
    Command->DisableDepthTest = true;
}

static void
PushNoShadow(render_group* RenderGroup)
{
    render_command* Command = GetLastEntry(RenderGroup);
    Command->DisableShadows = true;
}

static void
PushShader(render_group* RenderGroup, shader_index Shader)
{
    render_command* Command = GetLastEntry(RenderGroup);
    Command->Shader = Shader;
}

/*
A   B
C   D
*/

static void 
DrawQuad(v2 A, v2 B, v2 C, v2 D, v4 Color)
{
    gui_vertex Vertices[4] = {
        {V2(A.X, A.Y), Color, V2(0, 1)},
        {V2(B.X, B.Y), Color, V2(1, 1)},
        {V2(C.X, C.Y), Color, V2(0, 0)},
        {V2(D.X, D.Y), Color, V2(1, 0)}
    };
    
    DrawVertices((f32*)Vertices, sizeof(Vertices), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, sizeof(Vertices[0]));
}

static void
DrawLine(v2 Start, v2 End, v4 Color, f32 Thickness)
{
    v2 XAxis = End - Start;
    v2 YAxis = UnitV(Perp(XAxis)) * Thickness;
    v2 Origin = Start - 0.5f * YAxis;
    
    DrawQuad(Origin + YAxis, Origin + YAxis + XAxis, Origin, Origin + XAxis, Color);
}

static void
DrawTexture(v3 P0, v3 P1, v2 UV0, v2 UV1)
{
    Assert(P0.Z == P1.Z);
    
    vertex VertexData[4] = {
        {V3(P0.X, P0.Y, P0.Z), {}, {}, V2(UV0.X, UV0.Y)},
        {V3(P0.X, P1.Y, P0.Z), {}, {}, V2(UV0.X, UV1.Y)},
        {V3(P1.X, P0.Y, P0.Z), {}, {}, V2(UV1.X, UV0.Y)},
        {V3(P1.X, P1.Y, P0.Z), {}, {}, V2(UV1.X, UV1.Y)}
    };
    
    DrawVertices((f32*) VertexData, sizeof(VertexData), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, sizeof(vertex));
}

static void
DrawString(string String, v2 Position, v4 Color, f32 Size, f32 AspectRatio)
{
    if (String.Length > 0)
    {
        Win32DrawText(*DefaultFont, String, Position, Color, Size, AspectRatio);
    }
}

//GUI has to account for different aspect ratios
static void
DrawGUIString(string String, v2 Position, v4 Color, f32 Size)
{
    DrawString(String, Position, Color, Size, GlobalAspectRatio);
}

static f32
GUIStringWidth(string String, f32 FontSize)
{
    f32 Result = PlatformTextWidth(String, FontSize, GlobalAspectRatio);
    return Result;
}

static void 
DrawRenderGroup(render_group* Group, game_assets* Assets, shader_constants Constants, render_draw_type Type)
{
    //TODO: Optimise this
    //Start with a known state
    bool DepthTestIsEnabled = true;
    shader_index CurrentShader = Shader_Null;
    texture CurrentTexture = {};
    
    SetDepthTest(DepthTestIsEnabled);
    SetShader({});
    SetTexture({});
    
    for (u32 CommandIndex = 0; CommandIndex < Group->CommandCount; CommandIndex++)
    {
        render_command* Command = Group->Commands + CommandIndex;
        
        if ((Type & Draw_Shadow) && Command->DisableShadows)
        {
            continue;
        }
        
        shader_index Shader = Command->Shader;
        
        //Override shader if only position is needed
        if (Type & Draw_OnlyDepth)
        {
            Shader = Shader_OnlyDepth;
        }
        
        Constants.ModelToWorldTransform = Command->ModelTransform;
        Constants.Color = Command->Color;
        
        if (CurrentShader != Shader)
        {
            SetShader(Assets->Shaders[Shader]);
            CurrentShader = Shader;
        }
        
        if (Command->Texture.TextureView && (Command->Texture.TextureView == CurrentTexture.TextureView))
        {
            SetTexture(Command->Texture);
            CurrentTexture = Command->Texture;
        }
        
        bool EnableDepthTest = !Command->DisableDepthTest;
        
        if (DepthTestIsEnabled != EnableDepthTest)
        {
            SetDepthTest(EnableDepthTest);
            DepthTestIsEnabled = EnableDepthTest;
        }
        
        SetGraphicsShaderConstants(Constants);
        
        if (Command->VertexBuffer)
        {
            DrawVertexBuffer(*Command->VertexBuffer);
        }
        else
        {
            DrawVertices((f32*)Command->VertexData, Command->VertexDataBytes, Command->Topology, Command->VertexDataStride);
        }
    }
}

static void
SetTransformForNonGraphicsShader(m4x4 Transform)
{
    Transform = Transpose(Transform);
    SetNonGraphicsShaderConstant(&Transform, sizeof(Transform));
}

static void
SetGraphicsShaderConstants(shader_constants Constants)
{
    Constants.WorldToClipTransform  = Transpose(Constants.WorldToClipTransform);
    Constants.ModelToWorldTransform = Transpose(Constants.ModelToWorldTransform);
    Constants.WorldToLightTransform = Transpose(Constants.WorldToLightTransform);
    SetGraphicsShaderConstant(&Constants, sizeof(Constants));
}

static m4x4
IdentityTransform()
{
    m4x4 Result = {};
    
    Result.Values[0][0] = 1.0f;
    Result.Values[1][1] = 1.0f;
    Result.Values[2][2] = 1.0f;
    Result.Values[3][3] = 1.0f;
    
    return Result;
}

static m4x4
ScaleTransform(f32 X, f32 Y, f32 Z)
{
    m4x4 Result = {};
    Result.Values[0][0] = X;
    Result.Values[1][1] = Y;
    Result.Values[2][2] = Z;
    Result.Values[3][3] = 1.0f;
    return Result;
}

static m4x4
TranslateTransform(f32 X, f32 Y, f32 Z)
{
    m4x4 Result = {};
    Result.Values[0][0] = 1.0f;
    Result.Values[1][1] = 1.0f;
    Result.Values[2][2] = 1.0f;
    Result.Values[3][3] = 1.0f;
    Result.Values[3][0] = X;
    Result.Values[3][1] = Y;
    Result.Values[3][2] = Z;
    return Result;
}

static m4x4
ModelRotateTransform()
{
    m4x4 Result = {};
    Result.Values[0][0] = 1.0f;
    Result.Values[2][1] = 1.0f;
    Result.Values[1][2] = -1.0f;
    Result.Values[3][3] = 1.0f;
    return Result;
}

static m4x4
ModelRotateTransformNew()
{
    m4x4 Result = {};
    Result.Values[0][0] = 1.0f;
    Result.Values[2][1] = 1.0f;
    Result.Values[1][2] = 1.0f;
    Result.Values[3][3] = 1.0f;
    return Result;
}

static m4x4 
RotateTransform(f32 Radians)
{
    m4x4 Result = {};
    Result.Values[0][0] = cosf(Radians);
    Result.Values[0][1] = -sinf(Radians);
    Result.Values[1][0] = sinf(Radians);
    Result.Values[1][1] = cosf(Radians);
    Result.Values[2][2] = 1.0f;
    Result.Values[3][3] = 1.0f;
    return Result;
}


//From: 
//https://learn.microsoft.com/en-us/windows/win32/direct3d9/projection-transform
static m4x4
PerspectiveTransform(f32 FOV, f32 Near, f32 Far)
{
    f32 FOVRadians = 3.14159f / 180.0f * FOV;
    f32 W = 1.0f / (GlobalAspectRatio * tanf(0.5f * FOVRadians));
    f32 H = 1.0f / tanf(0.5f * FOVRadians);
    f32 Q = Far / (Far - Near);
    
    m4x4 Result = {};
    Result.Values[0][0] = W;
    Result.Values[1][1] = H;
    Result.Values[2][2] = Q;
    Result.Values[2][3] = 1.0f;
    Result.Values[3][2] = -Near*Q;
    
    return Result;
}

//From:
//https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dxmatrixlookatlh
static m4x4
ViewTransform(v3 Eye, v3 At)
{
    v3 Up = V3(0.0f, 0.0f, -1.0f);
    
    v3 ZAxis = UnitV(At - Eye);
    v3 XAxis = UnitV(CrossProduct(Up, ZAxis));
    v3 YAxis = CrossProduct(ZAxis, XAxis);
    
    m4x4 Result = {};
    
    Result.Vectors[0] = V4(XAxis, -DotProduct(XAxis, Eye));
    Result.Vectors[1] = V4(YAxis, -DotProduct(YAxis, Eye));;
    Result.Vectors[2] = V4(ZAxis, -DotProduct(ZAxis, Eye));;
    Result.Vectors[3] = V4(0.0f, 0.0f, 0.0f, 1.0f);
    
    return Transpose(Result);
}

static m4x4
OrthographicTransform(f32 Left, f32 Right, f32 Bottom, f32 Top, f32 Near, f32 Far)
{
    m4x4 Result = {};
    
    Result.Values[0][0] = 2.0f / (Right - Left);
    Result.Values[1][1] = 2.0f / (Top - Bottom);
    Result.Values[2][2] = 1.0f / (Far - Near);
    Result.Values[3][0] = (Left + Right) / (Left - Right);
    Result.Values[3][1] = (Top + Bottom) / (Bottom - Top);
    Result.Values[3][2] = Near / (Near - Far);
    Result.Values[3][3] = 1.0f;
    
    return Result;
}

static int 
TextPixelWidth(font_asset* Font, string String)
{
    int Width = 0;
    
    for (int I = 0; I < String.Length; I++)
    {
        u8 C = (u8)String.Text[I];
        Assert(C < ArrayCount(Font->BakedChars));
        
        Width += Font->BakedChars[C].xadvance;
    }
    
    return Width;
}

static v2
TextPixelSize(font_asset* Font, string String)
{
    f32 Height = Font->Size;
    f32 Width = (f32)TextPixelWidth(Font, String);
    return {Width, Height};
}

static void
DrawString_(string String)
{
    
}

static ssao_kernel
CreateSSAOKernel()
{
    ssao_kernel Kernel = {};
    for (u64 I = 0; I < ArrayCount(Kernel.Samples); I++)
    {
        v3 Sample = {Random() * 2.0f - 1.0f, Random() * 2.0f - 1.0f, Random()};
        
        f32 Scale = (f32)I / ArrayCount(Kernel.Samples);
        Scale = LinearInterpolate(0.1f, 1.0f, Square(Scale));
        
        Kernel.Samples[I] = Scale * UnitV(Sample);
    }
    
    for (u64 I = 0; I < ArrayCount(Kernel.Noise); I++)
    {
        Kernel.Noise[I] = UnitV(V3(Random() * 2.0f - 1.0f, Random() * 2.0f - 1.0f, 0.0f));
    }
    
    return Kernel;
}