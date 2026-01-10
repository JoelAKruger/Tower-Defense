#include "Engine.h"
#include "Engine.cpp"

struct app_callbacks
{
    void (*ResizeAssets)(void** ApplicationData);
    void (*UpdateAndRender)(void** ApplicationData, game_assets* Assets, f32 DeltaTime, game_input* Input, allocator Allocator);
};

app_callbacks Callbacks;

extern "C" __declspec(dllexport) void _StartEngine(app_callbacks Callbacks_)
{
    Callbacks = Callbacks_;
    
    HINSTANCE Instance = GetModuleHandle(0);
    LPWSTR CommandLine = GetCommandLine();
    
    EngineMain(Instance, CommandLine, 0);
}

extern "C" __declspec(dllexport) void _SetDepthTest(bool Value)
{
    SetDepthTest(Value);
}

extern "C" __declspec(dllexport) void _SetFrameBufferAsOutput()
{
    SetFrameBufferAsOutput();
}

extern "C" __declspec(dllexport) void _SetShader(int Shader)
{
    SetShader((shader)Shader);
}

extern "C" __declspec(dllexport) void _SetGraphicsShaderConstants(shader_constants* Constants)
{
    SetGraphicsShaderConstants(*Constants);
}

void ResizeAssets(void** ApplicationState)
{
    Callbacks.ResizeAssets(ApplicationState);
}

void UpdateAndRender(void** ApplicationData, game_assets* Assets, f32 DeltaTime, game_input* Input, allocator Allocator)
{
    Callbacks.UpdateAndRender(ApplicationData, Assets, DeltaTime, Input, Allocator);
}

