struct color_vertex
{
    v3 Position;
    v4 Color;
};

void SetShader(shader Shader);
void SetVertexShaderConstant(u32 Index, void* Data, u32 Bytes);
void SetTexture(texture Texture);
void SetDepthTest(bool Value);

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

static void
DrawTexture(v3 P0, v3 P1, v2 UV0 = {0.0f, 0.0f}, v2 UV1 = {1.0f, 1.0f})
{
    Win32DrawTexture(P0, P1, UV0, UV1);
}

static void
PushRectangle(render_group* Group, v2 Position, v2 Size, v4 Color)
{
    render_shape Shape = {Render_Rectangle};
    Shape.Rectangle.Position = Position;
    Shape.Rectangle.Size = Size;
    Shape.Color = Color;
    Group->Shapes[Group->ShapeCount++] = Shape;
    Assert(Group->ShapeCount <= ArrayCount(Group->Shapes));
}

static void
PushRectangle(render_group* Group, rect Rect, v4 Color)
{
    PushRectangle(Group, Rect.MinCorner, Rect.MaxCorner - Rect.MinCorner, Color);
}

static void
PushRectangleOutline(render_group* Group, rect Rect, v4 Color, f32 Thickness = 0.01f)
{
    v2 MinCorner = Rect.MinCorner;
    v2 MaxCorner = Rect.MaxCorner;
    v2 Size = MaxCorner - MinCorner;
    
    PushRectangle(Group, V2(MinCorner.X, MaxCorner.Y), V2(Size.X, Thickness), Color);
    PushRectangle(Group, V2(MinCorner.X, MinCorner.Y - Thickness), V2(Size.X, Thickness), Color);
    PushRectangle(Group, V2(MinCorner.X - Thickness, MinCorner.Y), V2(Thickness, Size.Y), Color);
    PushRectangle(Group, V2(MaxCorner.X, MinCorner.Y), V2(Thickness, Size.Y), Color);
}

static void
PushCircle(render_group* Group, v2 Position, f32 Radius, v4 Color)
{
    render_shape Shape = {Render_Circle};
    Shape.Circle.Position = Position;
    Shape.Circle.Radius = Radius;
    Shape.Color = Color;
    Group->Shapes[Group->ShapeCount++] = Shape;
    Assert(Group->ShapeCount <= ArrayCount(Group->Shapes));
}

static void
PushLine(render_group* Group, v2 Start, v2 End, v4 Color, f32 Thickness)
{
    render_shape Shape = {Render_Line};
    Shape.Line.Start = Start;
    Shape.Line.End = End;
    Shape.Line.Thickness = Thickness;
    Shape.Color = Color;
    Group->Shapes[Group->ShapeCount++] = Shape;
    Assert(Group->ShapeCount <= ArrayCount(Group->Shapes));
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
PushText(render_group* Group, string String, v2 Position, v4 Color = {1.0f, 1.0f, 1.0f, 1.0f}, f32 Size = 0.05f)
{
    render_shape Shape = {Render_Text};
    Shape.Text.String = String;
    Shape.Text.Position = Position;
    Shape.Text.Size = Size;
    Shape.Color = Color;
    Group->Shapes[Group->ShapeCount++] = Shape;
    Assert(Group->ShapeCount <= ArrayCount(Group->Shapes));
}

static void
PushBackground(render_group* Group, v2 Position, v2 Size, v4 Color)
{
    render_shape Shape = {Render_Background};
    Shape.Rectangle.Position = Position;
    Shape.Rectangle.Size = Size;
    Shape.Color = Color;
    Group->Shapes[Group->ShapeCount++] = Shape;
    Assert(Group->ShapeCount <= ArrayCount(Group->Shapes));
}

static void
PushTris(render_group* Group, span<triangle> Triangles)
{
    render_shape Shape = {Render_Triangles};
    Shape.TriangleList = Triangles;
    Group->Shapes[Group->ShapeCount++] = Shape;
    Assert(Group->ShapeCount <= ArrayCount(Group->Shapes));
}

static void
PushTexture(render_group* Group, v2 P0, v2 P1)
{
    render_shape Shape = {Render_Texture};
    Shape.Texture.P0 = P0;
    Shape.Texture.P1 = P1;
    Shape.Texture.UV0 = V2(0.0f, 0.0f);
    Shape.Texture.UV1 = V2(1.0f, 1.0f);
    Group->Shapes[Group->ShapeCount++] = Shape;
    Assert(Group->ShapeCount <= ArrayCount(Group->Shapes));
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
