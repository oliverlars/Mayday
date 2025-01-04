#include "misc.h"
#include "maths.h"
#include "strings.h"

#include "draw.h"

#include "edit.h"

#include "os.h"
#include "fdg.h"

#define UI_TABLE_SIZE (4096)


typedef struct ui_widget
{
	u64 Hash;
} ui_widget;

typedef enum ui_view
{
	UI_VIEW_TREE,
	UI_VIEW_EDIT,
} ui_view;


typedef struct ui_tree_info
{
	fdg_node *Nodes;
	int NodeCount;

	fdg_link *Links;
	int LinkCount;

	os_directory_tree Tree;
} ui_tree_info;

typedef struct ui_tree
{
	m_arena *Arena;
	f32 Temperature;

	ui_tree_info Info;

	u64 TreeHash;
} ui_tree;

typedef struct ui_context
{
	u64 Hot;
	u64 Active;

	f32 HotAnimation;

	d_font Font;

	v2f ScrollOffset;

	b32 MouseClicked;

	b32 MouseIsDragging;
	v2i MousePosition;
	b32 MouseWasPressed;
	b32 MouseWentUp;

	u8 MultiClickCount;
	u64 TimeStampWhenMousePressed;

	ui_view View;

	ui_widget Widgets[UI_TABLE_SIZE];
} ui_context;


void UI_Begin(ui_context *UI);


b32 UI_TextButton(ui_context *UI, draw_context *Draw,
				  m_arena *Arena, str8 String,
				  f32 HeightInPixels,
				  v2f Position, v4f Colour);

int UI_GetTextWidth(d_font Font, draw_context *Draw, str8 String, f32 HeightInPixels);


void UI_Buffer(ui_context *UI, draw_context *Draw, edit_context *Edit,
			   m_arena *Arena, d_font Font,
			   editable_buffer *Buffer,
			   cursor *Cursors,
			   v2i Size);

void UI_TreeView(ui_context *UI, draw_context *Draw, shared_wave_info *Shared,
				 m_arena *Arena, ui_tree *TreeView, str8 Path, v2f Size);

void UI_StatusBar(ui_context *UI, draw_context *Draw, edit_context *Edit,
				  m_arena *Arena, d_font Font,
				  cursor *Cursors, editable_buffer *Buffer,
				  v2i Size);