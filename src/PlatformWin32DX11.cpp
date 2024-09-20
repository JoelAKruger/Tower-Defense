#pragma comment(lib, "user32.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define UNICODE
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>

#include <string>

#include "Utilities.cpp"
#include "Maths.cpp"
#include "Timer.cpp"
#include "Profiler.cpp"

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_truetype.h"
#include "stb_image.h"

memory_arena GlobalDebugArena;
#define LOG(...) \
OutputDebugStringA(ArenaPrint(&GlobalDebugArena, __VA_ARGS__).Text)

static std::string GlobalTextInput;
static f32 GlobalScrollDelta;

LRESULT CALLBACK WindowProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam);

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
    v2 Cursor;
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
    f32 ScrollDelta;
};


enum render_type
{
    Render_Rectangle,
    Render_Circle,
    Render_Line,
    Render_Text,
    Render_Background,
    Render_Triangles,
    Render_Texture
};

struct tri_vertex
{
    v3 P;
    v4 Col;
};

struct triangle
{
    tri_vertex Vertices[3];
};

struct texture
{
    ID3D11SamplerState* SamplerState;
    ID3D11ShaderResourceView* TextureView;
};

struct render_output
{
    ID3D11Texture2D* Texture;
    ID3D11RenderTargetView* RenderTargetView;
    
    ID3D11DepthStencilView* DepthStencilView;
    ID3D11ShaderResourceView* DepthStencilShaderResourceView;
};

struct font_texture
{
    stbtt_bakedchar* BakedChars;
    ID3D11SamplerState* SamplerState;
    ID3D11ShaderResourceView* TextureView;
    
    f32 TextureWidth;
    f32 TextureHeight;
    f32 RasterisedSize;
};

font_texture* DefaultFont;

struct d3d11_shader
{
    ID3D11VertexShader* VertexShader;
    ID3D11PixelShader* PixelShader;
    ID3D11InputLayout* InputLayout;
};

typedef d3d11_shader shader;

//Platform Functions
void Win32DebugOut(string String);
void Win32Sleep(int Milliseconds);
span<u8> Win32LoadFile(memory_arena* Arena, char* Path);
void Win32SaveFile(char* Path, span<u8> Data);
f32 Win32TextWidth(string String, f32 FontSize, f32 AspectRatio = 1.0f);

#define PlatformDebugOut    Win32DebugOut
#define PlatformSleep       Win32Sleep
#define PlatformLoadFile    Win32LoadFile
#define PlatformSaveFile    Win32SaveFile
#define PlatformTextWidth   Win32TextWidth

void KeyboardAndMouseInputState(input_state* InputState, HWND Window);
memory_arena Win32CreateMemoryArena(u64 Size, memory_arena_type Type);
font_texture CreateFontTexture(allocator Allocator, char* Path);
texture CreateTexture(char* Path);
void Win32DrawText(font_texture Font, string Text, v2 Position, v4 Color, f32 Size, f32 AspectRatio = 1.0f);
void Win32DrawTexture(v3 P0, v3 P1, v2 UV0, v2 UV1);

void DrawVertices(f32* VertexData, u32 VertexDataBytes, D3D11_PRIMITIVE_TOPOLOGY Topology, u32 Stride = 7 * sizeof(f32));

void ClearOutput(render_output Output);
void SetOutput(render_output Output);

static ID3D11Device1* D3D11Device;
static ID3D11DeviceContext* D3D11DeviceContext;

render_output RenderOutput;

static ID3D11DepthStencilView* DepthBufferView;
memory_arena GraphicsArena;

#include "Defense.h"
#include "Win32Network.cpp"
#include "Graphics.cpp"
#include "Defense.cpp"

static bool GlobalWindowDidResize;

static void
CreateD3D11Device()
{
    ID3D11Device* BaseDevice;
    ID3D11DeviceContext* BaseDeviceContext;
    
    D3D_FEATURE_LEVEL FeatureLevels[] = {D3D_FEATURE_LEVEL_11_0};
    UINT CreationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    
#ifdef DEBUG
    CreationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    HRESULT HResult = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE,
                                        0, CreationFlags,
                                        FeatureLevels, ArrayCount(FeatureLevels),
                                        D3D11_SDK_VERSION, &BaseDevice,
                                        0, &BaseDeviceContext);
    Assert(SUCCEEDED(HResult));
    
    //Get 1.1 interface of device and context
    HResult = BaseDevice->QueryInterface(IID_PPV_ARGS(&D3D11Device));
    Assert(SUCCEEDED(HResult));
    BaseDevice->Release();
    
    HResult = BaseDeviceContext->QueryInterface(IID_PPV_ARGS(&D3D11DeviceContext));
    Assert(SUCCEEDED(HResult));
    BaseDeviceContext->Release();
    
#ifdef DEBUG
    //Setup debug to break on D3D11 error
    ID3D11Debug* D3DDebug;
    D3D11Device->QueryInterface(IID_PPV_ARGS(&D3DDebug));
    if (D3DDebug)
    {
        ID3D11InfoQueue* D3DInfoQueue;
        HResult = D3DDebug->QueryInterface(IID_PPV_ARGS(&D3DInfoQueue));
        if (SUCCEEDED(HResult))
        {
            D3DInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
            D3DInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
            D3DInfoQueue->Release();
        }
        D3DDebug->Release();
    }
#endif
}

IDXGISwapChain1* CreateD3D11SwapChain(HWND Window)
{
    //Get DXGI Adapter
    IDXGIDevice1* DXGIDevice;
    
    HRESULT HResult = D3D11Device->QueryInterface(IID_PPV_ARGS(&DXGIDevice));
    Assert(SUCCEEDED(HResult));
    
    IDXGIAdapter* DXGIAdapter;
    HResult = DXGIDevice->GetAdapter(&DXGIAdapter);
    Assert(SUCCEEDED(HResult));
    
    DXGIDevice->Release();
    
    DXGI_ADAPTER_DESC AdapterDesc;
    DXGIAdapter->GetDesc(&AdapterDesc);
    
    OutputDebugStringA("Graphics Device: ");
    OutputDebugStringW(AdapterDesc.Description);
    
    //Get DXGI Factory
    IDXGIFactory2* DXGIFactory;
    HResult = DXGIAdapter->GetParent(IID_PPV_ARGS(&DXGIFactory));
    DXGIAdapter->Release();
    
    DXGI_SWAP_CHAIN_DESC1 SwapChainDesc = {};
    SwapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    SwapChainDesc.SampleDesc.Count = 4;
    SwapChainDesc.SampleDesc.Quality = 0;
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.BufferCount = 2;
    SwapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    SwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    SwapChainDesc.Flags = 0;
    
    IDXGISwapChain1* SwapChain;
    HResult = DXGIFactory->CreateSwapChainForHwnd(D3D11Device, Window, &SwapChainDesc, 0, 0, &SwapChain);
    Assert(SUCCEEDED(HResult));
    
    DXGIFactory->Release();
    
    return SwapChain;
}

static render_output
CreateRenderOutput(IDXGISwapChain1* SwapChain)
{
    render_output Result = {};
    
    //Get frame buffer
    ID3D11Texture2D* FrameBuffer;
    HRESULT HResult = SwapChain->GetBuffer(0, IID_PPV_ARGS(&FrameBuffer));
    Assert(SUCCEEDED(HResult));
    
    //Get dimensions and set aspect ratio
    D3D11_TEXTURE2D_DESC FrameBufferDesc;
    FrameBuffer->GetDesc(&FrameBufferDesc);
    GlobalAspectRatio = (f32)FrameBufferDesc.Width / (f32)FrameBufferDesc.Height;
    
    HResult = D3D11Device->CreateRenderTargetView(FrameBuffer, 0, &Result.RenderTargetView);
    Assert(SUCCEEDED(HResult));
    FrameBuffer->Release();
    
    //Create depth buffer
    D3D11_TEXTURE2D_DESC DepthBufferDesc;
    FrameBuffer->GetDesc(&DepthBufferDesc);
    DepthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    DepthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    
    ID3D11Texture2D* DepthBuffer;
    HResult = D3D11Device->CreateTexture2D(&DepthBufferDesc, 0, &DepthBuffer);
    Assert(SUCCEEDED(HResult));
    
    HResult = D3D11Device->CreateDepthStencilView(DepthBuffer, 0, &Result.DepthStencilView);
    Assert(SUCCEEDED(HResult));
    
    DepthBuffer->Release();
    
    return Result;
}

static void
ClearRenderOutput(render_output Output)
{
    if (Output.Texture) Output.Texture->Release();
    if (Output.RenderTargetView) Output.RenderTargetView->Release();
    if (Output.DepthStencilView) Output.DepthStencilView->Release();
    if (Output.DepthStencilShaderResourceView) Output.DepthStencilShaderResourceView->Release();
}

d3d11_shader CreateShader(wchar_t* Path, D3D11_INPUT_ELEMENT_DESC* InputElementDesc, u32 InputElementDescCount)
{
    d3d11_shader Result = {};
    
    //Create Vertex Shader
    ID3DBlob* VertexShaderBlob;
    ID3DBlob* CompileErrorsBlob;
    HRESULT HResult = D3DCompileFromFile(Path, 0, 0, "vs_main", "vs_5_0", 0, 0, &VertexShaderBlob, &CompileErrorsBlob);
    
    if (FAILED(HResult))
    {
        if (HResult == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
        {
            OutputDebugStringA("Failed to compile shader, file not found");
        }
        else if (CompileErrorsBlob)
        {
            OutputDebugStringA("Failed to compile shader:");
            OutputDebugStringA((char*)CompileErrorsBlob->GetBufferPointer());
            CompileErrorsBlob->Release();
        }
    }
    
    HResult = D3D11Device->CreateVertexShader(VertexShaderBlob->GetBufferPointer(), VertexShaderBlob->GetBufferSize(), 0, &Result.VertexShader);
    Assert(SUCCEEDED(HResult));
    
    //Create Pixel Shader
    ID3DBlob* PixelShaderBlob;
    HResult = D3DCompileFromFile(Path, 0, 0, "ps_main", "ps_5_0", 0, 0, &PixelShaderBlob, &CompileErrorsBlob);
    
    if (FAILED(HResult))
    {
        if (HResult == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
        {
            OutputDebugStringA("Failed to compile shader, file not found");
        }
        else if (CompileErrorsBlob)
        {
            OutputDebugStringA("Failed to compile shader:");
            OutputDebugStringA((char*)CompileErrorsBlob->GetBufferPointer());
            CompileErrorsBlob->Release();
        }
    }
    
    HResult = D3D11Device->CreatePixelShader(PixelShaderBlob->GetBufferPointer(), PixelShaderBlob->GetBufferSize(), 0, &Result.PixelShader);
    Assert(SUCCEEDED(HResult));
    PixelShaderBlob->Release();
    
    HResult = D3D11Device->CreateInputLayout(InputElementDesc, InputElementDescCount, 
                                             VertexShaderBlob->GetBufferPointer(), VertexShaderBlob->GetBufferSize(), 
                                             &Result.InputLayout);
    Assert(SUCCEEDED(HResult));
    VertexShaderBlob->Release();
    
    return Result;
}

void DrawVertices(f32* VertexData, u32 VertexDataBytes, D3D11_PRIMITIVE_TOPOLOGY Topology, u32 Stride)
{
    if (VertexDataBytes == 0)
    {
        return;
    }
    
    u32 VertexCount = VertexDataBytes / Stride;
    u32 Offset = 0;
    
    D3D11_BUFFER_DESC VertexBufferDesc = {};
    VertexBufferDesc.ByteWidth = VertexDataBytes;
    VertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    
    D3D11_SUBRESOURCE_DATA VertexSubresourceData = { VertexData };
    
    ID3D11Buffer* VertexBuffer;
    HRESULT HResult = D3D11Device->CreateBuffer(&VertexBufferDesc, &VertexSubresourceData, &VertexBuffer);
    Assert(SUCCEEDED(HResult));
    
    //Draw
    D3D11DeviceContext->IASetPrimitiveTopology(Topology);
    D3D11DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
    D3D11DeviceContext->Draw(VertexCount, 0);
    
    VertexBuffer->Release();
}

void DrawTris(f32* VertexData, u32 VertexDataBytes)
{
    DrawVertices(VertexData, VertexDataBytes, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

int WINAPI wWinMain(HINSTANCE Instance, HINSTANCE, LPWSTR CommandLine, int ShowCode)
{
    //Required for profiling
    GetCPUFrequency(10);
    
    WNDCLASS WindowClass = {};
    WindowClass.lpfnWndProc = WindowProc;
    WindowClass.hInstance = Instance;
    WindowClass.hCursor = LoadCursor(0, IDC_ARROW);
    WindowClass.lpszClassName = L"MainWindow";
    
    if (!RegisterClass(&WindowClass))
    {
        return -1;
    }
    
    int DefaultWidth = 1280, DefaultHeight = 720;
    
    RECT ClientRect = {0, 0, DefaultWidth, DefaultHeight};
    AdjustWindowRect(&ClientRect, WS_OVERLAPPEDWINDOW, FALSE);
    
    HWND Window = CreateWindow(WindowClass.lpszClassName,
                               L"Puzzle Game",
                               WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                               CW_USEDEFAULT, CW_USEDEFAULT,
                               ClientRect.right - ClientRect.left,
                               ClientRect.bottom - ClientRect.top,
                               0, 0, Instance, 0);
    
    if (!Window)
    {
        return -1;
    }
    
    CreateD3D11Device();
    IDXGISwapChain1* SwapChain = CreateD3D11SwapChain(Window);
    RenderOutput = CreateRenderOutput(SwapChain);
    
    D3D11_INPUT_ELEMENT_DESC InputElementDesc[] = 
    {
        {"POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    
    d3d11_shader Shader = CreateShader(L"assets/shaders.hlsl", InputElementDesc, ArrayCount(InputElementDesc));
    ColorShader = Shader;
    
    d3d11_shader BackgroundShader = CreateShader(L"assets/background.hlsl", InputElementDesc, ArrayCount(InputElementDesc));
    
    D3D11_INPUT_ELEMENT_DESC FontShaderInputElementDesc[] = 
    {
        {"POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEX", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    FontShader = CreateShader(L"assets/fontshaders.hlsl", FontShaderInputElementDesc, ArrayCount(FontShaderInputElementDesc));
    
    D3D11_INPUT_ELEMENT_DESC TextureShaderElementDesc[] = 
    {
        {"POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEX", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    TextureShader = CreateShader(L"assets/texture.hlsl", TextureShaderElementDesc, ArrayCount(TextureShaderElementDesc));
    WaterShader = CreateShader(L"assets/water.hlsl", TextureShaderElementDesc, ArrayCount(TextureShaderElementDesc));
    
    D3D11_INPUT_ELEMENT_DESC ModelShaderElementDesc[] = 
    {
        {"POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    
    ModelShader = CreateShader(L"assets/modelshader.hlsl", ModelShaderElementDesc, ArrayCount(ModelShaderElementDesc));
    
    D3D11_BLEND_DESC BlendDesc = {};
    BlendDesc.AlphaToCoverageEnable = false;
    BlendDesc.IndependentBlendEnable = false;
    BlendDesc.RenderTarget[0].BlendEnable = true;
    BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    
    BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    BlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    
    ID3D11BlendState* BlendState;
    HRESULT HResult = D3D11Device->CreateBlendState(&BlendDesc, &BlendState);
    Assert(SUCCEEDED(HResult));
    D3D11DeviceContext->OMSetBlendState(BlendState, NULL, 0xFFFFFFFF);
    
    SetDepthTest(false);
    
    memory_arena TransientArena = Win32CreateMemoryArena(Megabytes(16), TRANSIENT);
    memory_arena PermanentArena = Win32CreateMemoryArena(Megabytes(16), PERMANENT);
    GraphicsArena = Win32CreateMemoryArena(Megabytes(16), TRANSIENT);
    GlobalDebugArena = Win32CreateMemoryArena(Megabytes(1), PERMANENT);
    
    allocator Allocator = {};
    Allocator.Transient = &TransientArena;
    Allocator.Permanent = &PermanentArena;
    
    LARGE_INTEGER CounterFrequency;
    QueryPerformanceFrequency(&CounterFrequency); //Counts per second
    
    int TargetFrameRate = 60;
    float SecondsPerFrame = 1.0f / (float)TargetFrameRate;
    int CountsPerFrame = (int)(CounterFrequency.QuadPart / TargetFrameRate);
    
    game_input PreviousInput = {};
    
    memory_arena PerFrameDebugInfoArena = CreateSubArena(&PermanentArena, Kilobytes(4), TRANSIENT);
    
    texture Texture = CreateTexture("assets/world.png");
    BackgroundTexture = Texture;
    
    TowerTexture     = CreateTexture("assets/tower.png");
    TargetTexture    = CreateTexture("assets/target.png");
    ExplosionTexture = CreateTexture("assets/explosion.png");
    
    font_texture FontTexture = CreateFontTexture(Allocator, "assets/LiberationMono-Regular.ttf");
    DefaultFont = &FontTexture;
    
    dynamic_array<string> Profile = {};
    
    app_state AppState = {};
    
    while (true)
    {
        BeginProfile();
        
        LARGE_INTEGER StartCount;
        QueryPerformanceCounter(&StartCount);
        
        MSG Message;
        while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
        {
            if (Message.message == WM_QUIT)
            {
                return 0;
            }
            
            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }
        
        if (GlobalWindowDidResize)
        {
            D3D11DeviceContext->OMSetRenderTargets(0, 0, 0);
            ClearRenderOutput(RenderOutput);
            //FrameBufferView->Release();
            //DepthBufferView->Release();
            
            HRESULT Result = SwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
            Assert(SUCCEEDED(Result));
            
            
            RenderOutput = CreateRenderOutput(SwapChain);
            
            /*
            ID3D11Texture2D* FrameBuffer;
            Result = SwapChain->GetBuffer(0, IID_PPV_ARGS(&FrameBuffer));
            Assert(SUCCEEDED(Result));
            
            //D3D11_TEXTURE2D_DESC FrameBufferDesc;
            //FrameBuffer->GetDesc(&FrameBufferDesc);
            
            //GlobalAspectRatio = (f32)FrameBufferDesc.Width / (f32)FrameBufferDesc.Height;
            
            Result = D3D11Device->CreateRenderTargetView(FrameBuffer, 0, &FrameBufferView);
            Assert(SUCCEEDED(Result));
            FrameBuffer->Release();
*/
            
            GlobalWindowDidResize = false;
        }
        
        //-------------------------
        
        game_input Input = {};
        input_state CurrentInputState = {};
        
        if (GetActiveWindow())
        {
            KeyboardAndMouseInputState(&CurrentInputState, Window);
            
#if USE_GAME_INPUT_API
            ControllerState(&CurrentInputState, GameInput);
#endif
        }
        
        Input.Button = CurrentInputState.Buttons;
        Input.ButtonDown = (~PreviousInput.Button & CurrentInputState.Buttons);
        Input.ButtonUp = (PreviousInput.Button & ~CurrentInputState.Buttons);
        Input.Movement = CurrentInputState.Movement;
        
        if (LengthSq(Input.Movement) > 1.0f)
        {
            Input.Movement = UnitV(Input.Movement);
        }
        
        Input.Cursor = CurrentInputState.Cursor;
        Input.TextInput = (char*)GlobalTextInput.c_str();
        
        Input.ScrollDelta = GlobalScrollDelta;
        GlobalScrollDelta = 0;
        
        PreviousInput = Input;
        ResetArena(&TransientArena);
        ResetArena(&GraphicsArena);
        
        {
            TimeBlock("Clear");
            ClearOutput(RenderOutput);
            
            RECT WindowRect;
            GetClientRect(Window, &WindowRect);
            D3D11_VIEWPORT Viewport = { 0.0f, 0.0f, (FLOAT)(WindowRect.right - WindowRect.left), (FLOAT)(WindowRect.bottom - WindowRect.top), 0.0f, 1.0f };
            D3D11DeviceContext->RSSetViewports(1, &Viewport);
            SetOutput(RenderOutput);
        }
        
        {
            TimeBlock("Update & Render");
            UpdateAndRender(&AppState, SecondsPerFrame, &Input, Allocator);
        }
        
        // Draw profiling information
        SetTransform(IdentityTransform());
        SetShader(FontShader);
        f32 X = -0.99f;
        f32 Y = 0.95f;
        for (string Text : Profile)
        {
            DrawGUIString(Text, V2(X, Y), V4(0.75f, 0.75f, 0.75f, 1.0f));
            Y -= 0.06f;
        }
        
        {
            TimeBlock("Present");
            SwapChain->Present(0, 0);
        }
        //---------------------------
        
        GlobalTextInput.clear();
        
        LARGE_INTEGER PerformanceCount;
        QueryPerformanceCounter(&PerformanceCount);
        if (PerformanceCount.QuadPart - StartCount.QuadPart > CountsPerFrame)
        {
            OutputDebugStringA("Can't keep up\n");
        }
        float TimeTaken = (float)(PerformanceCount.QuadPart - StartCount.QuadPart) / CounterFrequency.QuadPart;
        float CurrentFrameRate = 1.0f / TimeTaken;
        
        while (PerformanceCount.QuadPart - StartCount.QuadPart < CountsPerFrame)
        {
            QueryPerformanceCounter(&PerformanceCount);
        }
        
        ResetArena(&PerFrameDebugInfoArena);
        Profile = GetProfileReadout(&PerFrameDebugInfoArena);
    }
}

void SetShader(d3d11_shader Shader)
{
    D3D11DeviceContext->IASetInputLayout(Shader.InputLayout);
    D3D11DeviceContext->VSSetShader(Shader.VertexShader, 0, 0);
    D3D11DeviceContext->PSSetShader(Shader.PixelShader, 0, 0);
}


LRESULT CALLBACK WindowProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;
    switch (Message)
    {
        case WM_DESTROY:
        {
            PostQuitMessage(0);
        } break;
        case WM_CHAR:
        {
            Assert(WParam < 128);
            char Char = (char)WParam;
            
            if (Char == '\r')
            {
                Char = '\n';
            }
            
            GlobalTextInput.append(1, Char);
        } break;
        
        case WM_SIZE:
        {
            GlobalWindowDidResize = true;
        } break;
        
        case WM_MOUSEWHEEL:
        {
            int ZDelta = GET_WHEEL_DELTA_WPARAM(WParam);
            GlobalScrollDelta += ((f32)ZDelta / 120.0f);
        } break;
        
        default:
        {
            Result = DefWindowProc(Window, Message, WParam, LParam);
        }
    }
    return Result;
}

static span<u8> 
Win32LoadFile(memory_arena* Arena, char* Path)
{
    span<u8> Result = {};
    bool Success = false;
    
    HANDLE File = CreateFileA(Path, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    
    if (File != INVALID_HANDLE_VALUE)
    {
        u64 FileSize;
        if (GetFileSizeEx(File, (LARGE_INTEGER*)&FileSize))
        {
            Result.Memory = Alloc(Arena, FileSize);
            DWORD Length;
            if (ReadFile(File, Result.Memory, (u32)FileSize, &Length, 0))
            {
                Success = true;
                Result.Count = Length;
            }
        }
        CloseHandle(File);
    }
    
#if DEBUG
    if (Success)
        LOG("Opened file: %s\n", Path);
    else
        LOG("Could not open file: %s\n", Path); 
#endif
    
    return Result;
}

static void
Win32SaveFile(char* Path, span<u8> Data)
{
    HANDLE File = CreateFileA(Path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    
    bool Success = false;
    
    if (File != INVALID_HANDLE_VALUE)
    {
        DWORD BytesWritten;
        if (WriteFile(File, Data.Memory, Data.Count, &BytesWritten, 0))
        {
            Success = true;
        }
        CloseHandle(File);
    }
    
#if DEBUG
    if (Success)
        LOG("Saved file: %s\n", Path);
    else
        LOG("Could not save file: %s\n", Path); 
#endif
}

void Win32DebugOut(string String)
{
    OutputDebugStringA(String.Text);
}

void Win32Sleep(int Milliseconds)
{
    Sleep(Milliseconds);
}

static memory_arena
Win32CreateMemoryArena(u64 Size, memory_arena_type Type)
{
    memory_arena Arena = {};
    
    Arena.Buffer = (u8*)VirtualAlloc(0, Size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    Arena.Size = Size;
    Arena.Type = Type;
    
    return Arena;
}

static void
KeyboardAndMouseInputState(input_state* InputState, HWND Window)
{
    //Buttons
    if (GetAsyncKeyState(VK_SPACE) & 0x8000)
        InputState->Buttons |= Button_Jump;
    if (GetAsyncKeyState('E') & 0x8000)
        InputState->Buttons |= Button_Interact;
    if (GetAsyncKeyState('C') & 0x8000)
        InputState->Buttons |= Button_Menu;
    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
        InputState->Buttons |= Button_LMouse;
    if (GetAsyncKeyState(VK_LSHIFT) & 0x8000)
        InputState->Buttons |= Button_LShift;
    if (GetAsyncKeyState(VK_F1) & 0x8000)
        InputState->Buttons |= Button_Console;
    if (GetAsyncKeyState(VK_LEFT) & 0x8000)
        InputState->Buttons |= Button_Left;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
        InputState->Buttons |= Button_Right;
    if (GetAsyncKeyState(VK_UP) & 0x8000)
        InputState->Buttons |= Button_Up;
    if (GetAsyncKeyState(VK_DOWN) & 0x8000)
        InputState->Buttons |= Button_Down;
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
        InputState->Buttons |= Button_Escape;
    
    //Movement
    if ((GetAsyncKeyState('A') & 0x8000))
        InputState->Movement.X -= 1.0f;
    if ((GetAsyncKeyState('D') & 0x8000))
        InputState->Movement.X += 1.0f;
    if ((GetAsyncKeyState('W') & 0x8000))
        InputState->Movement.Y += 1.0f;
    if ((GetAsyncKeyState('S') & 0x8000))
        InputState->Movement.Y -= 1.0f;
    
    //Cursor
    //TODO: Call OpenInputDesktop() to determine whether the current desktop is the input desktop
    POINT CursorPos = {};
    GetCursorPos(&CursorPos);
    ScreenToClient(Window, &CursorPos);
    
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    
    i32 WindowWidth = ClientRect.right - ClientRect.left;
    i32 WindowHeight = ClientRect.bottom - ClientRect.top;
    
    if (WindowWidth > 0 && WindowHeight > 0)
    {
        InputState->Cursor.X = -1.0f + 2.0f * (f32)CursorPos.x / WindowWidth;
        InputState->Cursor.Y = -1.0f + 2.0f * (f32)(WindowHeight - CursorPos.y) / WindowHeight;
    }
}

stbtt_bakedchar BakedChars[128];

font_texture CreateFontTexture(allocator Allocator, char* Path)
{
    f32 RasterisedSize = 64.0f;
    
    font_texture Result = {};
    Result.RasterisedSize = RasterisedSize;
    
    //Bake characters
    u32 TextureWidth = 512;
    u32 TextureHeight = 512;
    
    Result.TextureWidth = (f32)TextureWidth;
    Result.TextureHeight = (f32)TextureHeight;
    
    span<u8> TrueTypeFile = Win32LoadFile(Allocator.Transient, Path);
    
    u8* TempBuffer = Alloc(Allocator.Transient, TextureWidth * TextureHeight * sizeof(u8));
    
    stbtt_BakeFontBitmap(TrueTypeFile.Memory, 0, RasterisedSize, TempBuffer, TextureWidth, TextureHeight, 0, 128, BakedChars);
    
    //Create Sampler
    D3D11_SAMPLER_DESC SamplerDesc = {};
    SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    SamplerDesc.BorderColor[0] = 1.0f;
    SamplerDesc.BorderColor[1] = 1.0f;
    SamplerDesc.BorderColor[2] = 1.0f;
    SamplerDesc.BorderColor[3] = 1.0f;
    SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    
    D3D11Device->CreateSamplerState(&SamplerDesc, &Result.SamplerState);
    
    D3D11_TEXTURE2D_DESC TextureDesc = {};
    TextureDesc.Width = TextureWidth;
    TextureDesc.Height = TextureHeight;
    TextureDesc.MipLevels = 1;
    TextureDesc.ArraySize = 1;
    TextureDesc.Format = DXGI_FORMAT_R8_UNORM;
    TextureDesc.SampleDesc.Count = 1;
    TextureDesc.Usage = D3D11_USAGE_IMMUTABLE;
    TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    
    D3D11_SUBRESOURCE_DATA TextureSubresourceData = {};
    TextureSubresourceData.pSysMem = TempBuffer;
    TextureSubresourceData.SysMemPitch = TextureWidth * sizeof(u8);
    
    ID3D11Texture2D* Texture;
    D3D11Device->CreateTexture2D(&TextureDesc, &TextureSubresourceData, &Texture);
    
    D3D11Device->CreateShaderResourceView(Texture, 0, &Result.TextureView);
    
    Result.BakedChars = BakedChars;
    return Result;
}

static texture
CreateTexture(char* Path)
{
    texture Result = {};
    
    int Width = 0, Height = 0, Channels = 0;
    stbi_set_flip_vertically_on_load(true);
    stbi_uc* TextureData = stbi_load(Path, &Width, &Height, &Channels, 4);
    
    D3D11_SAMPLER_DESC SamplerDesc = {};
    SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    SamplerDesc.BorderColor[0] = 1.0f;
    SamplerDesc.BorderColor[1] = 1.0f;
    SamplerDesc.BorderColor[2] = 1.0f;
    SamplerDesc.BorderColor[3] = 1.0f;
    SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    
    D3D11Device->CreateSamplerState(&SamplerDesc, &Result.SamplerState);
    
    D3D11_TEXTURE2D_DESC TextureDesc = {};
    TextureDesc.Width = Width;
    TextureDesc.Height = Height;
    TextureDesc.MipLevels = 1;
    TextureDesc.ArraySize = 1;
    TextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    TextureDesc.SampleDesc.Count = 1;
    TextureDesc.Usage = D3D11_USAGE_IMMUTABLE;
    TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    
    D3D11_SUBRESOURCE_DATA TextureSubresourceData = {};
    TextureSubresourceData.pSysMem = TextureData;
    TextureSubresourceData.SysMemPitch = Width * sizeof(u32);
    
    ID3D11Texture2D* Texture;
    D3D11Device->CreateTexture2D(&TextureDesc, &TextureSubresourceData, &Texture);
    
    D3D11Device->CreateShaderResourceView(Texture, 0, &Result.TextureView);
    
    free(TextureData);
    
    return Result;
}

static render_output
CreateShadowDepthTexture(int Width, int Height)
{
    //Create depth map
    D3D11_TEXTURE2D_DESC Desc = {};
    Desc.Width = Width;
    Desc.Height = Height;
    Desc.MipLevels = 1;
    Desc.ArraySize = 1;
    Desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    Desc.SampleDesc.Count = 1;
    Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
    
    render_output Result = {};
    D3D11Device->CreateTexture2D(&Desc, 0, &Result.Texture);
    
    //Create depth stencil view
    D3D11_DEPTH_STENCIL_VIEW_DESC DepthStencilViewDesc = {};
    DepthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    DepthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    
    D3D11Device->CreateDepthStencilView(Result.Texture, &DepthStencilViewDesc, &Result.DepthStencilView);
    
    //Create shader resource view
    D3D11_SHADER_RESOURCE_VIEW_DESC ShaderResourceViewDesc = {};
    ShaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    ShaderResourceViewDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    ShaderResourceViewDesc.Texture2D.MipLevels = 1;
    D3D11Device->CreateShaderResourceView(Result.Texture, &ShaderResourceViewDesc, &Result.DepthStencilShaderResourceView);
    
    return Result;
}

struct char_vertex
{
    v3 Position;
    v2 UV;
    v4 Color;
};

void Win32DrawText(font_texture Font, string Text, v2 Position, v4 Color, f32 Size, f32 AspectRatio)
{
    f32 FontTexturePixelsToScreenY = Size / Font.RasterisedSize;
    f32 FontTexturePixelsToScreenX = FontTexturePixelsToScreenY / AspectRatio;
    
    f32 X = Position.X;
    f32 Y = Position.Y;
    
    u32 Stride = sizeof(char_vertex);
    u32 Offset = 0;
    u32 VertexCount = 6 * Text.Length;
    
    char_vertex* VertexData = AllocArray(&GraphicsArena, char_vertex, VertexCount);
    
    for (u32 I = 0; I < Text.Length; I++)
    {
        uint8_t Char = (uint8_t)Text.Text[I];
        Assert(Char < 128);
        stbtt_bakedchar BakedChar = Font.BakedChars[Char];
        
        f32 X0 = X + BakedChar.xoff * FontTexturePixelsToScreenX;
        f32 Y1 = Y - BakedChar.yoff * FontTexturePixelsToScreenY; //+ 0.5f * Font.RasterisedSize * FontTexturePixelsToScreen;
        
        f32 Width = FontTexturePixelsToScreenX * (f32)(BakedChar.x1 - BakedChar.x0);
        f32 Height = FontTexturePixelsToScreenY * (f32)(BakedChar.y1 - BakedChar.y0);
        
        VertexData[6 * I + 0].Position = {X0, Y1 - Height};
        VertexData[6 * I + 1].Position = {X0, Y1};
        VertexData[6 * I + 2].Position = {X0 + Width, Y1};
        VertexData[6 * I + 3].Position = {X0 + Width, Y1};
        VertexData[6 * I + 4].Position = {X0 + Width, Y1 - Height};
        VertexData[6 * I + 5].Position = {X0, Y1 - Height};
        
        VertexData[6 * I + 0].UV = {BakedChar.x0 / Font.TextureWidth, BakedChar.y1 / Font.TextureHeight};
        VertexData[6 * I + 1].UV = {BakedChar.x0 / Font.TextureWidth, BakedChar.y0 / Font.TextureHeight};
        VertexData[6 * I + 2].UV = {BakedChar.x1 / Font.TextureWidth, BakedChar.y0 / Font.TextureHeight};
        VertexData[6 * I + 3].UV = {BakedChar.x1 / Font.TextureWidth, BakedChar.y0 / Font.TextureHeight};
        VertexData[6 * I + 4].UV = {BakedChar.x1 / Font.TextureWidth, BakedChar.y1 / Font.TextureHeight};
        VertexData[6 * I + 5].UV = {BakedChar.x0 / Font.TextureWidth, BakedChar.y1 / Font.TextureHeight};
        
        VertexData[6 * I + 0].Color = Color;
        VertexData[6 * I + 1].Color = Color;
        VertexData[6 * I + 2].Color = Color;
        VertexData[6 * I + 3].Color = Color;
        VertexData[6 * I + 4].Color = Color;
        VertexData[6 * I + 5].Color = Color;
        
        X += BakedChar.xadvance * FontTexturePixelsToScreenX;
    }
    
    D3D11_BUFFER_DESC VertexBufferDesc = {};
    VertexBufferDesc.ByteWidth = VertexCount * sizeof(char_vertex);
    VertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    
    D3D11_SUBRESOURCE_DATA VertexSubresourceData = { VertexData };
    
    ID3D11Buffer* VertexBuffer;
    HRESULT HResult = D3D11Device->CreateBuffer(&VertexBufferDesc, &VertexSubresourceData, &VertexBuffer);
    Assert(SUCCEEDED(HResult));
    
    D3D11DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    D3D11DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
    
    D3D11DeviceContext->PSSetShaderResources(0, 1, &Font.TextureView);
    D3D11DeviceContext->PSSetSamplers(0, 1, &Font.SamplerState);
    
    D3D11DeviceContext->Draw(VertexCount, 0);
    
    VertexBuffer->Release();
}

f32 Win32TextWidth(string String, f32 Size, f32 AspectRatio)
{
    f32 Result = 0.0f;
    f32 FontTexturePixelsToScreenX = (Size / DefaultFont->RasterisedSize) / AspectRatio;
    for (u32 I = 0; I < String.Length; I++)
    {
        stbtt_bakedchar* BakedChar = DefaultFont->BakedChars + String.Text[I];
        Result += BakedChar->xadvance * FontTexturePixelsToScreenX;
    }
    return Result;
}

static void
SetVertexShaderConstant(u32 Index, void* Data, u32 Bytes)
{
    D3D11_BUFFER_DESC ConstantBufferDesc = {};
    ConstantBufferDesc.ByteWidth = Bytes;
    ConstantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    ConstantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ConstantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    
    D3D11_SUBRESOURCE_DATA SubresourceData = {Data};
    ID3D11Buffer* Buffer;
    D3D11Device->CreateBuffer(&ConstantBufferDesc, &SubresourceData, &Buffer);
    
    D3D11DeviceContext->VSSetConstantBuffers(Index, 1, &Buffer);
    
    Buffer->Release();
}

static void
SetTexture(texture Texture)
{
    D3D11DeviceContext->PSSetShaderResources(0, 1, &Texture.TextureView);
    D3D11DeviceContext->PSSetSamplers(0, 1, &Texture.SamplerState);
}

static void
SetDepthTest(bool Value)
{
    //TODO: Do not create the depth stencil state every frame
    D3D11_DEPTH_STENCIL_DESC DepthStencilDesc = {};
    DepthStencilDesc.DepthEnable = Value;
    DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    DepthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
    
    ID3D11DepthStencilState* DepthStencilState;
    D3D11Device->CreateDepthStencilState(&DepthStencilDesc, &DepthStencilState);
    
    D3D11DeviceContext->OMSetDepthStencilState(DepthStencilState, 1);
    
    DepthStencilState->Release();
}

static void
ClearOutput(render_output Output)
{
    if (Output.RenderTargetView)
    {
        FLOAT Color[4] = {0.2f, 0.4f, 0.6f, 1.0f};
        D3D11DeviceContext->ClearRenderTargetView(Output.RenderTargetView, Color);
    }
    
    if (Output.DepthStencilView)
    {
        D3D11DeviceContext->ClearDepthStencilView(Output.DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
    }
}

static void
SetOutput(render_output Output)
{
    if (Output.RenderTargetView)
    {
        D3D11DeviceContext->OMSetRenderTargets(1, &Output.RenderTargetView, Output.DepthStencilView);
    }
    else
    {
        D3D11DeviceContext->OMSetRenderTargets(0, 0, Output.DepthStencilView);
    }
}

static_assert(__COUNTER__ <= ArrayCount(GlobalProfileEntries), "Too many profiles");
