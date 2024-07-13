struct color_vertex
{
    v2 Position;
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
texture TowerTexture;

f32 GlobalAspectRatio;

static void 
DrawQuad(v2 A, v2 B, v2 C, v2 D, v4 Color)
{
    f32 VertexData[] = {
        A.X, A.Y, Color.R, Color.G, Color.B, Color.A,
        B.X, B.Y, Color.R, Color.G, Color.B, Color.A,
        C.X, C.Y, Color.R, Color.G, Color.B, Color.A,
        D.X, D.Y, Color.R, Color.G, Color.B, Color.A
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
Drawline(v2 Start, v2 End, v4 Color, v3 Thickness)
{
    Assert(0);
}

static void
DrawTexture(v2 P0, v2 P1, v2 UV0 = {0.0f, 0.0f}, v2 UV1 = {1.0f, 1.0f})
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

