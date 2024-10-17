typedef u32 button_state;
enum
{
    Button_Jump     = (1 << 1),
    Button_Interact = (1 << 2),
    Button_Menu     = (1 << 3),
    Button_LMouse   = (1 << 4),
    Button_LShift   = (1 << 5),
    Button_Console  = (1 << 6),
    Button_Left     = (1 << 7),
    Button_Right    = (1 << 8),
    Button_Up       = (1 << 9),
    Button_Down     = (1 << 10),
    Button_Escape   = (1 << 11)
};

struct input_state
{
    button_state Buttons;
    v2 Cursor; // [-1, 1] is the window
    v2 Movement;
};

struct game_input
{
    button_state Button;
    button_state ButtonDown;
    button_state ButtonUp;
    
    char* TextInput;
    v2 Movement;
	v2 Cursor;
    f32 ScrollDelta;
};
