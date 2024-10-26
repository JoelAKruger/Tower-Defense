memory_arena GraphicsArena;

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
    
    Result.Width = FrameBufferDesc.Width;
    Result.Height = FrameBufferDesc.Height;
    
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

d3d11_shader CreateShader(wchar_t* Path, D3D11_INPUT_ELEMENT_DESC* InputElementDesc, u32 InputElementDescCount, char* PixelShaderEntry = "ps_main", char* VertexShaderEntry = "vs_main")
{
    d3d11_shader Result = {};
    
    //Create Vertex Shader
    ID3DBlob* VertexShaderBlob;
    ID3DBlob* CompileErrorsBlob;
    HRESULT HResult = D3DCompileFromFile(Path, 0, D3D_COMPILE_STANDARD_FILE_INCLUDE, VertexShaderEntry, "vs_5_0", 0, 0, &VertexShaderBlob, &CompileErrorsBlob);
    
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
    HResult = D3DCompileFromFile(Path, 0, D3D_COMPILE_STANDARD_FILE_INCLUDE, PixelShaderEntry, "ps_5_0", 0, 0, &PixelShaderBlob, &CompileErrorsBlob);
    
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

static renderer_vertex_buffer
CreateVertexBuffer(void* Data, u64 Bytes, D3D11_PRIMITIVE_TOPOLOGY Topology, u64 Stride)
{
    Assert(Bytes > 0);
    Assert(Stride > 0);
    
    u32 VertexCount = Bytes / Stride;
    u32 Offset = 0;
    
    D3D11_BUFFER_DESC VertexBufferDesc = {};
    VertexBufferDesc.ByteWidth = Bytes;
    VertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    
    D3D11_SUBRESOURCE_DATA VertexSubresourceData = { Data };
    
    renderer_vertex_buffer Result = {};
    
    HRESULT HResult = D3D11Device->CreateBuffer(&VertexBufferDesc, &VertexSubresourceData, &Result.Buffer);
    Assert(SUCCEEDED(HResult));
    
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


void SetShader(d3d11_shader Shader)
{
    D3D11DeviceContext->IASetInputLayout(Shader.InputLayout);
    D3D11DeviceContext->VSSetShader(Shader.VertexShader, 0, 0);
    D3D11DeviceContext->PSSetShader(Shader.PixelShader, 0, 0);
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
    
    Result.Width = Width;
    Result.Height = Height;
    
    return Result;
}


void Win32DrawText(font_texture Font, string Text, v2 Position, v4 Color, f32 Size, f32 AspectRatio)
{
    f32 FontTexturePixelsToScreenY = Size / Font.RasterisedSize;
    f32 FontTexturePixelsToScreenX = FontTexturePixelsToScreenY / AspectRatio;
    
    f32 X = Position.X;
    f32 Y = Position.Y;
    
    u32 Stride = sizeof(vertex);
    u32 Offset = 0;
    u32 VertexCount = 6 * Text.Length;
    
    vertex* VertexData = AllocArray(&GraphicsArena, vertex, VertexCount);
    
    for (u32 I = 0; I < Text.Length; I++)
    {
        uint8_t Char = (uint8_t)Text.Text[I];
        Assert(Char < 128);
        stbtt_bakedchar BakedChar = Font.BakedChars[Char];
        
        f32 X0 = X + BakedChar.xoff * FontTexturePixelsToScreenX;
        f32 Y1 = Y - BakedChar.yoff * FontTexturePixelsToScreenY; //+ 0.5f * Font.RasterisedSize * FontTexturePixelsToScreen;
        
        f32 Width = FontTexturePixelsToScreenX * (f32)(BakedChar.x1 - BakedChar.x0);
        f32 Height = FontTexturePixelsToScreenY * (f32)(BakedChar.y1 - BakedChar.y0);
        
        VertexData[6 * I + 0].P = {X0, Y1 - Height};
        VertexData[6 * I + 1].P = {X0, Y1};
        VertexData[6 * I + 2].P = {X0 + Width, Y1};
        VertexData[6 * I + 3].P = {X0 + Width, Y1};
        VertexData[6 * I + 4].P = {X0 + Width, Y1 - Height};
        VertexData[6 * I + 5].P = {X0, Y1 - Height};
        
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
    VertexBufferDesc.ByteWidth = VertexCount * sizeof(vertex);
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

static f32 
D3D11TextWidth(string String, f32 Size, f32 AspectRatio)
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
SetShadowMap(render_output Texture)
{
    D3D11DeviceContext->PSSetShaderResources(1, 1, &Texture.DepthStencilShaderResourceView);
}

static void
UnsetShadowMap()
{
    ID3D11ShaderResourceView* ShaderResourceView = 0;
    D3D11DeviceContext->PSSetShaderResources(1, 1, &ShaderResourceView);
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
SetFrameBufferAsOutput()
{
    SetOutput(RenderOutput);
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
    
    D3D11_VIEWPORT Viewport = { 0.0f, 0.0f, (f32)Output.Width, (f32)Output.Height, 0.0f, 1.0f };
    D3D11DeviceContext->RSSetViewports(1, &Viewport);
}

static void
LoadShaders(game_assets* Assets)
{
    D3D11_INPUT_ELEMENT_DESC InputElementDesc[] = 
    {
        {"POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    
    D3D11_INPUT_ELEMENT_DESC ColorInputElementDesc[] = 
    {
        {"POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    
    D3D11_INPUT_ELEMENT_DESC FontShaderInputElementDesc[] = 
    {
        {"POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEX", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    
    D3D11_INPUT_ELEMENT_DESC TextureShaderElementDesc[] = 
    {
        {"POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEX", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    
    D3D11_INPUT_ELEMENT_DESC ModelShaderElementDesc[] = 
    {
        {"POS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    
    //Used for the GUI
    Assets->Shaders[Shader_Color] = CreateShader(L"assets/colourshaders.hlsl", InputElementDesc, ArrayCount(InputElementDesc));
    
    
    Assets->Shaders[Shader_Background]= CreateShader(L"assets/background.hlsl", ColorInputElementDesc, ArrayCount(ColorInputElementDesc));
    Assets->Shaders[Shader_Font]= CreateShader(L"assets/fontshaders.hlsl", InputElementDesc, ArrayCount(InputElementDesc));
    
    Assets->Shaders[Shader_Texture]= CreateShader(L"assets/texture.hlsl", TextureShaderElementDesc, ArrayCount(TextureShaderElementDesc));
    Assets->Shaders[Shader_Water]= CreateShader(L"assets/water.hlsl", TextureShaderElementDesc, ArrayCount(TextureShaderElementDesc));
    
    Assets->Shaders[Shader_Model]= CreateShader(L"assets/modelshader.hlsl", InputElementDesc, ArrayCount(InputElementDesc));
    Assets->Shaders[Shader_OnlyDepth]= CreateShader(L"assets/modelshader.hlsl", InputElementDesc, ArrayCount(InputElementDesc), 
                                                    "ps_main_shadow", "vs_main_shadow");
}
