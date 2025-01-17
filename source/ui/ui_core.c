#include "os.h"
#include "render.h"
#include "ui.h"
#include "maths.h"

#include "draw.h"
#include "strings.h"
#include <assert.h>

void RemoveEventFromList(ui_context *UI, ui_event *Event)
{
	assert(Event);
	if(Event->Previous)
	{
		Event->Previous->Next = Event->Next;
	}
	else
	{
		UI->EventFirst = Event->Next;
	}

	if(Event->Next && Event->Previous)
	{
		Event->Next->Previous = Event->Previous->Next;
	}
	else
	{
		UI->EventLast = Event->Previous;
	}
}

b32 UI_LeftMouseWentDown(ui_context *UI)
{
	b32 Result = False;
	for(ui_event *Event = UI->EventFirst; Event; Event = Event->Next)
	{
		if(Event->E.type == SAPP_EVENTTYPE_MOUSE_DOWN)
		{
			Result = True;
			RemoveEventFromList(UI, Event);
			break;
		}
	}
	return Result;
}

b32 UI_LeftMouseWentDownWithModifiers(ui_context *UI, int Modifiers)
{
	b32 Result = False;
	for(ui_event *Event = UI->EventFirst; Event; Event = Event->Next)
	{
		if(Event->E.type == SAPP_EVENTTYPE_MOUSE_DOWN && Event->E.modifiers & Modifiers)
		{
			Result = True;
			RemoveEventFromList(UI, Event);
			break;
		}
	}
	return Result;
}

b32 UI_LeftMouseWentUp(ui_context *UI)
{
	b32 Result = False;
	for(ui_event *Event = UI->EventFirst; Event; Event = Event->Next)
	{
		if(Event->E.type == SAPP_EVENTTYPE_MOUSE_UP)
		{
			Result = True;
			RemoveEventFromList(UI, Event);
			break;
		}
	}
	return Result;
}

b32 UI_KeyDown(ui_context *UI, sapp_keycode Key, u32 Modifiers)
{
	b32 Result = False;
	for(ui_event *Event = UI->EventFirst; Event; Event = Event->Next)
	{
		if(Event->E.type == SAPP_EVENTTYPE_KEY_DOWN && Event->E.key_code == Key &&
		   Event->E.modifiers == Modifiers)
		{
			Result = True;
			RemoveEventFromList(UI, Event);
			break;
		}
	}
	return Result;
}

b32 UI_KeyUp(ui_context *UI, sapp_keycode Key, u32 Modifiers)
{
	b32 Result = False;
	for(ui_event *Event = UI->EventFirst; Event; Event = Event->Next)
	{
		if(Event->E.type == SAPP_EVENTTYPE_KEY_DOWN && Event->E.key_code == Key)
		{
			Result = True;
			RemoveEventFromList(UI, Event);
			break;
		}
	}
	return Result;
}

v2f UI_Scroll(ui_context *UI)
{
	v2f Result = {0};
	for(ui_event *Event = UI->EventFirst; Event; Event = Event->Next)
	{
		if(Event->E.type == SAPP_EVENTTYPE_MOUSE_SCROLL)
		{
			Result.X +=  Event->E.scroll_x;
			Result.Y += -Event->E.scroll_y;
		}
	}
	return Result;
}

b32 UI_DoubleClicked(ui_context *UI)
{
	return UI->MultiLeftClickCount == 1 && UI_LeftMouseWentDown(UI);
}

b32 UI_TripleClicked(ui_context *UI)
{
	return UI->MultiLeftClickCount == 2  && UI_LeftMouseWentDown(UI);
}

b32 UI_QuadClicked(ui_context *UI)
{
	return UI->MultiLeftClickCount == 3  && UI_LeftMouseWentDown(UI);
}

str8 UI_DisplayString(m_arena *Arena, str8 String)
{
	str8 Result = {0};
	for(int I = 1; I < String.Size; I++)
	{
		if(String.Bytes[I] == '#' &&
		   I + 1 < String.Size && String.Bytes[I+1] == '#')
		{
			Result = Str8Copy(Arena, (str8){I, String.Bytes});
			break;
		}
	}
	return Result;
}

str8 UI_HashString(m_arena *Arena, str8 String)
{
	str8 Result = {0};
	for(int I = 1; I < String.Size; I++)
	{
		if(String.Bytes[I] == '#' &&
		   I + 1 < String.Size && String.Bytes[I+1] == '#' &&
		   I + 2 < String.Size && String.Bytes[I+2] == '#')
		{
			Result = Str8PushF(Arena, "?S", (str8){ String.Size - (I + 3), String.Bytes});
			break;
		}
	}
	return Result;
}

int UI_GetTextWidth(d_font Font, draw_context *Draw, str8 String, f32 HeightInPixels)
{
	f32 Scale = stbtt_ScaleForPixelHeight(&Font.Info, HeightInPixels);
	f32 Width = 0;
	for(int I = 0; I < String.Size; I++)
	{
		d_atlas_glyph *Glyph = D_GlyphFromCodePoint(Draw, Font, 1.0, String.Bytes[I]);
		Width += Glyph->AdvanceX * Scale;
	}

	return Width;
}

void UI_DrawBoxCorners(ui_context *UI, draw_context *Draw, m_arena *Arena, v2f Position, v2f Size, f32 LengthOfCornersInPixels)
{

	f32 Width = Size.X;
	f32 Height = Size.Y;
	v2f Corners[4] =
	{
		v2f(Position.X, Position.Y ),
		v2f(Position.X + Width, Position.Y),
		v2f(Position.X , Position.Y + Height),
		v2f(Position.X + Width, Position.Y + Height),
	};
	D_DrawQuad(Draw, Arena, Corners[0], LengthOfCornersInPixels, 2, v4f(0.5, 0.5, 0.5, 1.0));
	D_DrawQuad(Draw, Arena, Corners[0], 2, LengthOfCornersInPixels, v4f(0.5, 0.5, 0.5, 1.0));

	D_DrawQuad(Draw, Arena, SubV2F(Corners[1], v2f(LengthOfCornersInPixels, 0)), LengthOfCornersInPixels, 2, v4f(0.5, 0.5, 0.5, 1.0));
	D_DrawQuad(Draw, Arena, Corners[1], 2, LengthOfCornersInPixels, v4f(0.5, 0.5, 0.5, 1.0));

	D_DrawQuad(Draw, Arena, Corners[2], LengthOfCornersInPixels, 2, v4f(0.5, 0.5, 0.5, 1.0));
	D_DrawQuad(Draw, Arena, SubV2F(Corners[2], v2f(0, LengthOfCornersInPixels)), 2, LengthOfCornersInPixels, v4f(0.5, 0.5, 0.5, 1.0));

	D_DrawQuad(Draw, Arena, SubV2F(Corners[3], v2f(LengthOfCornersInPixels, 0)), LengthOfCornersInPixels, 2, v4f(0.5, 0.5, 0.5, 1.0));
	D_DrawQuad(Draw, Arena, SubV2F(Corners[3], v2f(0, LengthOfCornersInPixels)), 2, LengthOfCornersInPixels, v4f(0.5, 0.5, 0.5, 1.0));
}

b32 UI_TextButton(ui_context *UI, draw_context *Draw, m_arena *Arena,
				  str8 String, f32 HeightInPixels, v2f Position, v4f Colour)
{
	u64 Hash = M_FNVHashString(M_FNV_HASH_START, String);
	f32 Scale = stbtt_ScaleForPixelHeight(&UI->Font.Info, HeightInPixels);
	str8 DisplayString = UI_DisplayString(Arena, String);

	f32 Height = D_LineHeightFromFont(UI->Font, HeightInPixels);
	f32 Width = UI_GetTextWidth(UI->Font, Draw, DisplayString, HeightInPixels);

	v2f Min = AddV2F(Position, v2f(0, UI->Font.Descent * Scale));
	v2f Max = v2f(Min.X + Width, Min.Y + Height);

	v4f MouseNDC = v4f(UI->MousePosition.X, UI->MousePosition.Y, 0, 1);
	MouseNDC.X = (MouseNDC.X/sapp_widthf())*2.0 - 1.0;
	MouseNDC.Y = (MouseNDC.Y/sapp_heightf())*2.0 -1.0;

	v4f MouseWorld = MulM44FV4F(M44FTranspose(R_CurrentInverseTransform(Draw->Render)), MouseNDC);
	v2f MousePosition = v2f(MouseWorld.X / MouseWorld.W, MouseWorld.Y / MouseWorld.W);

	if(MousePosition.X >= Min.X && MousePosition.X <= (Max.X) &&
	   MousePosition.Y >= Min.Y && MousePosition.Y <= (Max.Y))
	{
		UI->Hot = Hash;
	}
	else
	{
		UI->HotAnimation = LerpF32(UI->HotAnimation, 0.0, 0.1);
	}

	b32 Clicked = False;
	if(UI->Hot == Hash)
	{
		if(UI_LeftMouseWentUp(UI))
		{
			Clicked = True;
		}

		UI->HotAnimation = LerpF32(UI->HotAnimation, 1.0, 0.1);
		D_DrawStr8(Draw, Arena, UI->Font, Position, v4f(1, 1, 1, 1), HeightInPixels, DisplayString);
	}
	else
	{
		//D_DrawQuad(Draw, Arena, BoxPosition, Width, Height, v4f(0, 0, 0, 1));
		D_DrawStr8(Draw, Arena, UI->Font, Position, Colour, HeightInPixels, DisplayString);
	}

	return Clicked;
}

b32 UI_TextButtonWrapping(ui_context *UI, draw_context *Draw, m_arena *Arena,
				 		  str8 String, f32 HeightInPixels, v2f Position, v4f Colour)
{
	u64 Hash = M_FNVHashString(M_FNV_HASH_START, String);
	f32 Scale = stbtt_ScaleForPixelHeight(&UI->Font.Info, HeightInPixels);

	int MaxCharacters = 32;

	str8 DisplayString = UI_DisplayString(Arena, String);

	str8 FirstLine = DisplayString;
	str8 SecondLine = DisplayString;
	int LineCount = 1;
	if(DisplayString.Size > MaxCharacters)
	{
		LineCount = 2;
		//SecondLine.Size = DisplayString.Size - MaxCharacters/2;
		//SecondLine.Bytes += MaxCharacters/2;
		FirstLine = Str8PushF(Arena, "?S...?S", (str8){MaxCharacters/2-1, DisplayString.Bytes}, (str8){MaxCharacters/2, DisplayString.Bytes + DisplayString.Size - MaxCharacters/2});
	}

	f32 Height = D_LineHeightFromFont(UI->Font, HeightInPixels);
	f32 Width = UI_GetTextWidth(UI->Font, Draw, DisplayString, HeightInPixels);

	v2f Min = AddV2F(Position, v2f(0, UI->Font.Descent * Scale));
	v2f Max = v2f(Min.X + Width, Min.Y + Height);

	v4f MouseNDC = v4f(UI->MousePosition.X, UI->MousePosition.Y, 0, 1);
	MouseNDC.X = (MouseNDC.X/sapp_widthf())*2.0 - 1.0;
	MouseNDC.Y = (MouseNDC.Y/sapp_heightf())*2.0 -1.0;

	v4f MouseWorld = MulM44FV4F(M44FTranspose(R_CurrentInverseTransform(Draw->Render)), MouseNDC);
	v2f MousePosition = v2f(MouseWorld.X / MouseWorld.W, MouseWorld.Y / MouseWorld.W);

	if(MousePosition.X >= Min.X && MousePosition.X <= (Max.X) &&
	   MousePosition.Y >= Min.Y && MousePosition.Y <= (Max.Y))
	{
		UI->Hot = Hash;
	}
	else
	{
		UI->HotAnimation = LerpF32(UI->HotAnimation, 0.0, 0.1);
	}

	b32 Clicked = False;
	if(UI->Hot == Hash)
	{
		if(UI_LeftMouseWentUp(UI))
		{
			Clicked = True;
		}

		UI->HotAnimation = LerpF32(UI->HotAnimation, 1.0, 0.1);
		if(LineCount == 2)
		{
			D_DrawStr8(Draw, Arena, UI->Font, Position, v4f(1, 1, 1, 1), HeightInPixels, FirstLine);
			//Position.Y -= HeightInPixels;
			//D_DrawStr8(Draw, Arena, UI->Font, Position, v4f(1, 1, 1, 1), HeightInPixels, SecondLine);
		}
		else
		{
			D_DrawStr8(Draw, Arena, UI->Font, Position, v4f(1, 1, 1, 1), HeightInPixels, DisplayString);
		}
	}
	else
	{
		if(LineCount == 2)
		{
			D_DrawStr8(Draw, Arena, UI->Font, Position, v4f(0.2, 0.5, 0.0, 1.0), HeightInPixels, FirstLine);
			//Position.Y -= HeightInPixels;
			//D_DrawStr8(Draw, Arena, UI->Font, Position, v4f(0.2, 0.5, 0.0, 1.0), HeightInPixels, SecondLine);
		}
		else
		{
			D_DrawStr8(Draw, Arena, UI->Font, Position, v4f(0.2, 0.5, 0.0, 1.0), HeightInPixels, DisplayString);
		}
	}

	return Clicked;
}

void UI_Begin(ui_context *UI)
{
	UI->Hot = 0;
}
