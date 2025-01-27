struct font_asset;
v2 TextPixelSize(font_asset* Font, string String);

//TODO: Get rid of this
struct multiplayer_context;
struct global_game_state;
bool PlatformSend(multiplayer_context* Context, u8* Data, u64 Length);
void CheckForServerUpdate(global_game_state* Game, multiplayer_context* Context);
void Host();
void ConnectToServer(multiplayer_context* Context, char* Hostname);


u64 ReadCPUTimer();
f32 CPUTimeToSeconds(u64 Ticks, u64 Frequency);
extern u64 CPUFrequency;
