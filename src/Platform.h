#include <stdint.h>

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

struct texture;
struct shader;
struct render_output;
struct renderer_vertex_buffer;
struct font_texture;
struct game_assets;

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
};

// --- Platform Layer ---

//Files
span<u8> LoadFile(memory_arena* Arena, char* Path);

//Textures
void SetTexture(texture Texture, int Index = 0);
void SetFontTexture(font_texture Texture);
void UnsetTexture(int Index = 0);
void DeleteTexture(texture* Texture);
texture CreateTexture(char* Path);
texture CreateTexture(u32* TextureData, int Width, int Height, int Channels = 4);

//Shaders
void LoadShaders(game_assets* Assets);
void SetShader(shader Shader);
void SetGraphicsShaderConstants(shader_constants Constants);
void SetGUIShaderConstant(m4x4 Transform);

//Rendering
renderer_vertex_buffer CreateVertexBuffer(void* Data, u64 Bytes, D3D11_PRIMITIVE_TOPOLOGY Topology, u64 Stride);
void FreeVertexBuffer(renderer_vertex_buffer VertexBuffer);
void DrawVertices(f32* VertexData, u32 VertexDataBytes, D3D11_PRIMITIVE_TOPOLOGY Topology, u32 Stride);
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
void ClearOutput(render_output Output, v4 Color = {0.2f, 0.4f, 0.6f, 1.0f});
void SetOutput(render_output Output);
void SetFrameBufferAsOutput();
render_output CreateShadowDepthTexture(int Width, int Height);

//Memory
memory_arena Win32CreateMemoryArena(u64 Size, memory_arena_type Type);

//Debug
void Assert(bool Value);
void Log(char* Format, ...);

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

struct app_state;

// --- Game Layer ---
void UpdateAndRender(app_state* App, f32 DeltaTime, game_input* Input, allocator Allocator);