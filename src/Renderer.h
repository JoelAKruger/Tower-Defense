struct vertex_buffer
{
    void* Data;
    u64 Bytes;
};

struct vertex
{
    v3 P;
    v3 Normal;
    v4 Color;
    v2 UV;
};

struct tri
{
    vertex Vertices[3];
};

struct cube_map
{
    texture Textures[6];
};

enum shader_index
{
    Shader_Null,
    Shader_Color,
    Shader_Font,
    Shader_Texture,
    Shader_Water,
    Shader_Model,
    Shader_Background,
    Shader_OnlyDepth,
    
    Shader_Count
};

enum vertex_buffer_index
{
    VertexBuffer_Null,
    VertexBuffer_Castle,
    VertexBuffer_Turret,
    VertexBuffer_World,
    
    VertexBuffer_Count
};

struct render_command
{
    void* VertexData;
    u32 VertexDataStride;
    u32 VertexDataBytes;
    
    D3D11_PRIMITIVE_TOPOLOGY Topology;
    shader_index Shader;
    texture Texture;
    m4x4 ModelTransform;
    v4 Color;
    bool DisableDepthTest;
    bool DisableShadows;
};

struct render_group
{
    memory_arena* Arena;
    render_command Commands[512];
    u32 CommandCount;
};

struct game_assets
{
    vertex_buffer VertexBuffers[VertexBuffer_Count];
    shader Shaders[Shader_Count];
    texture Textures[32];
    cube_map Skybox;
    
    render_output ShadowMaps[1];
};

enum render_draw_type
{
    Draw_Regular,
    Draw_OnlyDepth,
};

//Initialisation
texture CreateTexture(char* Path);

//Render Group
render_command* GetNextEntry(render_group* RenderGroup);
render_command* GetLastEntry(render_group* RenderGroup);

void PushRect(render_group* RenderGroup, v3 P0, v3 P1);
void PushTexturedRect(render_group* RenderGroup, texture Texture, v3 P0, v3 P1, v2 UV0 = {0.0f, 0.0f}, v2 UV1 = {1.0f, 1.0f});
void PushVertices(render_group* RenderGroup, void* Data, u32 Bytes, u32 Stride, D3D11_PRIMITIVE_TOPOLOGY Topology, shader_index Shader);
void PushColor(render_group* RenderGroup, v4 Color);
void PushModelTransform(render_group* RenderGroup, m4x4 Transform);

//Immediate Mode
void DrawQuad(v2 A, v2 B, v2 C, v2 D, v4 Color);
void DrawRectangle(v2 Position, v2 Size, v4 Color);
void DrawLine(v2 Start, v2 End, v4 Color, f32 Thickness);
void DrawTexture(v3 P0, v3 P1, v2 UV0 = {0.0f, 0.0f}, v2 UV1 = {1.0f, 1.0f});
void DrawString(string String, v2 Position, v4 Color = {1.0f, 1.0f, 1.0f, 1.0f}, f32 Size = 0.05f, f32 AspectRatio = 1.0f);
void DrawGUIString(string String, v2 Position, v4 Color = {1.0f, 1.0f, 1.0f, 1.0f}, f32 Size = 0.05f);
f32 GUIStringWidth(string String, f32 FontSize);
void DrawRenderGroup(render_group* Group, game_assets* Assets, render_draw_type Type = Draw_Regular);

//Transforms
m4x4 IdentityTransform();
m4x4 ScaleTransform(f32 X, f32 Y, f32 Z);
m4x4 TranslateTransform(f32 X, f32 Y, f32 Z);
m4x4 ModelRotateTransform();
m4x4 RotateTransform(f32 Radians);
m4x4 PerspectiveTransform(f32 FOV, f32 Near, f32 Far);
m4x4 ViewTransform(v3 Eye, v3 At);
m4x4 OrthographicTransform(f32 Left, f32 Right, f32 Bottom, f32 Top, f32 Near, f32 Far);

//Shader constants
void SetTransform(m4x4 Transform);
void SetLightTransform(m4x4 Transform);
void SetShaderTime(f32 Time);
void SetModelTransform(m4x4 Transform);
void SetModelColor(v4 Color);

//Utility functions
void CalculateModelVertexNormals(tri* Triangles, u64 TriangleCount);