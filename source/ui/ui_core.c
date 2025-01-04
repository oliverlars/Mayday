#include "ui.h"
#include "maths.h"

#include "draw.h"
#include "strings.h"
#include <assert.h>


str8 UI_DisplayString(m_arena *Arena, str8 String)
{
	str8 Result = {0};
	for(int I = 1; I < String.Size; I++)
	{
		if(String.Bytes[I] == '#' && 
		   I + 1 < String.Size && String.Bytes[I+1] == '#')
		{
			Result = Str8PushF(Arena, "?S", (str8){I, String.Bytes});
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

	v2f BoxPosition = AddV2F(Position, v2f(0, UI->Font.Descent * Scale));
	
	if(UI->MousePosition.X >= BoxPosition.X && UI->MousePosition.X <= (BoxPosition.X + Width) &&
	   UI->MousePosition.Y >= BoxPosition.Y && UI->MousePosition.Y <= (BoxPosition.Y + Height))
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
		if(UI->MouseWasPressed)
		{
			Clicked = True;
		}

		UI->HotAnimation = LerpF32(UI->HotAnimation, 1.0, 0.1);
		D_DrawStr8(Draw, Arena, UI->Font, Position, v4f(1, 1, 1, 1), HeightInPixels, DisplayString);
	}
	else
	{
		//D_DrawQuad(Draw, Arena, BoxPosition, Width, Height, v4f(0, 0, 0, 1));
		D_DrawStr8(Draw, Arena, UI->Font, Position, v4f(0.2, 0.5, 0.0, 1.0), HeightInPixels, DisplayString);
	}

	return Clicked;
}

void UI_Begin(ui_context *UI)
{
	UI->Hot = 0;
}