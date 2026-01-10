#include <stdint.h>
#include <stdio.h>
#include <cstdarg>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <emmintrin.h>

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

typedef uint64_t u64;
typedef int64_t i64;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint16_t u16;
typedef int16_t i16;
typedef uint8_t u8;
typedef int8_t i8;
typedef float f32;
typedef double f64;
typedef u32 b32;

#define Kilobytes(n) (n * 1024)
#define Megabytes(n) (n * 1024 * 1024)

#define ArrayCount(x) (sizeof((x))/sizeof((x)[0]))

struct string
{
	char* Text;
	u64 Length;
};

//Debug
#if DEBUG == 1
#define Assert(value) Assert_(value, __LINE__, __FILE__)
#else
#define Assert(value)
#endif
void Assert_(bool Value, int Line, char* File);
void Log(char* Format, ...);

template <typename type>
struct span
{
	type* Memory;
	u64 Count;
    
    type& operator[](u64 Index)
    {
#if DEBUG
        Assert(Index < Count);
#endif
        return Memory[Index];
    }
};

enum memory_arena_type
{
	NORMAL, PERMANENT, TRANSIENT
};

struct memory_arena
{
	u8* Buffer;
	u64 Used;
	u64 Size;
	memory_arena_type Type;
};

struct allocator
{
	memory_arena* Permanent;
	memory_arena* Transient;
};

struct v2 { f32 X, Y; };
struct v2i { i32 X, Y; };

struct v3
{ 
    union
    {
        struct
        {
            f32 X, Y, Z;
        };
        struct
        {
            v2 XY;
        };
    };
};

struct v4
{
    union
    {
        struct
        {
            f32 X, Y, Z, W;
        };
        struct
        {
            v2 XY;
            //f32 Z, W;
        };
        struct
        {
            v3 XYZ;
            //f32 W;
        };
        struct
        {
            f32 R, G, B, A;
        };
        struct
        {
            v3 RGB;
            //f32 A;
        };
    };
};

struct m4x4
{
    //row major ordering ( [col][row] )
    union
    {
        f32 Values[4][4];
        v4 Vectors[4];
    };
};

enum channel : u32
{
    Channel_Null,
    Channel_Init,
    Channel_Message,
    Channel_GameState,
    
    Channel_Count
};

//----------------------------------

#include "Utilities.cpp"
#include "Maths.cpp"

struct texture;
struct renderer_shader;
struct render_output;
struct renderer_vertex_buffer;
struct font_texture;
struct game_assets;

struct texture_handle 
{
    u64 Index;
    operator bool() { return Index != 0; }
};
struct render_output_handle 
{
    u64 Index;
    operator bool() { return Index != 0; }
};

template <typename type>
bool operator==(type A, type B)
{
    return (A.Index == B.Index);
}

template <typename type>
bool operator!=(type A, type B)
{
    return (A.Index != B.Index);
}

#ifdef _WIN32
#include "Platform_Win32_DX11.h"
#endif
//----------------------------------

enum shader
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

enum blend_mode
{
    BlendMode_Add,
    BlendMode_Blend
};

struct shader_constants
{
    m4x4 WorldToClipTransform; 
    m4x4 ModelToWorldTransform;
    m4x4 WorldToLightTransform;
    v4 Color;
    
    f32 Time;
    f32 Pad0[3];
    
    v4 ClipPlane;
    
    v3 CameraPos;
    f32 Pad1;
    
    v3 LightDirection;
    f32 Pad2;
    
    v3 LightColor;
    f32 Pad3;
    
    v3 Albedo;
    float Roughness;
    
    float Metallic;
    float Occlusion;
    float Pad4[2];
    
    v3 FresnelColor;
    float Pad5;
    
    v3 WindDirection;
    f32 WindStrength;
    
    f32 ShadowIntensity;
    f32 ShadowRemove;
    f32 Pad6[2];
};

// --- Platform Layer ---

//Files
span<u8> LoadFile(memory_arena* Arena, char* Path);

//Textures
void SetTexture(texture_handle Texture, int Index = 0);
void SetFontTexture(font_texture Texture);
void UnsetTexture(int Index = 0);
void DeleteTexture(texture* Texture);
texture CreateTexture(char* Path);
texture_handle PlatformCreateTexture(u32* TextureData, int Width, int Height, int Channels = 4);

//Shaders
void LoadShaders(game_assets* Assets);
void SetShader(shader Shader);
void SetGraphicsShaderConstants(shader_constants Constants);
void SetGUIShaderConstant(m4x4 Transform);

//Rendering
renderer_vertex_buffer PlatformCreateVertexBuffer(void* Data, u64 Bytes, int Topology, u64 Stride);
void FreeVertexBuffer(renderer_vertex_buffer VertexBuffer);
void DrawVertices(f32* VertexData, u32 VertexDataBytes, int Topology, u32 Stride);
void DrawVertexBuffer(renderer_vertex_buffer VertexBuffer);
void SetBlendMode(blend_mode Mode);
void SetFrontCullMode(bool Value);

//Render options
void SetDepthTest(bool Value);

//Render to texture
render_output CreateRenderOutput(int Width, int Height);
void Delete(render_output* Output);
void SetShadowMap(render_output Texture);
void UnsetShadowMap();
void ClearOutput(render_output_handle Output, v4 Color = {0.2f, 0.4f, 0.6f, 1.0f});
void SetOutput(render_output_handle Output);
void SetFrameBufferAsOutput();
render_output CreateShadowDepthTexture(int Width, int Height);

//Memory
memory_arena Win32CreateMemoryArena(u64 Size, memory_arena_type Type);

//Input
void SetCursorState(bool State);

typedef u32 button_state;
enum
{
    Button_Jump     = (1 << 1),
    Button_Interact = (1 << 2),
    Button_Menu     = (1 << 3),
    Button_LMouse   = (1 << 4),
    Button_LShift   = (1 << 5),
    Button_Console  = (1 << 6),
    Button_Left     = (1 << 7),
    Button_Right    = (1 << 8),
    Button_Up       = (1 << 9),
    Button_Down     = (1 << 10),
    Button_Escape   = (1 << 11)
};

struct input_state
{
    button_state Buttons;
    v2 Cursor; // [-1, 1] is the window
    v2 Movement;
};

struct game_input
{
    button_state Button;
    button_state ButtonDown;
    button_state ButtonUp;
    
    char* TextInput;
    v2 Movement;
	v2 Cursor;
    v2 CursorDelta;
    f32 ScrollDelta;
};

// --- Game Layer ---
void UpdateAndRender(void** ApplicationData, game_assets* Assets, f32 DeltaTime, game_input* Input, allocator Allocator);
void ResizeAssets(void** ApplicationData);

struct font_asset;
v2 TextPixelSize(font_asset* Font, string String);

u64 ReadCPUTimer();
f32 CPUTimeToSeconds(u64 Ticks, u64 Frequency);
extern u64 CPUFrequency;

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

typedef u64 mesh_handle;
typedef u64 model_handle;
typedef u64 font_handle;
typedef u64 vertex_buffer_handle;
typedef u64 material_handle;

struct cube_map
{
    texture_handle Textures[6];
};

struct material;

struct render_command
{
    void* VertexData;
    u32 VertexDataStride;
    u32 VertexDataBytes;
    
    D3D11_PRIMITIVE_TOPOLOGY Topology;
    shader Shader;
    vertex_buffer_handle VertexBuffer;
    texture_handle Texture;
    m4x4 ModelTransform;
    v4 Color;
    material* Material;
    bool DisableDepthTest;
    bool DoesNotCastShadow;
    bool EnableWind;
    bool NoShadows;
};

struct font_asset
{
    texture_handle Texture;
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
    texture_handle Ambient;
    texture_handle Diffuse;
    texture_handle Normal;
    texture_handle Specular;
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

struct mesh
{
    string MaterialLibrary;
    string MaterialName;
    vertex_buffer_handle VertexBuffer;
    span<triangle> Triangles;
};

struct renderer_pbr
{
    
};

struct model
{
    string Name;
    
    mesh_handle Meshes[16];
    u64 MeshCount;
    m4x4 LocalTransform;
};

struct game_assets
{
    bool Initialised;
    allocator Allocator;
    
    font_handle Font;
    
    //New asset system
    material Materials[32];
    int MaterialCount;
    
    material DefaultMaterial;
    
    mesh Meshes[128];
    int MeshCount;
    
    model Models[128];
    int ModelCount;
    
    font_asset Fonts[8];
    int FontCount;
    
    renderer_vertex_buffer VertexBuffers[128];
    int VertexBufferCount;
    
    texture_handle ButtonTextureHandle;
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
render_command* PushTexturedRect(render_group* RenderGroup, texture_handle Texture, v3 P0, v3 P1, v2 UV0 = {0.0f, 0.0f}, v2 UV1 = {1.0f, 1.0f});
void PushTexturedRect(render_group* RenderGroup, texture Texture, v3 P0, v3 P1, v3 P2, v3 P3, v2 UV0, v2 UV1, v2 UV2, v2 UV3);
void PushVertices(render_group* RenderGroup, void* Data, u32 Bytes, u32 Stride, D3D11_PRIMITIVE_TOPOLOGY Topology, shader Shader);
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
void DrawRenderGroup(render_group* Group, shader_constants Constants, render_draw_type Type = Draw_Regular);

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
void SetModelLocalTransform(game_assets* Assets, char* ModelName, m4x4 Transform);
void SetModelLocalTransform(game_assets* Assets, model_handle Model, m4x4 Transform);

//Assets
void SetVertexBuffer(game_assets* Assets, vertex_buffer_handle* VertexBuffer, renderer_vertex_buffer Buffer);

extern f32 GlobalAspectRatio;
extern int GlobalOutputWidth;
extern int GlobalOutputHeight;

extern memory_arena GraphicsArena;

u16   const DefaultPort = 22333;
char* const DefaultPortString = "22333";

//TODO: Delete this
struct multiplayer_context;
struct platform_multiplayer_context;

struct packet
{
    u8* Data;
    u64 Length;
    u64 SenderIndex;
};

//Client API
struct client_network_thread_data
{
    char* Hostname;
    int MessagePacketSize;
    int GameStatePacketSize;
};

void         ClientNetworkThread(client_network_thread_data* Data);

span<packet> PollClientConnection(memory_arena* Arena);
bool         SendToServer(packet Packet);
bool         IsConnectedToServer();

//Server API
void         ServerNetworkThread();

span<packet> PollServerConnection(memory_arena* Arena);
void         ServerSendPacket(packet Packet, u64  ClientIndex);
void         ServerBroadcastPacket(packet Packet);

#include "Engine.cpp"