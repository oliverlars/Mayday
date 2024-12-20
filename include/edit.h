#include "misc.h"
#include "maths.h"
#include "draw.h"

typedef struct cursor
{
	v2f Position;
	v2f TargetPosition;

	int Start;
	int End;

	int VisualX;

	struct cursor *Next;
	struct cursor *Previous;
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
	logger *Logger;

	editable_buffer Buffer;

	// HACK(Oliver): This is dumb, just add a pool allocator
	u8* Temp;
} edit_context;

void E_InsertTextAtCursor(edit_context *E, cursor *Cursor, editable_buffer *Buffer, str8 TextToInsert);

v2f E_GetPositionFromEditableBuffer(edit_context *E,
									draw_context *D_Context,
									cursor *Cursor,
									editable_buffer Buffer,
									d_font Font,
									v2f Position);

int E_DistanceFromPreviousNewLine(edit_context *E, cursor *Cursor,
								  editable_buffer *Buffer);

int E_DistanceFromNextNewLine(edit_context *E, cursor *Cursor,
							  editable_buffer *Buffer);

void E_RemoveSelectedTextAtCursors(edit_context *E_Context, cursor *Cursor, editable_buffer *Buffer);

int E_LineFromBufferIndex(edit_context *E_Context, cursor *Cursor, editable_buffer *Buffer);

int E_GetMaxTextWidthFromRange(edit_context *E, draw_context *D, d_font Font, cursor *Cursor,
							   editable_buffer *Buffer, int RangeStart, int RangeEnd);

int E_RangeEndFromWidth(edit_context *E, draw_context *D, d_font Font,
						cursor *Cursor, editable_buffer *Buffer, int Width, int RangeStart);

void E_AdvanceSingleCursorXByN(edit_context *E, cursor *Cursor, editable_buffer *Buffer, int N);

void E_AdvanceCursorsXByN(edit_context *E, cursor *Cursor, editable_buffer *Buffer, int N);
void E_AdvanceCursorsYByN(edit_context *E, draw_context *D, d_font Font, cursor *Cursor, editable_buffer *Buffer, int N);

void E_AppendCursor(edit_context *E_Context, cursor *Cursor, cursor NextCursor);