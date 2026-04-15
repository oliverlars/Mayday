#include <math.h>

#include "fdg.h"
#include "log.h"
#include "maths.h"
#include "memory.h"
#include "misc.h"
#include "os.h"
#include "strings.h"
#include "ui.h"
#include "wave.h"


ui_tree_info TreeInfoFromPath(m_arena *Arena, str8 Path, v3f Size)
{
	ui_tree_info Info = {0};
	Info.Tree = OS_DirectoryTreeFromPath(Arena, Str8Copy(Arena, Path), (os_directory_tree_options){ .FoldersOnly = True});

	Info.NodePositions = ArenaPushArrayZero(Arena, v4f, Info.Tree.Count);
	Info.NodeVelocities = ArenaPushArrayZero(Arena, v4f, Info.Tree.Count);
	Info.NodeInfos = ArenaPushArrayZero(Arena, fdg_node_info, Info.Tree.Count);
	Info.Froms = ArenaPushArrayZero(Arena, int, Info.Tree.Count);
	Info.Tos = ArenaPushArrayZero(Arena, int, Info.Tree.Count);

	m_scratch_arena Scratch = ScratchBegin(&Arena, 1);

	random32_state RandomState = {0};
	RandomState.State = (u32)M_FNV_HASH_START;
	for(os_directory_tree_iterator It = {0};
		OS_DirectoryTreeIterate(Info.Tree, &It);)
	{
		if(It.Current->Type != OS_DIR_ENTRY_FOLDER) continue;
		f32 X = RandomBilateral(&RandomState);
		f32 Y = RandomBilateral(&RandomState);
		f32 Z = RandomBilateral(&RandomState);
		Info.NodePositions[It.Index] = v4f(X, Y, Z, 0);
		Info.NodeInfos[It.Index].Name = It.Current->PathAndName;
		assert(It.Current->Type == OS_DIR_ENTRY_FOLDER);
		Info.NodeInfos[It.Index].IsFolder = It.Current->Type == OS_DIR_ENTRY_FOLDER;
		Info.NodeInfos[It.Index].Depth = It.Depth;

		Info.NodeInfos[It.Index].Radius = 0*Str8UpToLastCharacter(Scratch.Arena, It.Current->PathAndName, '/').Size;

	}

	Info.NodeCount = Info.Tree.Count;

	int LinkCount = 0;
	for(os_directory_tree_iterator It = {0};
		OS_DirectoryTreeIterateWithStack(Arena, Info.Tree, &It);)
	{
		//if(It.Index == 0) continue;
		Info.Froms[LinkCount] = It.ParentIndex;
		Info.Tos[LinkCount] = It.Index;
		LinkCount += 1;
	}
	Info.LinkCount = LinkCount;

	ScratchEnd(Scratch);
	return Info;
}

void UI_TreeView(ui_context *UI, draw_context *Draw, shared_wave_info *Shared,
				 m_arena *Arena, ui_tree *TreeView, str8 Path, v3f Size)
{
	if(!OS_ValidPath(Path)) return;
	u64 Hash = M_FNVHashString(M_FNV_HASH_START, Path);
	if(Hash != TreeView->TreeHash)
	{
		if(!TreeView->Arena)
		{
			TreeView->Arena = ArenaInitialise(MegaBytes(256));
		}

		/*

		NOTE(Oliver): We do this just so we can avoid the jarring position
		reset. It can probably be done better than this but it's fine really

		Save old positions and find the matching index BEFORE resetting the
		arena, since the old Info pointers live in TreeView->Arena.

		*/
		int OldIndex = 0;
		int OldNodeCount = TreeView->Info.NodeCount;
		v4f *OldPositions = Null;
		if(TreeView->Info.NodePositions && OldNodeCount > 0)
		{
			for(int I = 0; I < OldNodeCount; I++)
			{
				if(Str8Equal(TreeView->Info.NodeInfos[I].Name, Path))
				{
					OldIndex = I;
				}
			}
			OldPositions = ArenaPushArray(Arena, v4f, OldNodeCount);
			for(int I = 0; I < OldNodeCount; I++)
			{
				OldPositions[I] = TreeView->Info.NodePositions[I];
			}
		}

		ArenaReset(TreeView->Arena);
		ui_tree_info NextInfo = TreeInfoFromPath(TreeView->Arena, Path, Size);

		if(OldPositions)
		{
			for(int I = 0; I < NextInfo.NodeCount; I++)
			{
				if(I + OldIndex < OldNodeCount)
				{
					NextInfo.NodePositions[I] = OldPositions[I + OldIndex];
				}
			}
		}

		TreeView->Info = NextInfo;
		TreeView->TreeHash = Hash;
		TreeView->Temperature = 15.0;
		TreeView->FocusNodeIndex = 0;
	}

	quadtree_header *QuadTree = ArenaPushArrayZero(Arena, quadtree_header, 1);
	QuadTree->Bounds = ArenaPushArrayZero(Arena, bounds, TreeView->Info.NodeCount*4);
	QuadTree->ChildrenCounts = ArenaPushArrayZero(Arena, int, TreeView->Info.NodeCount*4);
	QuadTree->Nodes = ArenaPushArrayZero(Arena, quadrant_node*, TreeView->Info.NodeCount*4);
	QuadTree->Masses = ArenaPushArrayZero(Arena, v4f, TreeView->Info.NodeCount*4);
	QuadTree->Quadrants = ArenaPushArrayZero(Arena, Quadrant, TreeView->Info.NodeCount*4);

	QuadTree->Bounds[0] = GetBoundsV4F(TreeView->Info.NodePositions, TreeView->Info.NodeCount);
	QuadTree->NextQuadrantIndex = 1;

	u64 FDGBuildStart = OS_TimeStampNow();
	for(int I = 0; I < TreeView->Info.NodeCount; I++)
	{
		QuadTreeBuild(Arena, QuadTree, 0, TreeView->Info.NodePositions[I], 0);
	}
	TreeView->DebugBuildMs = OS_MilliSecondsFromTimeStamp(OS_TimeStampNow() - FDGBuildStart);

	u64 FDGAttrStart = OS_TimeStampNow();
	QuadTree->Masses[0] = QuadTreeCalculateAttributes(QuadTree, 0);
	TreeView->DebugAttributesMs = OS_MilliSecondsFromTimeStamp(OS_TimeStampNow() - FDGAttrStart);

	u64 FDGSimStart = OS_TimeStampNow();
	SimulateForceDirectedGraphQuadTree
	(
		Arena,
		TreeView->Info.NodeVelocities,
		TreeView->Info.NodePositions,
		TreeView->Info.NodeCount,
		TreeView->Info.Froms,
		TreeView->Info.Tos,
		TreeView->Info.LinkCount,
		&TreeView->Temperature,
		QuadTree,
		Shared
	);
	TreeView->DebugSimulateMs = OS_MilliSecondsFromTimeStamp(OS_TimeStampNow() - FDGSimStart);

	// Determine visibility levels: 1=active, 2=above, 3=below
	int FocusIndex = TreeView->FocusNodeIndex;
	int *NodeLevel = ArenaPushArrayZero(Arena, int, TreeView->Info.NodeCount);

	// Find parent of focus
	int ParentOfFocus = -1;
	for(int I = 0; I < TreeView->Info.LinkCount; I++)
	{
		if(TreeView->Info.Tos[I] == FocusIndex && TreeView->Info.Froms[I] != FocusIndex)
		{
			ParentOfFocus = TreeView->Info.Froms[I];
			break;
		}
	}

	// Active level: focus node + its children
	NodeLevel[FocusIndex] = 1;
	b32 *IsActiveChild = ArenaPushArrayZero(Arena, b32, TreeView->Info.NodeCount);
	for(int I = 0; I < TreeView->Info.LinkCount; I++)
	{
		if(TreeView->Info.Froms[I] == FocusIndex && TreeView->Info.Tos[I] != FocusIndex)
		{
			NodeLevel[TreeView->Info.Tos[I]] = 1;
			IsActiveChild[TreeView->Info.Tos[I]] = True;
		}
	}

	// Level above: children of parent (siblings of focus)
	if(ParentOfFocus >= 0)
	{
		if(NodeLevel[ParentOfFocus] == 0) NodeLevel[ParentOfFocus] = 2;
		for(int I = 0; I < TreeView->Info.LinkCount; I++)
		{
			if(TreeView->Info.Froms[I] == ParentOfFocus && TreeView->Info.Tos[I] != ParentOfFocus)
			{
				if(NodeLevel[TreeView->Info.Tos[I]] == 0)
					NodeLevel[TreeView->Info.Tos[I]] = 2;
			}
		}
	}

	// Level below: children of active children (grandchildren)
	for(int I = 0; I < TreeView->Info.LinkCount; I++)
	{
		if(IsActiveChild[TreeView->Info.Froms[I]] && TreeView->Info.Tos[I] != TreeView->Info.Froms[I])
		{
			if(NodeLevel[TreeView->Info.Tos[I]] == 0)
				NodeLevel[TreeView->Info.Tos[I]] = 3;
		}
	}

	// Collect positions per brightness level
	int ActiveCount = 0, DimCount = 0;
	for(int I = 0; I < TreeView->Info.NodeCount; I++)
	{
		if(NodeLevel[I] == 1) ActiveCount++;
		else if(NodeLevel[I] > 1) DimCount++;
	}

	v3f *ActivePositions = ArenaPushArray(Arena, v3f, ActiveCount);
	v3f *DimPositions = ArenaPushArray(Arena, v3f, DimCount);
	int AI = 0, DI = 0;
	for(int I = 0; I < TreeView->Info.NodeCount; I++)
	{
		v4f P = TreeView->Info.NodePositions[I];
		v3f P3 = v3f(P.X, P.Y, P.Z);
		if(NodeLevel[I] == 1) ActivePositions[AI++] = P3;
		else if(NodeLevel[I] > 1) DimPositions[DI++] = P3;
	}

	// Lines: from focus to active children
	int ActiveLineCount = 0;
	for(int I = 0; I < TreeView->Info.LinkCount; I++)
	{
		if(TreeView->Info.Froms[I] == FocusIndex && TreeView->Info.Tos[I] != FocusIndex
		   && NodeLevel[TreeView->Info.Tos[I]] == 1)
			ActiveLineCount++;
	}

	v3f *ActiveLines = ArenaPushArray(Arena, v3f, ActiveLineCount * 2);
	int ALI = 0;
	for(int I = 0; I < TreeView->Info.LinkCount; I++)
	{
		if(TreeView->Info.Froms[I] == FocusIndex && TreeView->Info.Tos[I] != FocusIndex
		   && NodeLevel[TreeView->Info.Tos[I]] == 1)
		{
			v4f FP = TreeView->Info.NodePositions[FocusIndex];
			v4f TP = TreeView->Info.NodePositions[TreeView->Info.Tos[I]];
			ActiveLines[ALI++] = v3f(FP.X, FP.Y, FP.Z);
			ActiveLines[ALI++] = v3f(TP.X, TP.Y, TP.Z);
		}
	}

	// Lines: sibling connections (parent-of-focus to its children)
	int SiblingLineCount = 0;
	if(ParentOfFocus >= 0)
	{
		for(int I = 0; I < TreeView->Info.LinkCount; I++)
		{
			if(TreeView->Info.Froms[I] == ParentOfFocus && TreeView->Info.Tos[I] != ParentOfFocus
			   && NodeLevel[TreeView->Info.Tos[I]] == 2)
				SiblingLineCount++;
		}
	}

	v3f *SiblingLines = ArenaPushArray(Arena, v3f, SiblingLineCount * 2);
	int SLI = 0;
	if(ParentOfFocus >= 0)
	{
		for(int I = 0; I < TreeView->Info.LinkCount; I++)
		{
			if(TreeView->Info.Froms[I] == ParentOfFocus && TreeView->Info.Tos[I] != ParentOfFocus
			   && NodeLevel[TreeView->Info.Tos[I]] == 2)
			{
				v4f FP = TreeView->Info.NodePositions[ParentOfFocus];
				v4f TP = TreeView->Info.NodePositions[TreeView->Info.Tos[I]];
				SiblingLines[SLI++] = v3f(FP.X, FP.Y, FP.Z);
				SiblingLines[SLI++] = v3f(TP.X, TP.Y, TP.Z);
			}
		}
	}

	// Lines: grandchild connections (active children to their children)
	int GrandchildLineCount = 0;
	for(int I = 0; I < TreeView->Info.LinkCount; I++)
	{
		if(IsActiveChild[TreeView->Info.Froms[I]] && TreeView->Info.Tos[I] != TreeView->Info.Froms[I]
		   && NodeLevel[TreeView->Info.Tos[I]] == 3)
			GrandchildLineCount++;
	}

	v3f *GrandchildLines = ArenaPushArray(Arena, v3f, GrandchildLineCount * 2);
	int GLI = 0;
	for(int I = 0; I < TreeView->Info.LinkCount; I++)
	{
		if(IsActiveChild[TreeView->Info.Froms[I]] && TreeView->Info.Tos[I] != TreeView->Info.Froms[I]
		   && NodeLevel[TreeView->Info.Tos[I]] == 3)
		{
			v4f FP = TreeView->Info.NodePositions[TreeView->Info.Froms[I]];
			v4f TP = TreeView->Info.NodePositions[TreeView->Info.Tos[I]];
			GrandchildLines[GLI++] = v3f(FP.X, FP.Y, FP.Z);
			GrandchildLines[GLI++] = v3f(TP.X, TP.Y, TP.Z);
		}
	}

	// Draw dim lines first (behind), then active on top
	D_LinesArray(Draw, Arena, SiblingLines, SiblingLineCount * 2, 1.0, v4f(0.3, 0.08, 0, 0.06));
	D_LinesArray(Draw, Arena, GrandchildLines, GrandchildLineCount * 2, 1.0, v4f(0.3, 0.08, 0, 0.06));
	D_LinesArray(Draw, Arena, ActiveLines, ActiveLineCount * 2, 2.0, v4f(0.5, 0.1, 0, 0.1));
	D_PointsArray(Draw, Arena, ActivePositions, ActiveCount, 5.0, v4f(0.008, 0.00, 0, 1.0));
	D_PointsArray(Draw, Arena, ActivePositions, ActiveCount, 10.0, v4f(0.1, 0.05, 0, 1.0));
}


ui_tree_view_signal UI_TreeViewFolderLabels(
	ui_context *UI, draw_context *Draw, edit_context *Edit,
	d_font Font,
	m_arena *Arena, int ActiveNodeIndex,
	ui_tree *TreeView, m44f TreeTransform, v3f CameraPosition, str8 CurrentPath, editable_buffer* SearchBuffer)
{
	ui_tree_view_signal Result = {0};

	if(!OS_ValidPath(CurrentPath)) return Result;

	ui_textbox_signal SearchSignal = UI_TextBox(Arena, Font, v2f(sapp_widthf()/2.0 - 500, 100), 1000,
												UI, Draw, Edit, 60, Str8("TreeViewTextBox"), SearchBuffer);

	str8 Filter = SearchSignal.String;

	ui_style Style = UI_GetStyle(UI);
	Style.Font = Font;
	UI_PushStyle(UI, Style);
	// Reference distance: camera to focus node (used to normalise per-node depth)
	v4f FocusP = TreeView->Info.NodePositions[TreeView->FocusNodeIndex];
	f32 RefDist = sqrtf(
		(FocusP.X - CameraPosition.X)*(FocusP.X - CameraPosition.X) +
		(FocusP.Y - CameraPosition.Y)*(FocusP.Y - CameraPosition.Y) +
		(FocusP.Z - CameraPosition.Z)*(FocusP.Z - CameraPosition.Z));

	// Determine three-level visibility for labels
	int FocusIdx = TreeView->FocusNodeIndex;
	int ParentOfFocus = -1;
	for(int I = 0; I < TreeView->Info.LinkCount; I++)
	{
		if(TreeView->Info.Tos[I] == FocusIdx && TreeView->Info.Froms[I] != FocusIdx)
		{
			ParentOfFocus = TreeView->Info.Froms[I];
			break;
		}
	}

	// Mark children of focus for grandchild lookup
	b32 *IsActiveChild = ArenaPushArrayZero(Arena, b32, TreeView->Info.NodeCount);
	for(int I = 0; I < TreeView->Info.LinkCount; I++)
	{
		if(TreeView->Info.Froms[I] == FocusIdx && TreeView->Info.Tos[I] != FocusIdx)
		{
			IsActiveChild[TreeView->Info.Tos[I]] = True;
		}
	}

	m_scratch_arena Scratch = ScratchBegin(0, 0);
	for(os_directory_tree_iterator It = {0};
		OS_DirectoryTreeIterateWithStack(Scratch.Arena, TreeView->Info.Tree, &It);)
	{
		// Four-level visibility: focus node, active children, above/siblings, below/grandchildren
		int Level = 0;
		if(It.Index == FocusIdx)
			Level = 4; // the focus node itself (root of current view)
		else if(It.ParentIndex == FocusIdx && It.Index != FocusIdx)
			Level = 1; // active: children of focus
		else if(ParentOfFocus >= 0 && It.ParentIndex == ParentOfFocus && It.Index != ParentOfFocus && It.Index != FocusIdx)
			Level = 2; // above: siblings of focus
		else if(IsActiveChild[It.ParentIndex] && It.Index != It.ParentIndex)
			Level = 3; // below: grandchildren

		if(Level == 0) continue;

		f32 FontSize = 30;
		v4f P = TreeView->Info.NodePositions[It.Index];

		v4f NodePosition4D = v4f(P.X, P.Y, P.Z, 1.0);
		v4f TransformedPoint = Mul(M44FTranspose(TreeTransform), NodePosition4D);
		TransformedPoint.X /= TransformedPoint.W;
		TransformedPoint.Y /= TransformedPoint.W;
		TransformedPoint.X = TransformedPoint.X*0.5 + 0.5;
		TransformedPoint.Y = TransformedPoint.Y*0.5 + 0.5;

		TransformedPoint.X *= sapp_widthf();
		TransformedPoint.Y *= sapp_heightf();
		v4f Colour = v4f(0.1, 0.05, 0, 1.0);
		if(It.Index == ActiveNodeIndex)
		{
			Colour = v4f(0.8, 0.6, 0.4, 1.0);
			FontSize = 30;
		}
		
		TransformedPoint.X += 15.0f;
		TransformedPoint.Y += 5.0f;

		// Fade colour based on camera distance (not font size — keeps text readable)
		f32 CamDist = sqrtf(
			(P.X - CameraPosition.X)*(P.X - CameraPosition.X) +
			(P.Y - CameraPosition.Y)*(P.Y - CameraPosition.Y) +
			(P.Z - CameraPosition.Z)*(P.Z - CameraPosition.Z));
		f32 RelDist = CamDist / (RefDist + 0.001f);
		f32 DepthFade = Max(0.2f, Min(1.0f, 1.0f / (RelDist * RelDist)));
		Colour.W *= DepthFade;

		// Focus node: larger, brighter, distinct
		if(Level == 4)
		{
			Colour = v4f(0.6, 0.35, 0.1, 1.0);
			FontSize = 40;
		}
		// Non-active levels: smaller and dimmed but still readable
		else if(Level > 1)
		{
			Colour.W *= 0.6f;
			FontSize *= 0.6f;
		}

		{
			str8 FolderName = Str8FromLastCharacter(Arena, TreeView->Info.NodeInfos[It.Index].Name, '/');
			if(Filter.Size == 0 || Str8FuzzyMatch(FolderName, Filter))
			{

				ui_signal Button = UI_TextHighlightButton(UI, Draw,
					Arena, Str8PushF(Arena, "?S##?u32", FolderName, It.Index),
					FontSize,
					v2f(TransformedPoint.X, TransformedPoint.Y), Colour, v4f(1, 1, 1, 1),
					Str8FuzzyMatchIndices(Scratch.Arena, FolderName, Filter));
				Result.Hovered |= Button.Hover;

				if(Button.Clicked || (SearchSignal.Submitted && Filter.Size > 0))
				{
					Result.NodeIndex = It.Index;
					Result.NodeDepth = It.Depth;
					Result.Clicked = True;
					Result.ClickedPosition = v2f(TransformedPoint.X, TransformedPoint.Y);
					Result.NewPath = Str8Copy(TreeView->Arena, TreeView->Info.NodeInfos[It.Index].Name);
				}
			}
		}

		if(It.ParentIndex == ActiveNodeIndex)
		{
			FontSize = 30;
			str8 FileName = Str8FromLastCharacter(Arena, TreeView->Info.NodeInfos[It.Index].Name, '/');
			if(!TreeView->Info.NodeInfos[It.Index].IsFolder)
			{
				if(Filter.Size == 0 || Str8FuzzyMatch(FileName, Filter))
				{
					ui_signal Button = UI_TextButton(UI, Draw,
						Arena, Str8PushF(Arena, "?S##?u32", FileName, It.Index),
						FontSize,
						v2f(TransformedPoint.X, TransformedPoint.Y), Colour);
					Result.Hovered |= Button.Hover;
					if(Button.Clicked || (SearchSignal.Submitted && Filter.Size > 0))
					{
						Result.Clicked = True;
						Result.FileToLoad = Str8Copy(TreeView->Arena, TreeView->Info.NodeInfos[It.Index].Name);
					}
				}
			}
		}
	}

	if(UI_KeyDown(UI, SAPP_KEYCODE_RIGHT, 0))
	{
		Result.NodeIndex = ActiveNodeIndex + 1;
	}

	if(UI_KeyDown(UI, SAPP_KEYCODE_LEFT, 0))
	{
		Result.NodeIndex = Max(ActiveNodeIndex - 1, 0);
	}

	ScratchEnd(Scratch);

	UI_PopStyle(UI);

	return Result;
}
