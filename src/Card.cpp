f32 CardWidth = 0.12f;
f32 CardHeight = 0.2f;

static vertex_buffer_handle
CreateCardOutlineMesh(game_assets* Assets)
{
    v4 Color = V4(V3(1,1,1),1);
    f32 BorderThickness = 0.01f;
    
    v2 Vertices[4] = {
        V2(0.0f, 0.0f),
        V2(0.0f, CardHeight),
        V2(CardWidth, CardHeight),
        V2(CardWidth, 0.0f)
    };
    
    vertex_buffer_handle Result = CreateOutlineMesh(Assets, { Vertices, ArrayCount(Vertices) }, BorderThickness, Color);
    return Result;
}

static void
UpdateAndRenderCards(game_state* Game, game_input* Input, v3 CursorWorldDirection, memory_arena* Arena, game_assets* Assets, defense_assets* Handles, f32 DeltaTime)
{
    static vertex_buffer_handle OutlineMesh = CreateCardOutlineMesh(Assets);
    
    render_group* RenderGroup = AllocStruct(Arena, render_group);
    RenderGroup->Arena = Arena;
    RenderGroup->Assets = Assets;
    
    player* Player = Game->GlobalState.Players + Game->MyClientID;
    
    v3 CardRowP = Game->FakeCameraP + V3(0.0f, 0.0f, 1.0f);
    v3 CardRowNormal = UnitV(V3(0.0f, -0.1f, -1.0f));
    v4 CardPlane = V4(CardRowNormal, -DotProduct(CardRowNormal, CardRowP));
    
    v3 Up = {0.0f, 0.0f, -1.0f};
    
    v3 CardRowX = UnitV(CrossProduct(CardRowNormal, Up));
    v3 CardRowY = UnitV(CrossProduct(CardRowX, CardRowNormal));
    
    m4x4 CardToWorldTransform = M4x4(V4(CardRowX, 0.0f), V4(CardRowY, 0.0f), V4(CardRowNormal, 0.0f), V4(CardRowP, 1.0f));
    v3 CursorP = RayPlaneIntersection(CardPlane, Game->CameraP, CursorWorldDirection);
    
    v4 CardFrameCursorP_ = V4(CursorP, 1.0f) * Inverse(CardToWorldTransform);
    v3 CardFrameCursorP = CardFrameCursorP_.XYZ * (1.0f / CardFrameCursorP_.W);
    
    f32 dX = CardWidth;
    
    f32 TotalWidth = CardWidth + dX * (Player->CardCount - 1);
    
    v2 CardP = V2(-TotalWidth * 0.5f, -0.1f); //Bottom left position
    
    v2 CardSize = V2(CardWidth, CardHeight);
    
    v2 SelectedCardP = {};
    
    for (int I = 0; I < Player->CardCount; I++)
    {
        rect DefaultCardRect = {
            .MinCorner = CardP,
            .MaxCorner = CardP + CardSize
        };
        
        rect CardRect = {
            .MinCorner = Game->LocalCardPositions[I],
            .MaxCorner = Game->LocalCardPositions[I] + CardSize
        };
        
        rect HitboxRect = {
            .MinCorner = CardRect.MinCorner - V2(0.0f, 1.0f),
            .MaxCorner = CardRect.MaxCorner
        };
        
        vertex Vertices[4] = {
            {.P = V3(CardRect.MinCorner.X, CardRect.MaxCorner.Y, 0), .Normal = CardRowNormal, .UV = V2(1, 0)},
            {.P = V3(CardRect.MaxCorner.X, CardRect.MaxCorner.Y, 0), .Normal = CardRowNormal, .UV = V2(0, 0)},
            {.P = V3(CardRect.MinCorner.X, CardRect.MinCorner.Y, 0), .Normal = CardRowNormal, .UV = V2(1, 1)},
            {.P = V3(CardRect.MaxCorner.X, CardRect.MinCorner.Y, 0), .Normal = CardRowNormal, .UV = V2(0, 1)}
        };
        
        PushVertices(RenderGroup, (f32*)Vertices, sizeof(Vertices), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, sizeof(vertex), Shader_Texture);
        PushNoDepthTest(RenderGroup);
        GetLastEntry(RenderGroup)->Texture = Handles->CardTexture;
        GetLastEntry(RenderGroup)->Color = V4(1,1,1,1);
        
        f32 AddedHeight = 0.0f;
        
        bool Hovering = PointInRect(HitboxRect, CardFrameCursorP.XY);
        if (Hovering)
        {
            AddedHeight = 0.03f;
        }
        
        if (Hovering && (Input->ButtonDown & Button_LMouse))
        {
            Game->SelectedCardIndex = I;
            SetMode(Game, Mode_PlayCard);
        }
        
        bool Selected = (Game->Mode == Mode_PlayCard) && (Game->SelectedCardIndex == I);
        if (Selected)
        {
            AddedHeight = 0.05f;
            SelectedCardP = Game->LocalCardPositions[I];
        }
        
        f32 CardSpeed = 12.0f;
        Game->LocalCardPositions[I] = LinearInterpolate(Game->LocalCardPositions[I], 
                                                        CardP + V2(0.0f, AddedHeight), CardSpeed * DeltaTime);
        
        CardP.X += dX;
    }
    
    if (Game->Mode == Mode_PlayCard)
    {
        PushVertexBuffer(RenderGroup, OutlineMesh, TranslateTransform(V3(SelectedCardP, 0.0f)));
    }
    
    span<render_batch> RenderBatches = CreateRenderBatches(RenderGroup, RenderGroup->Arena);
    
    shader_constants Constants = {
        .WorldToClipTransform = CardToWorldTransform * Game->WorldTransform,
        .Time = (f32) Game->Time,
        .CameraPos = Game->CameraP
    };
    
    DrawRenderBatches(RenderBatches, Constants, Draw_Regular, Assets);
}

