#include "misc.h"
#include "maths.h"
#include "draw.h"

typedef struct cursor
{
	v2f Position;
	v2f TargetPosition;


	/* 
		NOTE(Oliver): We maintain a fixed horizontal position
		that clamps to the length of the line, column tells us
		which line we are on
	*/
	int StickyX;
	int Column;
} cursor;

typedef struct editable_buffer
{
	str8 String;

	f32 HeightInPixels;

	struct editable_buffer *NextInFreeList;
} editable_buffer;

typedef struct edit_context
{
	// NOTE(Oliver): This gets its own arena to just allocate all buffers from
	memory_arena Arena;

	editable_buffer Buffer;

	// HACK(Oliver): This is dumb, just add a pool allocator
	u8* Temp;
} edit_context;

void E_AdvanceCursorXByN(cursor *Cursor, editable_buffer *Buffer, int N);

int E_BufferIndexFromCursor(editable_buffer *Buffer, cursor *Cursor);

void E_InsertTextAtCursor(edit_context *E, cursor *Cursor, editable_buffer *Buffer, str8 TextToInsert);

v2f E_GetPositionFromEditableBuffer(draw_context *D_Context,
									cursor *Cursor,
									editable_buffer Buffer,
									d_font Font,
									v2f Position);

int E_DistanceFromPreviousNewLine(cursor *Cursor,
								  editable_buffer Buffer);

int E_DistanceFromNextNewLine(cursor *Cursor,
							  editable_buffer Buffer);

void E_RemoveTextFromTextRange(edit_context *E_Context, cursor *Cursor, editable_buffer *Buffer,
							   int RangeStart, int RangeEnd);