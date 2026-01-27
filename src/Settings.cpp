

void SettingsUpdateAndRender(game_settings* Settings, game_input* Input, game_assets* Assets, f32 SecondsPerFrame, allocator Allocator)
{
    BeginGUI(Input, Assets);
    
    DrawGUIStringCentered(String("Hello"), V2(0, 0), V4(1,1,1,1), 0.1f);
}