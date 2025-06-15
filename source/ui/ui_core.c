#include "os.h"
#include "render.h"
#include "ui.h"
#include "maths.h"

#include "draw.h"
#include "strings.h"
#include <assert.h>

void UI_RemoveEventFromList(ui_context *UI, ui_event *Event)
{
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
			UI_RemoveEventFromList(UI, Event);
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
			UI_RemoveEventFromList(UI, Event);
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
			UI_RemoveEventFromList(UI, Event);
			break;
		}
	}
	return Result;
}

b32 UI_EventIterate(ui_context *UI, ui_event_iterator *Iter)
{
	if(Iter->Current == UI->EventLast) return False;
	if(!Iter->Current)
	{
		Iter->Current = UI->EventFirst;
	}
	else
	{
		Iter->Current = Iter->Current->Next;
	}
	return True;
}

b32 UI_KeyDownAnyModifiers(ui_context *UI, sapp_keycode Key)
{
		b32 Result = False;
	for(ui_event *Event = UI->EventFirst; Event; Event = Event->Next)
	{
		if(Event->E.type == SAPP_EVENTTYPE_KEY_DOWN && Event->E.key_code == Key)
		{
			Result = True;
			UI_RemoveEventFromList(UI, Event);
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
			UI_RemoveEventFromList(UI, Event);
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
			UI_RemoveEventFromList(UI, Event);
			break;
		}
	}
	return Result;
}

str8 UI_Text(ui_context *UI, m_arena *Arena)
{
	u8 NoOneCanTypeThisManyCharactersInOneFrame[1024];
	int Index = 0;
	for(ui_event *Event = UI->EventFirst; Event; Event = Event->Next)
	{
		b32 AnyNonShiftModifiersPressed = Event->E.modifiers & ~SAPP_MODIFIER_SHIFT;
		if(Event->E.type == SAPP_EVENTTYPE_CHAR && Event->E.char_code >= 32 && Event->E.char_code <= 126 && !AnyNonShiftModifiersPressed)
		{
			NoOneCanTypeThisManyCharactersInOneFrame[Index++] = Event->E.char_code;
		}
	}
	str8 Temporary = {Index, NoOneCanTypeThisManyCharactersInOneFrame};
	return Str8Copy(Arena, Temporary);
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
	str8 Result = String;
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
		u32 CodePoint = String.Bytes[I];
		u32 NextCodePoint = String.Bytes[Min(String.Size, I + 1)];
		f32 TabMultiplier = 1.0;
		d_atlas_glyph *Glyph = D_GlyphFromCodePoint(Draw, Font, 1.0, CodePoint);
		if(CodePoint == '\t') TabMultiplier = 4.0;

		int Kern = stbtt_GetCodepointKernAdvance(&Font.Info, String.Bytes[I], NextCodePoint);
		Width += (Kern + Glyph->AdvanceX) * Scale * TabMultiplier;
	}

	return Width;
}

void UI_DrawBoxCorners(ui_context *UI, draw_context *Draw, m_arena *Arena, v2f Position, v2f Size, f32 LengthOfCornersInPixels, v4f Colour)
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


	D_DrawQuad(Draw, Arena, Corners[0], LengthOfCornersInPixels, 2, Colour);
	D_DrawQuad(Draw, Arena, Corners[0], 2, LengthOfCornersInPixels, Colour);

	D_DrawQuad(Draw, Arena, Sub(Corners[1], v2f(LengthOfCornersInPixels, 0)), LengthOfCornersInPixels, 2, Colour);
	D_DrawQuad(Draw, Arena, Corners[1], 2, LengthOfCornersInPixels, Colour);

	D_DrawQuad(Draw, Arena, Corners[2], LengthOfCornersInPixels, 2, Colour);
	D_DrawQuad(Draw, Arena, Sub(Corners[2], v2f(0, LengthOfCornersInPixels)), 2, LengthOfCornersInPixels, Colour);

	D_DrawQuad(Draw, Arena, Sub(Corners[3], v2f(LengthOfCornersInPixels, 0)), LengthOfCornersInPixels, 2, Colour);
	D_DrawQuad(Draw, Arena, Sub(Corners[3], v2f(0, LengthOfCornersInPixels)), 2, LengthOfCornersInPixels, Colour);
}

void UI_DrawBoxOutline(ui_context *UI, draw_context *Draw, m_arena *Arena, v2f Position, v2f Size, v4f Colour)
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


	D_DrawQuad(Draw, Arena, Corners[0], Width, 2, Colour);
	D_DrawQuad(Draw, Arena, Corners[0], 2, Height, Colour);

	D_DrawQuad(Draw, Arena, Corners[1], 2, Height, Colour);
	D_DrawQuad(Draw, Arena, Corners[2], Width, 2, Colour);
}

ui_style UI_GetStyle(ui_context *UI)
{
	assert(UI->StyleCount > 0);
	return UI->StyleStack[UI->StyleCount - 1];
}

void UI_PushStyle(ui_context *UI, ui_style Style)
{
	UI->StyleStack[UI->StyleCount++] = Style;
}

void UI_PopStyle(ui_context *UI)
{
	assert(UI->StyleCount > 0);
	UI->StyleCount--;
}

ui_widget *UI_WidgetFromString(ui_context *UI, str8 String)
{
	per_thread_context *ThreadContext = GetThreadLocalContext();
	u64 Hash = M_FNVHashString(M_FNV_HASH_START, String);

	ui_widget *Widget = UI->WidgetTable + (Hash % UI_TABLE_SIZE);
	for(;Widget->NextInHashSlot != Null && Widget->Hash != Hash; Widget = Widget->NextInHashSlot);

	if(Widget->Hash != 0 && Widget->Hash != Hash)
	{
		ui_widget *NextWidget = ArenaPushArrayZero(UI->Arena, ui_widget, 1);
		Widget->NextInHashSlot = NextWidget;
		Widget = NextWidget;
		Widget->Name = Str8Copy(UI->Arena, String);
		Widget->Hash = Hash;

		Widget = NextWidget;
	}
	else if(Widget->Hash == 0)
	{
		ui_widget Zeroed = {0};
		Widget[0] = Zeroed;

		Widget->Hash = Hash;
		Widget->Name = Str8Copy(UI->Arena, String);
	}

	assert(Widget->Hash != 0);
	assert(Str8Equal(Widget->Name, String));

	return Widget;
}

void UI_Label(ui_context *UI, draw_context *Draw, m_arena *Arena,
				  str8 String, f32 HeightInPixels, v2f Position, v4f Colour)
{
	ui_signal Result = {0};
	ui_widget *Widget = UI_WidgetFromString(UI, String);
	u64 Hash = Widget->Hash;

	f32 Scale = stbtt_ScaleForPixelHeight(&UI->Font.Info, HeightInPixels);
	str8 DisplayString = UI_DisplayString(Arena, String);

	f32 Height = D_LineHeightFromFont(UI->Font, HeightInPixels);
	f32 Width = UI_GetTextWidth(UI->Font, Draw, DisplayString, HeightInPixels);

	v2f Min = Add(Position, v2f(0, UI->Font.Descent * Scale));
	v2f Max = v2f(Min.X + Width, Min.Y + Height);

	v4f MouseNDC = v4f(UI->MousePosition.X, UI->MousePosition.Y, 0, 1);
	MouseNDC.X = (MouseNDC.X/sapp_widthf())*2.0 - 1.0;
	MouseNDC.Y = (MouseNDC.Y/sapp_heightf())*2.0 -1.0;

	v4f MouseWorld = Mul(M44FTranspose(R_CurrentInverseTransform(Draw->Render)), MouseNDC);
	v2f MousePosition = v2f(MouseWorld.X / MouseWorld.W, MouseWorld.Y / MouseWorld.W);

	ui_style Style = UI_GetStyle(UI);

	D_DrawStr8(Draw, Arena, Style.Font, Position, Colour, HeightInPixels, DisplayString);
}

ui_signal UI_TextButton(ui_context *UI, draw_context *Draw, m_arena *Arena,
				  str8 String, f32 HeightInPixels, v2f Position, v4f Colour)
{
	ui_signal Result = {0};
	ui_widget *Widget = UI_WidgetFromString(UI, String);
	u64 Hash = Widget->Hash;
	f32 Scale = stbtt_ScaleForPixelHeight(&UI->Font.Info, HeightInPixels);
	str8 DisplayString = UI_DisplayString(Arena, String);

	f32 Height = D_LineHeightFromFont(UI->Font, HeightInPixels);
	f32 Width = UI_GetTextWidth(UI->Font, Draw, DisplayString, HeightInPixels);

	v2f Min = Add(Position, v2f(0, UI->Font.Descent * Scale));
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

	ui_style Style = UI_GetStyle(UI);

	if(UI->Hot == Hash)
	{
		if(UI_LeftMouseWentUp(UI))
		{
			Result.Clicked = True;
		}
		Result.Hover = True;

		Widget->HotAnimation = Lerp(Widget->HotAnimation, 1.0, 0.1);
		D_DrawStr8(Draw, Arena, Style.Font, Position, v4f(1, 1, 1, 1), HeightInPixels, DisplayString);
	}
	else
	{
		//D_DrawQuad(Draw, Arena, BoxPosition, Width, Height, v4f(0, 0, 0, 1));
		D_DrawStr8(Draw, Arena, Style.Font, Position, Colour, HeightInPixels, DisplayString);
		Widget->HotAnimation = Lerp(Widget->HotAnimation, 0.0, 0.1);
	}

	Result.HotAnimation = Widget->HotAnimation;

	return Result;
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

	v2f Min = Add(Position, v2f(0, UI->Font.Descent * Scale));
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
		UI->HotAnimation = Lerp(UI->HotAnimation, 0.0, 0.1);
	}

	b32 Clicked = False;
	if(UI->Hot == Hash)
	{
		if(UI_LeftMouseWentUp(UI))
		{
			Clicked = True;
		}

		UI->HotAnimation = Lerp(UI->HotAnimation, 1.0, 0.1);
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
	UI->StyleCount = 0;
	ui_style Style = {0};
	Style.Font = UI->Font;
	UI_PushStyle(UI, Style);
}

u64 UI_IDFromString(str8 String)
{
	return M_FNVHashString(M_FNV_HASH_START, String);
}

void UI_SetActive(ui_context *UI, u64 Hash)
{
	UI->LastActive = UI->Active;
	UI->Active = Hash;
}

ui_textbox_signal UI_TextBox(m_arena *Arena,
							 d_font Font,
							 v2f Position,
							 f32 BoxWidth,
							 ui_context *UI,
							 draw_context *Draw,
							 edit_context *Edit,
							 f32 HeightInPixels,
							 str8 String,
							 editable_buffer *Buffer)
{
	u64 Hash = M_FNVHashString(M_FNV_HASH_START, String);
	f32 Scale = stbtt_ScaleForPixelHeight(&UI->Font.Info, HeightInPixels);
	str8 DisplayString = UI_DisplayString(Arena, String);

	f32 Height = D_LineHeightFromFont(Font, HeightInPixels);
	f32 Width = UI_GetTextWidth(Font, Draw, DisplayString, HeightInPixels);

	f32 CursorXOffset = Max(0, Edit->Cursors[0].Position.X - BoxWidth*0.8 - Position.X);
	Width = Max(Width, BoxWidth);

	v2f Min = Add(Position, v2f(0, UI->Font.Descent * Scale));
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
		UI->HotAnimation = Lerp(UI->HotAnimation, 0.0, 0.1);
	}

	b32 Clicked = False;
	if(UI->Hot == Hash)
	{
		if(UI_LeftMouseWentUp(UI))
		{
			Clicked = True;
			UI_SetActive(UI, Hash);
		}

		UI->HotAnimation = Lerp(UI->HotAnimation, 1.0, 0.1);
	}
	else
	{
		UI->HotAnimation = Lerp(UI->HotAnimation, 0.0, 0.1);
	}

	if(UI_KeyDown(UI, SAPP_KEYCODE_LEFT, 0))
	{
		E_MoveCursors(Edit, Draw, Font, Edit->Cursors, Buffer, E_MOVE_LEFT, E_MOVE_NOT_SELECTING);
	}
	else if(UI_KeyDown(UI, SAPP_KEYCODE_LEFT, SAPP_MODIFIER_SHIFT))
	{
		E_MoveCursors(Edit, Draw, Font, Edit->Cursors, Buffer, E_MOVE_LEFT, E_MOVE_SELECTING);
	}

	if(UI_KeyDown(UI, SAPP_KEYCODE_RIGHT, 0))
	{
		E_MoveCursors(Edit, Draw, Font, Edit->Cursors, Buffer, E_MOVE_RIGHT, E_MOVE_NOT_SELECTING);
	}
	else if(UI_KeyDown(UI, SAPP_KEYCODE_RIGHT, SAPP_MODIFIER_SHIFT))
	{
		E_MoveCursors(Edit, Draw, Font, Edit->Cursors, Buffer, E_MOVE_RIGHT, E_MOVE_SELECTING);
	}

	if(UI_KeyDown(UI, SAPP_KEYCODE_TAB, 0))
	{
		E_InsertTo(Edit, Edit->Cursors, Buffer, Str8("\t"));
	}

	if(UI_KeyDown(UI, SAPP_KEYCODE_SPACE, 0))
	{
		E_InsertTo(Edit, Edit->Cursors, Buffer, Str8(" "));
	}

	ui_textbox_signal Result = {0};
	Result.String = Str8Copy(Arena, Buffer->String);

	if(UI_KeyDown(UI, SAPP_KEYCODE_ENTER, 0))
	{
		Result.Submitted = True;
		Buffer->String.Size = 0;
		Edit->Cursors[0].Selection = v2i(0, 0);
	}


	if(UI_KeyDown(UI, SAPP_KEYCODE_BACKSPACE, 0))
	{
		E_DeleteTo(Edit, Edit->Cursors, Buffer);
	}

	v2f BoxPosition = Position;
	BoxPosition.Y += UI->Font.Descent * Scale;

	{
		v2f LargerBoxPosition = BoxPosition;
		v2f LargerBoxSize = v2f(Width, Height - UI->Font.Descent * Scale);
		LargerBoxSize.X += 20;
		LargerBoxSize.Y += 20;
		LargerBoxPosition.X -= 10;
		LargerBoxPosition.Y -= 10;
		UI_DrawBoxOutline(UI, Draw, Arena, LargerBoxPosition, LargerBoxSize, v4f(0.2, 0.02, 0.0, 1.0));
		LargerBoxPosition.X -= 10;
		LargerBoxPosition.Y += 10;
		UI_DrawBoxOutline(UI, Draw, Arena, LargerBoxPosition, LargerBoxSize, v4f(0.2, 0.02, 0.0, 1.0));
	}

	v2f StringPosition = Position;
	StringPosition.X -= CursorXOffset;
	StringPosition.Y -= UI->Font.Descent * Scale;
	R_PushScissor(Draw->Render, v4i(Position.X, sapp_height() - Position.Y - Height, Width, Height));
	D_DrawStr8(Draw,
			   Arena, Font,
			   StringPosition, v4f(0.8, 0.3, 0, 1),
			   Height, Buffer->String);
	R_PopScissor(Draw->Render);

	//LogInfo("Cursor pos: ?u32", Edit->Cursors[0].Selection.X);

	if(UI->Active == Hash)
	{
		v2i Selection = E_GetSortedSelection(Edit->Cursors[0].Selection);
		D_DrawSelectionRange(Draw,
							 Arena,
							 Font,
							 Position,
							 v4f(0.04, 0.01, 0, 0.1),
							 HeightInPixels,
							 Buffer->String,
							 v2i(0, Buffer->String.Size),
							 Selection.X,
							 Selection.Y);
		if(UI->MouseIsDragging)
		{
			int Index = E_IndexFromPosition(Edit, Draw, Font,
											Buffer, v2i(0, Buffer->String.Size), Position, UI->MousePosition);
			Edit->Cursors[0].Selection.Y = Index;

			Edit->Cursors[0].TargetPosition = E_GetPositionFromEditableBuffer(Edit,
																		Draw,
																		Edit->Cursors[0].Selection.Y,
																		Buffer[0],
																		Font,
																		Position);
			Edit->Cursors[0].Position = Edit->Cursors[0].TargetPosition;
			D_DrawQuad(Draw, Arena, Edit->Cursors[0].Position, 5, Height, v4f(0.3, 0.7, 0, 1));
		}
		else
		{
			Edit->Cursors[0].TargetPosition = E_GetPositionFromEditableBuffer(Edit,
																	 Draw,
																	 Edit->Cursors[0].Selection.Y,
																	 Buffer[0],
																	 Font,
																	 Position);

			Edit->Cursors[0].Position = LerpV2F(Edit->Cursors[0].Position, Edit->Cursors[0].TargetPosition, 0.3);
			v2f CursorPosition = Edit->Cursors[0].Position;
			CursorPosition.X -= CursorXOffset;
			CursorPosition.Y -= UI->Font.Descent * Scale;
			D_DrawQuad(Draw, Arena, CursorPosition, 2, Height, v4f(0.3, 0.7, 0, 1));

		}

		if(UI_DoubleClicked(UI))
		{
			int Index = E_IndexFromPosition(Edit, Draw, Font,
										Buffer, v2i(0, Buffer->String.Size), Position,
										UI->MousePosition);
			cursor *Cursor = Edit->Cursors;

			v2i InitialSelection = Cursor->Selection;
			E_MoveCursors(Edit, Draw, Font, Cursor, Buffer, E_MOVE_WORD_RIGHT, E_MOVE_SELECTING);
			v2i Selection = Cursor->Selection;
			Cursor->Selection = InitialSelection;
			E_MoveCursors(Edit, Draw, Font, Cursor, Buffer, E_MOVE_WORD_LEFT, E_MOVE_NOT_SELECTING);
			Cursor->Selection = v2i(Cursor->Selection.X, Selection.Y);
		}
		else if(UI_TripleClicked(UI))
		{
			int Index = E_IndexFromPosition(Edit, Draw, Font,
											Buffer, v2i(0, Buffer->String.Size), Position,
											UI->MousePosition);

			int LineStart = E_LineStartFromIndex(Edit, Edit->Cursors[0].Selection.Y, Buffer);
			int LineEnd = E_LineEndFromIndex(Edit, Edit->Cursors[0].Selection.Y, Buffer);
			Edit->Cursors[0].Selection = v2i(LineStart, LineEnd);
		}
		else if(UI_QuadClicked(UI))
		{
			Edit->Cursors[0].Selection = v2i(0, Buffer->String.Size);

			int Index = E_IndexFromPosition(Edit, Draw, Font,
											Buffer, v2i(0, Buffer->String.Size), Position,
											UI->MousePosition);
		}
		else if(UI_LeftMouseWentDown(UI))
		{
			int Index = E_IndexFromPosition(Edit, Draw, Font,
										Buffer, v2i(0, Buffer->String.Size), Position,
										UI->MousePosition);
			cursor *Cursor = Edit->Cursors;
			Cursor->Selection = v2i(Index, Index);
		}

		str8 TextToInsert = UI_Text(UI, Arena);
		if(TextToInsert.Size > 0)
		{
			E_InsertTo(Edit, Edit->Cursors, Buffer, TextToInsert);
		}
	}
	else
	{
		//UI_DrawBoxCorners(UI, Draw, Arena, Position, v2f(Width, HeightInPixels), 10);
	}

	return Result;
}
