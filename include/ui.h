#include "misc.h"
#include "maths.h"
#include "strings.h"

#include "draw.h"

#define UI_TABLE_SIZE (4096)


typedef struct ui_widget
{
	u64 Hash;
} ui_widget;

typedef struct ui_context
{
	u64 Hot;
	u64 Active;

	f32 HotAnimation;

	d_font Font;

	v2f ScrollOffset;

	b32 MouseIsDragging;
	v2i MousePosition;
	b32 MouseWasPressed;

	u8 MultiClickCount;
	u64 TimeStampWhenMousePressed;

	ui_widget Widgets[UI_TABLE_SIZE];
} ui_context;

b32 UI_TextButton(ui_context *UI, draw_context *Draw,
				  memory_arena *Arena, str8 String,
				  f32 HeightInPixels,
				  v2f Position, v4f Colour);