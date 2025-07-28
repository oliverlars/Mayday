
#include "draw.h"
#include "log.h"
#include "maths.h"
#include "memory.h"
#include "misc.h"
#include "os.h"
#include "strings.h"

#include "draw.h"
#include "fonts.h"
#include "render.h"
#include "edit.h"

#include "ui.h"

#include "fdg.h"

// TODO(Oliver): Remove platform specific code from here
#include "darwin/menu_bar.h"

#include "wave.h"

#include <assert.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include <float.h>

typedef struct mayday_context
{
	m_arena *FrameArena;
	m_arena *PreviousFrameArena;

	m_arena *PermanentArena;


	render_context *Render;
	draw_context   *Draw;
	edit_context   *Edit;
	ui_context     *UI;

	os_window Window;

	d_font CodeFont;
	d_font LogoFont;
	d_font StatusFont;

	d_font RegularFont;
	d_font OutlineFont;
	d_font GradientFont;


	f32 BufferFontHeight;
	f32 BufferFontHeightTarget;
	editable_buffer Buffer;
	cursor Cursor;


	editable_buffer TextBoxBuffer;

	f32 ViewSize;
	f32 ViewSizeTarget;

	v3f LookAtTarget;
	v3f LookAt;

	v3f LookFromTarget;
	v3f LookFrom;

	ui_tree TreeView;

	f32 OrbitAnimation;
	f32 OrbitAnimationTarget;
	int OrbitItem;
	int OrbitDepth;
	v3f OrbitPosition;


	os_directory_tree FolderTree;
	str8 ActiveFilePath;
	str8 Folder;
	u64 FolderHash;

	shared_wave_info Shared;


	f32 ScrollAccumulation;
	f32 ScrollAccumulationTarget;

	f32 ZoomLevel;
	f32 ZoomLevelTarget;


	str8_list RecentlyLoadedFolders;


	b32 UnFocused;
}
mayday_context;

#include "shaders.h"

#define SOKOL_LOG_IMPL
#include "sokol_log.h"

void OS_Update_(void *Data)
{
	mayday_context *M = Data;

	M->Draw->ViewportSize = v2f(sapp_widthf(), sapp_heightf());
	UI_Begin(M->UI);
	R_PushScissor(M->Render, v4i(0, 0, sapp_width(), sapp_height()));
	R_PushTransform(M->Render, Orthographic(0.0, sapp_widthf()/2.0, sapp_heightf()/2.0, 0.0, -1000.0, 1000.0));

	//D_DrawStr8(M->Draw, M->FrameArena, M->CodeFont, v2f(200, 500), v4f(0, 1, 0, 1), 100, Str8("This text is upside down"));
	v3f Line[2] = 
	{
		v3f(0, 0, 0),
		v3f(2, 2, 0)
	};

	//D_DrawLinesArray(M->Draw, M->FrameArena, Line, 2, v4f(0, 1, 0, 1));	
	m44f P = Perspective(sapp_widthf()/sapp_heightf(), 35, 0.01, 10000.0);
	m44f PV = Mul( M44FLookAt(v3f(0, 0, -10), v3f(0, 0, 0), v3f(0, 1, 0)), P);
	R_PushTransform(M->Render, PV);
	D_DrawLinesArray(M->Draw, M->FrameArena, Line, 2, 5.0, v4f(1, 1, 1, 1));
	//D_DrawPointsArray(M->Draw, M->FrameArena, Line, 2, 5, v4f(1, 1, 1, 1));
	R_PopTransform(M->Render);

	//D_DrawQuad(M->Draw, M->FrameArena, v2f(200, 200), 200, 200, v4f(1, 1, 1, 1));
	//D_DrawQuad(M->Draw, M->FrameArena, v2f(600, 600), 100, 200, v4f(1, 0, 1, 1));
	D_DrawStr8(M->Draw, M->FrameArena, M->CodeFont, v2f(300, 200), v4f(0, 1, 0, 1), 100, Str8("This text is upside "));
	R_PopTransform(M->Render);
	R_PopScissor(M->Render);

	R_Update(M->Render, M->FrameArena);
	ArenaReset(M->FrameArena);
	m_arena *TempArena = M->FrameArena;
	M->FrameArena = M->PreviousFrameArena;
	M->PreviousFrameArena = TempArena;

	M->UI->EventLast = Null;
	M->UI->EventFirst = Null;
}

void OS_Update(void *Data)
{
	mayday_context *M = Data;

	if(M->UnFocused) return;

	R_PushScissor(M->Render, v4i(0, 0, sapp_width(), sapp_height()));
	M->Draw->ViewportSize = v2f(sapp_widthf(), sapp_heightf());

	UI_Begin(M->UI);

	v2f Scroll = UI_Scroll(M->UI);
	M->ScrollAccumulationTarget += Max(0.0, Scroll.Y)/5.0;

	if(M->ScrollAccumulationTarget >= 5.0)
	{
		M->ScrollAccumulationTarget = 0;

		M->OrbitAnimationTarget -= 10.0;
		M->OrbitItem = 0;
		M->OrbitDepth = 0;
	}

	M->ScrollAccumulationTarget = Lerp(M->ScrollAccumulationTarget, 0.0, 0.3);

	M->ScrollAccumulation = Lerp(M->ScrollAccumulation, M->ScrollAccumulationTarget, 0.1);

	M->ZoomLevel = Lerp(M->ZoomLevel, M->ZoomLevelTarget, 0.1);

	M->ZoomLevel += M->ScrollAccumulation;

	int WindowHeight = OS_WindowHeight(&M->Window);
	int WindowWidth = OS_WindowWidth(&M->Window);

	f32 Size = M->ViewSize;
	static f32 AngleOffset = 0.0;
	static f32 Angle = 0.0;

	static f32 AngleTarget = 0.0;


	m44f P = Perspective(sapp_widthf()/sapp_heightf(), 35, 0.01, 10000.0);
	v3f CameraPosition = M->LookFrom;
	CameraPosition.Z += M->ZoomLevel;
	v3f CameraForward = Normalize(Sub(M->LookAt, CameraPosition));
	m44f View = M44FLookAt(CameraPosition, M->LookAt, v3f(0, 1, 0));

	//LogInfo("?v3f", CameraForward);

	m44f TranslateToOrigin = Translate(v3f(-M->LookAt.X, -M->LookAt.Y, -M->LookAt.Z));

	m44f Rotation = M44FromYAngle(Angle + 2.0f*M_PI*AngleOffset);

	m44f TranslateBack = Translate(M->LookAt);

	m44f Model = Mul(TranslateToOrigin, Mul(Rotation, TranslateBack));

	m44f ModelView = Mul(Model, View);
	ModelView = Mul(ModelView, Translate(v3f(10, 0, 0)));

	m44f Transform = Mul(ModelView, P);

	R_PushTransform(M->Render, Transform);
	
	D_DrawQuad(M->Draw, M->FrameArena, v2f(0, 0), 100, 200, v4f(0, 1, 0, 1));


	if(UI_KeyDown(M->UI, SAPP_KEYCODE_EQUAL, SAPP_MODIFIER_SUPER))
	{
		M->BufferFontHeightTarget += 5;
	}
	else if(UI_KeyDown(M->UI, SAPP_KEYCODE_MINUS, SAPP_MODIFIER_SUPER))
	{
		M->BufferFontHeightTarget -= 5;
	}

	M->BufferFontHeight = Lerp(M->BufferFontHeight, M->BufferFontHeightTarget, 0.1);
	M->Buffer.HeightInPixels = M->BufferFontHeight;

	if(M->UI->View == UI_VIEW_EDIT)
	{
		u64 Start = OS_TimeStampNow();
		R_PushTransform(M->Render, Orthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0));
		D_DrawQuad(M->Draw, M->FrameArena, v2f(0, 0),  WindowWidth, WindowHeight, v4f(0.008, 0.00, 0, 1.0));
		D_DrawGrid2D(M->Draw, M->FrameArena, M->Buffer.HeightInPixels, 5*sapp_widthf() / M->Buffer.HeightInPixels);
	
		UI_Buffer(M->UI, M->Draw, M->Edit,
			   M->FrameArena, M->CodeFont,
			   M->ActiveFilePath, &M->Buffer,
			   v2i(WindowWidth, WindowHeight));

		UI_StatusBar(M->UI, M->Draw, M->Edit, M->FrameArena, M->StatusFont,
					 M->Edit->Cursors, &M->Buffer,
					 v2i(WindowWidth, WindowHeight));

		u64 End = OS_TimeStampNow();
		//LogInfo("Time: ?f32", OS_MilliSecondsFromTimeStamp(End - Start));
		R_PopTransform(M->Render);
	}
	else if(M->UI->View == UI_VIEW_TREE)
	{
		u64 FolderHash = M_FNVHashString(M_FNV_HASH_START, M->Folder);
		if(FolderHash != M->FolderHash && M->Folder.Size > 0)
		{
			LogInfo("?S", M->Folder);
			M->FolderTree = OS_DirectoryTreeFromPath(M->PermanentArena, M->Folder);
			M->FolderHash = FolderHash;
		}

		v2f Centre = {0};
		R_PushTransform(M->Render, Orthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0));
		D_DrawQuad(M->Draw, M->FrameArena, v2f(0, 0),  WindowWidth, WindowHeight, v4f(0.008, 0.00, 0, 1.0));
		//D_DrawGrid2D(M->Draw, M->FrameArena, M->Buffer.HeightInPixels, M->Buffer.HeightInPixels*5);
		

		ui_orbit Orbit = {0};
		f32 MinDepth = 0.0f;
		f32 MaxDepth = 0.0f;
		if(OS_ValidPath(M->Folder))
		{
			// HACK(Oliver): This is annoying i have to do this twice
			R_PopTransform(M->Render);

			
			UI_TreeView(M->UI, M->Draw, &M->Shared, M->FrameArena, &M->TreeView, M->Folder, v3f(0.1, 0.1, 0.1));
		}
		else
		{
			R_PopTransform(M->Render);
		}

		R_PushTransform(M->Render, Orthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0));

		ui_tree_view_signal TreeSignal = {0};
		if(!OS_ValidPath(M->Folder))
		{
			f32 CentreX = sapp_widthf()/2.0 - UI_GetTextWidth(M->CodeFont, M->Draw, Str8("MAYDAY!"), 600)/2.0;
			f32 LogoHeight = 100.0;
			int Count = 0;
			int Middle = sapp_heightf()/(LogoHeight*0.8)/2;
			for(f32 Y = sapp_heightf(); Y >= 0; Y -= LogoHeight*0.8)
			{
				if(Count == Middle)
				{
					D_DrawStr8(M->Draw, M->FrameArena, M->RegularFont,
					   v2f(sapp_widthf()/2.0 + 200.0, Y),
					   v4f(1.0, 0.0, 0.0, 0.1), LogoHeight, Str8("MAYDAY!"));
				}
				else
				{
					D_DrawStr8(M->Draw, M->FrameArena, M->OutlineFont,
					   v2f(sapp_widthf()/2.0 + 200.0, Y),
					   v4f(1.0, 0.0, 0.0, 0.1), LogoHeight, Str8("MAYDAY!"));
				}
				Count += 1;
			}

			D_DrawQuad(M->Draw, M->FrameArena, v2f(100.0, sapp_heightf()-300), 500, 200, v4f(0.2, 0.0, 0.0, 1.0));
			D_DrawStr8(M->Draw, M->FrameArena, M->OutlineFont, v2f(100.0, sapp_heightf()-300), v4f(0.3, 0.3, 0.3, 1.0), 150, Str8("RECENT"));

			ui_textbox_signal SearchSignal = UI_TextBox(M->FrameArena, M->LogoFont, v2f(100.0, sapp_heightf() - 400), 500,
														M->UI, M->Draw, M->Edit, 30, Str8("FolderTextBox"), &M->TextBoxBuffer);
			if(SearchSignal.Submitted)
			{
				// HACK(Oliver): This is leaking memory
				M->Folder = Str8Copy(M->PermanentArena, SearchSignal.String);
				M->UI->Active = UI_IDFromString(Str8("FolderTextBox"));
				Str8ListAppend(M->PermanentArena, &M->RecentlyLoadedFolders, SearchSignal.String);
			}

			v2f Pos = v2f(0, sapp_heightf() - 500);
			for(str8_list_node *Node = M->RecentlyLoadedFolders.Last; Node; Node = Node->Previous)
			{
				CentreX = sapp_widthf()/2.0 - UI_GetTextWidth(M->UI->Font, M->Draw, Node->String, 60)/2.0;
				Pos.X = 100.0;

				str8 Folder = Str8PushF(M->FrameArena, "?S##f32", Node->String, Pos.Y);

				UI_PushStyle(M->UI, (ui_style){.Font = M->CodeFont });
				str8 LastModified = OS_GetLastModifiedTime(M->FrameArena, Node->String);
				UI_Label(M->UI, M->Draw, M->FrameArena, LastModified, 60, Add(Pos, v2f(0, 20)), v4f(0.2, 0.01, 0.0, 0.1));
				ui_signal Button = UI_TextButton(M->UI, M->Draw, M->FrameArena, Folder, 30, Pos, v4f(0.3, 0.3, 0.3, 1));
				UI_PopStyle(M->UI);
				if(Button.Clicked)
				{
					M->Folder = Str8Copy(M->PermanentArena, Node->String);
					M->UI->Active = UI_IDFromString(Str8("TreeViewTextBox"));
				}
				Pos.Y -= 100;
			}
		}

		//D_DrawStr8(M->Draw, M->FrameArena, M->LogoFont, v2f(50, 50), v4f(1.0, 0.05, 0.0, 1), 100, Str8("MAYDAY!"));
		R_PopTransform(M->Render);
	}

	R_PopTransform(M->Render);

	if(OS_MilliSecondsFromTimeStamp(OS_TimeStampNow() - M->UI->TimeStampWhenLeftMousePressed) > 150.0 &&
	   M->UI->LeftMouseIsDown && abs(M->UI->MousePosition.X - M->UI->MouseDragStart.X) > 2
	   && abs(M->UI->MousePosition.Y - M->UI->MouseDragStart.Y) > 2)
	{
		M->UI->MouseIsDragging = True;
	}

	if(!M->UI->LeftMouseIsDown)
	{
		M->UI->MouseIsDragging = False;
	}

	R_Update(M->Render, M->FrameArena);

	ArenaReset(M->FrameArena);
	m_arena *TempArena = M->FrameArena;
	M->FrameArena = M->PreviousFrameArena;
	M->PreviousFrameArena = TempArena;

	M->UI->EventLast = Null;
	M->UI->EventFirst = Null;

	f32 MillisecondsSinceLastClick = OS_MilliSecondsFromTimeStamp(OS_TimeStampNow() - M->UI->TimeStampWhenLeftMousePressed);
	if(MillisecondsSinceLastClick >= 400)
	{
		M->UI->MultiLeftClickCount = 0;
	}

}

void OS_Event(const sapp_event *Event, void *Data)
{
	mayday_context *M = (mayday_context*)Data;

	{
		ui_event *UIEvent = ArenaPushArrayZero(M->FrameArena, ui_event, 1);
		UIEvent->E = Event[0];
		if(M->UI->EventLast)
		{
			M->UI->EventLast->Next = UIEvent;
			UIEvent->Previous = M->UI->EventLast;
			M->UI->EventLast = UIEvent;
		}
		else
		{
			M->UI->EventLast = UIEvent;
			M->UI->EventFirst = UIEvent;
		}
	}

	if(Event->type == SAPP_EVENTTYPE_MOUSE_SCROLL)
	{
		M->UI->ScrollOffset.Y -= Event->scroll_y;
		M->UI->ScrollOffset.X += Event->scroll_x;
	}

	if(Event->type == SAPP_EVENTTYPE_FOCUSED)
	{
		M->UnFocused = False;
	}

	if(Event->type == SAPP_EVENTTYPE_UNFOCUSED)
	{
		M->UnFocused = True;
	}

	if(Event->type == SAPP_EVENTTYPE_MOUSE_MOVE)
	{
		M->UI->MousePosition.X = Event->mouse_x;
		M->UI->MousePosition.Y = OS_WindowHeight(&M->Window) - Event->mouse_y;

		M->UI->MouseDelta.X = Event->mouse_dx;
		M->UI->MouseDelta.Y = Event->mouse_dy;
	}

	if(Event->type == SAPP_EVENTTYPE_MOUSE_DOWN)
	{
		if(Event->mouse_button == SAPP_MOUSEBUTTON_LEFT)
		{
			M->UI->MouseDragStart = v2i(Event->mouse_x, Event->mouse_y);
			M->UI->LeftMouseIsDown = True;

			f32 MillisecondsSinceLastClick = OS_MilliSecondsFromTimeStamp(OS_TimeStampNow() - M->UI->TimeStampWhenLeftMousePressed);

			if(MillisecondsSinceLastClick < 400 &&
			   M->UI->MultiLeftClickCount == 0)
			{
				M->UI->MultiLeftClickCount += 1;
			}
			else if(MillisecondsSinceLastClick < 400 && M->UI->MultiLeftClickCount == 1)
			{
				M->UI->MultiLeftClickCount += 1;
			}
			else if(MillisecondsSinceLastClick < 400 && M->UI->MultiLeftClickCount == 2)
			{
				M->UI->MultiLeftClickCount += 1;
			}

			M->UI->TimeStampWhenLeftMousePressed = OS_TimeStampNow();
		}

	}

	if(Event->type == SAPP_EVENTTYPE_MOUSE_UP)
	{
		if(Event->mouse_button == SAPP_MOUSEBUTTON_LEFT)
		{
			M->UI->LeftMouseIsDown = False;
		}
	}

}

void OS_Initialise(void *Data)
{
	mayday_context *M = Data;
	M->Buffer.HeightInPixels = 35;
	M->BufferFontHeight = 35;
	M->BufferFontHeightTarget = 35;
	R_Init(M->Render, M->PermanentArena, v2i(sapp_width() , sapp_height()));

	M->Buffer.String.Bytes = ArenaPushZero(M->PermanentArena, MegaBytes(1));
	M->Buffer.String.Size = sizeof(G_TriangleShader);
	MemoryCopy(M->Buffer.String.Bytes, G_TriangleShader.Data, G_TriangleShader.Length);

	M->TextBoxBuffer.String.Bytes = ArenaPushArrayZero(M->PermanentArena, u8, 8192);
	M->TextBoxBuffer.HeightInPixels = 60;

	UI_SetActive(M->UI, UI_IDFromString(Str8("FolderTextBox")));

#if defined(OS_DARWIN)
	str8 File = OS_LoadEntireFileIntoMemory(M->PermanentArena, Str8("/users/olivercruickshank/nomoretext/recent.txt"));

	M->RecentlyLoadedFolders = Str8Split(M->PermanentArena, File, '\n');


	DarwinMenuBar();
#elif defined(OS_WINDOWS)
	str8 File = OS_LoadEntireFileIntoMemory(M->PermanentArena, Str8("/users/olivercruickshank/nomoretext/recent.txt"));

	M->RecentlyLoadedFolders = Str8Split(M->PermanentArena, File, '\n');
#endif
}

void OS_Exit(void *Data)
{
	mayday_context *M = Data;


	int Count = Str8ListCount(M->RecentlyLoadedFolders);

	u64 *Hashes = ArenaPushArrayZero(M->PermanentArena, u64, Count);
	int HashesSoFar = 0;

	str8_list FinalList = {0};
	for(str8_list_node *Node = M->RecentlyLoadedFolders.First; Node; Node = Node->Next)
	{
		int AlreadyExists = False;
		u64 Hash = M_FNVHashString(M_FNV_HASH_START, Node->String);
		for(int I = 0; I < HashesSoFar; I++)
		{
			if(Hashes[I] == Hash)
			{
				AlreadyExists = True;
			}
		}
		if(!AlreadyExists)
		{
			Str8ListAppend(M->PermanentArena, &FinalList, Node->String);
			Hashes[HashesSoFar++] = Hash;
		}
	}

	str8 RecentlyOpened = Str8ListJoinBy(M->PermanentArena, &FinalList, '\n');

#if defined(OS_DARWIN)
	OS_WriteStringToDisk(M->PermanentArena, Str8("/users/olivercruickshank/nomoretext/recent.txt"), RecentlyOpened);
#elif defined(OS_WINDOWS)
	// TODO(Oliver): Put this in Local folder
	OS_WriteStringToDisk(M->PermanentArena, Str8("recent.txt"), RecentlyOpened);
#endif
}

int main()
{
	m_arena *Arena = ArenaInitialise(GigaBytes(64));
	m_arena *FrameArena = ArenaInitialise(GigaBytes(1));
	m_arena *PreviousFrameArena = ArenaInitialise(GigaBytes(1));

	logger Logger = {0};
	Logger.Arena = ArenaInitialise(GigaBytes(1));
	per_thread_context *ThreadContext = GetThreadLocalContext();
	ThreadContext->Logger = &Logger;
	ThreadContext->ScratchArenas[0] = ArenaInitialise(GigaBytes(1));
	ThreadContext->ScratchArenas[1] = ArenaInitialise(GigaBytes(1));

	mayday_context M = {0};

	M.PermanentArena = Arena;
	M.FrameArena = FrameArena;
	M.PreviousFrameArena = PreviousFrameArena;

	M.Draw = ArenaPushArrayZero(Arena, draw_context, 1);
	M.Render = ArenaPushArrayZero(Arena, render_context, 1);
	M.Edit = ArenaPushArrayZero(Arena, edit_context, 1);
	M.UI = ArenaPushArrayZero(Arena, ui_context, 1);
	M.UI->Arena = Arena;
	M.Draw->Render = M.Render;
	M.Draw->Arena = Arena;
	M.Render->Arena = Arena;
	M.Edit->Arena = Arena;
	M.Edit->Temp = ArenaPushZero(Arena, MegaBytes(1));

	cursor Cursor = {0};
	M.Edit->Cursors = &Cursor;

	M.Window = OS_MakeWindow(Str8("Mayday!"), 3024, 1964);

	M.CodeFont = D_LoadFontFromTTF(M.Draw, G_BerkeleyMonoRegular.Data);
	M.RegularFont = D_LoadFontFromTTF(M.Draw, G_BasementGrotesque.Data);
	M.OutlineFont = D_LoadFontFromTTF(M.Draw, G_BerkeleyMonoRegular.Data);
	M.LogoFont = D_LoadFontFromTTF(M.Draw, G_BerkeleyMonoRegular.Data);
	M.GradientFont = D_LoadFontFromTTF(M.Draw,G_BerkeleyMonoRegular.Data);
	M.StatusFont = D_LoadFontFromTTF(M.Draw, G_Doto.Data);

	M.UI->Font = M.CodeFont;
	M.LookFrom.Z = 500;
	M.LookFromTarget.Z = 70.0;
	M.LookFromTarget.X = -20;

	M.Shared.Arena = ArenaInitialise(GigaBytes(1));

	u64 Start = OS_TimeStampNow();

	DispatchInitialise(&M.Shared, 15);

	//M.Folder = Str8("C:\\mayday\\");

	sapp_desc *SAPPDesc = ArenaPushArrayZero(Arena, sapp_desc, 1);
	SAPPDesc->window_title = "assdfasdfsd";
	SAPPDesc->init_userdata_cb = OS_Initialise;
	SAPPDesc->frame_userdata_cb = OS_Update;
	SAPPDesc->event_userdata_cb = OS_Event;
	SAPPDesc->width = M.Window.Width;
	SAPPDesc->height = M.Window.Height;
	SAPPDesc->user_data = &M;
#if defined(OS_WINDOWS)
	SAPPDesc->high_dpi = true;
#elif defined(OS_DARWIN)
	SAPPDesc->high_dpi = true;
#endif
	SAPPDesc->logger.func = slog_func;
	SAPPDesc->cleanup_userdata_cb = OS_Exit;
	sapp_run(SAPPDesc);
}
