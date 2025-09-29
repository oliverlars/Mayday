
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

	d_font Rajdhani;
	d_font Orbitron;
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


#define COLOUR_BACKGROUND v4f(0.008, 0.00, 0, 1.0)

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

	//if(M->UnFocused) return;

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
			M->FolderTree = OS_DirectoryTreeFromPath(M->PermanentArena, M->Folder, (os_directory_tree_options){.FoldersOnly = True});
			M->FolderHash = FolderHash;
		}

		v2f Centre = {0};
		R_PushTransform(M->Render, Orthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0));
		D_Quad(M->Draw, M->FrameArena, v2f(0, 0),  WindowWidth, WindowHeight, v4f(0.008, 0.00, 0, 1.0));
		//D_Grid2D(M->Draw, M->FrameArena, M->Buffer.HeightInPixels, M->Buffer.HeightInPixels*5);
		
		// Vertical text
		f32 TextWidth = sapp_heightf() - 100.0f;
		f32 TextHeight = D_Str8VisualHeightFromWidth(M->Rajdhani, Str8("Mayday!"), TextWidth);
		v4f BBox = D_Str8BBoxFromHeight(M->Rajdhani, v2f(0, 0), Str8("Mayday!"), TextHeight);
		f32 Height = BBox.W - BBox.Y + 10.0;
		f32 TotalHeight = (Height + Height/3.0);
		// HACK(Oliver): I account for the line thickness here, but that
		// should be done in the height functions
		f32 Offsets[3] = 
		{
			2.0*Height/3.0f - 40,
			Height/3.0f - 40,
			-40,
		};

		m44f T = R_CurrentTransform(M->Render);
		m44f TextTransform = Mul(
			M44FTranslate(v3f((sapp_heightf() - TextWidth)/2.0, -TotalHeight, 0)), 
			Mul(M44FromZAngle(-M_PI/2.0), M44FTranslate(v3f(0, 0, 0)))
		);
		R_PushTransform(M->Render, Mul(TextTransform, T));

		for(int I = 0; I < 3; I++)
		{
			f32 Offset = Offsets[I];
			ui_style Style = UI_GetStyle(M->UI);
			UI_PushStyle(M->UI, Style);
			
			if(I < 2)
			{
				D_Str8(M->Draw, M->FrameArena, Str8("MAYDAY!"), .Font = M->Rajdhani,.Position = v3f(0, Offset, 0), .Colour = v4f(0.2, 0.1, 0.0, 1.0), .WidthInPixels = TextWidth, .OutlineThickness = 5.0);
				// HACK(Oliver): Not sure why this needs to be offset by -10 in the X axis
				D_Quad(M->Draw, M->FrameArena, v2f(-10, Offset - 10), sapp_heightf(), Height/3.0 + 40.0, COLOUR_BACKGROUND);
				//D_Quad(M->Draw, M->FrameArena, v2f(-10, Offset - Height/2.0), sapp_heightf(), Height, COLOUR_BACKGROUND); 
			}
			else
			{
				D_Str8(M->Draw, M->FrameArena, Str8("MAYDAY!"), .Font = M->Rajdhani, .Position = v3f(0, Offset, 0), .Colour = v4f(0.2, 0.1, 0.0, 1.0), .WidthInPixels = TextWidth);
			} 
			
			UI_PopStyle(M->UI);
		}
		R_PopTransform(M->Render);


		f32 HeaderHeight = 100;
		D_Quad(M->Draw, M->FrameArena, v2f(TotalHeight + 50, 50), sapp_widthf() - TotalHeight - 10 - 50, sapp_heightf() - 100, v4f(0.2, 0.1, 0.0, 1.0));
		D_Quad(M->Draw, M->FrameArena, v2f(TotalHeight + 50 + 5, 55), sapp_widthf() - TotalHeight - 10 - 50 - 10, sapp_heightf() - 100 - 10 - HeaderHeight, COLOUR_BACKGROUND);
		D_Str8(
			M->Draw, M->FrameArena,
			Str8("WELCOME BACK, MR. WICK"),
			.Position = v3f(TotalHeight + 40 + 60, sapp_heightf() - 55 - 100 + HeaderHeight/2.0 - 20.0, 0),
			.Font = M->Rajdhani,
			.Colour = COLOUR_BACKGROUND,
			.OutlineThickness = 2.0,
			.HeightInPixels = 100
		);

		
		ui_orbit Orbit = {0};
		f32 MinDepth = 0.0f;
		f32 MaxDepth = 0.0f;


		// TOOD(Oliver): Get rid of all this static shit
		static v2f MouseRotation = v2f(0, 0);

		R_PushScissor(M->Render, v4i(TotalHeight + 50 + 5, 55, sapp_widthf() - TotalHeight - 10 - 50 - 10, sapp_heightf() - 100 - 10 - HeaderHeight));
		if(OS_ValidPath(M->Folder))
		{
			MouseRotation.X += UI_Scroll(M->UI).X;
			MouseRotation.Y += UI_Scroll(M->UI).Y;
			// HACK(Oliver): This is annoying i have to do this twice
			R_PopTransform(M->Render);

			static float Angle2 = 0.0f;
			m44f P = Perspective(sapp_widthf()/sapp_heightf(), 35, 0.01, 10000.0);
			m44f V =  M44FLookAt(v3f(0, 0, -5), v3f(0, 0, 0), v3f(0, 1, 0));
			m44f RotateAroundOrigin =
				Mul(M44FTranslate(v3f(0, 0, 5)),
				Mul(M44FromYAngle(Angle2 + MouseRotation.X*0.01),
				M44FTranslate(v3f(0, 0, -5))));
			V = Mul(V, RotateAroundOrigin);
			m44f PV = Mul(V, P);
			R_PushTransform(M->Render, PV);
			
			UI_TreeView(M->UI, M->Draw, &M->Shared, M->FrameArena, &M->TreeView, M->Folder, v3f(0.1, 0.1, 0.1));

			R_PopTransform(M->Render);

			R_PushTransform(M->Render, Orthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0));
			ui_tree_view_signal TreeViewSignal = UI_TreeViewFolderLabels(M->UI, M->Draw, M->Edit, M->Orbitron, M->FrameArena, 0, &M->TreeView, PV, M->Folder, &M->TextBoxBuffer);
			if(M->ShowFileList)
			{
				D_Blur(M->Draw, M->FrameArena, v4i(TotalHeight + 50 + 5, 55 + 1, sapp_widthf() - TotalHeight - 10 - 50 - 10, sapp_heightf() - 100 - 10 - HeaderHeight), 10.0);
				os_directory_tree ActiveTree = OS_DirectoryTreeFromPath(M->FrameArena, M->FileListPath, (os_directory_tree_options){});
				
				f32 Height = 50.0;
				M->FileListScrollTarget += UI_Scroll(M->UI).Y*0.4;
				M->FileListScroll = Lerp(M->FileListScroll, M->FileListScrollTarget, 0.2);
				v2f Position = M->FileListPosition;
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
					CurrentStyle.Font = M->Orbitron;
					UI_PushStyle(M->UI, CurrentStyle);
					if(fabs(DistanceFromActiveItem) < Height*10.0)
					{
						ui_signal Signal = UI_TextButton(M->UI, M->Draw, M->FrameArena, FileName, Height*Scale, Position, v4f(1, 1, 1, 1));
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
			else if(TreeViewSignal.Hovered)
			{
				Angle2 += 0.0005;
			}
			else
			{
				Angle2 += 0.001;
			}

			if(TreeViewSignal.Clicked)
			{
				M->FileListPath = Str8Copy(M->PermanentArena, TreeViewSignal.NewPath);
				M->ShowFileList = True;
				M->FileListPosition = TreeViewSignal.ClickedPosition;
			}
			R_PopTransform(M->Render);
		}
		else
		{
			R_PopTransform(M->Render);
		}
		R_PopScissor(M->Render);

		R_PushTransform(M->Render, Orthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0));

		ui_tree_view_signal TreeSignal = {0};
		if(!OS_ValidPath(M->Folder))
		{
			MouseRotation.X += UI_Scroll(M->UI).X;
			MouseRotation.Y += UI_Scroll(M->UI).Y;
			// RECENTLY OPENED
			{
				static u64 TreeHash = 0;
				static v4f *TargetPositions = 0;
				static v4f *Positions = 0;
				static v4f *Velocities = 0;
				static int *Froms = 0;
				static int *Tos = 0;
				static int FromCount = 0;
				static int LinkCount = 0;
				static f32 Temperature = 15.0f;

				// NOTE(Oliver): This wont change but it's fun to hash stuff so why not
				// and it can change later to something that could change
				u64 CurrentHash = M_FNV_HASH_START;
				for(
					str8_list_node *Node = M->RecentlyLoadedFolders.First;
					Node != Null; Node = Node->Next
				)
				{
					CurrentHash = M_FNVHashString(CurrentHash, Node->String);
				}

				int NodeCount = Str8ListCount(M->RecentlyLoadedFolders);
				NodeCount += 1; // For the main orbit node
				
				if(TreeHash != CurrentHash)
				{
					Positions = ArenaPushArrayZero(M->PermanentArena, v4f, NodeCount);
					TargetPositions = ArenaPushArrayZero(M->PermanentArena, v4f, NodeCount);
					Velocities = ArenaPushArrayZero(M->PermanentArena, v4f, NodeCount);

					Froms = ArenaPushArrayZero(M->PermanentArena, int, NodeCount);
					Tos = ArenaPushArrayZero(M->PermanentArena, int, NodeCount);

					for(int I = 0; I < NodeCount-1; I++)
					{
						Froms[I] = 0;
						Tos[I] = I + 1;
					}
					LinkCount = NodeCount-1;

					random32_state RandomState = {0};
					RandomState.State = (u32)M_FNV_HASH_START;
					for(int I = 0; I < NodeCount; I++)
					{
						f32 X = RandomBilateral(&RandomState);
						f32 Y = RandomBilateral(&RandomState);
						f32 Z = RandomBilateral(&RandomState);
						TargetPositions[I] = v4f(X, Y, Z, 0);
						Positions[I] = v4f(X, Y, Z, 0);
						X = RandomBilateral(&RandomState)*0.1;
						Y = RandomBilateral(&RandomState)*0.1;
						Z = RandomBilateral(&RandomState)*0.1;
						Velocities[I] = v4f(X, Y, Z, 0);
					}
					TreeHash = CurrentHash;
				}

				TargetPositions[0] = v4f(0, 0, 0, 0);

				quadtree_header *QuadTree = ArenaPushArrayZero(M->FrameArena, quadtree_header, 1);
				QuadTree->Bounds = ArenaPushArrayZero(M->FrameArena, bounds, NodeCount*4);
				QuadTree->ChildrenCounts = ArenaPushArrayZero(M->FrameArena, int, NodeCount*4);
				QuadTree->Nodes = ArenaPushArrayZero(M->FrameArena, quadrant_node*, NodeCount*4);
				QuadTree->Masses = ArenaPushArrayZero(M->FrameArena, v4f, NodeCount*4);
				QuadTree->Quadrants = ArenaPushArrayZero(M->FrameArena, Quadrant, NodeCount*4);
				
				QuadTree->Bounds[0] = GetBoundsV4F(TargetPositions, NodeCount);
				QuadTree->NextQuadrantIndex = 1;

				for(int I = 0; I < NodeCount; I++)
				{
					QuadTreeBuild(M->FrameArena, QuadTree, 0, TargetPositions[I], 0);
				}

				QuadTree->Masses[0] = QuadTreeCalculateAttributes(QuadTree, 0);

				SimulateForceDirectedGraphQuadTree(
					M->FrameArena,
					Velocities,
					TargetPositions,
					NodeCount,
					Froms,
					Tos,
					LinkCount,
					&Temperature,
					QuadTree,
					&M->Shared
				);

				v3f *PointPositions = ArenaPushArray(M->FrameArena, v3f, NodeCount);
				for(int I = 0; I < NodeCount; I++)
				{
					PointPositions[I].X = Positions[I].X;
					PointPositions[I].Y = Positions[I].Y;
					PointPositions[I].Z = Positions[I].Z;
				}

				v3f *LinePositions = ArenaPushArray(M->FrameArena, v3f, LinkCount*2);
				int LineIndex = 0;
				for(int I = 0; I < LinkCount; I++)
				{
					if(Froms[I] == Tos[I]) continue;
					v4f From = Positions[Froms[I]];
					v4f To = Positions[Tos[I]];
					LinePositions[LineIndex + 0] = v3f(From.X, From.Y, From.Z);
					LinePositions[LineIndex + 1] = v3f(To.X, To.Y, To.Z);
					LineIndex += 2;
				}
				m44f P = Perspective(sapp_widthf()/sapp_heightf(), 70, 0.01, 10000.0);
				v3f At = V3FromV4(Positions[0]);
				v3f From = At;
				From.Z -= 3;

				m44f TranslateToOrigin = Translate(v3f(-From.X, -From.Y, -From.Z));

				m44f RotationY = M44FromYAngle(Angle + 2.0f*M_PI*AngleOffset + MouseRotation.X*0.01);
				m44f RotationZ = M44FromZAngle(Angle*0.5 + 2.0 + 2.0f*M_PI*AngleOffset - MouseRotation.Y*0.01);

				m44f TranslateBack = Translate(v3f(From.X, From.Y, From.Z));

				Angle += 0.002;
				m44f Model = Mul(TranslateToOrigin, Mul(Mul(RotationZ, RotationY), TranslateBack));
				m44f View = M44FLookAt(From, At, v3f(0, 1, 0));
				m44f PV = Mul(Mul(View, Model), P);
				R_PushTransform(M->Render, PV);
				//PointPositions[0] = At;

				str8_list_node *FileNode = M->RecentlyLoadedFolders.First;
				for(int I = 1; I < NodeCount; I++)
				{
					v4f P = Positions[I];
					v4f NodePosition4D = v4f(P.X, P.Y, P.Z, 1.0);
					v4f TransformedPoint = Mul(M44FTranspose(PV), NodePosition4D);
					TransformedPoint.X /= TransformedPoint.W;
					TransformedPoint.Y /= TransformedPoint.W;
					TransformedPoint.X = TransformedPoint.X*0.5 + 0.5;
					TransformedPoint.Y = TransformedPoint.Y*0.5 + 0.5;
					TransformedPoint.X *= sapp_widthf();
					TransformedPoint.Y *= sapp_heightf();

					R_PushTransform(M->Render, Orthographic(0.0, sapp_widthf(), sapp_heightf(), 0.0, -1000.0, 1000.0));					
					v4f LabelBBox = D_Str8BBoxFromHeight(
						M->Orbitron, v2f(TransformedPoint.X + 5, TransformedPoint.Y + 5),
						FileNode->String, 
						50.0
					);
					v2f MP = v2f(M->UI->MousePosition.X, M->UI->MousePosition.Y);
					v4f LabelColour = v4f(0.1, 0.05, 0, 1.0);
					if(
						MP.X >= LabelBBox.X && MP.X < LabelBBox.Z &&
						MP.Y >= LabelBBox.Y && MP.Y < LabelBBox.W)
					{
						LabelColour = v4f(0.5, 0.1, 0, 1);
						if(UI_LeftMouseWentDown(M->UI))
						{
							M->Folder = Str8Copy(M->PermanentArena, FileNode->String);
							M->LookFrom.Z = 500;
							M->LookAt = v3f(0, 0, 0);
						}
					}
					D_Str8(M->Draw, M->FrameArena, FileNode->String,
					.Font = M->Orbitron,
					.HeightInPixels = 50.0f,
					.Colour = LabelColour,
					.Position = v3f(TransformedPoint.X + 5, TransformedPoint.Y + 5, 0.0));
					R_PopTransform(M->Render);
					
					FileNode = FileNode->Next;
				}

				D_LinesArray(M->Draw, M->FrameArena, LinePositions, LineIndex, 5.0, v4f(0.5, 0.1, 0, 0.1));
				D_PointsArray(M->Draw, M->FrameArena, PointPositions, NodeCount, 10.0, v4f(0.1, 0.05, 0, 1.0));
				D_PointsArray(M->Draw, M->FrameArena, PointPositions, NodeCount, 5.0, COLOUR_BACKGROUND);
				R_PopTransform(M->Render);
				v3f Points = v3f(800, 800, 0);

				for(int I = 0; I < NodeCount; I++)
				{
					v4f P = Positions[I];
					v4f Target = TargetPositions[I];
					v3f Result = LerpV3F(v3f(P.X, P.Y, P.Z), v3f(Target.X, Target.Y, Target.Z), 0.1);
					Positions[I] = v4f(Result.X, Result.Y, Result.Z, 0.0);
				}
			}
#if 0
			D_Quad(M->Draw, M->FrameArena, v2f(100.0, sapp_heightf()-300), 500, 200, v4f(0.2, 0.0, 0.0, 1.0));
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

				UI_PushStyle(M->UI, (ui_style){.Font = M->Rajdhani });
				str8 LastModified = OS_GetLastModifiedTime(M->FrameArena, Node->String);
				UI_Label(M->UI, M->Draw, M->FrameArena, LastModified, 120, Add(Pos, v2f(0, 20)), v4f(0.2, 0.01, 0.0, 0.1));
				ui_signal Button = UI_TextButton(M->UI, M->Draw, M->FrameArena, Folder, 60, Pos, v4f(0.3, 0.3, 0.3, 1));
				UI_PopStyle(M->UI);
				if(Button.Clicked)
				{
					M->Folder = Str8Copy(M->PermanentArena, Node->String);
					M->UI->Active = UI_IDFromString(Str8("TreeViewTextBox"));
				}
				Pos.Y -= 100;
			}
#endif
		}

		D_Grid2D(M->Draw, M->FrameArena, 20, 500);

		//D_Str8(M->Draw, M->FrameArena, M->LogoFont, v2f(50, 50), v4f(1.0, 0.05, 0.0, 1), 100, Str8("MAYDAY!"));
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

	M.UI->Font = M.CodeFont;
	M.LookFrom.Z = 500;
	M.LookFromTarget.Z = 70.0;
	M.LookFromTarget.X = -20;

	M.Shared.Arena = ArenaInitialise(GigaBytes(1));

	u64 Start = OS_TimeStampNow();

	DispatchInitialise(&M.Shared, 15);

	//M.Folder = Str8("/");

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
