#include "ui.h"
#include "maths.h"

#include "draw.h"
#include <assert.h>

int E_GetTextWidth(ui_context *UI, draw_context *Draw, str8 String, f32 HeightInPixels)
{
	f32 Scale = stbtt_ScaleForPixelHeight(&UI->Font.Info, HeightInPixels);
	f32 Width = 0;
	for(int I = 0; I < String.Size; I++)
	{
		d_atlas_glyph *Glyph = D_GlyphFromCodePoint(Draw, UI->Font, 1.0, String.Bytes[I]);
		Width += Glyph->AdvanceX * Scale;
	}

	return Width;
}

void UI_DrawBoxCorners(ui_context *UI, draw_context *Draw, memory_arena *Arena, v2f Position, v2f Size, f32 LengthOfCornersInPixels)
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

b32 UI_TextButton(ui_context *UI, draw_context *Draw, memory_arena *Arena, str8 String, f32 HeightInPixels, v2f Position)
{
	u64 Hash = M_FNVHashString(M_FNV_HASH_START, String);
	f32 Scale = stbtt_ScaleForPixelHeight(&UI->Font.Info, HeightInPixels);
	f32 Height = D_LineHeightFromFont(UI->Font, HeightInPixels);
	f32 Width = E_GetTextWidth(UI, Draw, String, HeightInPixels);

	v4f Colour = v4f(1, 1, 1, 1);
	if(UI->Hot == Hash)
	{
		//Colour = v4f(1, 0, 0, 1);
		UI->HotAnimation = LerpF32(UI->HotAnimation, 1.0, 0.1);
	}

	
	if(UI->MousePosition.X >= Position.X && UI->MousePosition.X <= (Position.X + Width) &&
			UI->MousePosition.Y >= Position.Y && UI->MousePosition.Y <= (Position.Y + Height))
	{
		UI->Hot = Hash;

		v2f CornersPosition = AddV2F(Position, v2f(0, UI->Font.Descent * Scale));
		CornersPosition = SubV2F(CornersPosition, v2f(UI->HotAnimation*5.0, UI->HotAnimation*5.0));
		v2f Size = AddV2F(v2f(Width, Height), v2f(UI->HotAnimation*10.0, UI->HotAnimation*10.0));
		UI_DrawBoxCorners(UI, Draw,Arena, CornersPosition, Size, 20);
	}
	else 
	{
		UI->HotAnimation = LerpF32(UI->HotAnimation, 0.0, 0.1);
		UI->Hot = 0;
	}

	D_DrawStr8(Draw, Arena, UI->Font, Position, Colour, HeightInPixels, String);

	return False;
}