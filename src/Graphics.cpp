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

struct texture_vertex
{
    v3 Position;
    v2 UV;
};

void PushRect(render_group* RenderGroup, v3 P0, v3 P1)
{
    render_command* Command = GetNextEntry(RenderGroup);
    
    model_vertex VertexData[6] = {
        {V3(P0.X, P0.Y, P0.Z)}, 
        {V3(P0.X, P1.Y, P0.Z)}, 
        {V3(P1.X, P1.Y, P0.Z)},
        {V3(P1.X, P1.Y, P0.Z)},
        {V3(P1.X, P0.Y, P0.Z)},
        {V3(P0.X, P0.Y, P0.Z)}
    };
    
    CalculateModelVertexNormals((model_triangle*)VertexData, 2);
    
    //Get rid of this
    u64 VertexDataBytes = sizeof(VertexData);
    
    Command->VertexData = CopyVertexData(RenderGroup->Arena, VertexData, VertexDataBytes);
    Command->VertexDataStride = sizeof(model_vertex);
    Command->VertexDataBytes = VertexDataBytes;
    
    Command->Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    Command->Shader = Shader_Model;
    Command->ModelTransform = IdentityTransform();
}

static void
PushTexturedRect(render_group* RenderGroup, texture Texture, v3 P0, v3 P1, v2 UV0, v2 UV1)
{
    render_command* Command = GetNextEntry(RenderGroup);
    
    texture_vertex VertexData[4] = {
        {V3(P0.X, P0.Y, P0.Z), V2(UV0.X, UV0.Y)},
        {V3(P0.X, P1.Y, P0.Z), V2(UV0.X, UV1.Y)},
        {V3(P1.X, P0.Y, P0.Z), V2(UV1.X, UV0.Y)},
        {V3(P1.X, P1.Y, P0.Z), V2(UV1.X, UV1.Y)}
    };
    
    u64 VertexDataBytes = sizeof(VertexData);
    
    Command->VertexData = CopyVertexData(RenderGroup->Arena, VertexData, VertexDataBytes);
    Command->VertexDataStride = sizeof(texture_vertex);
    Command->VertexDataBytes = VertexDataBytes;
    
    Command->Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    Command->Shader = Shader_Texture;
    Command->Texture = Texture;
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

shader ColorShader;
shader FontShader;
shader TextureShader;
shader WaterShader;
shader ModelShader;

texture BackgroundTexture;
texture TargetTexture;
texture TowerTexture;
texture ExplosionTexture;

f32 GlobalAspectRatio;

static void 
DrawQuad(v2 A, v2 B, v2 C, v2 D, v4 Color)
{
    f32 VertexData[] = {
        A.X, A.Y, 0.0f, Color.R, Color.G, Color.B, Color.A,
        B.X, B.Y, 0.0f, Color.R, Color.G, Color.B, Color.A,
        C.X, C.Y, 0.0f, Color.R, Color.G, Color.B, Color.A,
        D.X, D.Y, 0.0f, Color.R, Color.G, Color.B, Color.A
    };
    
    DrawVertices(VertexData, sizeof(VertexData), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, 7 * sizeof(f32));
}

static void
DrawRectangle(v2 Position, v2 Size, v4 Color)
{
    v2 Origin = Position;
    v2 XAxis = V2(Size.X, 0.0f);
    v2 YAxis = V2(0.0f, Size.Y);
    DrawQuad(Origin + YAxis, Origin + YAxis + XAxis, Origin, Origin + XAxis, Color);
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
    
    texture_vertex VertexData[4] = {
        {V3(P0.X, P0.Y, P0.Z), V2(UV0.X, UV0.Y)},
        {V3(P0.X, P1.Y, P0.Z), V2(UV0.X, UV1.Y)},
        {V3(P1.X, P0.Y, P0.Z), V2(UV1.X, UV0.Y)},
        {V3(P1.X, P1.Y, P0.Z), V2(UV1.X, UV1.Y)}
    };
    
    DrawVertices((f32*) VertexData, sizeof(VertexData), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, sizeof(texture_vertex));
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
DrawRenderGroup(render_group* Group, game_assets* Assets)
{
    //TODO: Optimise this
    for (u32 CommandIndex = 0; CommandIndex < Group->CommandCount; CommandIndex++)
    {
        render_command* Command = Group->Commands + CommandIndex;
        
        shader Shader = Assets->Shaders[Command->Shader];
        SetShader(Shader);
        
        SetTexture(Command->Texture);
        SetModelTransform(Command->ModelTransform);
        SetModelColor(Command->Color);
        SetDepthTest(!Command->DisableDepthTest);
        
        DrawVertices((f32*)Command->VertexData, Command->VertexDataBytes, Command->Topology, Command->VertexDataStride);
    }
}

static void
SetTransform(m4x4 Transform)
{
    Transform = Transpose(Transform);
    SetVertexShaderConstant(0, &Transform, sizeof(Transform));
}

static void
SetLightTransform(m4x4 Transform)
{
    Transform = Transpose(Transform);
    SetVertexShaderConstant(4, &Transform, sizeof(Transform));
}

static void
SetShaderTime(f32 Time)
{
    f32 Constant[4] = {Time};
    SetVertexShaderConstant(1, Constant, sizeof(Constant));
}

static void
SetModelTransform(m4x4 Transform)
{
    Transform = Transpose(Transform);
    SetVertexShaderConstant(2, &Transform, sizeof(Transform));
}

static void
SetModelColor(v4 Color)
{
    SetVertexShaderConstant(3, &Color, sizeof(Color));
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