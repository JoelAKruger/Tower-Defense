struct console;
struct game_state;

typedef void (*console_command_callback)(int ArgCount, string* Args, console* Console, game_state* GameState, game_assets* Assets,  memory_arena* Arena);

struct console_command
{
    string Command;
    console_command_callback Callback;
};

struct console
{
    f32 Height;
    f32 TargetHeight;
    
    console_command Commands[64];
    u64 CommandCount;
    
    string History[16];
    
    char Input[256];
    u32 InputLength;
    u32 InputCursor;
    
    bool CursorOn;
    f32  CursorCountdown;
};

void AddLine(console* Console, string String);
void ClearConsole(console* Console);

#define CONSOLE_COMMAND(Console, Command) \
void Command_ ## Command(int, string*, console*, game_state*, game_assets* Assets, memory_arena*); \
AddCommand(Console, String(#Command), Command_ ## Command)

void AddCommand(console* Console, string Command, console_command_callback Callback)
{
    Console->Commands[Console->CommandCount].Command = Command;
    Console->Commands[Console->CommandCount].Callback = Callback;
    Console->CommandCount++;
    Assert(Console->CommandCount < ArrayCount(Console->Commands));
}

//void Command_null_map_elem_count(int ArgCount, string* Args, console* Console, game_state* GameState, memory_arena* Arena)

static void
AddLine(console* Console, string String)
{
    u32 Bytes = String.Length;
    string NewLine = {};
    NewLine.Length = Bytes;
    NewLine.Text = (char*)malloc(Bytes);
    memcpy(NewLine.Text, String.Text, Bytes);
    
    u32 MaxHistory = ArrayCount(Console->History);
    free(Console->History[MaxHistory - 1].Text);
    memmove(Console->History + 1, Console->History, sizeof(string) * (MaxHistory - 1));
    Console->History[0] = NewLine;
}

static void
ClearConsole(console* Console)
{
    for (string& String : Console->History)
    {
        free(String.Text);
        String = {};
    }
}

static void
ParseAndRunCommand(console* Console, string Command, game_state* GameState, game_assets* Assets, memory_arena* TArena)
{
    int const MaxArgs = 16;
    string Args[MaxArgs];
    int ArgCount = 0;
    
    bool InsideArg = false;
    u32 CharIndex = 0;
    while (ArgCount < MaxArgs &&
           CharIndex < Command.Length)
    {
        char C = Command.Text[CharIndex];
        if (C == ' ')
        {
            if (InsideArg)
            {
                ArgCount++;
                InsideArg = false;
            }
        }
        else
        {
            if (InsideArg)
            {
                Args[ArgCount].Length++;
            }
            else
            {
                Args[ArgCount].Text = Command.Text + CharIndex;
                Args[ArgCount].Length = 1;
                InsideArg = true;
            }
        }
        
        CharIndex++;
    }
    
    if (InsideArg)
    {
        ArgCount++;
    }
    
    string Input = ArenaPrint(TArena, "> %.*s", Console->InputLength, Console->Input);
    AddLine(Console, Input);
    
    if (ArgCount > 0)
    {
        if (StringsAreEqual(Args[0], String("help")))
        {
            for (u64 CommandIndex = 0; CommandIndex < Console->CommandCount; CommandIndex++)
            {
                AddLine(Console, Console->Commands[CommandIndex].Command);
            }
        }
        else
        {
            for (u64 CommandIndex = 0; CommandIndex < Console->CommandCount; CommandIndex++)
            {
                if (StringsAreEqual(Console->Commands[CommandIndex].Command, Args[0]))
                {
                    Console->Commands[CommandIndex].Callback(ArgCount, Args, Console, GameState, Assets, TArena);
                    return;
                }
            }
            
            AddLine(Console, String("Invalid Command"));
        }
    }
}

static void
UpdateConsole(game_state* GameState, console* Console, game_input* Input, memory_arena* TArena, game_assets* Assets, f32 DeltaTime)
{
    if (Console->CommandCount == 0)
    {
        CONSOLE_COMMAND(Console, p);
        CONSOLE_COMMAND(Console, name);
        CONSOLE_COMMAND(Console, reset);
        CONSOLE_COMMAND(Console, color);
        CONSOLE_COMMAND(Console, create_server);
        CONSOLE_COMMAND(Console, connect);
        CONSOLE_COMMAND(Console, new_world);
        CONSOLE_COMMAND(Console, clear);
        CONSOLE_COMMAND(Console, water_flow);
    }
    
    //Check if toggled
    bool IsOpen = (Console->TargetHeight > 0.0f);
    if (Input->ButtonDown & Button_Console)
    {
        if (IsOpen)
        {
            Console->TargetHeight = 0.0f;
        }
        else
        {
            Console->TargetHeight = 0.6f;
        }
    }
    
    //Update openness
    f32 ConsoleSpeed = 3.0f;
    f32 dHeight = ConsoleSpeed * DeltaTime;
    
    if (Console->Height < Console->TargetHeight)
    {
        Console->Height = Min(Console->TargetHeight, Console->Height + dHeight);
    }
    
    if (Console->Height > Console->TargetHeight)
    {
        Console->Height = Max(Console->TargetHeight, Console->Height - dHeight);
    }
    
    //Update cursor
    f32 CursorTime = 0.4f;
    
    Console->CursorCountdown -= DeltaTime;
    if (Console->CursorCountdown < 0.0f)
    {
        Console->CursorCountdown += CursorTime;
        Console->CursorOn = !Console->CursorOn;
    }
    
    //Update input
    if (IsOpen)
    {
        if (Input->ButtonDown & Button_Left)
        {
            Console->InputCursor = Max(0, Console->InputCursor - 1);
        }
        
        if (Input->ButtonDown & Button_Right)
        {
            Console->InputCursor = Min((i32)Console->InputLength, (i32)(Console->InputCursor + 1));
        }
        
        //Clear input to prevent other actions
        Input->Button = 0;
        Input->ButtonDown = 0;
        Input->ButtonUp = 0;
        Input->Movement = {};
        
        for (char* TextInput = Input->TextInput; *TextInput; TextInput++)
        {
            char C = *TextInput;
            switch (C)
            {
                case '\n':
                {
                    string Command = {Console->Input, (u32)Console->InputLength};
                    ParseAndRunCommand(Console, Command, GameState, Assets, TArena);
                    
                    Console->InputLength = 0;
                    Console->InputCursor = 0;
                } break;
                case '\b':
                {
                    if (Console->InputCursor > 0)
                    {
                        memmove(Console->Input + Console->InputCursor - 1, 
                                Console->Input + Console->InputCursor,
                                Console->InputLength - Console->InputCursor);
                        Console->InputLength--;
                        Console->InputCursor--;
                    }
                } break;
                default:
                {
                    if (Console->InputLength + 1 < ArrayCount(Console->Input))
                    {
                        memmove(Console->Input + Console->InputCursor + 1, 
                                Console->Input + Console->InputCursor,
                                Console->InputLength - Console->InputCursor);
                        
                        Console->Input[Console->InputCursor++] = C;
                        Console->InputLength++;
                    }
                } break;
            }
        }
    }
}
static void
DrawConsole(console* Console, memory_arena* Arena)
{
    SetShader(ColorShader);
    f32 ScreenTop = 0.5625;
    
    f32 X0 = -0.8f;
    f32 X1 =  0.8f;
    
    f32 InputTextHeight = 0.05f;
    
    f32 Y0 = 1.0f - Console->Height;
    f32 Width = X1 - X0;
    
    DrawRectangle(V2(X0, Y0 + InputTextHeight), V2(Width, Console->Height), V4(0.0f, 0.0f, 0.0f, 0.75f));
    DrawRectangle(V2(X0, Y0), V2(Width, InputTextHeight), V4(0.0f, 0.0f, 0.0f, 1.0f));
    
    string Input = ArenaPrint(Arena, "> %.*s", Console->InputLength, Console->Input);
    
    //Find cursor position
    Assert(Console->InputCursor + 2 <= Input.Length);
    string InputA = {Input.Text, Console->InputCursor + 2}; //Adjust for "> "
    
    f32 Pad = 0.002f;
    X0 += Pad;
    f32 WidthA = PlatformTextWidth(InputA, InputTextHeight, GlobalAspectRatio);
    
    //Draw cursor
    v4 TextColor = V4(1.0f, 1.0f, 1.0f, 1.0f);
    
    if (Console->CursorOn)
    {
        DrawRectangle(V2(X0 + WidthA, Y0 + Pad), 
                      V2(Pad, InputTextHeight - 2.0f * Pad),
                      TextColor);
    }
    
    SetShader(FontShader);
    DrawGUIString(Input, V2(X0, Y0 + 0.25f * InputTextHeight), TextColor, InputTextHeight);
    
    for (string History : Console->History)
    {
        Y0 += InputTextHeight + Pad;
        DrawGUIString(History, V2(X0, Y0 + 0.25f * InputTextHeight), TextColor, InputTextHeight);
    }
}

void Command_clear(int ArgCount, string* Args, console* Console, game_state* GameState, game_assets* Assets, memory_arena* Arena)
{
    for (u64 HistoryIndex = 0; HistoryIndex < ArrayCount(Console->History); HistoryIndex++)
    {
        free(Console->History[HistoryIndex].Text);
        Console->History[HistoryIndex] = {};
    }
}

void Command_water_flow(int ArgCount, string* Args, console* Console, game_state* GameState, game_assets* Assets, memory_arena* Arena)
{
    CreateWaterFlowMap(&GameState->GlobalState.World, Assets, Arena);
}