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
	str8 Name;
	f32 HotAnimation;
	struct ui_widget *NextInHashSlot;
} ui_widget;

typedef enum ui_view
{
	UI_VIEW_TREE,
	UI_VIEW_EDIT,
} ui_view;


typedef struct ui_tree_info
{
	_Alignas(128) v4f *NodeTargetPositions;
	_Alignas(128) v4f *NodePositions;
	fdg_node_info *NodeInfos;
	int NodeCount;

	_Alignas(128) int *Froms;
	_Alignas(128) int *Tos;
	int LinkCount;

	os_directory_tree Tree;
} ui_tree_info;

typedef struct ui_tree
{
	m_arena *Arena;
	f32 Temperature;

	ui_tree_info Info;

	u64 TreeHash;

	v2f Offset;
} ui_tree;

typedef enum ui_mouse_buttons
{
	MOUSE_BUTTON_LEFT,
	MOUSE_BUTTON_RIGHT,
	MOUSE_BUTTON_MIDDLE,
	MOUSE_BUTTON_COUNT,
} ui_mouse_buttons;

typedef enum ui_modifiers
{
	MODIFIER_ALT,
	MODIFIER_CTRL,
	MODIFIER_CMD,
	MODIFIER_OPTION,
} ui_modifiers;


typedef struct ui_event
{
	sapp_event E;
	struct ui_event *Next;
	struct ui_event *Previous;
} ui_event;


typedef struct ui_event_iterator
{
	ui_event *Current;
} ui_event_iterator;

typedef struct ui_style
{
	// TODO(Oliver): Add more to this
	d_font Font;

} ui_style;

typedef struct ui_context
{
	m_arena *Arena;

	u64 Hot;
	u64 Active;
	u64 LastActive;

	f32 HotAnimation;

	d_font Font;

	v2f ScrollOffset;

	b32 MouseClicked;

	b32 MouseIsDragging;
	v2i MousePosition;

	v2f MouseDelta;

	b32 LeftMouseIsDown;
	u8 MultiLeftClickCount;
	u64 TimeStampWhenLeftMousePressed;

	ui_view View;

	ui_event *EventFirst;
	ui_event *EventLast;

	ui_widget WidgetTable[UI_TABLE_SIZE];


	ui_style StyleStack[4096];
	int StyleCount;
} ui_context;

ui_style UI_GetStyle(ui_context *UI);
void UI_PushStyle(ui_context *UI, ui_style Style);
void UI_PopStyle(ui_context *UI);

void UI_Begin(ui_context *UI);

void UI_SetActive(ui_context *UI, u64 Hash);

typedef struct ui_signal ui_signal;


ui_signal UI_TextButton(ui_context *UI, draw_context *Draw,
				  m_arena *Arena, str8 String,
				  f32 HeightInPixels,
				  v2f Position, v4f Colour);

b32 UI_TextButtonWrapping(ui_context *UI, draw_context *Draw, m_arena *Arena,
				 		  str8 String, f32 HeightInPixels, v2f Position, v4f Colour);


int UI_GetTextWidth(d_font Font, draw_context *Draw, str8 String, f32 HeightInPixels);


void UI_Buffer(ui_context *UI, draw_context *Draw, edit_context *Edit,
			   m_arena *Arena, d_font Font,
			   editable_buffer *Buffer,
			   v2i Size);

void UI_TreeView(ui_context *UI, draw_context *Draw, shared_wave_info *Shared,
				 m_arena *Arena, ui_tree *TreeView, str8 Path, v3f Size);

typedef struct ui_tree_view_signal
{
	b32 Clicked;
	str8 NewPath;
	int NewPathIndex;
	str8 FileToLoad;

	int NodeDepth;
	int NodeIndex;
} ui_tree_view_signal;

ui_tree_view_signal UI_TreeViewFolderLabels(ui_context *UI, draw_context *Draw, edit_context *Edit, d_font Font,
							 				m_arena *Arena, int ActiveNodeIndex,
											ui_tree *TreeView, m44f TreeTransform, str8 CurrentPath, editable_buffer* SearchBuffer);


typedef struct ui_orbit
{
	v3f *Positions;
	int Count;
} ui_orbit;

typedef struct ui_orbit_signal
{
	int ActiveItem;
	int ActiveItemDepth;
	str8 Path;
	str8 FileToLoad;
	b32 Clicked;
	v3f ActiveItemPosition;
	os_directory_entry *ActiveEntry;
} ui_orbit_signal;

typedef struct ui_signal
{
	b32 Clicked;
	b32 Hover;
	f32 HotAnimation;
} ui_signal;


ui_orbit_signal UI_OrbitLabels(ui_context *UI, draw_context *Draw, edit_context *Edit,
					d_font Font, d_font LogoFont, m_arena *Arena, ui_orbit Orbit, os_directory_tree Tree,
					v3f CameraPosition, v3f CameraForward, f32 MinDepth, f32 MaxDepth,
					int ActiveItem, int ActiveDepth, os_directory_entry *ActiveEntry,
					m44f MVP, m44f ModelTransform, editable_buffer* SearchBuffer);

ui_orbit UI_OrbitView(ui_context *UI, draw_context *Draw, m_arena *Arena,
					  os_directory_tree Tree, int Active, int Depth,
					  os_directory_entry *ActiveEntry);

u64 UI_IDFromString(str8 String);

void UI_StatusBar(ui_context *UI, draw_context *Draw, edit_context *Edit,
				  m_arena *Arena, d_font Font,
				  cursor *Cursors, editable_buffer *Buffer,
				  v2i Size);

typedef struct ui_textbox_signal
{
	b32 Submitted;
	str8 String;
} ui_textbox_signal;

ui_textbox_signal UI_TextBox(m_arena *Arena,
							 d_font Font,
							 v2f Position,
							 f32 BoxWidth,
							 ui_context *UI,
							 draw_context *Draw,
							 edit_context *Edit,
							 f32 HeightInPixels,
							 str8 String,
							 editable_buffer *Buffer);

b32 UI_LeftMouseWentDown(ui_context *UI);
b32 UI_LeftMouseWentDownWithModifiers(ui_context *UI, int Modifiers);
b32 UI_LeftMouseWentUp(ui_context *UI);

b32 UI_DoubleClicked(ui_context *UI);
b32 UI_TripleClicked(ui_context *UI);
b32 UI_QuadClicked(ui_context *UI);

b32 UI_EventIterate(ui_context *UI, ui_event_iterator *Iter);

void UI_RemoveEventFromList(ui_context *UI, ui_event *Event);

str8 UI_Text(ui_context *UI, m_arena *Arena);
v2f UI_Scroll(ui_context *UI);
b32 UI_KeyUp(ui_context *UI, sapp_keycode Key, u32 Modifiers);
b32 UI_KeyDown(ui_context *UI, sapp_keycode Key, u32 Modifiers);
