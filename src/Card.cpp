static v3
TransformCard_(v2 P, v3 CardRowP, v3 CardRowX, v3 CardRowY)
{
    v3 Result = CardRowP + P.X * CardRowX + P.Y * CardRowY;
    return Result;
}

#define TransformCard(P) TransformCard_(P, CardRowP, CardRowX, CardRowY)

static void
RenderCards(render_group* RenderGroup, game_state* Game, game_assets* Assets, defense_assets* Handles)
{
    player* Player = Game->GlobalState.Players + Game->MyClientID;
    
    v3 CardRowP = Game->CameraP + V3(0.0f, 0.0f, 1.0f);
    v3 CardRowNormal = UnitV(V3(0.0f, -0.1f, -1.0f));
    v3 Up = {0.0f, 0.0f, -1.0f};
    
    v3 CardRowX = UnitV(CrossProduct(CardRowNormal, Up));
    v3 CardRowY = UnitV(CrossProduct(CardRowX, CardRowNormal));
    
    f32 CardWidth = 0.12f;
    f32 CardHeight = 0.2f;
    
    int CardCount = 4;
    
    f32 dX = 0.5f * CardWidth;
    
    if (Player->CardCount > 0)
    {
        f32 TotalWidth = CardWidth + dX * (CardCount - 1);
        v2 CardP = V2(-TotalWidth * 0.5f, 0.0f);
        //v2 CardP = V2(0.0f, 0.0f);
        
        for (int I = 0; I < Player->CardCount; I++)
        {
            vertex Vertices[4] = {
                {.P = TransformCard(CardP + V2(0.0f, 0.5f * CardHeight)), .Normal = CardRowNormal, .UV = V2(0, 0)},
                {.P = TransformCard(CardP + V2(CardWidth, 0.5f * CardHeight)), .Normal = CardRowNormal, .UV = V2(1, 0)},
                {.P = TransformCard(CardP + V2(0.0f, -0.5f * CardHeight)), .Normal = CardRowNormal, .UV = V2(0, 1)},
                {.P = TransformCard(CardP + V2(CardWidth, -0.5f * CardHeight)), .Normal = CardRowNormal, .UV = V2(1, 1)}
            };
            
            PushVertices(RenderGroup, (f32*)Vertices, sizeof(Vertices), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, sizeof(vertex), Shader_Texture);
            GetLastEntry(RenderGroup)->Texture = Handles->CardTexture;
            GetLastEntry(RenderGroup)->Color = V4(1,1,1,1);
            CardP.X += dX;
        }
    }
}