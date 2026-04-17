
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

#define DEBUG_TIMING_HISTORY 120

typedef struct debug_timings
{
	f32 FrameMs;
	f32 FrameMsMax;
	f32 FrameMsHistory[DEBUG_TIMING_HISTORY];
	int FrameMsHistoryIndex;

	f32 CPUMs;
	f32 CPUMsMax;

	f32 FPS;
	f32 FPSMin;
	f32 FPSMax;
	f32 FPSAvg;
	f32 FPSAccumulator;
	int FPSFrameCount;

	f32 FDGBuildMs;
	f32 FDGBuildMsMax;

	f32 FDGAttributesMs;
	f32 FDGAttributesMsMax;

	f32 FDGSimulateMs;
	f32 FDGSimulateMsMax;

	f32 FDGTotalMs;
	f32 FDGTotalMsMax;

	f32 EditViewMs;
	f32 EditViewMsMax;

	f32 TreeViewMs;
	f32 TreeViewMsMax;

	f32 RenderMs;
	f32 RenderMsMax;

	u64 LastFrameTimeStamp;
	u64 CPUStart;
} debug_timings;

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

	d_font Rajdhani;
	d_font Orbitron;
	d_font CodeFont;
	d_font LogoFont;
	d_font StatusFont;

	d_font BebasNeue;

	d_font RegularFont;
	d_font OutlineFont;
	d_font GradientFont;


	f32 BufferFontHeight;
	f32 BufferFontHeightTarget;
	editable_buffer Buffer;
	cursor Cursor;


	str8 FileListPath;
	b32 ShowFileList;
	f32 FileListScroll;
	f32 FileListScrollTarget;
	v2f FileListPosition;


	editable_buffer TextBoxBuffer;

	f32 ViewSize;
	f32 ViewSizeTarget;

	v3f LookAtTarget;
	v3f LookAt;

	v3f LookFromTarget;
	v3f LookFrom;

	ui_tree TreeView;
	ui_tree HomescreenTree;

	v3f TreeCameraLookAt;
	v3f TreeCameraLookAtTarget;
	f32 TreeCameraZoom;
	f32 TreeCameraZoomTarget;
	str8 PendingFolder;
	b32 TreeTransitioning;
	b32 PendingShowFileList;
	v2f PendingFileListPosition;

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
	b32 ShowDebugOverlay;

	// Split-flap carousel rotation (homescreen -> FDG, one-way)
	f32 CameraAngle;
	f32 CameraVelocity;
	f32 CameraTarget;
	b32 HomescreenLocked;

	debug_timings Debug;
}
mayday_context;

#include "shaders.h"

#define SOKOL_LOG_IMPL
#include "sokol_log.h"


#define COLOUR_BACKGROUND v4f(0.753, 0.036, 0.0, 1.0)
#define COLOUR_BLACK v4f(0.0, 0.0, 0.0, 1.0)

#ifndef BUILD_NUMBER
#define BUILD_NUMBER 0
#endif

str8 IntToRomanNumerals(m_arena *Arena, int Value)
{
	struct { int Val; char *Str; int Len; } Table[] = {
		{1000, "M", 1}, {900, "CM", 2}, {500, "D", 1}, {400, "CD", 2},
		{100, "C", 1}, {90, "XC", 2}, {50, "L", 1}, {40, "XL", 2},
		{10, "X", 1}, {9, "IX", 2}, {5, "V", 1}, {4, "IV", 2}, {1, "I", 1}
	};
	char Buf[128] = {0};
	int At = 0;
	for(int I = 0; I < 13 && Value > 0; I++)
	{
		while(Value >= Table[I].Val)
		{
			for(int J = 0; J < Table[I].Len; J++) Buf[At++] = Table[I].Str[J];
			Value -= Table[I].Val;
		}
	}
	u8 *Bytes = ArenaPush(Arena, At);
	MemoryCopy(Bytes, Buf, At);
	return (str8){At, Bytes};
}

#define RenderTest 0

#if RenderTest
void OS_Update(void *Data)
#else
void OS_Update_(void *Data)
#endif
{
	mayday_context *M = Data;

	
	M->Draw->ViewportSize = v2f(sapp_widthf(), sapp_heightf());
	UI_Begin(M->UI);
	R_PushScissor(M->Render, v4i(0, 0, sapp_width(), sapp_height()));
	R_PushTransform(M->Render, Orthographic(0.0, sapp_widthf()/2.0, sapp_heightf()/2.0, 0.0, -1000.0, 1000.0));

	//D_Str8(M->Draw, M->FrameArena, M->CodeFont, v2f(200, 500), v4f(0, 1, 0, 1), 100, Str8("This text is upside down"));
	v3f Line[4] = 
	{
		v3f(500, 700, 0),
		v3f(525, 725, 0),
		v3f(325, 1025, 0),
		v3f(500, 400,  0),
	};

	//D_LinesArray(M->Draw, M->FrameArena, Line, 2, v4f(0, 1, 0, 1));	
	m44f P = Perspective(sapp_widthf()/sapp_heightf(), 35, 0.01, 10000.0);
	m44f PV = Mul( M44FLookAt(v3f(0, 0, -10), v3f(0, 0, 0), v3f(0, 1, 0)), P);
	R_PushTransform(M->Render, PV);
	//D_LinesArray(M->Draw, M->FrameArena, Line, 2, 5.0, v4f(1, 1, 1, 1));
	//D_PointsArray(M->Draw, M->FrameArena, Line, 1, 50, v4f(1, 1, 1, 1));
	R_PopTransform(M->Render);
	f32 W = 800;
	f32 H = D_Str8HeightFromWidth(M->Rajdhani, Str8("MAYDAY!"), W);
	v4f BBox = D_Str8BBoxFromHeight(M->Rajdhani, v2f(50, 50), Str8("MAYDAY!"), H);
	D_Quad(M->Draw, M->FrameArena, v2f(50, 50), BBox.Z - BBox.X, BBox.W - BBox.Y, v4f(1, 0, 1, 1));
	D_Str8(
		M->Draw, M->FrameArena, 
		Str8("MAYDAY!"), .Font = M->Rajdhani,
		.Position = v3f(50, 50, 0),
		.Colour = v4f(0, 1, 0, 1),
		.OutlineThickness = 5.0,
		.HeightInPixels = H
	);
	//D_Quad(M->Draw, M->FrameArena, v2f(600, 600), 100, 200, v4f(1, 0, 1, 1));

	//D_OutlineStr8(M->Draw, M->FrameArena, M->Rajdhani, v2f(500, 600), v4f(0.2, 0.1, 0.0, 1.0), 200, 10.0, Str8("RUNGALO BUNGALO!"));

	//D_Str8(M->Draw, M->FrameArena,  Str8("ABC DEF"), .Font = M->CodeFont, .Position = v3f(300, 200, 0), .Colour = v4f(0, 1, 0, 1), .HeightInPixels = 100);
	D_LinesArray(M->Draw, M->FrameArena, Line, 2, 5.0, v4f(1, 1, 1, 1));
	D_PointsArray(M->Draw, M->FrameArena, Line, 1, 50, v4f(1, 1, 1, 0.2));
	D_LinesArray(M->Draw, M->FrameArena, Line + 2, 2, 10.0, v4f(1, 1, 1, 1));
	v3f Points[] =
	{
		v3f(100, 100, 0),
		v3f(200, 200, 0)
	};
	//D_PointsArray(M->Draw, M->FrameArena, Points, 1, 200.0, v4f(0, 1, 1, 1));
	//D_OutlineStr8(M->Draw, M->FrameArena, M->Rajdhani, v2f(100, 0), v4f(0.2, 0.1, 0.0, 1.0), 500, 5.0, Str8("MAYDAY!2"));
	//D_PointsArray(M->Draw, M->FrameArena, Points + 1, 1, 70.0, v4f(1, 0, 1, 1));
	
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

#if RenderTest
void OS_Update_(void *Data)
#else
void OS_Update(void *Data)
#endif
{
	mayday_context *M = Data;

	u64 FrameStart = OS_TimeStampNow();
	if(M->Debug.LastFrameTimeStamp)
	{
		M->Debug.FrameMs = OS_MilliSecondsFromTimeStamp(FrameStart - M->Debug.LastFrameTimeStamp);
		if(M->Debug.FrameMs > M->Debug.FrameMsMax) M->Debug.FrameMsMax = M->Debug.FrameMs;

		M->Debug.FrameMsHistory[M->Debug.FrameMsHistoryIndex % DEBUG_TIMING_HISTORY] = M->Debug.FrameMs;
		M->Debug.FrameMsHistoryIndex++;

		M->Debug.FPS = (M->Debug.FrameMs > 0.001f) ? 1000.0f / M->Debug.FrameMs : 0;
		if(M->Debug.FPSMin == 0 || M->Debug.FPS < M->Debug.FPSMin) M->Debug.FPSMin = M->Debug.FPS;
		if(M->Debug.FPS > M->Debug.FPSMax) M->Debug.FPSMax = M->Debug.FPS;

		M->Debug.FPSAccumulator += M->Debug.FPS;
		M->Debug.FPSFrameCount++;
		M->Debug.FPSAvg = M->Debug.FPSAccumulator / (f32)M->Debug.FPSFrameCount;
	}
	M->Debug.LastFrameTimeStamp = FrameStart;
	M->Debug.CPUStart = FrameStart;

	//if(M->UnFocused) return;

	R_PushScissor(M->Render, v4i(0, 0, sapp_width(), sapp_height()));
	M->Draw->ViewportSize = v2f(sapp_widthf(), sapp_heightf());

	UI_Begin(M->UI);

	if(M->UI->View != UI_VIEW_TREE)
	{
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
	}

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
	
	D_Quad(M->Draw, M->FrameArena, v2f(0, 0), 100, 200, v4f(0, 1, 0, 1));


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
		D_Quad(M->Draw, M->FrameArena, v2f(0, 0),  WindowWidth, WindowHeight, COLOUR_BACKGROUND);
		//D_DiscoGrid2D(M->Draw, M->FrameArena, v2f(sapp_widthf(), sapp_heightf()), v2i(1, sapp_height()/M->Buffer.HeightInPixels));
	
		UI_Buffer(M->UI, M->Draw, M->Edit,
			   M->FrameArena, M->CodeFont,
			   M->Rajdhani,
			   M->ActiveFilePath, &M->Buffer,
			   v2i(WindowWidth, WindowHeight));

		UI_StatusBar(M->UI, M->Draw, M->Edit, M->FrameArena, M->Rajdhani,
					 M->Edit->Cursors, &M->Buffer,
					 v2i(WindowWidth, WindowHeight));

		u64 End = OS_TimeStampNow();
		M->Debug.EditViewMs = OS_MilliSecondsFromTimeStamp(End - Start);
		if(M->Debug.EditViewMs > M->Debug.EditViewMsMax) M->Debug.EditViewMsMax = M->Debug.EditViewMs;
		R_PopTransform(M->Render);
	}
	else if(M->UI->View == UI_VIEW_TREE)
	{
		u64 FolderHash = M_FNVHashString(M_FNV_HASH_START, M->Folder);
		if(FolderHash != M->FolderHash && M->Folder.Size > 0)
		{
			LogInfo("?S", M->Folder);
			M->FolderTree = OS_DirectoryTreeFromPath(M->PermanentArena, M->Folder, (os_directory_tree_options){.FoldersOnly = True});
			M->FolderHash = FolderHash;
		}

		// === SPLIT-FLAP CAROUSEL: one-way homescreen -> FDG ===
		// Model: a single camera at origin looking down -Z. Homescreen panel
		// lives at world z = -z0, FDG panel lives at world z = +z1. The camera
		// Y-rotates; when |angle| > threshold we snap + lock to face the FDG.
		//
		// Because we draw pixel-space text with Orthographic() and the FDG's
		// own PV already flattens to clip space, we can't apply a full 3D model
		// rotation directly. Instead, for each panel we apply a POST-projection
		// homography that matches what a camera looking at a flat card at
		// distance D (in NDC units) would see if that card was rotated around
		// its vertical axis. This gives real perspective foreshortening —
		// the near edge stretches, the far edge bunches up toward the vanishing
		// point, rather than just squishing uniformly.
		{
			f32 dt = 1.0f / 60.0f;

			// Before lock, consume scroll events so the FDG doesn't also see them
			if(!M->HomescreenLocked)
			{
				f32 ScrollX = 0;
				ui_event *Event = M->UI->EventFirst;
				while(Event)
				{
					ui_event *Next = Event->Next;
					if(Event->E.type == SAPP_EVENTTYPE_MOUSE_SCROLL)
					{
						ScrollX += Event->E.scroll_x;
						UI_RemoveEventFromList(M->UI, Event);
					}
					Event = Next;
				}
				M->CameraVelocity -= ScrollX * 0.035f;
			}

			// Critically damped spring
			f32 K = 15.0f;
			f32 Damping = 2.0f * sqrtf(K);
			f32 Target = M->HomescreenLocked ? M->CameraTarget : 0.0f;
			f32 Accel = -K * (M->CameraAngle - Target) - Damping * M->CameraVelocity;
			M->CameraVelocity += Accel * dt;
			M->CameraAngle   += M->CameraVelocity * dt;

			// Once we pass ±15°, commit to ±180° forever
			if(!M->HomescreenLocked && fabsf(M->CameraAngle) > (f32)M_PI / 12.0f)
			{
				M->HomescreenLocked = True;
				M->CameraTarget = (M->CameraAngle > 0) ? (f32)M_PI : -(f32)M_PI;
			}
		}

		// === Carousel: two panels orbiting around the camera ===
		// Conceptually this is a standard Model·View·Projection for a flat panel:
		// scale panel-local (u, v) ∈ [-1, 1] to world units, place at z=-D, rotate
		// around the Y axis through the camera, project with a standard frustum.
		// Worked out symbolically, D and the panel half-extents cancel and the
		// end-to-end transform collapses to the 4×4 below, with
		//   c = aspect · tan(fov/2):
		//
		//     [ cos α    0    0    -sin α/c ]   ← clip_x row
		//     [ 0        1    0     0       ]   ← clip_y identity
		//     [ 0        0    1     0       ]   ← clip_z identity
		//     [ c·sin α  0    0     cos α   ]   ← clip_w row
		//
		// Why the collapsed form rather than the explicit MVP: we compose this
		// AFTER another projection in the pipeline — either Ortho (for 2D UI)
		// or PV (for the 3D FDG graph). The input to this matrix is already in
		// clip/screen space, not panel-local world space. The identity rows for
		// y and z are what let 3D content preserve its own depth through the
		// carousel rotation. An explicit MVP would mangle clip_z and clip_w for
		// pre-projected input, breaking 3D line depth.
		//
		// The FDG version is the same matrix with α shifted by π, encoded directly
		// (cos(α+π) = −cos α, sin(α+π) = −sin α): FDG faces the camera at α = ±π
		// and has clip_w < 0 (clipped) at α = 0, symmetric to Home.

		f32 CarouselFOV    = 45.0f;
		f32 CarouselAspect = sapp_widthf() / sapp_heightf();
		f32 CarouselC      = CarouselAspect * tanf(CarouselFOV * 0.5f * (f32)M_PI / 180.0f);

		f32 CosA = cosf(M->CameraAngle);
		f32 SinA = sinf(M->CameraAngle);

		m44f HomeCarousel = M44F_IDENTITY;
		HomeCarousel.E[0][0] =  CosA;
		HomeCarousel.E[3][0] = -SinA / CarouselC;
		HomeCarousel.E[0][3] =  CarouselC * SinA;
		HomeCarousel.E[3][3] =  CosA;

		m44f FDGCarousel = M44F_IDENTITY;
		FDGCarousel.E[0][0] = -CosA;
		FDGCarousel.E[3][0] =  SinA / CarouselC;
		FDGCarousel.E[0][3] = -CarouselC * SinA;
		FDGCarousel.E[3][3] = -CosA;

		// cos(angle) interpolates 1 → -1 as the camera swings 0 → ±π,
		// which maps 1→0 via (cos+1)/2 for the red↔black background blend.
		f32 RotBlendT = (cosf(M->CameraAngle) + 1.0f) * 0.5f; // 1 = home, 0 = FDG
		v4f ClearCol  = v4f(
			LerpF32(COLOUR_BLACK.X, COLOUR_BACKGROUND.X, RotBlendT),
			LerpF32(COLOUR_BLACK.Y, COLOUR_BACKGROUND.Y, RotBlendT),
			LerpF32(COLOUR_BLACK.Z, COLOUR_BACKGROUND.Z, RotBlendT),
			1.0f);

		// Clear the whole screen with a color that lerps between the two sides.
		R_PushTransform(M->Render, Orthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0));
		D_Quad(M->Draw, M->FrameArena, v2f(0, 0), WindowWidth, WindowHeight, ClearCol);
		R_PopTransform(M->Render);

		// Sidebar removed — FDG gets the full window. Keep a TotalHeight=0 shim
		// so downstream arithmetic that still subtracts it stays correct.
		f32 TotalHeight = 0.0f;

		// The FDG side only faces us once we're past ±90°. Before that, the
		// home panel occludes it (and we skip work for it entirely).
		b32 DrawFDGSide = fabsf(M->CameraAngle) > (f32)M_PI * 0.5f;

		f32 HeaderHeight = 300;

		// Static vars that need to persist across frames even when FDG not visible
		static v2f MouseRotation = v2f(0, 0);

		// Panel metrics for centering FDG projections on the viewport
		f32 PanelLeft = TotalHeight + 55;
		f32 PanelTop = 55;
		f32 PanelWidth = sapp_widthf() - TotalHeight - 70;
		f32 PanelHeight = sapp_heightf() - 210;
		f32 PanelCenterX = PanelLeft + PanelWidth * 0.5f;
		f32 PanelCenterY = PanelTop + PanelHeight * 0.5f;
		f32 PanelAspect = PanelWidth / PanelHeight;
		// Off-center projection: shift frustum so world origin maps to panel centre
		f32 PanelNDCOffsetX = -(2.0f * PanelCenterX / sapp_widthf() - 1.0f);
		f32 PanelNDCOffsetY = (2.0f * PanelCenterY / sapp_heightf() - 1.0f);

		if(DrawFDGSide)
		{
		// Welcome header only; background is the shared lerping clear color.
		// Lerp the welcome text with the rotation: red at angle=±π, fades to black at 0.
		v4f WelcomeCol = v4f(
			LerpF32(COLOUR_BACKGROUND.X, COLOUR_BLACK.X, RotBlendT),
			LerpF32(COLOUR_BACKGROUND.Y, COLOUR_BLACK.Y, RotBlendT),
			LerpF32(COLOUR_BACKGROUND.Z, COLOUR_BLACK.Z, RotBlendT),
			1.0f);
		R_PushTransform(M->Render, Mul(Orthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0), FDGCarousel));
		D_Str8(
			M->Draw, M->FrameArena,
			Str8("WELCOME BACK, MR. WICK"),
			.Position = v3f(TotalHeight + 75, sapp_heightf() - 55 - HeaderHeight + HeaderHeight/2.0 - 20.0, 0),
			.Font = M->BebasNeue,
			.Colour = WelcomeCol,
			.HeightInPixels = HeaderHeight
		);
		R_PopTransform(M->Render);

		ui_orbit Orbit = {0};
		f32 MinDepth = 0.0f;
		f32 MaxDepth = 0.0f;

		// This push is consumed by the pops at line ~534 or ~766 below
		R_PushTransform(M->Render, Mul(Orthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0), FDGCarousel));

		R_PushScissor(M->Render, v4i(TotalHeight + 50 + 5, 55, sapp_widthf() - TotalHeight - 10 - 50 - 10, sapp_heightf() - 100 - 10 - HeaderHeight));
		if(OS_ValidPath(M->Folder))
		{
			// HACK(Oliver): This is annoying i have to do this twice
			R_PopTransform(M->Render);

			static float Angle2 = 0.0f;

			if(M->TreeView.Info.NodePositions && M->TreeView.Info.NodeCount > 0 && !M->TreeTransitioning)
			{
				// Track the focus node position directly
				v4f FP = M->TreeView.Info.NodePositions[M->TreeView.FocusNodeIndex];
				M->TreeCameraLookAt = v3f(FP.X, FP.Y, FP.Z);
				M->TreeCameraLookAtTarget = v3f(FP.X, FP.Y, FP.Z);
			}
			else if(M->TreeTransitioning)
			{
				M->TreeCameraLookAt = LerpV3F(M->TreeCameraLookAt, M->TreeCameraLookAtTarget, 0.12f);
			}
			M->TreeCameraZoom = Lerp(M->TreeCameraZoom, M->TreeCameraZoomTarget, M->TreeTransitioning ? 0.12f : 0.08f);

			// Check if transition lerp is done
			if(M->TreeTransitioning)
			{
				v3f Diff = SubV3F(M->TreeCameraLookAt, M->TreeCameraLookAtTarget);
				f32 Dist = sqrtf(DotV3F(Diff, Diff));
				f32 ZoomDiff = fabsf(M->TreeCameraZoom - M->TreeCameraZoomTarget);
				if(Dist < 0.02f && ZoomDiff < 0.1f)
				{
					M->TreeTransitioning = False;
					if(M->PendingShowFileList)
					{
						M->FileListPath = M->PendingFolder;
						M->ShowFileList = True;
						M->FileListPosition = M->PendingFileListPosition;
						M->PendingShowFileList = False;
						M->TreeView.ActiveLabelFontSizeTarget = 55.0f;
					}
				}
			}

			f32 CamAngle = Angle2 + MouseRotation.X * 0.01f;
			v3f CamOffset = v3f(
				-sinf(CamAngle) * M->TreeCameraZoom,
				0,
				-cosf(CamAngle) * M->TreeCameraZoom
			);
			v3f CamPos = AddV3F(M->TreeCameraLookAt, CamOffset);
			m44f P = Perspective(PanelAspect, 35, 0.01, 10000.0);
			P.E[2][0] = PanelNDCOffsetX;
			P.E[2][1] = PanelNDCOffsetY;
			m44f V = M44FLookAt(CamPos, M->TreeCameraLookAt, v3f(0, 1, 0));
			m44f PV = Mul(V, P);
			R_PushTransform(M->Render, Mul(PV, FDGCarousel));
			u64 TreeViewStart = OS_TimeStampNow();
			UI_TreeView(M->UI, M->Draw, &M->Shared, M->FrameArena, &M->TreeView, M->Folder, v3f(0.1, 0.1, 0.1));
			M->Debug.TreeViewMs = OS_MilliSecondsFromTimeStamp(OS_TimeStampNow() - TreeViewStart);
			if(M->Debug.TreeViewMs > M->Debug.TreeViewMsMax) M->Debug.TreeViewMsMax = M->Debug.TreeViewMs;

			M->Debug.FDGBuildMs = M->TreeView.DebugBuildMs;
			M->Debug.FDGAttributesMs = M->TreeView.DebugAttributesMs;
			M->Debug.FDGSimulateMs = M->TreeView.DebugSimulateMs;
			M->Debug.FDGTotalMs = M->Debug.FDGBuildMs + M->Debug.FDGAttributesMs + M->Debug.FDGSimulateMs;
			if(M->Debug.FDGBuildMsMax < M->Debug.FDGBuildMs) M->Debug.FDGBuildMsMax = M->Debug.FDGBuildMs;
			if(M->Debug.FDGAttributesMsMax < M->Debug.FDGAttributesMs) M->Debug.FDGAttributesMsMax = M->Debug.FDGAttributesMs;
			if(M->Debug.FDGSimulateMsMax < M->Debug.FDGSimulateMs) M->Debug.FDGSimulateMsMax = M->Debug.FDGSimulateMs;
			if(M->Debug.FDGTotalMsMax < M->Debug.FDGTotalMs) M->Debug.FDGTotalMsMax = M->Debug.FDGTotalMs;

			R_PopTransform(M->Render);

			R_PushTransform(M->Render, Mul(Orthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0), FDGCarousel));
			ui_tree_view_signal TreeViewSignal = UI_TreeViewFolderLabels(M->UI, M->Draw, M->Edit, M->Orbitron, M->BebasNeue, M->FrameArena, M->OrbitItem, &M->TreeView, PV, CamPos, M->Folder, &M->TextBoxBuffer);
			
			// HACK(Oliver): Find a better way to do this
			v4i PanelRect = v4i(TotalHeight + 50 + 5, 55 + 1, sapp_widthf() - TotalHeight - 10 - 50 - 10, sapp_heightf() - 100 - 10 - HeaderHeight);
			if(M->ShowFileList)
			{
				D_Blur(M->Draw, M->FrameArena, PanelRect, 10.0);
				if(UI_MouseInV4I(M->UI, PanelRect))
				{
					M->UI->NextHot = M_FNVHashString(M_FNV_HASH_START, Str8("Background Panel"));
				}
				os_directory_tree ActiveTree = OS_DirectoryTreeFromPath(M->FrameArena, M->FileListPath, (os_directory_tree_options){});

				if(UI_KeyDown(M->UI, SAPP_KEYCODE_ESCAPE, 0))
				{
					M->ShowFileList = False;
					M->TreeView.ActiveLabelFontSizeTarget = 30.0f;
				}
				// Track the active node's projected position each frame
				M->FileListPosition = TreeViewSignal.ActiveScreenPosition;

				// Draw header label AFTER blur so it stays crisp
				{
					str8 HeaderName = Str8FromLastCharacter(M->FrameArena,
						M->TreeView.Info.NodeInfos[M->OrbitItem].Name, '/');
					if(HeaderName.Size == 0)
						HeaderName = M->TreeView.Info.NodeInfos[M->OrbitItem].Name;
					D_Str8(M->Draw, M->FrameArena, HeaderName,
						.Font = M->BebasNeue,
						.Position = v3f(M->FileListPosition.X, M->FileListPosition.Y, 0),
						.Colour = v4f(0.0, 0.0, 0.0, 1.0),
						.HeightInPixels = M->TreeView.ActiveLabelFontSize);
				}

				f32 Height = 50.0;
				M->FileListScrollTarget += UI_Scroll(M->UI).Y*0.4;
				M->FileListScroll = Lerp(M->FileListScroll, M->FileListScrollTarget, 0.2);
				v2f Position = M->FileListPosition;
				// Offset file list below the header label
				Position.Y -= M->TreeView.ActiveLabelFontSize + 10.0f;
				Position.Y += M->FileListScroll*10.0;
				f32 VisibleRadius = 5.0f;
				f32 MinScale = 0.8f;
				f32 MaxScale = 1.35f;
				f32 ScrollSpeed = 0.3f;
				int Index = 0;
				for(
					os_directory_tree_iterator Iter = {0};
					OS_DirectoryTreeIterate(ActiveTree, &Iter);
				)
				{
					str8 FileName = Str8FromLastCharacter(M->FrameArena, Iter.Current->PathAndName, '/');
					f32 DistanceFromActiveItem = Position.Y - M->FileListPosition.Y;
					f32 Value = fabs(DistanceFromActiveItem)*0.01;
					f32 W = 1.0f - SmoothStep(Value);
					f32 Scale = MinScale  + (MaxScale - MinScale)*W;
					ui_style CurrentStyle = UI_GetStyle(M->UI);
					CurrentStyle.Font = M->BebasNeue;
					UI_PushStyle(M->UI, CurrentStyle);
					if(fabs(DistanceFromActiveItem) < Height*10.0)
					{
						ui_signal Signal = UI_TextButton(
							M->UI, M->Draw, M->FrameArena,
							Str8PushF(M->FrameArena, "?S###?u32", FileName, Index), Height*Scale, Position,
							v4f(0.0, 0.0, 0.0, 1)
						);
						if(Signal.Clicked)
						{
							M->Buffer.String = OS_LoadEntireFileIntoMemory(M->PermanentArena, Iter.Current->PathAndName);
							M->UI->View = UI_VIEW_EDIT;
						}
					}
					UI_PopStyle(M->UI);
					Position.Y -= Height*Scale;
					Index++;
				}

			}
			else
			{
				v2f TreeScroll = UI_Scroll(M->UI);
				if(fabsf(TreeScroll.X) > fabsf(TreeScroll.Y))
				{
					MouseRotation.X += TreeScroll.X;
				}
				else
				{
					M->TreeCameraZoomTarget -= TreeScroll.Y * 0.1f;
					M->TreeCameraZoomTarget = Max(2.0f, M->TreeCameraZoomTarget);
					M->TreeCameraZoomTarget = Min(15.0f, M->TreeCameraZoomTarget);
				}

				if(!TreeViewSignal.Hovered)
				{
					Angle2 += 0.0005;
				}
			}

			if(!M->ShowFileList)
			{
				M->OrbitItem = TreeViewSignal.NodeIndex;
			}

			if(TreeViewSignal.Clicked && TreeViewSignal.NewPath.Size > 0 && !M->TreeTransitioning)
			{
				int ClickedIndex = TreeViewSignal.NodeIndex;
				if(ClickedIndex >= 0 && ClickedIndex < M->TreeView.Info.NodeCount)
				{
					// Check if this node has children in the already-loaded tree
					b32 HasChildren = False;
					for(int I = 0; I < M->TreeView.Info.LinkCount; I++)
					{
						if(M->TreeView.Info.Froms[I] == ClickedIndex && M->TreeView.Info.Tos[I] != ClickedIndex)
						{
							HasChildren = True;
							break;
						}
					}

					if(!HasChildren)
					{
						// Leaf folder — show file browser; label grows into header
						M->FileListPath = Str8Copy(M->PermanentArena, TreeViewSignal.NewPath);
						M->ShowFileList = True;
						M->FileListPosition = TreeViewSignal.ClickedPosition;
						M->FileListScroll = 0;
						M->FileListScrollTarget = 0;
						M->TreeView.ActiveLabelFontSizeTarget = 55.0f;
					}
					else
					{
						// Focus on this subtree — orbit the clicked node
						M->TreeView.FocusNodeIndex = ClickedIndex;
						v4f ClickedPos = M->TreeView.Info.NodePositions[ClickedIndex];
						M->TreeCameraLookAtTarget = v3f(ClickedPos.X, ClickedPos.Y, ClickedPos.Z);
						M->TreeCameraZoomTarget = 2.5f;
						M->TreeTransitioning = True;
					}
				}
			}

			if(UI_KeyDown(M->UI, SAPP_KEYCODE_ESCAPE, 0) && !M->ShowFileList)
			{
				if(M->TreeView.FocusNodeIndex > 0)
				{
					// Navigate up: find parent of current focus node
					for(int I = 0; I < M->TreeView.Info.LinkCount; I++)
					{
						if(M->TreeView.Info.Tos[I] == M->TreeView.FocusNodeIndex)
						{
							int ParentIndex = M->TreeView.Info.Froms[I];
							M->TreeView.FocusNodeIndex = ParentIndex;
							v4f ParentPos = M->TreeView.Info.NodePositions[ParentIndex];
							M->TreeCameraLookAtTarget = v3f(ParentPos.X, ParentPos.Y, ParentPos.Z);
							M->TreeCameraZoomTarget = 3.0f;
							M->TreeTransitioning = True;
							break;
						}
					}
				}
			}
			R_PopTransform(M->Render);
		}
		else
		{
			R_PopTransform(M->Render);
		}
		R_PopScissor(M->Render);

		R_PushTransform(M->Render, Mul(Orthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0), FDGCarousel));

		ui_tree_view_signal TreeSignal = {0};
		if(!OS_ValidPath(M->Folder))
		{
			// Homescreen: use same tree view rendering as the directory browser
			R_PopTransform(M->Render);

			static float HomescreenAngle = 0.0f;
			MouseRotation.X += UI_Scroll(M->UI).X;
			MouseRotation.Y += UI_Scroll(M->UI).Y;

			// Camera orbits the root node
			if(M->HomescreenTree.Info.NodePositions && M->HomescreenTree.Info.NodeCount > 0)
			{
				v4f FP = M->HomescreenTree.Info.NodePositions[M->HomescreenTree.FocusNodeIndex];
				M->TreeCameraLookAt = v3f(FP.X, FP.Y, FP.Z);
			}

			f32 CamAngle = HomescreenAngle + MouseRotation.X * 0.01f;
			f32 HomescreenZoom = 3.0f;
			v3f CamOffset = v3f(
				-sinf(CamAngle) * HomescreenZoom,
				0,
				-cosf(CamAngle) * HomescreenZoom
			);
			v3f CamPos = AddV3F(M->TreeCameraLookAt, CamOffset);
			m44f P = Perspective(PanelAspect, 35, 0.01, 10000.0);
			P.E[2][0] = PanelNDCOffsetX;
			P.E[2][1] = PanelNDCOffsetY;
			m44f V = M44FLookAt(CamPos, M->TreeCameraLookAt, v3f(0, 1, 0));
			m44f PV = Mul(V, P);
			R_PushTransform(M->Render, Mul(PV, FDGCarousel));

			UI_TreeViewFromPaths(M->UI, M->Draw, &M->Shared, M->FrameArena,
								&M->HomescreenTree, M->RecentlyLoadedFolders);

			R_PopTransform(M->Render);

			R_PushScissor(M->Render, v4i(TotalHeight + 50 + 5, 55, sapp_widthf() - TotalHeight - 10 - 50 - 10, sapp_heightf() - 100 - 10 - HeaderHeight));
			R_PushTransform(M->Render, Mul(Orthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0), FDGCarousel));
			ui_tree_view_signal HomescreenSignal = UI_TreeViewFolderLabels(
				M->UI, M->Draw, M->Edit, M->Orbitron, M->BebasNeue, M->FrameArena, -1,
				&M->HomescreenTree, PV, CamPos, Str8("/"), Null);
			R_PopScissor(M->Render);

			if(HomescreenSignal.Clicked && HomescreenSignal.NewPath.Size > 0)
			{
				// Check if the clicked path is a real directory — if so, open it
				if(OS_ValidPath(HomescreenSignal.NewPath))
				{
					M->Folder = Str8Copy(M->PermanentArena, HomescreenSignal.NewPath);
					M->UI->Active = UI_IDFromString(Str8("TreeViewTextBox"));
				}
				else
				{
					// Navigate within the homescreen tree
					M->HomescreenTree.FocusNodeIndex = HomescreenSignal.NodeIndex;
				}
			}
		}

		//D_Grid2D(M->Draw, M->FrameArena, 15, 500);

		//D_Str8(M->Draw, M->FrameArena, M->LogoFont, v2f(50, 50), v4f(1.0, 0.05, 0.0, 1), 100, Str8("MAYDAY!"));
		R_PopTransform(M->Render);
		} // end if(DrawFDGSide)

		// === MAYDAY! front panel (homescreen) ===
		// The red background is already drawn by the shared lerping clear color,
		// so the panel only contains its text (which rotates with the carousel).
		// Text lerps from black (at angle=0) to red (at ±π) as the bg inverts the other way.
		// Only draw while the home side is actually facing us (|angle| < π/2).
		f32 CarouselCX = sapp_widthf() * 0.5f;
		f32 CarouselCY = sapp_heightf() * 0.5f;
		if(fabsf(M->CameraAngle) < (f32)M_PI * 0.5f)
		{
			m44f HomeOrtho = Mul(Orthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0), HomeCarousel);
			R_PushTransform(M->Render, HomeOrtho);

			v4f HomeTextCol = v4f(
				LerpF32(COLOUR_BACKGROUND.X, COLOUR_BLACK.X, RotBlendT),
				LerpF32(COLOUR_BACKGROUND.Y, COLOUR_BLACK.Y, RotBlendT),
				LerpF32(COLOUR_BACKGROUND.Z, COLOUR_BLACK.Z, RotBlendT),
				1.0f);

			// Centered MAYDAY! — 3D layered extrusion
			//
			// Front copy is solid black. Behind it, N copies staggered in 2D
			// screen space; each back copy is drawn twice at the same position
			// — first as solid red (COLOUR_BACKGROUND) to fill the glyph body,
			// then as a black outline-only pass on top. Back copies are drawn
			// farthest-first so painter's order handles occlusion (depth test
			// is disabled for every text path in the Metal backend, so paint
			// order is *the* ordering).
			//
			// Why 2D offsets instead of per-layer Z:
			// Giving each layer a pixel-space Z and rotating around a pivot
			// inevitably gives each GLYPH VERTEX a Z value proportional to its
			// distance from the pivot (Z_final ≈ -dx·sinθy + Z_layer). That Z
			// spread is fine in isolation, but the carousel's HomeCarousel
			// transform has E[0][3] = C·sinα which folds clip_x into clip_w,
			// so any per-vertex Z variance interacts with the perspective
			// divide unevenly as α swings — producing the "missing lines" and
			// the glitching during carousel rotation. Keeping all glyph
			// vertices on the Z=0 plane removes that interaction entirely;
			// the stair shift comes purely from per-layer 2D offsets.
			//
			// A small uniform Y-rotation is still applied to the whole group
			// for the slight glyph perspective tilt — all vertices are on the
			// same rotated plane, so no per-layer Z fighting can occur.
			//
			// Y-up pixel coords (Ortho args are (Left, Right, Top=h, Bottom=0)):
			// D_Str8 Position.Y is the baseline; glyphs rise upward, so the
			// text extends from Y up to Y + MaydayCap.
			f32 MaydayHeight = 760.0f;
			str8 MaydayStr = Str8("MAYDAY!");
			f32 MaydayW = (f32)UI_GetTextWidth(M->BebasNeue, M->Draw, MaydayStr, MaydayHeight);
			f32 MaydayX = CarouselCX - MaydayW * 0.5f;
			f32 MaydayY = CarouselCY - MaydayHeight * 0.32f;

			// TOKYO-style 3D layered extrusion.
			//
			// Stacking is done as 2D pixel offsets (not per-layer Z), and
			// a small uniform Y-rotation tilts the whole group. Each back
			// layer draws red-fill + black-outline; a solid-black front
			// copy is drawn last so painter's order (depth test is off
			// for every text path in the Metal backend) puts it on top.
			//
			// Why the base Z is negative:
			// Metal's NDC_Z range is [0, 1] (unlike OpenGL's [-1, 1]).
			// The app's Orthographic(Near=-1000, Far=1000) gives
			// NDC_Z = -z/1000, so any vertex with input z > 0 maps to
			// NDC_Z < 0 and is clipped by the near plane. The Y-rotation
			// around the pivot at the text centre gives vertices LEFT of
			// the pivot z = -dx·sinθ > 0 (and vertices RIGHT of it z < 0).
			// If we left Position.Z at 0, the whole left half of MAYDAY!
			// (the "MAY" letters) silently gets its glyph triangles
			// clipped, while the right half ("DAY!") renders normally.
			// The bezier pipeline is a compute shader in pixel space so
			// it ignores NDC_Z entirely — which is why, when this bug
			// was alive, MAY appeared as wireframe-only (outlines
			// drawn, fills clipped) and DAY! rendered fully solid.
			//
			// Setting BaseZ = -200 pushes every vertex comfortably into
			// z < 0 after the rotation (the worst-case rotation shift
			// is |dx|·sinθ ≈ 475·0.08 ≈ 38 px), so NDC_Z stays inside
			// [0, 1] for every glyph vertex regardless of its side.
			f32 MaydayScale = stbtt_ScaleForPixelHeight(&M->BebasNeue.Info, MaydayHeight);
			f32 MaydayCap   = (f32)M->BebasNeue.Ascent * MaydayScale;

			f32 PivotX = CarouselCX;
			f32 PivotY = MaydayY + MaydayCap * 0.5f;
			f32 MaydayAngleY = 0.08f;  // ~4.6° — subtle tilt
			f32 BaseZ = -200.0f;       // see comment above — keeps NDC_Z valid after rotation

			m44f T1 = M44FTranslate(v3f(-PivotX, -PivotY, 0.0f));
			m44f Ry = M44FromYAngle(MaydayAngleY);
			m44f T2 = M44FTranslate(v3f( PivotX,  PivotY, 0.0f));
			m44f LocalRot = Mul(Mul(T1, Ry), T2);
			m44f MaydayTransform = Mul(LocalRot, HomeOrtho);

			R_PushTransform(M->Render, MaydayTransform);

			int NumLayers = 6;
			f32 LayerDX = 14.0f;   // per-step X offset (right)
			f32 LayerDY = 10.0f;   // per-step Y offset (down in Y-up = -)
			f32 OutlineThickness = 3.0f;

			// Back copies, farthest first (painter's order).
			for(int Layer = NumLayers; Layer >= 1; Layer--)
			{
				f32 OffX = (f32)Layer * LayerDX;
				f32 OffY = (f32)Layer * -LayerDY; // -Y because Y-up means "down on screen" is -Y

				// Red body fill.
				D_Str8(M->Draw, M->FrameArena, MaydayStr,
					.Font = M->BebasNeue,
					.Position = v3f(MaydayX + OffX, MaydayY + OffY, BaseZ),
					.Colour = COLOUR_BACKGROUND,
					.HeightInPixels = MaydayHeight);

				// Black outline — this is what makes each layer read as
				// a distinct 3D step when overlapping the next layer forward.
				D_Str8(M->Draw, M->FrameArena, MaydayStr,
					.Font = M->BebasNeue,
					.Position = v3f(MaydayX + OffX, MaydayY + OffY, BaseZ),
					.Colour = COLOUR_BLACK,
					.HeightInPixels = MaydayHeight,
					.OutlineThickness = OutlineThickness);
			}

			// Front: solid black, no offset, drawn last.
			D_Str8(M->Draw, M->FrameArena, MaydayStr,
				.Font = M->BebasNeue,
				.Position = v3f(MaydayX, MaydayY, BaseZ),
				.Colour = COLOUR_BLACK,
				.HeightInPixels = MaydayHeight);

			R_PopTransform(M->Render);

			// Build info below, centered horizontally
			f32 InfoSize = 36.0f;
			f32 InfoLineGap = InfoSize * 1.5f;
			f32 InfoY = MaydayY - InfoSize * 2.5f;

			str8 BuildRoman = IntToRomanNumerals(M->FrameArena, BUILD_NUMBER);
			str8 BuildStr = Str8PushF(M->FrameArena, "BUILD: ?S", BuildRoman);
			f32 BuildW = (f32)UI_GetTextWidth(M->CodeFont, M->Draw, BuildStr, InfoSize);
			D_Str8(M->Draw, M->FrameArena, BuildStr,
				.Font = M->CodeFont,
				.Position = v3f(CarouselCX - BuildW * 0.5f, InfoY, 0),
				.Colour = HomeTextCol,
				.HeightInPixels = InfoSize);

			InfoY -= InfoLineGap;
			str8 DateStr = Str8(__DATE__);
			f32 DateW = (f32)UI_GetTextWidth(M->CodeFont, M->Draw, DateStr, InfoSize);
			D_Str8(M->Draw, M->FrameArena, DateStr,
				.Font = M->CodeFont,
				.Position = v3f(CarouselCX - DateW * 0.5f, InfoY, 0),
				.Colour = HomeTextCol,
				.HeightInPixels = InfoSize);

			InfoY -= InfoLineGap;
#if defined(OS_DARWIN)
			str8 PlatStr = Str8("DARWIN/ARM64");
#elif defined(OS_WINDOWS)
			str8 PlatStr = Str8("WIN64");
#else
			str8 PlatStr = Str8("UNKNOWN");
#endif
			f32 PlatW = (f32)UI_GetTextWidth(M->CodeFont, M->Draw, PlatStr, InfoSize);
			D_Str8(M->Draw, M->FrameArena, PlatStr,
				.Font = M->CodeFont,
				.Position = v3f(CarouselCX - PlatW * 0.5f, InfoY, 0),
				.Colour = HomeTextCol,
				.HeightInPixels = InfoSize);

			R_PopTransform(M->Render);
		}
	}

	R_PopTransform(M->Render);

	if(UI_KeyDown(M->UI, SAPP_KEYCODE_F3, 0))
	{
		M->ShowDebugOverlay = !M->ShowDebugOverlay;
	}

	if(UI_KeyDown(M->UI, SAPP_KEYCODE_F4, 0))
	{
		M->Debug.FrameMsMax = 0;
		M->Debug.CPUMsMax = 0;
		M->Debug.FPSMin = 0;
		M->Debug.FPSMax = 0;
		M->Debug.FPSAvg = 0;
		M->Debug.FPSAccumulator = 0;
		M->Debug.FPSFrameCount = 0;
		M->Debug.RenderMsMax = 0;
		M->Debug.EditViewMsMax = 0;
		M->Debug.TreeViewMsMax = 0;
		M->Debug.FDGBuildMsMax = 0;
		M->Debug.FDGAttributesMsMax = 0;
		M->Debug.FDGSimulateMsMax = 0;
		M->Debug.FDGTotalMsMax = 0;
	}

	// Debug timing overlay
	if(M->ShowDebugOverlay)
	{
		R_PushTransform(M->Render, Orthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0));

		f32 DebugH = 40;
		f32 DebugSpacing = 50;
		f32 DebugPadding = 15;
		f32 DebugX = DebugPadding + 10;
		v4f Col = v4f(1, 1, 1, 1);
		v4f ColDim = v4f(0.6, 0.6, 0.6, 1);
		v4f ColHeader = v4f(1, 0.5, 0, 1);
		d_font Font = M->Rajdhani;

		int TotalRows = 12;
		if(M->UI->View == UI_VIEW_EDIT) TotalRows += 3;
		else if(M->UI->View == UI_VIEW_TREE) TotalRows += 13;

		f32 PanelWidth = 450;
		f32 PanelHeight = DebugSpacing * (f32)TotalRows + DebugPadding * 2;
		f32 PanelY = sapp_heightf() - DebugPadding;
		f32 DebugY = PanelY - DebugPadding - DebugH * 0.25f;

		D_Quad(M->Draw, M->FrameArena, v2f(DebugPadding, PanelY - PanelHeight), PanelWidth, PanelHeight, v4f(0, 0, 0, 0.85));

		int Row = 0;

		#define DEBUG_ROW(colour, fmt, ...) \
			D_Str8(M->Draw, M->FrameArena, \
				Str8PushF(M->FrameArena, fmt, ##__VA_ARGS__), \
				.Font = Font, .Position = v3f(DebugX, DebugY - DebugSpacing*(f32)(Row), 0), \
				.Colour = colour, .HeightInPixels = DebugH); \
			Row++

		DEBUG_ROW(ColHeader, "--- FRAME (F4 reset) ---");
		DEBUG_ROW(Col,    "Frame:    ?f32 ms", M->Debug.FrameMs);
		DEBUG_ROW(ColDim, "Max:      ?f32 ms", M->Debug.FrameMsMax);
		DEBUG_ROW(Col,    "CPU:      ?f32 ms", M->Debug.CPUMs);
		DEBUG_ROW(ColDim, "Max:      ?f32 ms", M->Debug.CPUMsMax);
		DEBUG_ROW(Col,    "FPS:      ?f32", M->Debug.FPS);
		DEBUG_ROW(ColDim, "FPS Avg:  ?f32", M->Debug.FPSAvg);
		DEBUG_ROW(ColDim, "FPS Min:  ?f32", M->Debug.FPSMin);
		DEBUG_ROW(ColDim, "FPS Max:  ?f32", M->Debug.FPSMax);

		DEBUG_ROW(ColHeader, "--- RENDER (vsync) ---");
		DEBUG_ROW(Col,    "R_Update: ?f32 ms", M->Debug.RenderMs);
		DEBUG_ROW(ColDim, "Max:      ?f32 ms", M->Debug.RenderMsMax);

		if(M->UI->View == UI_VIEW_EDIT)
		{
			DEBUG_ROW(ColHeader, "--- EDIT VIEW ---");
			DEBUG_ROW(Col,    "Total:    ?f32 ms", M->Debug.EditViewMs);
			DEBUG_ROW(ColDim, "Max:      ?f32 ms", M->Debug.EditViewMsMax);
		}
		else if(M->UI->View == UI_VIEW_TREE)
		{
			DEBUG_ROW(ColHeader, "--- TREE VIEW ---");
			DEBUG_ROW(Col,    "Total:    ?f32 ms", M->Debug.TreeViewMs);
			DEBUG_ROW(ColDim, "Max:      ?f32 ms", M->Debug.TreeViewMsMax);

			DEBUG_ROW(ColHeader, "--- FDG ---");
			DEBUG_ROW(Col,    "Build:    ?f32 ms", M->Debug.FDGBuildMs);
			DEBUG_ROW(ColDim, "Max:      ?f32 ms", M->Debug.FDGBuildMsMax);
			DEBUG_ROW(Col,    "Attribs:  ?f32 ms", M->Debug.FDGAttributesMs);
			DEBUG_ROW(ColDim, "Max:      ?f32 ms", M->Debug.FDGAttributesMsMax);
			DEBUG_ROW(Col,    "Simulate: ?f32 ms", M->Debug.FDGSimulateMs);
			DEBUG_ROW(ColDim, "Max:      ?f32 ms", M->Debug.FDGSimulateMsMax);
			DEBUG_ROW(Col,    "FDG Total:?f32 ms", M->Debug.FDGTotalMs);
			DEBUG_ROW(ColDim, "Max:      ?f32 ms", M->Debug.FDGTotalMsMax);

			DEBUG_ROW(ColDim, "Nodes:    ?s32", M->TreeView.Info.NodeCount);
			DEBUG_ROW(ColDim, "Links:    ?s32", M->TreeView.Info.LinkCount);
		}

		#undef DEBUG_ROW

		R_PopTransform(M->Render);
	}

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

	M->Debug.CPUMs = OS_MilliSecondsFromTimeStamp(OS_TimeStampNow() - M->Debug.CPUStart);
	if(M->Debug.CPUMs > M->Debug.CPUMsMax) M->Debug.CPUMsMax = M->Debug.CPUMs;

	u64 RenderStart = OS_TimeStampNow();
	R_Update(M->Render, M->FrameArena);
	M->Debug.RenderMs = OS_MilliSecondsFromTimeStamp(OS_TimeStampNow() - RenderStart);
	if(M->Debug.RenderMs > M->Debug.RenderMsMax) M->Debug.RenderMsMax = M->Debug.RenderMs;

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

	assert(M->Edit->Arena);
}

void OS_Initialise(void *Data)
{
	LogInfo("ABC");
	mayday_context *M = Data;
	M->Buffer.HeightInPixels = 35;
	M->BufferFontHeight = 35;
	M->BufferFontHeightTarget = 35;
	R_Init(M->Render, M->PermanentArena, v2i(sapp_width() , sapp_height()));
	M->Render->BloomEnabled = False;

	M->Buffer.String.Bytes = ArenaPushZero(M->PermanentArena, MegaBytes(1));
	M->Buffer.String.Size = sizeof(G_TriangleShader);
	MemoryCopy(M->Buffer.String.Bytes, G_TriangleShader.Data, G_TriangleShader.Length);


	M->Edit->SearchBox.String.Bytes = ArenaPushArrayZero(M->PermanentArena, u8, 8192);

	M->TextBoxBuffer.String.Bytes = ArenaPushArrayZero(M->PermanentArena, u8, 8192);
	M->TextBoxBuffer.HeightInPixels = 60;

	M->TreeCameraZoom = 4.0f;
	M->TreeCameraZoomTarget = 4.0f;

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


	int TotalCount = 0;
	for(int I = 0; I < 26; I++)
	{
		d_atlas_glyph *Glyph = D_GlyphFromCodePoint(M->Draw, M->CodeFont, 65 + I);
		TotalCount += Glyph->CurveCount;
	}

	for(int I = 0; I < 10; I++)
	{
		d_atlas_glyph *Glyph = D_GlyphFromCodePoint(M->Draw, M->CodeFont, 48 + I);
		TotalCount += Glyph->CurveCount;
	}

	LogInfo("Total point count for ascii A-Z and 0-9: ?u32", TotalCount);

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
	M.Orbitron = D_LoadFontFromTTF(M.Draw, G_OrbitronRegular.Data);
	M.Rajdhani = D_LoadFontFromTTF(M.Draw, G_RajdhaniMedium.Data);
	M.BebasNeue = D_LoadFontFromTTF(M.Draw, G_BebasNeueRegular.Data);

	M.UI->Font = M.CodeFont;
	M.LookFrom.Z = 500;
	M.LookFromTarget.Z = 70.0;
	M.LookFromTarget.X = -20;

	M.Shared.Arena = ArenaInitialise(GigaBytes(1));

	u64 Start = OS_TimeStampNow();

	DispatchInitialise(&M.Shared, 12);

	//M.Folder = Str8("/");

	sapp_desc *SAPPDesc = ArenaPushArrayZero(Arena, sapp_desc, 1);
	SAPPDesc->window_title = "Mayday!";
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
