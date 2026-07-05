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

static card*
GetCard(game_state* Game, u64 Identifier)
{
    player* Player = Game->GlobalState.Players + Game->MyClientID;
    
    card* Result = 0;
    
    for (u64 I = 0; I < Player->CardCount; I++)
    {
        if (Player->Cards[I].Identifier == Identifier)
        {
            Result = Player->Cards + I;
            break;
        }
    }
    
    return Result;
}

static v2*
GetLocalCardPosition(game_state* Game, u64 Identifier)
{
    v2* Result = 0;
    for (u64 I = 0; I < ArrayCount(Game->LocalCardIdentifiers); I++)
    {
        if (Game->LocalCardIdentifiers[I] == Identifier)
        {
            Result = Game->LocalCardPositions + I;
            break;
        }
    }
    return Result;
}

static void
CheckLocalCardPositionArray(game_state* Game)
{
    v2 CardStartPosition = {};
    
    player* Player = Game->GlobalState.Players + Game->MyClientID;
    
    // Check for new cards
    for (u64 CardIndex = 0; CardIndex < Player->CardCount; CardIndex++)
    {
        u64 Identifier = Player->Cards[CardIndex].Identifier;
        if (!GetLocalCardPosition(Game, Identifier))
        {
            for (u64 I = 0; I < ArrayCount(Game->LocalCardIdentifiers); I++)
            {
                if (Game->LocalCardIdentifiers[I] == 0)
                {
                    Game->LocalCardIdentifiers[I] = Identifier;
                    Game->LocalCardPositions[I] = CardStartPosition;
                    break;
                }
            }
        }
    }
    
    // Discard old cards
    for (u64 I = 0; I < ArrayCount(Game->LocalCardIdentifiers); I++)
    {
        u64 Identifier = Game->LocalCardIdentifiers[I];
        bool Exists = false;
        for (u64 CardIndex = 0; CardIndex < Player->CardCount; CardIndex++)
        {
            if (Player->Cards[CardIndex].Identifier == Identifier)
            {
                Exists = true;
                break;
            }
        }
        if (!Exists)
        {
            Game->LocalCardIdentifiers[I] = 0;
        }
        
    }
}

struct card_render
{
    span<render_batch> RenderBatches;
    shader_constants Constants;
};

static card_render
UpdateAndRenderCards(game_state* Game, game_input* Input, v3 CursorWorldDirection, memory_arena* Arena, game_assets* Assets, defense_assets* Handles, f32 DeltaTime)
{
    CheckLocalCardPositionArray(Game);
    
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
    
    m4x4 WorldToCardTransform = Inverse(CardToWorldTransform);
    
    rect PutCardBackRect = {
        .MinCorner = CardP,
        .MaxCorner = {CardP.X + TotalWidth, CardP.Y + CardHeight}
    };
    
    bool CursorInPutBackArea = PointInRect(PutCardBackRect, CardFrameCursorP.XY);
    
    if (Game->Mode == Mode_PlayCard && CursorInPutBackArea)
    {
        SetMode(Game, Mode_PutCardBack);
    }
    else if ((Game->Mode == Mode_PutCardBack || Game->Mode == Mode_TakeCard)
             && !CursorInPutBackArea)
    {
        SetMode(Game, Mode_PlayCard);
    }
    
    for (int I = 0; I < Player->CardCount; I++)
    {
        card* Card = Player->Cards + I;
        
        v2 CurrentP = Game->LocalCardPositions[I];
        v2 TargetP = CardP;
        
        bool Selected = ((Game->Mode == Mode_TakeCard || Game->Mode == Mode_PlayCard || Game->Mode == Mode_PutCardBack) &&
                         (Game->SelectedCardIdentifier == Card->Identifier));
        if (Selected)
        {
            if (Game->Mode == Mode_TakeCard || Game->Mode == Mode_PlayCard)
            {
                // Want: CardToWorld * (CardP + CursorCardPos) = Cursor
                // So  : CardP = CardToWorld^-1 * Cursor - CursorCardPos
                v4 P_ = V4(CursorP, 1.0f) * WorldToCardTransform;
                TargetP = (P_.XYZ * (1.0f / P_.W)).XY - Game->CursorCardPos;
                
                CurrentP = TargetP;
            }
            else if (Game->Mode == Mode_PutCardBack)
            {
                TargetP = CardP + V2(0.0f, 0.03f);
            }
            
            // Delay switching mode for one frame to handle button going up
            if ((Input->ButtonUp & Button_LMouse) == 0 && (Input->Button & Button_LMouse) == 0)
            {
                // Let card go
                SetMode(Game, Mode_MyTurn);
            }
        }
        
        rect DefaultCardRect = {
            .MinCorner = CardP,
            .MaxCorner = CardP + CardSize
        };
        
        rect CardRect = {
            .MinCorner = CurrentP,
            .MaxCorner = CurrentP + CardSize
        };
        
        rect HitboxRect = {
            .MinCorner = CardRect.MinCorner - V2(0.0f, 1.0f),
            .MaxCorner = CardRect.MaxCorner
        };
        
        if (!Selected)
        {
            bool Hovering = !Game->Dragging && PointInRect(HitboxRect, CardFrameCursorP.XY);
            if (Hovering)
            {
                TargetP = CardP + V2(0.0f, 0.03f);
            }
            
            if (Hovering && (Input->ButtonDown & Button_LMouse))
            {
                // Select card
                Game->SelectedCardIdentifier = Card->Identifier;
                Game->CursorCardPos = CardFrameCursorP.XY - CardRect.MinCorner;
                Selected = true;
                SetMode(Game, Mode_TakeCard);
            }
        }
        
        f32 CardSpeed = 18.0f;
        v2 P = LinearInterpolate(CurrentP, 
                                 TargetP, CardSpeed * DeltaTime);
        Game->LocalCardPositions[I] = P;
        
        if (Selected)
        {
            SelectedCardP = P;
        }
        
        vertex Vertices[4] = {
            {.P = V3(CardRect.MinCorner.X, CardRect.MaxCorner.Y, 0), .Normal = CardRowNormal, .UV = V2(0, 1)},
            {.P = V3(CardRect.MaxCorner.X, CardRect.MaxCorner.Y, 0), .Normal = CardRowNormal, .UV = V2(1, 1)},
            {.P = V3(CardRect.MinCorner.X, CardRect.MinCorner.Y, 0), .Normal = CardRowNormal, .UV = V2(0, 0)},
            {.P = V3(CardRect.MaxCorner.X, CardRect.MinCorner.Y, 0), .Normal = CardRowNormal, .UV = V2(1, 0)}
        };
        
        PushVertices(RenderGroup, (f32*)Vertices, sizeof(Vertices), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, sizeof(vertex), Shader_Texture);
        PushNoDepthTest(RenderGroup);
        Assert(Card->Type < ArrayCount(Handles->CardTextures));
        GetLastEntry(RenderGroup)->Texture = Handles->CardTextures[Card->Type];
        GetLastEntry(RenderGroup)->Color = V4(1,1,1,1);
        
        CardP.X += dX;
    }
    
    if (Game->Mode == Mode_TakeCard || Game->Mode == Mode_PlayCard)
    {
        PushVertexBuffer(RenderGroup, OutlineMesh, TranslateTransform(V3(SelectedCardP, 0.0f)));
    }
    
    span<render_batch> RenderBatches = CreateRenderBatches(RenderGroup, RenderGroup->Arena);
    
    shader_constants Constants = {
        .WorldToClipTransform = CardToWorldTransform * Game->WorldTransform,
        .Time = (f32) Game->Time,
        .CameraPos = Game->CameraP
    };
    
    card_render Result = {
        .RenderBatches = RenderBatches,
        .Constants = Constants
    };
    
    return Result;
}

