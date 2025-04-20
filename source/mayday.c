
#include "draw.h"
#include "log.h"
#include "maths.h"
#include "memory.h"
#include "misc.h"
#include "os.h"
#include "strings.h"

#include "draw.h"
#include "berkeley_mono_font.h"
#include "render.h"
#include "edit.h"

#include "ui.h"

#include "fdg.h"

// TODO(Oliver): Remove platform specific code from here
#include "darwin/menu_bar.h"

#include "wave.h"

#include <assert.h>
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
	// HACK(Oliver): We probably don't want to rely
	// on this
	os_directory_entry *ActiveEntry;


	os_directory_tree FolderTree;
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

void OS_Update(void *Data)
{
	mayday_context *M = Data;

	if(M->UnFocused) return;

	R_PushScissor(M->Render, v4i(0, 0, sapp_width(), sapp_height()));

	UI_Begin(M->UI);

	v2f Scroll = UI_Scroll(M->UI);
	M->ScrollAccumulationTarget += Max(0.0, Scroll.Y)/5.0;

	if(M->ScrollAccumulationTarget >= 5.0)
	{
		M->ScrollAccumulationTarget = 0;

		M->ActiveEntry = Null;

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
	static f32 Angle = 0.0;

	static f32 AngleTarget = 0.0;


	m44f P = Perspective(sapp_widthf()/sapp_heightf(), 35, 0.01, 10000.0);
	v3f CameraPosition = M->LookFrom;
	CameraPosition.Z += M->ZoomLevel;
	v3f CameraForward = Normalize(Sub(M->LookAt, CameraPosition));
	m44f View = M44FLookAt(CameraPosition, M->LookAt, v3f(0, 1, 0));

	//LogInfo("?v3f", CameraForward);

	m44f TranslateToOrigin = Translate(v3f(-M->LookAt.X, -M->LookAt.Y, -M->LookAt.Z));

	m44f Rotation = M44FromYAngle(Angle);

	m44f TranslateBack = Translate(M->LookAt);

	m44f Model = Mul(TranslateToOrigin, Mul(Rotation, TranslateBack));

	m44f ModelView = Mul(Model, View);
	ModelView = Mul(ModelView, Translate(v3f(10, 0, 0)));

	m44f Transform = Mul(ModelView, P);

	R_PushTransform(M->Render, Transform);


	if(UI_KeyDown(M->UI, SAPP_KEYCODE_EQUAL, SAPP_MODIFIER_SUPER))
	{
		M->Buffer.HeightInPixels += 5;
	}
	else if(UI_KeyDown(M->UI, SAPP_KEYCODE_MINUS, SAPP_MODIFIER_SUPER))
	{
		M->Buffer.HeightInPixels -= 5;
	}

	if(M->UI->View == UI_VIEW_EDIT)
	{
		u64 Start = OS_TimeStampNow();
		R_PushTransform(M->Render, Orthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0));
		D_DrawQuad(M->Draw, M->FrameArena, v2f(0, 0),  WindowWidth, WindowHeight, v4f(0.008, 0.00, 0, 1.0));
		D_DrawGrid2D(M->Draw, M->FrameArena, M->Buffer.HeightInPixels, 5*sapp_widthf() / M->Buffer.HeightInPixels);

		UI_Buffer(M->UI, M->Draw, M->Edit,
			   M->FrameArena, M->CodeFont,
			   &M->Buffer,
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
		D_DrawGrid2D(M->Draw, M->FrameArena, M->Buffer.HeightInPixels, M->Buffer.HeightInPixels*5);
		R_PopTransform(M->Render);

		ui_orbit Orbit = {0};
		f32 MinDepth = 0.0f;
		f32 MaxDepth = 0.0f;
		if(OS_ValidPath(M->Folder))
		{
			// HACK(Oliver): This is only here because i'm too lazy to make a new view
			// for the homescreen
			M->ViewSize = Lerp(M->ViewSize, M->ViewSizeTarget, 0.1);
			M->LookAt = LerpV3F(M->LookAt, M->LookAtTarget, 0.1);
			M->LookFrom = LerpV3F(M->LookFrom, M->LookFromTarget, 0.1);


			if(M->OrbitDepth >= 1)
			{
				m44f CurrentTransform = R_CurrentTransform(M->Render);
				v3f AP = M->OrbitPosition;
				m44f T = Translate(v3f(AP.X, AP.Y, AP.Z));
				R_PushTransform(M->Render, Mul(T, CurrentTransform));
				ui_orbit OuterOrbit = UI_OrbitView(M->UI, M->Draw, M->FrameArena, M->FolderTree, M->OrbitItem, M->OrbitDepth, 25);
				R_PopTransform(M->Render);

				Orbit = UI_OrbitView(M->UI, M->Draw, M->FrameArena, M->FolderTree, M->OrbitItem, M->OrbitDepth + 1, 15);
			}
			else
			{
				Orbit = UI_OrbitView(M->UI, M->Draw, M->FrameArena, M->FolderTree, M->OrbitItem, M->OrbitDepth + 1, 15);
			}

			{
				v4f P4D = v4f(Orbit.Positions[0].X, Orbit.Positions[0].Y, Orbit.Positions[0].Z, 1.0);
				P4D = Mul(ModelView, P4D);

				v3f P3D = v3f(P4D.X, P4D.Y, P4D.Z);

				MinDepth = Dot(Sub(P3D, CameraPosition), CameraForward);
				MaxDepth = MinDepth;
			}

			for(int I = 1; I < Orbit.Count; I++)
			{
				v4f P4D = v4f(Orbit.Positions[I].X, Orbit.Positions[I].Y, Orbit.Positions[I].Z, 1.0);
				P4D = Mul(ModelView, P4D);

				v3f P3D = v3f(P4D.X, P4D.Y, P4D.Z);

				f32 Depth = Dot(Sub(P3D, CameraPosition), CameraForward);

				if(Depth < MinDepth)
				{
					MinDepth = Depth;
				}

				if(Depth > MaxDepth)
				{
					MaxDepth = Depth;
				}
			}
		}

		R_PushTransform(M->Render, Orthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0));

		ui_orbit_signal OrbitSignal = {0};
		if(OS_ValidPath(M->Folder))
		{
			m44f PVM = Mul(ModelView, P);
			OrbitSignal = UI_OrbitLabels(M->UI, M->Draw, M->Edit,
						   M->UI->Font, M->LogoFont, M->FrameArena, Orbit, M->FolderTree,
						   CameraPosition, CameraForward, MinDepth, MaxDepth, M->OrbitItem, M->OrbitDepth + 1, M->ActiveEntry,
						   Transform, ModelView, &M->TextBoxBuffer);
		}

		ui_tree_view_signal TreeSignal = {0};
		if(!OS_ValidPath(M->Folder))
		{
			f32 CentreX = sapp_widthf()/2.0 - UI_GetTextWidth(M->LogoFont, M->Draw, Str8("MAYDAY!"), 600)/2.0;
			D_DrawStr8(M->Draw, M->FrameArena, M->LogoFont,
					   v2f(50, 50),
					   v4f(0.1, 0.00, 0.0, 0.1), 600, Str8("MAYDAY!"));
			ui_textbox_signal SearchSignal = UI_TextBox(M->FrameArena, M->StatusFont, v2f(sapp_widthf()/2.0 - 500, sapp_heightf() - 300), 1000,
														M->UI, M->Draw, M->Edit, 60, Str8("FolderTextBox"), &M->TextBoxBuffer);
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
				Pos.X = CentreX;

				str8 Folder = Str8PushF(M->FrameArena, "?S##f32", Node->String, Pos.Y);

				UI_PushStyle(M->UI, (ui_style){.Font = M->LogoFont});
				str8 LastModified = OS_GetLastModifiedTime(M->FrameArena, Node->String);
				UI_Label(M->UI, M->Draw, M->FrameArena, LastModified, 60, Pos, v4f(0.2, 0.01, 0.0, 1));
				ui_signal Button = UI_TextButton(M->UI, M->Draw, M->FrameArena, Folder, 60, Pos, v4f(0.1, 0.01, 0.0, 1));
				UI_PopStyle(M->UI);
				if(Button.Clicked)
				{
					M->Folder = Str8Copy(M->PermanentArena, Node->String);
					M->UI->Active = UI_IDFromString(Str8("TreeViewTextBox"));
				}
				Pos.Y -= 150;
			}
		}

		if(OrbitSignal.AnyHovering)
		{
			AngleTarget -= 0.0001 / (f32)(M->OrbitDepth*2 + 1) + Scroll.X*0.01;
		}
		else
		{
			AngleTarget -= 0.001 / (f32)(M->OrbitDepth*2 + 1) + Scroll.X*0.01;

		}
		Angle = Lerp(Angle, AngleTarget, 0.1);
		M->OrbitAnimation = Lerp(M->OrbitAnimation, M->OrbitAnimationTarget, 0.1);

		if(OrbitSignal.Clicked)
		{
			M->ActiveEntry = OrbitSignal.ActiveEntry;
			M->OrbitItem = OrbitSignal.ActiveItem;
			M->OrbitDepth = OrbitSignal.ActiveItemDepth;
			M->OrbitPosition = OrbitSignal.ActiveItemPosition;
			M->OrbitAnimationTarget += 10.0f;
			//M->LookAtTarget = OrbitSignal.ActiveItemPosition;
			//M->LookFromTarget = OrbitSignal.ActiveItemPosition;
			//M->LookFromTarget.Z = (f32)(1.0 + M->OrbitDepth*35.0);

			if(OrbitSignal.FileToLoad.Size > 0)
			{
				M->Buffer.String = OS_LoadEntireFileIntoMemory(M->PermanentArena, OrbitSignal.FileToLoad);
				M->UI->View = UI_VIEW_EDIT;
			}
			//M->ViewSizeTarget = (f32)(1.0 + M->OrbitDepth*25.0);
		}

		//D_DrawStr8(M->Draw, M->FrameArena, M->LogoFont, v2f(50, 50), v4f(1.0, 0.05, 0.0, 1), 100, Str8("MAYDAY!"));
		R_PopTransform(M->Render);
	}

	R_PopTransform(M->Render);

	if(OS_MilliSecondsFromTimeStamp(OS_TimeStampNow() - M->UI->TimeStampWhenLeftMousePressed) > 150.0 &&
	   M->UI->LeftMouseIsDown)
	{
		M->UI->MouseIsDragging = True;
	}

	if(!M->UI->LeftMouseIsDown)
	{
		M->UI->MouseIsDragging = False;
	}

	R_Update(M->Render, M->FrameArena);

	D_GlyphsToAtlas(M->Draw);

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
		if(Event->type == SAPP_EVENTTYPE_CHAR)
		{
			LogInfo("[EVENT] CHAR: ?char | MODIFIER: ?u32", Event->char_code, Event->modifiers);
		}
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
	M->Buffer.HeightInPixels = 48;
	R_Init(M->Render, M->PermanentArena, v2i(sapp_width() , sapp_height()));

	M->Buffer.String.Bytes = ArenaPushZero(M->PermanentArena, MegaBytes(1));
	M->Buffer.String.Size = sizeof(G_TriangleShader);
	MemoryCopy(M->Buffer.String.Bytes, G_TriangleShader, sizeof(G_TriangleShader));

	M->TextBoxBuffer.String.Bytes = ArenaPushArrayZero(M->PermanentArena, u8, 8192);
	M->TextBoxBuffer.HeightInPixels = 60;

	UI_SetActive(M->UI, UI_IDFromString(Str8("FolderTextBox")));

	str8 File = OS_LoadEntireFileIntoMemory(M->PermanentArena, Str8("/users/olivercruickshank/nomoretext/recent.txt"));

	M->RecentlyLoadedFolders = Str8Split(M->PermanentArena, File, '\n');


	DarwinMenuBar();
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


	OS_WriteStringToDisk(M->PermanentArena, Str8("/users/olivercruickshank/nomoretext/recent.txt"), RecentlyOpened);
}

int main()
{
	m_arena *Arena = ArenaInitialise(GigaBytes(64));
	m_arena *FrameArena = ArenaInitialise(GigaBytes(1));
	m_arena *PreviousFrameArena = ArenaInitialise(GigaBytes(1));

	logger Logger = {0};
	Logger.Arena = ArenaInitialise(GigaBytes(1));
	per_thread_context *ThreadContext = GetThreadContext();
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
	M.Edit->Arena = Arena;
	M.Edit->Temp = ArenaPushZero(Arena, MegaBytes(1));

	cursor Cursor = {0};
	M.Edit->Cursors = &Cursor;

	cursor Cursor2 = {0};
	M.TextBoxBuffer.Cursors = &Cursor2;

	M.Window = OS_MakeWindow(Str8("Mayday!"), 3024, 1964);

	M.CodeFont = D_LoadFontFromTTF(M.Draw, G_BerkeleyMonoRegular);
	M.LogoFont = D_LoadFontFromTTF(M.Draw, G_BasementGrotesque);
	M.StatusFont = D_LoadFontFromTTF(M.Draw, G_Doto);

	M.UI->Font = M.CodeFont;
	M.LookFrom.Z = 120;
	M.LookFromTarget.Z = 70.0;
	M.LookFromTarget.X = -20;

	M.Shared.Arena = ArenaInitialise(GigaBytes(1));

	u64 Start = OS_TimeStampNow();

	DispatchInitialise(&M.Shared, 15);

	M.Folder = Str8("");

	OS_Run(M.Window, &M);
}
