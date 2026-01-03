#pragma once
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment (lib, "Ws2_32.lib")

#define UNICODE
#include <Windows.h>

#include <d3dcompiler.h>
#include <hidusage.h>

#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "Timer.cpp"
#include "Profiler.h"

#include "D3D11.h"

#include "D3D11.cpp"

//#include "Network/GameNetworkingSockets.cpp"
#include "Network/Winsock.cpp"

#include "Profiler.cpp"
#include "Parser.cpp"
#include "Graphics.cpp"
#include "GUI.cpp"
#include "Console.cpp"
#include "Assets.cpp"

memory_arena GlobalDebugArena;

static std::string GlobalTextInput;
static f32 GlobalScrollDelta;
static v2 GlobalCursorDelta;

f32 GlobalAspectRatio;
int GlobalOutputWidth;
int GlobalOutputHeight;
HWND GlobalWindow;

LRESULT CALLBACK WindowProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam);

//Platform Functions
void KeyboardAndMouseInputState(input_state* InputState, HWND Window);
memory_arena Win32CreateMemoryArena(u64 Size, memory_arena_type Type);
font_texture CreateFontTexture(allocator Allocator, char* Path);
void Win32DrawText(font_texture Font, string Text, v2 Position, v4 Color, f32 Size, f32 AspectRatio);
void Win32DrawTexture(v3 P0, v3 P1, v2 UV0, v2 UV1);
void Win32DebugOut(string String);
void Win32Sleep(int Milliseconds);
span<u8> LoadFile(memory_arena* Arena, char* Path);
void Win32SaveFile(char* Path, span<u8> Data);

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
                               L"Tower Defence Game",
                               WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                               CW_USEDEFAULT, CW_USEDEFAULT,
                               ClientRect.right - ClientRect.left,
                               ClientRect.bottom - ClientRect.top,
                               0, 0, Instance, 0);
    
    GlobalWindow = Window;
    
    if (!Window)
    {
        return -1;
    }
    
    CreateD3D11Device();
    IDXGISwapChain1* SwapChain = CreateD3D11SwapChain(Window);
    RenderOutput = GetDefaultRenderOutput(SwapChain);
    SetBlendMode(BlendMode_Blend);
    
    CreateSamplers();
    
    SetDepthTest(false);
    
    memory_arena TransientArena = Win32CreateMemoryArena(Megabytes(64), TRANSIENT);
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
    
    font_texture FontTexture = CreateFontTexture(Allocator, "assets/LiberationMono-Regular.ttf");
    DefaultFont = &FontTexture;
    
    dynamic_array<string> Profile = {};
    
    app_state* AppState = 0;
    game_assets Assets = {.Allocator = Allocator};
    
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
            
            HRESULT Result = SwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
            Assert(SUCCEEDED(Result));
            
            RenderOutput = GetDefaultRenderOutput(SwapChain);
            GlobalAspectRatio = (f32)RenderOutput.Width / (f32)RenderOutput.Height;
            GlobalOutputWidth = RenderOutput.Width;
            GlobalOutputHeight = RenderOutput.Height;
            
            GlobalWindowDidResize = false;
            
            ResizeAssets(&Assets);
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
        
        Input.CursorDelta = GlobalCursorDelta;
        GlobalCursorDelta = {};
        
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
            UpdateAndRender(&AppState, &Assets, SecondsPerFrame, &Input, Allocator);
        }
        
        // Draw profiling information
        if (Assets.Initialised)
        {
            SetGUIShaderConstant(IdentityTransform());
            SetShader(Shader_GUI_Font);
            f32 X = -0.99f;
            f32 Y = 0.95f;
            for (string Text : Profile)
            {
                DrawGUIString(Text, V2(X, Y), V4(0.75f, 0.75f, 0.75f, 1.0f));
                Y -= 0.06f;
            }
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
        case WM_CREATE:
        {
            RAWINPUTDEVICE RawInputDevice = {};
            RawInputDevice.usUsagePage = HID_USAGE_PAGE_GENERIC;
            RawInputDevice.usUsage = HID_USAGE_GENERIC_MOUSE;
            RegisterRawInputDevices(&RawInputDevice, 1, sizeof(RawInputDevice));
        } break;
        
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
        
        case WM_INPUT:
        {
            UINT DataSize = 0;
            GetRawInputData((HRAWINPUT)LParam, RID_INPUT, 0, &DataSize, sizeof(RAWINPUTHEADER));
            
            if (DataSize > 0)
            {
                RAWINPUT* RawInput = new RAWINPUT {};
                
                GetRawInputData((HRAWINPUT)LParam, RID_INPUT, RawInput, &DataSize, sizeof(RAWINPUTHEADER));
                if (RawInput->header.dwType == RIM_TYPEMOUSE && RawInput->data.mouse.usFlags == MOUSE_MOVE_RELATIVE)
                {
                    GlobalCursorDelta.X += RawInput->data.mouse.lLastX;
                    GlobalCursorDelta.Y += RawInput->data.mouse.lLastY;
                }
                
                delete RawInput;
            }
        } break;
        
        default:
        {
            Result = DefWindowProc(Window, Message, WParam, LParam);
        }
    }
    return Result;
}

static span<u8> 
LoadFile(memory_arena* Arena, char* Path)
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
        Log("Opened file: %s\n", Path);
    else
        Log("Could not open file: %s\n", Path); 
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
        Log("Saved file: %s\n", Path);
    else
        Log("Could not save file: %s\n", Path); 
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

void SetCursorState(bool ShouldShow)
{
    static bool OldState = true;
    
    if (ShouldShow != OldState)
    {
        if (!ShouldShow)
        {
            // Hide cursor
            ShowCursor(FALSE);
            
            // Clip to window
            RECT ClientRect;
            GetClientRect(GlobalWindow, &ClientRect);
            POINT TL = { ClientRect.left, ClientRect.top };
            POINT BR = { ClientRect.right, ClientRect.bottom };
            
            ClientToScreen(GlobalWindow, &TL);
            ClientToScreen(GlobalWindow, &BR);
            
            RECT ClipRect = { TL.x, TL.y, BR.x, BR.y };
            ClipCursor(&ClipRect);
        }
        else
        {
            // Show cursor
            ShowCursor(TRUE);
            ClipCursor(nullptr);
        }
        
        OldState = ShouldShow;
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

void Log(char* Format, ...)
{
    DWORD threadId = GetCurrentThreadId();
    
    va_list Args;
    va_start(Args, Format);
    
    string String = ArenaPrintInternal(&GlobalDebugArena, Format, Args);
    
    string Final = ArenaPrint(&GlobalDebugArena, "[Thread %lu] %s", threadId, String.Text);
    
    OutputDebugStringA(Final.Text);
    va_end(Args);
}


void Assert_(bool Value, int Line, char* File)
{
    if (Value == false)
    {
        __debugbreak();
    }
}

static_assert(__COUNTER__ <= ArrayCount(GlobalProfileEntries), "Too many profiles");