void GUI_DrawRectangle(v2 Position, v2 Size, v4 Color = {});
void GUI_DrawTexture(texture Texture, v2 P, v2 Size);

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

game_assets* GlobalAssets;
shader GUIColorShader;
shader GUIFontShader;
shader GUITextureShader;

static void
BeginGUI(game_input* Input, game_assets* Assets)
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
    
    GlobalAssets = Assets;
    GUIColorShader = Assets->Shaders[Shader_GUI_Color];
    GUIFontShader = Assets->Shaders[Shader_GUI_Font];
    GUITextureShader = Assets->Shaders[Shader_GUI_Texture];
}

static inline bool
GUIInputIsBeingHandled()
{
	return GlobalGUIState.Hot > 0;
}

enum gui_element_status
{
    GUI_InActive,
    GUI_Hot,
    GUI_Pressed
};

static gui_element_status
DoGUIElement(v2 Position, v2 Size)
{
    u32 Identifier = GlobalGUIIdentifierCounter++;
	bool Pressed = false;
	
	if (GlobalGUIState.Active == Identifier)
	{
		if (GlobalGUIState.MouseWentUp)
		{
			if (GlobalGUIState.Hot == Identifier)
			{
                Pressed = true;
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
    
    gui_element_status Result = GUI_InActive;
    if (Pressed)
    {
        Result = GUI_Pressed;
    }
    else if (GlobalGUIState.Hot == Identifier)
    {
        Result = GUI_Hot;
    }
    return Result;
}

static bool 
Button(v2 Position, v2 Size, string String)
{
    gui_element_status Status = DoGUIElement(Position, Size);
    
    v4 Color = {0.5f, 0.5f, 1.0f, 1.0f};
	if (Status == GUI_Hot)
	{
		Color = {0.4f, 0.4f, 1.0f, 1.0f};
	}
    
    //TODO: Buffer GUI elements
    SetShader(GUIColorShader);
	GUI_DrawRectangle(Position, Size, Color);
    
    f32 FontSize = 0.7f * Size.Y;
    f32 TextWidth = GUIStringWidth(String, FontSize);
	SetShader(GUIFontShader);
    DrawGUIString(String, V2(Position.X + 0.5f * Size.X - 0.5f * TextWidth, Position.Y + 0.25f * Size.Y), V4(1.0f, 1.0f, 1.0f, 1.0f), FontSize);
    
	return (Status == GUI_Pressed);
}

static bool
GUIButton(v2 Position, v2 Size, string String)
{
    gui_element_status Status = DoGUIElement(Position, Size);
    
    //TODO: Buffer GUI elements
    SetShader(GlobalAssets->Shaders[Shader_GUI_Texture]);
    SetTexture(GlobalAssets->Button);
    rect Rect = PixelAccurateTexPosition(GlobalAssets->Button, Position, 2.0f); 
    
    f32 FontSize = 0.7f * Size.Y;
    f32 TextWidth = GUIStringWidth(String, FontSize);
	SetShader(GUIFontShader);
    DrawGUIString(String, V2(Position.X + 0.5f * Size.X - 0.5f * TextWidth, Position.Y + 0.25f * Size.Y), V4(1.0f, 1.0f, 1.0f, 1.0f), FontSize);
    return (Status == GUI_Pressed);
}



struct gui_layout
{
    f32 X0;
	f32 X, Y;
	f32 XPad;
	f32 RowHeight;
    f32 ColumnWidth;
    
	bool Button(char* Text);
	bool Button(string String);
    bool GUIButton(string String);
    
	void Label(char* Text);
	void Label(string Text);
	void NextRow();
};

static gui_layout 
DefaultLayout(f32 X, f32 Y)
{
	gui_layout Result = {};
    
	Result.RowHeight = 0.06f;
	Result.XPad = 0.01f;
    Result.X0 = X;
	Result.X = X + Result.XPad;
	Result.Y = Y - 0.01f - Result.RowHeight;
    Result.ColumnWidth = 0.04f;
    
	return Result;
}

bool gui_layout::Button(char* Text)
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

bool gui_layout::GUIButton(string Text)
{
    f32 ButtonHeight = 0.7f * RowHeight;
	f32 ButtonWidth = 3.0f * ButtonHeight / GlobalAspectRatio;
	bool Result = ::GUIButton(V2(X, Y), V2(ButtonWidth, ButtonHeight), Text);
	X += ButtonWidth + XPad;
	return Result;
}

void gui_layout::Label(char* Text)
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

struct panel_layout
{
    f32 X0, Y0;
    f32 PixelWidth, PixelHeight;
    
    int X, Y;
    int XPad;
    int YPad;
    int Width, Height;
    
    void DoBackground();
    bool Button(char* Text);
};


static panel_layout
DefaultPanelLayout(f32 X0, f32 Y0, f32 Scale = 1.0f)
{
    panel_layout Layout = {};
    
    Layout.X0 = X0;
    Layout.Y0 = Y0;
    Layout.PixelWidth = 2.0f / GlobalOutputWidth;
    Layout.PixelHeight = 2.0f / GlobalOutputHeight;
    Layout.XPad = 5;
    Layout.YPad = 5;
    Layout.X = Layout.XPad;
    Layout.Y = Layout.YPad;
    
    return Layout;
}

void panel_layout::DoBackground()
{
    texture Texture = GlobalAssets->Panel;
    f32 W = Texture.Width * PixelWidth;
    f32 H = Texture.Height * PixelHeight;
    
    Width = Texture.Width;
    Height = Texture.Height;
    
    GUI_DrawTexture(Texture, V2(X0, Y0 - H), V2(W, H));
}

bool panel_layout::Button(char* Text)
{
    texture Texture = GlobalAssets->Button;
    f32 W = Texture.Width * PixelWidth;
    f32 H = Texture.Height * PixelHeight;
    
    SetShader(GlobalAssets->Shaders[Shader_GUI_Texture]);
    SetTexture(GlobalAssets->Button);
    v2 P = V2(X0, Y0 - H);
    v2 Size = V2(W, H);
    
    GUI_DrawTexture(Texture, P, Size);
    
    gui_element_status Status = DoGUIElement(P, Size);
    
    DrawGUIString(String(Text), P + V2(0.0f, 0.25f * 0.04f), V4(1.0f, 1.0f, 1.0f, 1.0f), 0.04f);
    
    return (Status == GUI_Pressed);
}

static void
GUI_DrawRectangle(v2 Position, v2 Size, v4 Color)
{
    v2 Origin = Position;
    v2 XAxis = V2(Size.X, 0.0f);
    v2 YAxis = V2(0.0f, Size.Y);
    DrawQuad(Origin + YAxis, Origin + YAxis + XAxis, Origin, Origin + XAxis, Color);
}

static void
GUI_DrawTexture(texture Texture, v2 P, v2 Size)
{
    SetTexture(Texture);
    SetShader(GlobalAssets->Shaders[Shader_GUI_Texture]);
    GUI_DrawRectangle(P, Size);
}