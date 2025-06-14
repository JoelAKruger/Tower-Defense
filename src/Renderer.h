struct vertex
{
    v3 P;
    v3 Normal;
    v4 Color;
    v2 UV;
};

struct gui_vertex
{
    v2 P;
    v4 Color;
    v2 UV;
};

struct tri
{
    vertex Vertices[3];
};

//TODO: Is this confusing?
struct triangle
{
    v3 Positions[3];
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
    Shader_TexturedModel,
    Shader_ModelWithTexture,
    Shader_Background,
    Shader_OnlyDepth,
    Shader_Bloom,
    Shader_PBR,
    
    Shader_GUI_Color,
    Shader_GUI_Texture,
    Shader_GUI_Font,
    Shader_GUI_HDR_To_SDR,
    
    Shader_Bloom_Filter,
    Shader_Bloom_Downsample,
    Shader_Bloom_Upsample,
    
    Shader_Count
};

enum vertex_buffer_index
{
    VertexBuffer_Null,
    VertexBuffer_Castle,
    VertexBuffer_Turret,
    VertexBuffer_World,
    VertexBuffer_Mine,
    
    VertexBuffer_Count
};

struct material;

struct render_command
{
    void* VertexData;
    u32 VertexDataStride;
    u32 VertexDataBytes;
    
    D3D11_PRIMITIVE_TOPOLOGY Topology;
    shader_index Shader;
    renderer_vertex_buffer* VertexBuffer;
    texture Texture;
    m4x4 ModelTransform;
    v4 Color;
    material* Material;
    bool DisableDepthTest;
    bool DisableShadows;
    bool EnableWind;
};

struct font_asset
{
    texture Texture;
    stbtt_bakedchar BakedChars[128];
    f32 Size;
};

struct game_assets;
struct render_group
{
    game_assets* Assets;
    memory_arena* Arena;
    render_command Commands[4096];
    u32 CommandCount;
};

struct model_textures
{
    texture Ambient;
    texture Diffuse;
    //texture Displacement;
    texture Normal;
    texture Specular;
};

// .mtl material
struct material
{
    string Library;
    string Name;
    f32 SpecularFocus;
    v3 AmbientColor;
    v3 DiffuseColor;
    v3 SpecularColor;
    v3 EmissiveColor;
    f32 OpticalDensity;
    f32 Dissolve;
    int IlluminationMode;
};
typedef u64 material_index;

struct mesh
{
    string MaterialLibrary;
    string MaterialName;
    renderer_vertex_buffer VertexBuffer;
    span<triangle> Triangles;
};
typedef u64 mesh_index;

struct renderer_pbr
{
    
};

struct model
{
    string Name;
    
    mesh_index Meshes[16];
    u64 MeshCount;
    m4x4 LocalTransform;
};

struct game_assets
{
    //Old asset system
    renderer_vertex_buffer VertexBuffers_[VertexBuffer_Count];
    renderer_vertex_buffer VertexBuffersNew[128];
    int VertexBufferCount;
    
    shader Shaders[Shader_Count];
    texture Textures[32];
    cube_map Skybox;
    
    font_asset Font;
    
    render_output WaterReflection;
    render_output WaterRefraction;
    texture WaterDuDv;
    texture WaterNormal;
    
    texture WaterFlow; //Dynamic
    
    render_output ShadowMaps[1];
    render_output Output1;
    
    render_output BloomMipmaps[8];
    render_output BloomAccum;
    
    texture Button;
    texture Panel;
    texture Crystal;
    texture Target;
    
    model_textures ModelTextures;
    
    //New asset system
    material Materials[32];
    int MaterialCount;
    
    material DefaultMaterial;
    
    mesh Meshes[128];
    int MeshCount;
    
    model Models[128];
    int ModelCount;
};

enum render_draw_type
{
    Draw_Regular = 0,
    Draw_OnlyDepth = 1,
    Draw_Shadow = 2
};

struct ssao_kernel
{
    v3 Samples[64];
    v3 Noise[16];
};

//Render Group
render_command* GetNextEntry(render_group* RenderGroup);
render_command* GetLastEntry(render_group* RenderGroup);

void PushRect(render_group* RenderGroup, v3 P0, v3 P1, v2 UV0 = {0.0f, 0.0f}, v2 UV1 = {1.0f, 1.0f});
void PushRectBetter(render_group* RenderGroup, v3 P0, v3 P1, v3 Normal, v2 UV0 = {0.0f, 0.0f}, v2 UV1 = {1.0f, 1.0f});
render_command* PushTexturedRect(render_group* RenderGroup, texture Texture, v3 P0, v3 P1, v2 UV0 = {0.0f, 0.0f}, v2 UV1 = {1.0f, 1.0f});
void PushTexturedRect(render_group* RenderGroup, texture Texture, v3 P0, v3 P1, v3 P2, v3 P3, v2 UV0, v2 UV1, v2 UV2, v2 UV3);
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
void DrawRenderGroup(render_group* Group, game_assets* Assets, shader_constants Constants, render_draw_type Type = Draw_Regular);

//Transforms
m4x4 IdentityTransform();
m4x4 ScaleTransform(f32 X, f32 Y, f32 Z);
m4x4 TranslateTransform(f32 X, f32 Y, f32 Z);
m4x4 ModelRotateTransform();
m4x4 RotateTransform(f32 Radians);
m4x4 PerspectiveTransform(f32 FOV, f32 Near, f32 Far);
m4x4 ViewTransform(v3 Eye, v3 At);
m4x4 OrthographicTransform(f32 Left, f32 Right, f32 Bottom, f32 Top, f32 Near, f32 Far);

//Utility functions
void CalculateModelVertexNormals(tri* Triangles, u64 TriangleCount);
void SetModelLocalTransform(game_assets* Assets, char* ModelName_, m4x4 Transform);

//Assets
void LoadVertexBuffer(game_assets* Assets, char* Description, renderer_vertex_buffer Buffer);
renderer_vertex_buffer* FindVertexBuffer(game_assets* Assets, char* Description_);

extern f32 GlobalAspectRatio;
extern int GlobalOutputWidth;
extern int GlobalOutputHeight;

extern memory_arena GraphicsArena;