#include "GUI.cpp"
#include "Console.cpp"
#include "Parser.cpp"

#include "World.cpp"
#include "Render.cpp"
#include "Server.cpp"
#include "Resources.cpp"
#include "Game.cpp"

static void UpdateAndRender(app_state* App, f32 DeltaTime, game_input* Input, allocator Allocator)
{
    if (!App->Assets)
    {
        App->Assets = LoadAssets(Allocator);
    }
    
    switch (App->CurrentScreen)
    {
        case Screen_MainMenu:
        {
            BeginGUI(Input, App->Assets);
            
            f32 ButtonWidth = 0.8f / GlobalAspectRatio;
            if (Button(V2(-0.5f * ButtonWidth, 0.0f), V2(ButtonWidth, 0.3f), String("Play")))
            {
                
                App->CurrentScreen = Screen_Game;
                App->GameState = GameInitialise(Allocator);
            }
        } break;
        case Screen_Game:
        {
            GameUpdateAndRender(App->GameState, App->Assets, DeltaTime, Input, Allocator);
        } break;
        
        default: Assert(0);
    }
}