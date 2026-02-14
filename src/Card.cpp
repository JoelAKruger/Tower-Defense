static void
RenderCards(render_group* RenderGroup, game_state* Game, game_assets* Assets, defense_assets* Handles)
{
    texture_handle Texture = Handles->CardTexture;
    
    v3 CardP = {0.0f, -0.5f, 0.0f};
    v3 CardNormal = UnitV(V3(0.0f, -0.5f, -1.0f));
    v3 Up = {0.0f, 0.0f, -1.0f};
    v3 CardX = 0.1f * UnitV(CrossProduct(CardNormal, Up));
    v3 CardY = 0.1f * UnitV(CrossProduct(CardX, CardNormal));
    
    vertex Vertices[4] = {
        {.P = CardP - CardX + CardY, .Normal = CardNormal, .UV = V2(0, 0)},
        {.P = CardP + CardX + CardY, .Normal = CardNormal, .UV = V2(1, 0)},
        {.P = CardP - CardX - CardY, .Normal = CardNormal, .UV = V2(0, 1)},
        {.P = CardP + CardX - CardY, .Normal = CardNormal, .UV = V2(1, 1)}
    };
    
    PushVertices(RenderGroup, (f32*)Vertices, sizeof(Vertices), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, sizeof(vertex), Shader_Texture);
    GetLastEntry(RenderGroup)->Texture = Handles->CardTexture;
    GetLastEntry(RenderGroup)->Color = V4(1,1,1,1);
}