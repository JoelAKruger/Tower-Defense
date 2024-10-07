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

void Win32DebugOut(string String);
void Win32Sleep(int Milliseconds);
span<u8> Win32LoadFile(memory_arena* Arena, char* Path);
void Win32SaveFile(char* Path, span<u8> Data);

#define PlatformDebugOut    Win32DebugOut
#define PlatformSleep       Win32Sleep
#define PlatformLoadFile    Win32LoadFile
#define PlatformSaveFile    Win32SaveFile

#include "D3D11.h"
#include "Renderer.h"
#include "Defense.h"
#include "Platform.h"
#include "Input.h"

#include "Win32Network.cpp"
#include "Graphics.cpp"
#include "Defense.cpp"
#include "D3D11.cpp"
#include "Renderer.cpp"

static std::string GlobalTextInput;
static f32 GlobalScrollDelta;

LRESULT CALLBACK WindowProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam);

//Platform Functions
void KeyboardAndMouseInputState(input_state* InputState, HWND Window);
memory_arena Win32CreateMemoryArena(u64 Size, memory_arena_type Type);
font_texture CreateFontTexture(allocator Allocator, char* Path);
texture CreateTexture(char* Path);
void Win32DrawText(font_texture Font, string Text, v2 Position, v4 Color, f32 Size, f32 AspectRatio);
void Win32DrawTexture(v3 P0, v3 P1, v2 UV0, v2 UV1);

static bool GlobalWindowDidResize;


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
    
    //Set default shadow map comparison
    D3D11_SAMPLER_DESC SamplerDesc = {};
    SamplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR; //TODO: Add low graphics option (point filtering)
    SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR;
    SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_MIRROR;
    SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS; // Shadow map comparison
    SamplerDesc.MinLOD = 0;
    SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    SamplerDesc.MipLODBias = 0.0f;
    SamplerDesc.MaxAnisotropy = 1;
    
    ID3D11SamplerState* ShadowSamplerState;
    D3D11Device->CreateSamplerState(&SamplerDesc, &ShadowSamplerState);
    D3D11DeviceContext->PSSetSamplers(1, 1, &ShadowSamplerState);
    
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

/*
static void
SetPixelShaderConstant(u32 Index, void* Data, u32 Bytes)
{
    D3D11_BUFFER_DESC ConstantBufferDesc = {};
    ConstantBufferDesc.ByteWidth = Bytes;
    ConstantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    ConstantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ConstantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    
    D3D11_SUBRESOURCE_DATA SubresourceData = {Data};
    ID3D11Buffer* Buffer;
    D3D11Device->CreateBuffer(&ConstantBufferDesc, &SubresourceData, &Buffer);
    
    D3D11DeviceContext->PSSetConstantBuffers(Index, 1, &Buffer);
    
    Buffer->Release();
}
*/

static_assert(__COUNTER__ <= ArrayCount(GlobalProfileEntries), "Too many profiles");
