#include "Engine/Engine.h"
#include "Engine/Engine.cpp"

static void
ResizeAssets(game_assets* Assets)
{
}

struct asset_handles
{
    model_handle Model;
};

void LoadAssets(game_assets* Assets, allocator Allocator)
{
    Assets->Initialised = true;
    
    LoadShaders(Assets);
    
    Assets->GameData = AllocStruct(Allocator.Permanent, asset_handles);
    asset_handles* Handles = (asset_handles*) Assets->GameData;
    LoadObjects(Assets, "assets/models/castle.obj");
    Handles->Model = GetModelHandle(Assets, "Castle");
}

void UpdateAndRender(void** ApplicationState, game_assets* Assets, f32 DeltaTime, game_input* Input, allocator Allocator)
{
    if (!Assets->Initialised)
    {
        LoadAssets(Assets, Allocator);
    }
    
    shader_constants Constants = {};
    
    Constants.ModelToWorldTransform = ModelRotateTransform() * TranslateTransform(0, 5, 0);
    Constants.WorldToClipTransform = ViewTransform(V3(0, 0, 0), V3(0, 1, 0)) * PerspectiveTransform(70.0f, 0.01f, 10.0f);
    Constants.CameraPos = V3(0, 0, 0);
    Constants.Color = V4(1,1,1,1);
    Constants.LightDirection = V3(1,1,1);
    
    asset_handles*  Handles = (asset_handles*) Assets->GameData;
    
    SetDepthTest(true);
    SetFrameBufferAsOutput();
    SetShader(Shader_Model);
    SetGraphicsShaderConstants(Constants);
    DrawVertexBuffer(Assets->VertexBuffers[Assets->Meshes[Assets->Models[Handles->Model].Meshes[0]].VertexBuffer]);
}