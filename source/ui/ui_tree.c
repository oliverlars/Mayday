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

// FDG side is now drawn on a black background post-rotation,
// so all the formerly-black UI elements become red.
#define FDG_FG_R (0.753f)
#define FDG_FG_G (0.036f)
#define FDG_FG_B (0.0f)


// Build a flat FDG from a list of recently opened paths:
// one root node + one child per path (no intermediate components)
ui_tree_info TreeInfoFromPaths(m_arena *Arena, str8_list Paths)
{
	ui_tree_info Info = {0};

	// Count valid paths
	int PathCount = 0;
	for(str8_list_node *Node = Paths.First; Node; Node = Node->Next)
	{
		if(Node->String.Size > 0) PathCount++;
	}

	int TotalNodes = 1 + PathCount; // root + one node per path

	os_directory_entry *Root = ArenaPushArrayZero(Arena, os_directory_entry, 1);
	Root->PathAndName = Str8("Recent");
	Root->Type = OS_DIR_ENTRY_FOLDER;

	for(str8_list_node *Node = Paths.First; Node; Node = Node->Next)
	{
		if(Node->String.Size == 0) continue;

		os_directory_entry *Entry = ArenaPushArrayZero(Arena, os_directory_entry, 1);
		Entry->PathAndName = Str8Copy(Arena, Node->String);
		Entry->Type = OS_DIR_ENTRY_FOLDER;
		Entry->Parent = Root;

		if(!Root->First)
		{
			Root->First = Entry;
			Root->Last = Entry;
		}
		else
		{
			Root->Last->Next = Entry;
			Root->Last = Entry;
		}
	}

	Info.Tree.Root = Root;
	Info.Tree.Count = TotalNodes;

	Info.NodePositions = ArenaPushArrayZero(Arena, v4f, TotalNodes);
	Info.NodeVelocities = ArenaPushArrayZero(Arena, v4f, TotalNodes);
	Info.NodeInfos = ArenaPushArrayZero(Arena, fdg_node_info, TotalNodes);
	Info.NodeOpacity = ArenaPushArrayZero(Arena, f32, TotalNodes);
	Info.Froms = ArenaPushArrayZero(Arena, int, TotalNodes);
	Info.Tos = ArenaPushArrayZero(Arena, int, TotalNodes);

	random32_state RandomState = {0};
	RandomState.State = (u32)M_FNV_HASH_START;
	for(os_directory_tree_iterator It = {0};
		OS_DirectoryTreeIterate(Info.Tree, &It);)
	{
		f32 X = RandomBilateral(&RandomState);
		f32 Y = RandomBilateral(&RandomState);
		f32 Z = RandomBilateral(&RandomState);
		Info.NodePositions[It.Index] = v4f(X, Y, Z, 0);
		Info.NodeInfos[It.Index].Name = It.Current->PathAndName;
		Info.NodeInfos[It.Index].IsFolder = True;
		Info.NodeInfos[It.Index].Depth = It.Depth;
	}

	Info.NodeCount = TotalNodes;

	int LinkCount = 0;
	for(os_directory_tree_iterator It = {0};
		OS_DirectoryTreeIterateWithStack(Arena, Info.Tree, &It);)
	{
		Info.Froms[LinkCount] = It.ParentIndex;
		Info.Tos[LinkCount] = It.Index;
		LinkCount += 1;
	}
	Info.LinkCount = LinkCount;

	return Info;
}

ui_tree_info TreeInfoFromPath(m_arena *Arena, str8 Path, v3f Size)
{
	ui_tree_info Info = {0};
	Info.Tree = OS_DirectoryTreeFromPath(Arena, Str8Copy(Arena, Path), (os_directory_tree_options){ .FoldersOnly = True});

	Info.NodePositions = ArenaPushArrayZero(Arena, v4f, Info.Tree.Count);
	Info.NodeVelocities = ArenaPushArrayZero(Arena, v4f, Info.Tree.Count);
	Info.NodeInfos = ArenaPushArrayZero(Arena, fdg_node_info, Info.Tree.Count);
	Info.NodeOpacity = ArenaPushArrayZero(Arena, f32, Info.Tree.Count);
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
		TreeView->ActiveLabelFontSize = 30.0f;
		TreeView->ActiveLabelFontSizeTarget = 30.0f;
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

	// Collect visible points using NodeOpacity
	int VisiblePointCount = 0;
	for(int I = 0; I < TreeView->Info.NodeCount; I++)
	{
		if(TreeView->Info.NodeOpacity[I] > 0.01f) VisiblePointCount++;
	}

	v3f *VisiblePositions = ArenaPushArray(Arena, v3f, VisiblePointCount);
	f32 *VisibleOpacities = ArenaPushArray(Arena, f32, VisiblePointCount);
	int VPI = 0;
	for(int I = 0; I < TreeView->Info.NodeCount; I++)
	{
		if(TreeView->Info.NodeOpacity[I] > 0.01f)
		{
			v4f P = TreeView->Info.NodePositions[I];
			VisiblePositions[VPI] = v3f(P.X, P.Y, P.Z);
			VisibleOpacities[VPI] = TreeView->Info.NodeOpacity[I];
			VPI++;
		}
	}

	// Lines: draw any link where both endpoints have opacity, using min opacity
	int VisibleLineCount = 0;
	for(int I = 0; I < TreeView->Info.LinkCount; I++)
	{
		int From = TreeView->Info.Froms[I];
		int To = TreeView->Info.Tos[I];
		if(From == To) continue;
		f32 FromOp = TreeView->Info.NodeOpacity[From];
		f32 ToOp = TreeView->Info.NodeOpacity[To];
		if(FromOp > 0.01f && ToOp > 0.01f) VisibleLineCount++;
	}

	v3f *Lines = ArenaPushArray(Arena, v3f, VisibleLineCount * 2);
	f32 *LineOpacities = ArenaPushArray(Arena, f32, VisibleLineCount);
	int LI = 0;
	for(int I = 0; I < TreeView->Info.LinkCount; I++)
	{
		int From = TreeView->Info.Froms[I];
		int To = TreeView->Info.Tos[I];
		if(From == To) continue;
		f32 FromOp = TreeView->Info.NodeOpacity[From];
		f32 ToOp = TreeView->Info.NodeOpacity[To];
		if(FromOp > 0.01f && ToOp > 0.01f)
		{
			v4f FP = TreeView->Info.NodePositions[From];
			v4f TP = TreeView->Info.NodePositions[To];
			Lines[LI * 2 + 0] = v3f(FP.X, FP.Y, FP.Z);
			Lines[LI * 2 + 1] = v3f(TP.X, TP.Y, TP.Z);
			LineOpacities[LI] = Min(FromOp, ToOp);
			LI++;
		}
	}

	// Draw lines grouped by opacity (batch into a few alpha levels to avoid too many draw calls)
	// Simple approach: draw all with average opacity scaled colour
	for(int I = 0; I < VisibleLineCount; I++)
	{
		f32 Thickness = 1.0f + LineOpacities[I];
		D_LinesArray(Draw, Arena, &Lines[I * 2], 2, Thickness, v4f(FDG_FG_R, FDG_FG_G, FDG_FG_B, 0.4));
	}

	D_PointsArray(Draw, Arena, VisiblePositions, VisiblePointCount, 6.0, v4f(FDG_FG_R, FDG_FG_G, FDG_FG_B, 1.0));
	D_PointsArray(Draw, Arena, VisiblePositions, VisiblePointCount, 12.0, v4f(FDG_FG_R, FDG_FG_G, FDG_FG_B, 1.0));
}


void UI_TreeViewFromPaths(ui_context *UI, draw_context *Draw, shared_wave_info *Shared,
						  m_arena *Arena, ui_tree *TreeView, str8_list Paths)
{
	if(Paths.First == Null) return;

	// Hash the path list to detect changes
	u64 Hash = M_FNV_HASH_START;
	for(str8_list_node *Node = Paths.First; Node; Node = Node->Next)
	{
		Hash = M_FNVHashString(Hash, Node->String);
	}

	if(Hash != TreeView->TreeHash)
	{
		if(!TreeView->Arena)
		{
			TreeView->Arena = ArenaInitialise(MegaBytes(64));
		}
		ArenaReset(TreeView->Arena);

		TreeView->Info = TreeInfoFromPaths(TreeView->Arena, Paths);
		TreeView->TreeHash = Hash;
		TreeView->Temperature = 15.0;
		TreeView->FocusNodeIndex = 0;
		TreeView->ActiveLabelFontSize = 30.0f;
		TreeView->ActiveLabelFontSizeTarget = 30.0f;
	}

	// FDG simulation (same as UI_TreeView)
	quadtree_header *QuadTree = ArenaPushArrayZero(Arena, quadtree_header, 1);
	QuadTree->Bounds = ArenaPushArrayZero(Arena, bounds, TreeView->Info.NodeCount*4);
	QuadTree->ChildrenCounts = ArenaPushArrayZero(Arena, int, TreeView->Info.NodeCount*4);
	QuadTree->Nodes = ArenaPushArrayZero(Arena, quadrant_node*, TreeView->Info.NodeCount*4);
	QuadTree->Masses = ArenaPushArrayZero(Arena, v4f, TreeView->Info.NodeCount*4);
	QuadTree->Quadrants = ArenaPushArrayZero(Arena, Quadrant, TreeView->Info.NodeCount*4);

	QuadTree->Bounds[0] = GetBoundsV4F(TreeView->Info.NodePositions, TreeView->Info.NodeCount);
	QuadTree->NextQuadrantIndex = 1;

	for(int I = 0; I < TreeView->Info.NodeCount; I++)
	{
		QuadTreeBuild(Arena, QuadTree, 0, TreeView->Info.NodePositions[I], 0);
	}
	QuadTree->Masses[0] = QuadTreeCalculateAttributes(QuadTree, 0);

	SimulateForceDirectedGraphQuadTree(
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

	// Visibility + rendering (same as UI_TreeView)
	int FocusIndex = TreeView->FocusNodeIndex;
	int *NodeLevel = ArenaPushArrayZero(Arena, int, TreeView->Info.NodeCount);

	int ParentOfFocus = -1;
	for(int I = 0; I < TreeView->Info.LinkCount; I++)
	{
		if(TreeView->Info.Tos[I] == FocusIndex && TreeView->Info.Froms[I] != FocusIndex)
		{
			ParentOfFocus = TreeView->Info.Froms[I];
			break;
		}
	}

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

	for(int I = 0; I < TreeView->Info.LinkCount; I++)
	{
		if(IsActiveChild[TreeView->Info.Froms[I]] && TreeView->Info.Tos[I] != TreeView->Info.Froms[I])
		{
			if(NodeLevel[TreeView->Info.Tos[I]] == 0)
				NodeLevel[TreeView->Info.Tos[I]] = 3;
		}
	}

	// Opacity-driven points and lines
	int VisiblePointCount = 0;
	for(int I = 0; I < TreeView->Info.NodeCount; I++)
	{
		if(TreeView->Info.NodeOpacity[I] > 0.01f) VisiblePointCount++;
	}

	v3f *VisiblePositions = ArenaPushArray(Arena, v3f, VisiblePointCount);
	int VPI = 0;
	for(int I = 0; I < TreeView->Info.NodeCount; I++)
	{
		if(TreeView->Info.NodeOpacity[I] > 0.01f)
		{
			v4f P = TreeView->Info.NodePositions[I];
			VisiblePositions[VPI++] = v3f(P.X, P.Y, P.Z);
		}
	}

	int VisibleLineCount = 0;
	for(int I = 0; I < TreeView->Info.LinkCount; I++)
	{
		int From = TreeView->Info.Froms[I];
		int To = TreeView->Info.Tos[I];
		if(From == To) continue;
		if(TreeView->Info.NodeOpacity[From] > 0.01f && TreeView->Info.NodeOpacity[To] > 0.01f)
			VisibleLineCount++;
	}

	v3f *VisLines = ArenaPushArray(Arena, v3f, VisibleLineCount * 2);
	f32 *LineOps = ArenaPushArray(Arena, f32, VisibleLineCount);
	int LI = 0;
	for(int I = 0; I < TreeView->Info.LinkCount; I++)
	{
		int From = TreeView->Info.Froms[I];
		int To = TreeView->Info.Tos[I];
		if(From == To) continue;
		f32 FromOp = TreeView->Info.NodeOpacity[From];
		f32 ToOp = TreeView->Info.NodeOpacity[To];
		if(FromOp > 0.01f && ToOp > 0.01f)
		{
			v4f FP = TreeView->Info.NodePositions[From];
			v4f TP = TreeView->Info.NodePositions[To];
			VisLines[LI * 2 + 0] = v3f(FP.X, FP.Y, FP.Z);
			VisLines[LI * 2 + 1] = v3f(TP.X, TP.Y, TP.Z);
			LineOps[LI] = Min(FromOp, ToOp);
			LI++;
		}
	}

	for(int I = 0; I < VisibleLineCount; I++)
	{
		f32 Thickness = 1.0f + LineOps[I];
		D_LinesArray(Draw, Arena, &VisLines[I * 2], 2, Thickness, v4f(FDG_FG_R, FDG_FG_G, FDG_FG_B, 0.4));
	}

	D_PointsArray(Draw, Arena, VisiblePositions, VisiblePointCount, 6.0, v4f(FDG_FG_R, FDG_FG_G, FDG_FG_B, 1.0));
	D_PointsArray(Draw, Arena, VisiblePositions, VisiblePointCount, 12.0, v4f(FDG_FG_R, FDG_FG_G, FDG_FG_B, 1.0));
}


ui_tree_view_signal UI_TreeViewFolderLabels(
	ui_context *UI, draw_context *Draw, edit_context *Edit,
	d_font Font, d_font BackdropFont,
	m_arena *Arena, int ActiveNodeIndex,
	ui_tree *TreeView, m44f TreeTransform, v3f CameraPosition, str8 CurrentPath, editable_buffer* SearchBuffer)
{
	ui_tree_view_signal Result = {0};

	if(!OS_ValidPath(CurrentPath)) return Result;

	v4i PanelScissor = R_CurrentScissor(Draw->Render);

	ui_style Style = UI_GetStyle(UI);
	Style.Font = BackdropFont;
	UI_PushStyle(UI, Style);

	// Breadcrumb: walk from focus node to root, then render left-to-right
	{
		int Chain[64];
		int ChainLen = 0;
		int WalkIdx = TreeView->FocusNodeIndex;
		Chain[ChainLen++] = WalkIdx;
		for(int Safety = 0; Safety < 64; Safety++)
		{
			int Parent = -1;
			for(int I = 0; I < TreeView->Info.LinkCount; I++)
			{
				if(TreeView->Info.Tos[I] == WalkIdx && TreeView->Info.Froms[I] != WalkIdx)
				{
					Parent = TreeView->Info.Froms[I];
					break;
				}
			}
			if(Parent < 0) break;
			Chain[ChainLen++] = Parent;
			WalkIdx = Parent;
		}
		// Reverse so root is first
		for(int I = 0; I < ChainLen / 2; I++)
		{
			int Tmp = Chain[I];
			Chain[I] = Chain[ChainLen - 1 - I];
			Chain[ChainLen - 1 - I] = Tmp;
		}

		f32 BcX = (f32)PanelScissor.X + 20.0f;
		f32 BcY = (f32)PanelScissor.Y + (f32)PanelScissor.W - 50.0f;
		f32 BcFontSize = 50.0f;

		for(int I = 0; I < ChainLen; I++)
		{
			str8 ComponentName = Str8FromLastCharacter(Arena,
				TreeView->Info.NodeInfos[Chain[I]].Name, '/');
			if(ComponentName.Size == 0)
				ComponentName = TreeView->Info.NodeInfos[Chain[I]].Name;

			if(I > 0)
			{
				f32 CharW = (f32)UI_GetTextWidth(BackdropFont, Draw, Str8("A"), BcFontSize);
				f32 SepTotal = CharW * 1.5f;
				str8 Sep = Str8(">");
				f32 SepW = (f32)UI_GetTextWidth(BackdropFont, Draw, Sep, BcFontSize);
				f32 Pad = (SepTotal - SepW) / 2.0f;
				BcX += Pad;
				D_Str8(Draw, Arena, Sep,
					.Font = BackdropFont, .Position = V3FromV2(v2f(BcX, BcY)),
					.Colour = v4f(FDG_FG_R, FDG_FG_G, FDG_FG_B, 1.0),
					.HeightInPixels = BcFontSize);
				BcX += SepW + Pad;
			}

			b32 IsLast = (I == ChainLen - 1);
			v4f BcColour = IsLast
				? v4f(FDG_FG_R, FDG_FG_G, FDG_FG_B, 1.0)
				: v4f(FDG_FG_R, FDG_FG_G, FDG_FG_B, 1.0);

			str8 BcLabel = Str8PushF(Arena, "?S##bc?u32", ComponentName, Chain[I]);
			ui_signal BcSignal = UI_TextButton(UI, Draw, Arena,
				BcLabel, BcFontSize, v2f(BcX, BcY), BcColour);

			if(BcSignal.Clicked && !IsLast)
			{
				Result.NodeIndex = Chain[I];
				Result.NodeDepth = TreeView->Info.NodeInfos[Chain[I]].Depth;
				Result.Clicked = True;
				Result.NewPath = Str8Copy(TreeView->Arena,
					TreeView->Info.NodeInfos[Chain[I]].Name);
			}

			BcX += (f32)UI_GetTextWidth(BackdropFont, Draw, ComponentName, BcFontSize);
		}
	}

	ui_textbox_signal SearchSignal = {0};
	str8 Filter = {0};
	if(SearchBuffer)
	{
		f32 SearchBoxWidth = 600;
		f32 SearchBoxX = PanelScissor.X + (PanelScissor.Z - SearchBoxWidth) * 0.5f;
		f32 SearchBoxY = PanelScissor.Y + 45;
		SearchSignal = UI_TextBox(Arena, Font, v2f(SearchBoxX, SearchBoxY), SearchBoxWidth,
													UI, Draw, Edit, 40, Str8("TreeViewTextBox"), SearchBuffer);
		Filter = SearchSignal.String;
	}
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

	// Compute per-node target opacity and lerp toward it
	f32 LerpRate = 0.08f;
	for(int I = 0; I < TreeView->Info.NodeCount; I++)
	{
		// Determine target opacity based on level
		int Level = 0;
		if(I == FocusIdx)
			Level = 4;
		else if(ParentOfFocus >= 0 && I == ParentOfFocus)
			Level = 2; // parent of focus is visible but dimmed
		else
		{
			for(int L = 0; L < TreeView->Info.LinkCount; L++)
			{
				if(TreeView->Info.Tos[L] == I)
				{
					int Parent = TreeView->Info.Froms[L];
					if(Parent == FocusIdx) { Level = 1; break; }
					else if(ParentOfFocus >= 0 && Parent == ParentOfFocus) { Level = 2; break; }
					else if(IsActiveChild[Parent] && I != Parent) { Level = 3; break; }
				}
			}
		}

		f32 TargetOpacity;
		if(Level == 4) TargetOpacity = 1.0f;
		else if(Level == 1) TargetOpacity = 1.0f;
		else if(Level > 1) TargetOpacity = 0.6f;
		else TargetOpacity = 0.0f;

		TreeView->Info.NodeOpacity[I] += (TargetOpacity - TreeView->Info.NodeOpacity[I]) * LerpRate;
	}

	m_scratch_arena Scratch = ScratchBegin(0, 0);
	int HoveredNodeIndex = -1;
	for(os_directory_tree_iterator It = {0};
		OS_DirectoryTreeIterateWithStack(Scratch.Arena, TreeView->Info.Tree, &It);)
	{
		f32 Opacity = TreeView->Info.NodeOpacity[It.Index];
		if(Opacity < 0.01f) continue;

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

		f32 FontSize = 45;
		v4f P = TreeView->Info.NodePositions[It.Index];

		v4f NodePosition4D = v4f(P.X, P.Y, P.Z, 1.0);
		v4f TransformedPoint = Mul(M44FTranspose(TreeTransform), NodePosition4D);
		TransformedPoint.X /= TransformedPoint.W;
		TransformedPoint.Y /= TransformedPoint.W;
		TransformedPoint.X = TransformedPoint.X*0.5 + 0.5;
		TransformedPoint.Y = TransformedPoint.Y*0.5 + 0.5;

		TransformedPoint.X *= sapp_widthf();
		TransformedPoint.Y *= sapp_heightf();
		v4f Colour = v4f(FDG_FG_R, FDG_FG_G, FDG_FG_B, 1.0);
		if(It.Index == ActiveNodeIndex)
		{
			// Lerp the active label font size toward its target
			if(TreeView->ActiveLabelFontSize < 1.0f) TreeView->ActiveLabelFontSize = 45.0f;
			if(TreeView->ActiveLabelFontSizeTarget < 1.0f) TreeView->ActiveLabelFontSizeTarget = 45.0f;
			TreeView->ActiveLabelFontSize += (TreeView->ActiveLabelFontSizeTarget - TreeView->ActiveLabelFontSize) * 0.12f;
			FontSize = TreeView->ActiveLabelFontSize;
			Colour = v4f(FDG_FG_R, FDG_FG_G, FDG_FG_B, 1.0);
			Result.ActiveScreenPosition = v2f(TransformedPoint.X + 15.0f, TransformedPoint.Y + 5.0f);
		}

		TransformedPoint.X += 15.0f;
		TransformedPoint.Y += 5.0f;

		// Fade based on camera distance for depth
		f32 CamDist = sqrtf(
			(P.X - CameraPosition.X)*(P.X - CameraPosition.X) +
			(P.Y - CameraPosition.Y)*(P.Y - CameraPosition.Y) +
			(P.Z - CameraPosition.Z)*(P.Z - CameraPosition.Z));
		f32 RelDist = CamDist / (RefDist + 0.001f);
		f32 DepthFade = Max(0.15f, Min(1.0f, 1.0f / (RelDist * RelDist)));
		Colour.W *= DepthFade;

		// Focus node: larger, distinct
		if(Level == 4)
		{
			Colour = v4f(FDG_FG_R, FDG_FG_G, FDG_FG_B, Opacity);
			FontSize = 60;
		}
		// Non-active levels: depth-faded
		else if(Level > 1)
		{
			Colour.W *= Opacity;
		}
		else
		{
			Colour.W *= Opacity;
		}

		{
			str8 FolderName = Str8FromLastCharacter(Arena, TreeView->Info.NodeInfos[It.Index].Name, '/');
			if(FolderName.Size == 0) FolderName = TreeView->Info.NodeInfos[It.Index].Name;
			str8 LabelString = Str8PushF(Arena, "?S##?u32", FolderName, It.Index);
			if(Filter.Size == 0 || Str8FuzzyMatch(FolderName, Filter))
			{
				// When filtering: non-matching chars dimmed, matching chars bright red (stand out on black bg)
				v4f LabelColour = (Filter.Size > 0) ? v4f(FDG_FG_R * 0.35f, FDG_FG_G * 0.35f, FDG_FG_B * 0.35f, 1.0) : Colour;
				v4f HighlightCol = v4f(FDG_FG_R, FDG_FG_G, FDG_FG_B, 1.0);
				ui_signal Button = UI_TextHighlightButton(UI, Draw,
					Arena, LabelString,
					FontSize,
					v2f(TransformedPoint.X, TransformedPoint.Y), LabelColour, HighlightCol,
					Str8FuzzyMatchIndices(Scratch.Arena, FolderName, Filter));
				Result.Hovered |= Button.Hover;
				if(Button.Hover) HoveredNodeIndex = It.Index;

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
			FontSize = 45;
			str8 FileName = Str8FromLastCharacter(Arena, TreeView->Info.NodeInfos[It.Index].Name, '/');
			if(FileName.Size == 0) FileName = TreeView->Info.NodeInfos[It.Index].Name;
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

	// Fade backdrop opacity in/out based on hover
	{
		f32 BackdropTarget = (HoveredNodeIndex >= 0) ? 1.0f : 0.0f;
		TreeView->BackdropOpacity += (BackdropTarget - TreeView->BackdropOpacity) * 0.1f;
	}

	// Large faint backdrop text for hovered folder — bottom-left of panel
	{
		if(HoveredNodeIndex >= 0)
		{
			str8 Name = Str8FromLastCharacter(Arena,
				TreeView->Info.NodeInfos[HoveredNodeIndex].Name, '/');
			if(Name.Size == 0)
				Name = TreeView->Info.NodeInfos[HoveredNodeIndex].Name;

			// Cache into persistent buffer on the struct
			s64 CopyLen = Name.Size < 255 ? Name.Size : 255;
			for(s64 I = 0; I < CopyLen; I++) TreeView->BackdropNameBuffer[I] = Name.Bytes[I];
			TreeView->BackdropName = (str8){ .Bytes = TreeView->BackdropNameBuffer, .Size = CopyLen };
			TreeView->BackdropStats = OS_DirectoryStats(TreeView->Info.NodeInfos[HoveredNodeIndex].Name);
		}

		if(TreeView->BackdropName.Size > 0 && TreeView->BackdropOpacity > 0.005f)
		{
			f32 InfoX = (f32)PanelScissor.X + 30.0f;
			f32 InfoAlpha = 0.5f * TreeView->BackdropOpacity;

			// Stats at the bottom-left of the panel, above the search box
			f32 StatFontSize = 48.0f;
			f32 StatY = (f32)PanelScissor.Y + 100.0f;

			// Size
			str8 SizeLine;
			if(TreeView->BackdropStats.TotalSizeBytes > 1024 * 1024)
				SizeLine = Str8PushF(Arena, "?s32 MB", (int)(TreeView->BackdropStats.TotalSizeBytes / (1024 * 1024)));
			else if(TreeView->BackdropStats.TotalSizeBytes > 1024)
				SizeLine = Str8PushF(Arena, "?s32 KB", (int)(TreeView->BackdropStats.TotalSizeBytes / 1024));
			else
				SizeLine = Str8PushF(Arena, "?s32 bytes", (int)TreeView->BackdropStats.TotalSizeBytes);
			D_Str8(Draw, Arena, SizeLine,
				.Font = BackdropFont, .Position = V3FromV2(v2f(InfoX, StatY)),
				.Colour = v4f(FDG_FG_R, FDG_FG_G, FDG_FG_B, InfoAlpha),
				.HeightInPixels = StatFontSize);
			StatY += StatFontSize + 4.0f;

			// Folders
			str8 FolderLine = Str8PushF(Arena, "?s32 folders", TreeView->BackdropStats.FolderCount);
			D_Str8(Draw, Arena, FolderLine,
				.Font = BackdropFont, .Position = V3FromV2(v2f(InfoX, StatY)),
				.Colour = v4f(FDG_FG_R, FDG_FG_G, FDG_FG_B, InfoAlpha),
				.HeightInPixels = StatFontSize);
			StatY += StatFontSize + 4.0f;

			// Files
			str8 FileLine = Str8PushF(Arena, "?s32 files", TreeView->BackdropStats.FileCount);
			D_Str8(Draw, Arena, FileLine,
				.Font = BackdropFont, .Position = V3FromV2(v2f(InfoX, StatY)),
				.Colour = v4f(FDG_FG_R, FDG_FG_G, FDG_FG_B, InfoAlpha),
				.HeightInPixels = StatFontSize);
			StatY += StatFontSize + 15.0f;

			// Large folder name above stats
			f32 InfoFontSize = 180.0f;
			D_Str8(Draw, Arena, TreeView->BackdropName,
				.Font = BackdropFont, .Position = V3FromV2(v2f(InfoX, StatY)),
				.Colour = v4f(FDG_FG_R, FDG_FG_G, FDG_FG_B, InfoAlpha),
				.HeightInPixels = InfoFontSize);
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
