#include "misc.h"
#include "maths.h"
#include "draw.h"

typedef struct cursor
{
	v2f Position;
	v2f TargetPosition;

	v2i Selection;

	int VisualX;

	struct cursor *Next;
	struct cursor *Previous;
} cursor;

typedef enum undo_type
{
	UNDO_NONE,
	UNDO_REMOVED,
	UNDO_INSERTED,
} undo_type;

typedef struct undo_stack
{
	undo_type Type;
	int Index;
	str8 String;
	struct undo_stack *Previous;
	struct undo_stack *Next;
} undo_stack;

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

	draw_context *Draw;

	editable_buffer Buffer;

	undo_stack *NewestAction;
	undo_stack *OldestAction;

	// HACK(Oliver): This is dumb, just add a pool allocator
	u8* Temp;
} edit_context;

typedef enum edit_movement
{
	E_MOVE_START,
	E_MOVE_END,
	E_MOVE_LEFT,
	E_MOVE_RIGHT,
	E_MOVE_UP,
	E_MOVE_DOWN,
	E_MOVE_WORD_LEFT,
	E_MOVE_WORD_RIGHT,

} edit_movement;

typedef enum edit_move_type
{
	E_MOVE_NOT_SELECTING,
	E_MOVE_SELECTING,
} edit_move_type;

v2i E_GetSortedSelection(v2i Selection);

void E_DeleteTo(edit_context *Edit, cursor *Cursor, editable_buffer *Buffer, edit_movement Movement);

void E_RemoveTextFromSelection(edit_context *Edit, editable_buffer *Buffer, v2i Selection);

void E_MoveCursors(edit_context *Edit, draw_context *Draw,
				   d_font Font, cursor *Cursor, editable_buffer *Buffer,
				   edit_movement Movement, edit_move_type MoveType);

void E_DeleteAtLeastOneCharacterInRange(edit_context *Edit,
										edit_movement Movement,
										cursor *Cursors);

void E_InsertTextAtCursor(edit_context *E, cursor *Cursor, editable_buffer *Buffer, str8 TextToInsert);

v2f E_GetPositionFromEditableBuffer(edit_context *E,
									draw_context *D,
									int Index,
									editable_buffer Buffer,
									d_font Font,
									v2f Position);

int E_DistanceFromPreviousNewLine(edit_context *E, int Index, editable_buffer *Buffer);
int E_DistanceFromNextNewLine(edit_context *E, int Index, editable_buffer *Buffer);

void E_RemoveSelectedTextAtCursors(edit_context *E_Context, cursor *Cursor, editable_buffer *Buffer);

int E_LineFromBufferIndex(edit_context *E_Context, int Index, editable_buffer *Buffer);

int E_VisualXFromIndex(edit_context *Edit, draw_context *Draw,
					   d_font Font, int Index, editable_buffer *Buffer);

int E_GetMaxTextWidthFromRange(edit_context *E, draw_context *D, d_font Font,
							   editable_buffer *Buffer, int RangeStart, int RangeEnd);

int E_IndexFromVisualXAndLineStart(edit_context *E, draw_context *D, d_font Font,
								   editable_buffer *Buffer, int VisualX, int LineStart);

void E_AdvanceSingleCursorXByN(edit_context *E, cursor *Cursor, editable_buffer *Buffer, int N);

void E_AdvanceCursorsXByN(edit_context *E, cursor *Cursor, editable_buffer *Buffer, int N);
void E_AdvanceCursorsYByN(edit_context *E, draw_context *D, d_font Font, cursor *Cursor, editable_buffer *Buffer, int N);

void E_AppendCursor(edit_context *E_Context, cursor *Cursor, cursor NextCursor);

void PushUndoAction(edit_context *E, cursor *Cursor, undo_type Type, str8 InsertedOrRemovedText);
undo_stack *PopOffUndoStack(edit_context *E);