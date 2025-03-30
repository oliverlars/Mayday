#include "draw.h"
#include "edit.h"
#include "memory.h"
#include "os.h"
#include "strings.h"
#include "ui.h"

#include <assert.h>


void UI_DrawBuffer(draw_context *D,
				   m_arena *Arena,
				   d_font Font,
				   v2f Position,
				   v4f Colour,
				   f32 HeightInPixels,
				   str8 String)
{
	render_context *R = D->Render;

	f32 Scale = stbtt_ScaleForPixelHeight(&Font.Info, HeightInPixels);
	int TotalTriangleCount = String.Size*6;
	r_command Command = {0};
	Command.Vertices = (r_vertex*)ArenaPushZero(Arena, TotalTriangleCount*sizeof(r_vertex));
	Command.VerticesCount = TotalTriangleCount;
	Command.VerticesFormat = R_VF_POSITION | R_VF_UV;

	u64 Hash = M_FNVHashBytes(M_FNV_HASH_START, (u8*)&Font, sizeof(d_font));
	Hash = M_FNVHashBytes(Hash, (u8*)&HeightInPixels, sizeof(f32));

	f32 StartX = Position.X;
	r_vertex *R_Vertices = Command.Vertices;

	b32 NewLine = False;
	v4f Colours[2] = {Colour, v4f(0, 0, 1, 1) };
	for(int I = 0; I < String.Size; I++)
	{
		// TODO(Oliver): Needs to actually iterate utf-8
		u32 CodePoint = String.Bytes[I];
		u32 NextCodePoint = String.Bytes[Min(String.Size, I + 1)];



		f32 TabMultiplier = 1.0f;
		if(CodePoint == '\t')
		{
			CodePoint = ' ';
			TabMultiplier *= 4.0;
		}

		if(CodePoint == '\n')
		{
			NewLine = True;
			Position.Y -= (Font.Ascent - Font.Descent + Font.LineGap)*Scale;
			Position.X = StartX;
			continue;
		}
		else
		{
			NewLine = False;
		}

		//if(NewLine) CodePoint = 'N';

		d_atlas_glyph *Glyph  = D_GlyphFromCodePoint2(D, Hash, Font, HeightInPixels, CodePoint);

		f32 GlyphWidth = ((f32)(Glyph->Box.Z - Glyph->Box.X)*Scale);
		f32 GlyphHeight = ((f32)(Glyph->Box.W - Glyph->Box.Y)*Scale);

		f32 GlyphBitmapWidth = ((f32)(Glyph->BitmapBox.Z - Glyph->BitmapBox.X));
		f32 GlyphBitmapHeight = ((f32)(Glyph->BitmapBox.W - Glyph->BitmapBox.Y));

		v2f Vertices[6] =
		{
			v2f(Position.X, Position.Y),
			v2f(Position.X + GlyphWidth, Position.Y),
			v2f(Position.X + GlyphWidth, Position.Y + GlyphHeight),

			v2f(Position.X, Position.Y),
			v2f(Position.X, Position.Y + GlyphHeight),
			v2f(Position.X + GlyphWidth, Position.Y + GlyphHeight),
		};

		v2f UVs[6] =
		{

			v2f(Glyph->AtlasPosition.X, Glyph->AtlasPosition.Y + GlyphBitmapHeight),
			v2f(Glyph->AtlasPosition.X + GlyphBitmapWidth, Glyph->AtlasPosition.Y + GlyphBitmapHeight),
			v2f(Glyph->AtlasPosition.X + GlyphBitmapWidth, Glyph->AtlasPosition.Y),

			v2f(Glyph->AtlasPosition.X, Glyph->AtlasPosition.Y + GlyphBitmapHeight),
			v2f(Glyph->AtlasPosition.X, Glyph->AtlasPosition.Y),
			v2f(Glyph->AtlasPosition.X + GlyphBitmapWidth, Glyph->AtlasPosition.Y)
		};

		for(int V = 0; V < 6; V++)
		{
			r_vertex Vertex = {
				.Position = v4f(Vertices[V].X, Vertices[V].Y, 0, 1),
				.UV = DivV2F(UVs[V], V2FromF32((f32)D_FONT_ATLAS_SIZE)),
				.Colour = Colours[NewLine],
			};

			Vertex.Position.Y += (Glyph->Box.Y*Scale);
			Vertex.Position.X += (Glyph->Box.X*Scale);

			if(CodePoint != ' ') *R_Vertices++ = Vertex;
		}

		if(NewLine)
		{
			Position.Y -= (Font.Ascent - Font.Descent + Font.LineGap)*Scale;
			Position.X = StartX;
			continue;
		}

		int Kern = stbtt_GetCodepointKernAdvance(&Font.Info, CodePoint, NextCodePoint);
		Position.X += (Kern + Glyph->AdvanceX) * Scale * TabMultiplier;
	}

	Command.Type = R_COMMAND_GLYPH;
	R_PushCommand(R, Arena, Command);
}

void UI_Buffer(ui_context *UI, draw_context *Draw, edit_context *Edit,
			   m_arena *Arena, d_font Font,
			   editable_buffer *Buffer,
			   v2i Size)
{
	//TODO(Oliver): Theres a better way to do this
	int WindowHeight = Size.Y;
	int WindowWidth = Size.X;

	v2f BufferPosition = v2f(100, (f32)WindowHeight - 50);
	BufferPosition.Y += Buffer->Offset.Y;

	v2f NumbersPosition = v2f(10, BufferPosition.Y);

	f32 LineHeight = D_LineHeightFromFont(Font, Buffer->HeightInPixels);


	f32 HeightOffset = 0;
	for(cursor *Cursor = Edit->Cursors; Cursor; Cursor = Cursor->Next)
	{
		// D_DrawStr8(M->Draw, M->FrameArena, M->CodeFont, v2f(1600, WindowHeight - 500 + HeightOffset), v4f(1, 1, 1, 1),
		// 	M->Buffer.HeightInPixels, Str8PushF(M->FrameArena, "Start: ?u32 | End: ?u32 | VisualX: ?u32",
		// 	Cursor->Selection.X, Cursor->Selection.Y, Cursor->VisualX));
		HeightOffset -= Buffer->HeightInPixels;
	}

	int MainCursorLine = E_LineFromBufferIndex(Edit, Edit->Cursors[0].Selection.Y, Buffer);
	int LineCount = 0;
	for(int I = 0; I < Buffer->String.Size; I++)
	{
		if(MainCursorLine == LineCount)
		{
			D_DrawStr8(Draw, Arena, Font, NumbersPosition,
				v4f(0.8, 0.3, 0, 1), LineHeight, Str8PushF(Arena, "?u32", LineCount + 1));
		}

		if(Buffer->String.Bytes[I] == '\n')
		{
			if(MainCursorLine != LineCount)
			{
				D_DrawStr8(Draw, Arena, Font, NumbersPosition,
						   v4f(0.1, 0.04, 0, 1), LineHeight, Str8PushF(Arena, "?u32", LineCount + 1));
			}
			LineCount += 1;
			NumbersPosition.Y -= LineHeight;
		}
	}

	for(cursor *Cursor = Edit->Cursors; Cursor; Cursor = Cursor->Next)
	{
		v2i Selection = E_GetSortedSelection(Cursor->Selection);
		str8 SelectedString = Buffer->String;
		SelectedString.Bytes += Selection.X;
		SelectedString.Size = Selection.Y - Selection.X;
		D_DrawSelectionRange(Draw,
							 Arena,
							 Font,
							 BufferPosition,
							 v4f(0.04, 0.01, 0, 0.1),
							 Buffer->HeightInPixels,
							 SelectedString,
							 Selection.X,
							 Selection.Y);
	}

	D_DrawStr8(Draw,
			   Arena, Font,
			   BufferPosition, v4f(0.8, 0.3, 0, 1),
			   Buffer->HeightInPixels, Buffer->String);

	//if(UI->MouseIsDragging || UI->MouseWentDown)
	if(UI->MouseIsDragging)
	{
		int Index = E_IndexFromPosition(Edit, Draw, Font,
										Buffer, BufferPosition, UI->MousePosition);
		Edit->Cursors[0].Selection.Y = Index;

		Edit->Cursors[0].TargetPosition = E_GetPositionFromEditableBuffer(Edit,
																	Draw,
																	Edit->Cursors[0].Selection.Y,
																	Buffer[0],
																	Font,
																	BufferPosition);
		Edit->Cursors[0].Position = Edit->Cursors[0].TargetPosition;
		D_DrawQuad(Draw, Arena, Edit->Cursors[0].Position, 5, LineHeight, v4f(0.3, 0.7, 0, 1));
	}
	else
	{
		for(cursor *Cursor = Edit->Cursors; Cursor; Cursor = Cursor->Next)
		{
			Cursor->TargetPosition = E_GetPositionFromEditableBuffer(Edit,
																	 Draw,
																	 Cursor->Selection.Y,
																	 Buffer[0],
																	 Font,
																	 BufferPosition);

			Cursor->Position = LerpV2F(Cursor->Position, Cursor->TargetPosition, 0.3);
			D_DrawQuad(Draw, Arena, Cursor->Position, 5, LineHeight, v4f(0.3, 0.7, 0, 1));
		}
	}

	v2f ScrollOffset = UI_Scroll(UI);
	Buffer->Offset.X += ScrollOffset.X*15;
	Buffer->Offset.Y = Max(Buffer->Offset.Y + ScrollOffset.Y*15, 0.0);


	if(UI_DoubleClicked(UI))
	{
		int Index = E_IndexFromPosition(Edit, Draw, Font,
									Buffer, BufferPosition,
									UI->MousePosition);
		cursor *LastCursor = E_LastCursor(Edit, Edit->Cursors);
		cursor *Cursor = LastCursor;

		v2i InitialSelection = Cursor->Selection;
		E_MoveCursors(Edit, Draw, Font, Cursor, Buffer, E_MOVE_WORD_RIGHT, E_MOVE_SELECTING);
		v2i Selection = Cursor->Selection;
		Cursor->Selection = InitialSelection;
		E_MoveCursors(Edit, Draw, Font, Cursor, Buffer, E_MOVE_WORD_LEFT, E_MOVE_NOT_SELECTING);
		Cursor->Selection = v2i(Cursor->Selection.X, Selection.Y);

		if(LastCursor != Cursor)
		{
			LastCursor[0] = Cursor[0];
			LastCursor->Next = Null;
		}
	}
	else if(UI_TripleClicked(UI))
	{
		int Index = E_IndexFromPosition(Edit, Draw, Font,
										Buffer, BufferPosition,
										UI->MousePosition);

		cursor *LastCursor = E_LastCursor(Edit, Edit->Cursors);
		cursor *Cursor = LastCursor;
		if(False)
		{
			cursor NextCursor = Cursor[0];
			NextCursor.Selection = v2i(Index, Index);
			E_AppendCursor(Edit, Edit->Arena, Edit->Cursors, NextCursor);
			Cursor = Cursor->Next;
		}
		else
		{
			Edit->Cursors[0].Next = Null;
		}

		int LineStart = E_LineStartFromIndex(Edit, Cursor->Selection.Y, Buffer);
		int LineEnd = E_LineEndFromIndex(Edit, Cursor->Selection.Y, Buffer);
		Cursor->Selection = v2i(LineStart, LineEnd);

		if(LastCursor != Cursor)
		{
			LastCursor[0] = Cursor[0];
			LastCursor->Next = Null;
		}
	}
	else if(UI_QuadClicked(UI))
	{
		Edit->Cursors[0].Selection = v2i(0, Buffer->String.Size);

		int Index = E_IndexFromPosition(Edit, Draw, Font,
										Buffer, BufferPosition,
										UI->MousePosition);

		cursor *LastCursor = E_LastCursor(Edit, Edit->Cursors);
		cursor *Cursor = LastCursor;

		if(LastCursor != Cursor)
		{
			LastCursor[0] = Cursor[0];
			LastCursor->Next = Null;
		}
	}
	else if(UI_LeftMouseWentDown(UI))
	{
		int Index = E_IndexFromPosition(Edit, Draw, Font,
									Buffer, BufferPosition,
									UI->MousePosition);
		cursor *LastCursor = E_LastCursor(Edit, Edit->Cursors);
		cursor *Cursor = LastCursor;
		Edit->Cursors->Next = Null;
		Cursor = Edit->Cursors;
		Cursor->Selection = v2i(Index, Index);

	}


	if(UI_KeyDown(UI, SAPP_KEYCODE_ENTER, 0))
	{
		E_InsertTo(Edit, Edit->Cursors, Buffer, Str8("\n"));

		int TotalWhiteSpaceInserted = 0;
		for(cursor *Cursor = Edit->Cursors; Cursor; Cursor = Cursor->Next)
		{
			int LineStart = E_LineStartFromIndex(Edit, Cursor->Selection.Y, Buffer);

			int FirstNonWhitespace = LineStart;
			for(;Buffer->String.Bytes[FirstNonWhitespace] == ' ' ||
				 Buffer->String.Bytes[FirstNonWhitespace] == '\t';)
			{
				FirstNonWhitespace++;
			}
			str8 WhiteSpaceToInsert = (str8){	FirstNonWhitespace - LineStart, Buffer->String.Bytes + LineStart};

			E_InsertTextAtIndex(Edit, Cursor->Selection.Y, Buffer, WhiteSpaceToInsert);
			TotalWhiteSpaceInserted += WhiteSpaceToInsert.Size;
			Cursor->Selection.Y += TotalWhiteSpaceInserted;
			Cursor->Selection.X = Cursor->Selection.Y;
		}
	}

	if(UI_KeyDown(UI, SAPP_KEYCODE_D, SAPP_MODIFIER_SUPER))
	{
		str8 StringToFind = {};
		cursor *Cursor = E_LastCursor(Edit, Edit->Cursors);
		if(Cursor->Selection.X == Cursor->Selection.Y)
		{
			int Index = E_IndexFromPosition(Edit, Draw, Font,
										Buffer, BufferPosition,
										UI->MousePosition);
			v2i InitialSelection = Cursor->Selection;
			E_MoveCursors(Edit, Draw, Font, Cursor, Buffer, E_MOVE_WORD_RIGHT, E_MOVE_SELECTING);
			v2i Selection = Cursor->Selection;
			Cursor->Selection = InitialSelection;
			E_MoveCursors(Edit, Draw, Font, Cursor, Buffer, E_MOVE_WORD_LEFT, E_MOVE_NOT_SELECTING);
			Cursor->Selection = v2i(Cursor->Selection.X, Selection.Y);
		}
		else
		{
			StringToFind.Size = Cursor->Selection.Y - Cursor->Selection.X;
			StringToFind.Bytes = Buffer->String.Bytes + Cursor->Selection.X;

			// TODO(Oliver): Unicode
			u8 *Start = Buffer->String.Bytes;
			u8 *End = Buffer->String.Bytes + Buffer->String.Size;
			for(u8 *Byte = Buffer->String.Bytes + Cursor->Selection.Y + 1; Byte < End; Byte++)
			{
				str8 CurrentString = {0};
				CurrentString.Bytes = Byte;
				CurrentString.Size = Min(StringToFind.Size, End-Byte);
				if(Str8Equal(CurrentString, StringToFind))
				{
					cursor NextCursor = Cursor[0];
					NextCursor.Selection.X = Byte-Start;
					NextCursor.Selection.Y = NextCursor.Selection.X + StringToFind.Size;
					E_AppendCursor(Edit, Edit->Arena, Edit->Cursors, NextCursor);
					break;
				}
			}

		}
	}

	if(UI_KeyDown(UI, SAPP_KEYCODE_TAB, 0))
	{
		E_InsertTo(Edit, Edit->Cursors, Buffer, Str8("\t"));
	}

	if(UI_KeyDown(UI, SAPP_KEYCODE_SPACE, 0))
	{
		E_InsertTo(Edit, Edit->Cursors, Buffer, Str8(" "));
	}

	if(UI_KeyDown(UI, SAPP_KEYCODE_P, SAPP_MODIFIER_SUPER))
	{
		UI->Active = UI_IDFromString(Str8("TreeViewTextBox"));
		UI->View = UI_VIEW_TREE;
	}

	b32 ShouldRemove = False;
	for(ui_event_iterator It = {0};UI_EventIterate(UI, &It);)
	{
		sapp_event Event = It.Current->E;
		if(ShouldRemove)
		{
			assert(It.Current && It.Current->Previous);
			UI_RemoveEventFromList(UI, It.Current->Previous);
			ShouldRemove = False;
		}
		if(Event.type == SAPP_EVENTTYPE_KEY_DOWN)
		{
			edit_move_type MoveType = E_MOVE_NOT_SELECTING;
			if(Event.modifiers & SAPP_MODIFIER_SHIFT)
			{
				MoveType = E_MOVE_SELECTING;
			}

			edit_movement Movement = E_MOVE_NONE;
			switch(Event.key_code)
			{
				case SAPP_KEYCODE_LEFT:
				{
					Movement = E_MOVE_LEFT;
					if(Event.modifiers & SAPP_MODIFIER_ALT)
					{
						Movement = E_MOVE_WORD_LEFT;
					}
					else if (Event.modifiers & SAPP_MODIFIER_SUPER)
					{
						Movement = E_MOVE_LINE_START;
					}

					E_MoveCursors(Edit, Draw, Font, Edit->Cursors, Buffer, Movement, MoveType);
					ShouldRemove = True;
				} break;
				case SAPP_KEYCODE_RIGHT:
				{
					Movement = E_MOVE_RIGHT;
					if(Event.modifiers & SAPP_MODIFIER_ALT)
					{
						Movement = E_MOVE_WORD_RIGHT;
					}
					else if (Event.modifiers & SAPP_MODIFIER_SUPER)
					{
						Movement = E_MOVE_LINE_END;
					}

					E_MoveCursors(Edit, Draw, Font, Edit->Cursors, Buffer, Movement, MoveType);
					ShouldRemove = True;
				} break;
				case SAPP_KEYCODE_UP:
				{
					if(Event.modifiers & SAPP_MODIFIER_SUPER)
					{
						cursor *LastCursor = E_LastCursor(Edit, Edit->Cursors);
						cursor NextCursor = LastCursor[0];
						E_AppendCursor(Edit, Edit->Arena, Edit->Cursors, NextCursor);
					}
					E_MoveCursors(Edit, Draw, Font, Edit->Cursors, Buffer, E_MOVE_UP, MoveType);
					ShouldRemove = True;
				} break;
				case SAPP_KEYCODE_DOWN:
				{
					if(Event.modifiers & SAPP_MODIFIER_SUPER)
					{
						cursor *LastCursor = E_LastCursor(Edit, Edit->Cursors);
						cursor NextCursor = LastCursor[0];
						E_AppendCursor(Edit, Edit->Arena, Edit->Cursors, NextCursor);
					}
					E_MoveCursors(Edit, Draw, Font, Edit->Cursors, Buffer, E_MOVE_DOWN, MoveType);
					ShouldRemove = True;
				} break;
				case SAPP_KEYCODE_BACKSPACE:
				{
					E_DeleteTo(Edit, Edit->Cursors, Buffer);
				}
				break;
				default:
				break;
			}
		}
	}

	// str8 TextToInsert = UI_Text(UI, Arena);
	// if(TextToInsert.Size > 0)
	// {
	// 	E_InsertTo(Edit, Edit->Cursors, Buffer, TextToInsert);
	// }

	// else if(UI_LeftMouseWentDownWithModifiers(UI, SAPP_MODIFIER_ALT))
	// {
	// 	int Index = E_IndexFromPosition(Edit, Draw, Font,
	// 								Buffer, BufferPosition,
	// 								UI->MousePosition);
	// 	cursor *LastCursor = E_LastCursor(Edit, Edit->Cursors);
	// 	cursor *Cursor = LastCursor;
	// 	{
	// 		cursor NextCursor = Cursor[0];
	// 		NextCursor.Selection = v2i(Index, Index);
	// 		E_AppendCursor(Edit, Edit->Arena, Cursor, NextCursor);
	// 		Cursor = Cursor->Next;
	// 	}
	// }
}

void UI_StatusBar(ui_context *UI, draw_context *Draw, edit_context *Edit,
				  m_arena *Arena, d_font Font,
				  cursor *Cursors, editable_buffer *Buffer,
				  v2i Size)
{
	D_DrawQuad(Draw, Arena, v2f(0, 0), (f32)Size.X, 50, v4f(0.3, 0.08, 0, 1));

	D_DrawStr8(Draw, Arena, Font, v2f((f32)Size.X - 200, 10), v4f(0.009, 0.00, 0, 1.0), 50, Str8("LF"));

	cursor *Cursor = Cursors;
	if(!Cursor->Next)
	{
		int Line = E_LineFromBufferIndex(Edit, Cursor->Selection.Y, Buffer);
		int LineStart = E_LineStartFromIndex(Edit, Cursor->Selection.Y, Buffer);
		int Column = Cursor->Selection.Y - LineStart;
		D_DrawStr8(Draw, Arena,
				   Font, v2f((f32)Size.X - 1000, 10),
				   v4f(0.009, 0.00, 0, 1.0),
				   50, Str8PushF(Arena, "?u32:?u32", Line, Column));
	}
	else
	{
		int Count = 0;
		for(;Cursor;Cursor = Cursor->Next)
		{
			Count += 1;
		}
		D_DrawStr8(Draw, Arena, Font, v2f((f32)Size.X - 1000, 10),
				   v4f(0.009, 0.00, 0, 1.0), 35,
				   Str8PushF(Arena, "?u32 SELECTIONS", Count));
	}

	v3f Position = v3f(Size.X - 100, 25, 0.0);
	D_DrawPointsArray(Draw, Arena, &Position, 1, v4f(1, 0.2, 0, 1));
}
