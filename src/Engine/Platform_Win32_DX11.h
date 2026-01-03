#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <d3d11_1.h>

struct renderer_shader
{
    ID3D11VertexShader* VertexShader;
    ID3D11PixelShader* PixelShader;
    ID3D11InputLayout* InputLayout;
};
typedef renderer_shader d3d11_shader;

//TODO: Get rid of this from the engine
struct font_texture
{
    stbtt_bakedchar* BakedChars;
    ID3D11ShaderResourceView* TextureView;
    
    f32 TextureWidth;
    f32 TextureHeight;
    f32 RasterisedSize;
};
typedef font_texture d3d11_font_texture;

struct d3d11_texture
{
    ID3D11ShaderResourceView* TextureView;
    int Width, Height;
};

struct render_output
{
    //Drawing to output
    ID3D11RenderTargetView* RenderTargetView;
    ID3D11DepthStencilView* DepthStencilView;
    
    //Sampling from output
    ID3D11ShaderResourceView* ShaderResourceView; //For colour information
    ID3D11ShaderResourceView* DepthStencilShaderResourceView;
    
    // Texture.TextureView is equal to ShaderResourceView
    d3d11_texture Texture;
    
    i32 Width, Height;
};
typedef render_output d3d11_render_output;

struct renderer_vertex_buffer
{
    string Description;
    
    ID3D11Buffer* Buffer;
    D3D11_PRIMITIVE_TOPOLOGY Topology;
    u32 VertexCount;
    u32 Stride;
};
typedef renderer_vertex_buffer d3d11_vertex_buffer;

static ID3D11Device1* D3D11Device;
static ID3D11DeviceContext* D3D11DeviceContext;
static ID3D11DepthStencilView* DepthBufferView;
font_texture* DefaultFont;

void CreateD3D11Device();
void CreateSamplers();
f32 D3D11TextWidth(string String, f32 Size, f32 AspectRatio = 1.0f);

#define PlatformTextWidth                D3D11TextWidth
#define PlatformCreateShadowDepthTexture D3D11CreateShadowDepthTexture
#define PlatformCreateVertexBuffer       D3D11CreateVertexBuffer
#define PlatformCreateTexture            D3D11CreateTexture
#define PlatformCreateRenderOutput       D3D11CreateRenderOutput
#define PlatformCreateVertexBuffer       D3D11CreateVertexBuffer