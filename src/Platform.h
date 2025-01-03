
//Graphics API (DirectX wrapper)
void SetTexture(texture Texture, int Index = 0);
void UnsetTexture(int Index = 0);
void SetShader(d3d11_shader Shader);
void SetDepthTest(bool Value);
void SetShadowMap(render_output Texture);
void UnsetShadowMap();
void DrawVertices(f32* VertexData, u32 VertexDataBytes, D3D11_PRIMITIVE_TOPOLOGY Topology, u32 Stride);
void DrawVertexBuffer(renderer_vertex_buffer VertexBuffer);
void Win32DrawText(font_texture Font, string Text, v2 Position, v4 Color, f32 Size, f32 AspectRatio = 1.0f);
void ClearOutput(render_output Output, v4 Color = V4(0.2f, 0.4f, 0.6f, 1.0f));
void SetOutput(render_output Output);
void SetFrontCullMode(bool Value);
void SetFrameBufferAsOutput();
render_output CreateShadowDepthTexture(int Width, int Height);
void LoadShaders(game_assets* Assets);
renderer_vertex_buffer CreateVertexBuffer(void* Data, u64 Bytes, D3D11_PRIMITIVE_TOPOLOGY Topology, u64 Stride);
void FreeVertexBuffer(renderer_vertex_buffer VertexBuffer);
void SetBlendMode(blend_mode Mode);
void SetNonGraphicsShaderConstant(void* Data, u32 Bytes);
void SetGraphicsShaderConstant(void* Data, u32 Bytes);