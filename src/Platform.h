
//Graphics API (DirectX wrapper)
void SetTexture(texture Texture);
void SetShader(d3d11_shader Shader);
void SetVertexShaderConstant(u32 Index, void* Data, u32 Bytes);
void SetDepthTest(bool Value);
void SetShadowMap(render_output Texture);
void UnsetShadowMap();
void DrawVertices(f32* VertexData, u32 VertexDataBytes, D3D11_PRIMITIVE_TOPOLOGY Topology, u32 Stride);
void Win32DrawText(font_texture Font, string Text, v2 Position, v4 Color, f32 Size, f32 AspectRatio = 1.0f);
void ClearOutput(render_output Output);
void SetOutput(render_output Output);
void SetFrameBufferAsOutput();
render_output CreateShadowDepthTexture(int Width, int Height);
void SetShadowMap(render_output Texture);
void UnsetShadowMap();
void LoadShaders(game_assets* Assets);
renderer_vertex_buffer CreateVertexBuffer(void* Data, u64 Bytes, D3D11_PRIMITIVE_TOPOLOGY Topology, u64 Stride);