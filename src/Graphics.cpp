
void SetShader(shader Shader);
void SetVertexShaderConstant(u32 Index, void* Data, u32 Bytes);
void SetTexture(texture Texture);
void SetDepthTest(bool Value);

struct color_vertex
{
    v3 Position;
    v4 Color;
};

enum render_command_type
{
    RenderCommand_Null,
    
    RenderCommand_Draw,
    RenderCommand_SetTexture,
    RenderCommand_SetConstant,
    RenderCommand_SetDepthTest,
    RenderCommand_SetShader
};

struct render_command
{
    render_command_type Type;
    
    union
    {
        //Draw
        struct
        {
            f32* VertexData;
            u64 VertexDataBytes;
            D3D11_PRIMITIVE_TOPOLOGY Topology;
            u64 Stride;
        };
        //Texture
        struct
        {
            texture Texture;
        };
        //SetConstant
        struct
        {
            u64 Index;
            void* Data;
            u64 Bytes;
        };
        //SetDepthTest
        bool DepthTest;
        //SetShader
        shader Shader;
    };
};

struct render_command_group
{
    render_command Entries[1024];
    u64 EntryCount;
};

//TODO: Move to platform layer
void RunRenderCommand(render_command Command)
{
    switch (Command.Type)
    {
        case RenderCommand_Draw:
        {
            DrawVertices(Command.VertexData, Command.VertexDataBytes, Command.Topology, Command.Stride);
        } break;
        case RenderCommand_SetTexture:
        {
            SetTexture(Command.Texture);
        } break;
        case RenderCommand_SetConstant:
        {
            SetVertexShaderConstant(Command.Index, Command.Data, Command.Bytes);
        } break;
        case RenderCommand_SetDepthTest:
        {
            SetDepthTest(Command.DepthTest);
        } break;
        case RenderCommand_SetShader:
        {
            SetShader(Command.Shader);
        } break;
    }
}

static void
Push(render_command_group* Group, render_command Entry)
{
    Group->Entries[Group->EntryCount++] = Entry;
    Assert(Group->EntryCount <= ArrayCount(Group->Entries));
}

class render
{
    public:
    
    //If null, then is an immediate context
    render_command_group* Group;
    
    void AddCommand(render_command Command)
    {
        if (Group)
        {
            Push(Group, Command);
        }
        else
        {
            RunRenderCommand(Command);
        }
    }
    
    void Draw(void* Buffer, u64 Bytes, D3D11_PRIMITIVE_TOPOLOGY Topology, u64 Stride)
    {
        render_command Command = {};
        Command.VertexData = (f32*)Buffer;
        Command.VertexDataBytes = Bytes;
        Command.Topology = Topology;
        Command.Stride = Stride;
        AddCommand(Command);
    }
    
    void SetDepthTest(bool Value)
    {
        render_command Command = {};
        Command.DepthTest = Value;
        AddCommand(Command);
    }
    
    void SetShader(shader Shader)
    {
        render_command Command = {};
        Command.Type = RenderCommand_SetShader;
        Command.Shader = Shader;
        AddCommand(Command);
    }
    
    void SetTexture(texture Texture)
    {
        render_command Command = {};
        Command.Type = RenderCommand_SetTexture;
        Command.Texture = Texture;
        AddCommand(Command);
    }
};

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

static render_command
CommandSetTexture(texture Texture)
{
    render_command Result = {};
    Result.Type = RenderCommand_SetTexture;
    Result.Texture = Texture;
    return Result;
}

static render_command
CommandSetConstant(u64 Index, void* Data, u64 Bytes)
{
    render_command Result = {};
    Result.Type = RenderCommand_SetConstant;
    Result.Index = Index;
    Result.Data = Data;
    Result.Bytes = Bytes;
    return Result;
}

static void
RunCommand(render_command Command)
{
    switch (Command.Type)
    {
        case RenderCommand_Draw:
        {
            DrawVertices(Command.VertexData, Command.VertexDataBytes, Command.Topology, Command.Stride);
        } break;
        RenderCommand_SetTexture:
        {
            SetTexture(Command.Texture);
        } break;
        RenderCommand_SetConstant:
        {
            SetVertexShaderConstant(Command.Index, Command.Data, Command.Bytes);
        } break;
        RenderCommand_SetDepthTest:
        {
            SetDepthTest(Command.DepthTest);
        } break;
        RenderCommand_SetShader:
        {
            SetShader(Command.Shader);
        } break;
    }
}

static void 
DrawQuad(v2 A, v2 B, v2 C, v2 D, v4 Color)
{
    f32 VertexData[] = {
        A.X, A.Y, 0.0f, Color.R, Color.G, Color.B, Color.A,
        B.X, B.Y, 0.0f, Color.R, Color.G, Color.B, Color.A,
        C.X, C.Y, 0.0f, Color.R, Color.G, Color.B, Color.A,
        D.X, D.Y, 0.0f, Color.R, Color.G, Color.B, Color.A
    };
    
    DrawVertices(VertexData, sizeof(VertexData), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
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

struct texture_vertex
{
    v3 Position;
    v2 UV;
};

static void
DrawTexture(v3 P0, v3 P1, v2 UV0 = {0.0f, 0.0f}, v2 UV1 = {1.0f, 1.0f})
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
DrawString(string String, v2 Position, v4 Color = {1.0f, 1.0f, 1.0f, 1.0f}, f32 Size = 0.05f, f32 AspectRatio = 1.0f)
{
    if (String.Length > 0)
    {
        Win32DrawText(*DefaultFont, String, Position, Color, Size, AspectRatio);
    }
}

//GUI has to account for different aspect ratios
static void
DrawGUIString(string String, v2 Position, v4 Color = {1.0f, 1.0f, 1.0f, 1.0f}, f32 Size = 0.05f)
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
SetTransform(m4x4 Transform)
{
    Transform = Transpose(Transform);
    SetVertexShaderConstant(0, &Transform, sizeof(Transform));
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

