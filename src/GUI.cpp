struct gui_state
{
	bool MouseWentDown, MouseWentUp;
	v2 CursorPosition;
    
	u32 Hot, Active;
	bool InputHandled;
};

//Hot = hovering
//Active = clicking

gui_state GlobalGUIState;
u32 GlobalGUIIdentifierCounter;
render_group* GlobalRenderGroup;

static void
BeginGUI(game_input* Input, render_group* RenderGroup)
{
	//Check if buttons no longer exist
	if (GlobalGUIState.Hot >= GlobalGUIIdentifierCounter)
		GlobalGUIState.Hot = 0;
	if (GlobalGUIState.Active >= GlobalGUIIdentifierCounter)
		GlobalGUIState.Active = 0;
    
	GlobalGUIState.MouseWentDown = (Input->ButtonDown & Button_LMouse);
	GlobalGUIState.MouseWentUp = (Input->ButtonUp & Button_LMouse);
	GlobalGUIState.CursorPosition = Input->Cursor;
	GlobalGUIState.InputHandled = false;
    
	GlobalGUIIdentifierCounter = 1;
    
    GlobalRenderGroup = RenderGroup;
}

static inline bool
GUIInputIsBeingHandled()
{
	return GlobalGUIState.Hot > 0;
}

static bool 
Button(v2 Position, v2 Size, string String)
{
	u32 Identifier = GlobalGUIIdentifierCounter++;
	bool Result = false;
	
	if (GlobalGUIState.Active == Identifier)
	{
		if (GlobalGUIState.MouseWentUp)
		{
			if (GlobalGUIState.Hot == Identifier)
			{
				Result = true;
				GlobalGUIState.InputHandled = true;
			}
			else
			{
				GlobalGUIState.Active = 0;
			}
		}
	}
	else if (GlobalGUIState.Hot == Identifier)
	{
		if (GlobalGUIState.MouseWentDown)
		{
			GlobalGUIState.Active = Identifier;
		}
	}
    
	if (PointInRect(Position, Position + Size, GlobalGUIState.CursorPosition))
	{
		GlobalGUIState.Hot = Identifier;
	}
	else if (GlobalGUIState.Hot == Identifier)
	{
		GlobalGUIState.Hot = 0;
	}
    
    v4 Color = {0.5f, 0.5f, 1.0f, 1.0f};
	if (GlobalGUIState.Hot == Identifier)
	{
		Color = {0.4f, 0.4f, 1.0f, 1.0f};
	}
    
    //TODO: Buffer GUI elements
    SetShader(ColorShader);
	DrawRectangle(Position, Size, Color /*, 0xFF8080FF*/);
    
    f32 FontSize = 0.7f * Size.Y;
    f32 TextWidth = GUIStringWidth(String, FontSize);
	SetShader(FontShader);
    DrawGUIString(String, V2(Position.X + 0.5f * Size.X - 0.5f * TextWidth, Position.Y + 0.25f * Size.Y), V4(1.0f, 1.0f, 1.0f, 1.0f), FontSize);
    
	return Result;
}

struct gui_layout
{
    f32 X0;
	f32 X, Y;
	f32 XPad;
	f32 RowHeight;
    f32 ColumnWidth;
    
    memory_arena* Arena;
    
	bool Button(const char* Text);
	bool Button(string String);
	void Label(const char* Text);
	void Label(string Text);
	void NextRow();
};

static gui_layout 
DefaultLayout(f32 X, f32 Y, memory_arena* Arena)
{
	gui_layout Result = {};
    
	Result.RowHeight = 0.06f;
	Result.XPad = 0.01f;
    Result.X0 = X;
	Result.X = X + Result.XPad;
	Result.Y = Y - 0.01f - Result.RowHeight;
    Result.ColumnWidth = 0.04f;
    Result.Arena = Arena;
    
	return Result;
}

bool gui_layout::Button(const char* Text)
{
	return Button(String(Text));
}

bool gui_layout::Button(string Text)
{
    f32 ButtonHeight = 0.7f * RowHeight;
	f32 ButtonWidth = 3.0f * ButtonHeight / GlobalAspectRatio;
	bool Result = ::Button(V2(X, Y), V2(ButtonWidth, ButtonHeight), Text);
	X += ButtonWidth + XPad;
	return Result;
}

void gui_layout::Label(const char* Text)
{
	Label(String(Text));
}

void gui_layout::Label(string Text)
{
    f32 LabelFontSize = 0.75f * RowHeight;
    
    f32 Width = PlatformTextWidth(Text, LabelFontSize);
    Width = RoundUpToMultipleOf(ColumnWidth, Width + XPad);
    
    DrawGUIString(Text, V2(X, Y + 0.25f * LabelFontSize), V4(1.0f, 1.0f, 1.0f, 1.0f), LabelFontSize);
    X += Width + XPad;
}

void gui_layout::NextRow()
{
	X = X0 + XPad;
	Y -= RowHeight;
}