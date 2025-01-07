#include "log.h"
#include "maths.h"
#include "memory.h"
#include "misc.h"
#include "os.h"
#include "strings.h"
#include "ui.h"
#include "wave.h"


ui_tree_info TreeInfoFromPath(m_arena *Arena, str8 Path, v2f Size)
{
	ui_tree_info Info = {0};
	Info.Tree = OS_DirectoryTreeFromPath(Arena, Str8Copy(Arena, Path));

	Info.Nodes = (fdg_node*)ArenaPushArrayZero(Arena, fdg_node, Info.Tree.Count);
	Info.Links = (fdg_link*)ArenaPushArrayZero(Arena, fdg_link, Info.Tree.Count);

	m_scratch_arena Scratch = ScratchBegin(&Arena, 1);

	random32_state RandomState = {0};
	RandomState.State = (u32)M_FNV_HASH_START;
	for(os_directory_tree_iterator It = {0};
		OS_DirectoryTreeIterateWithStack(Arena, Info.Tree, &It);)
	{
		f32 X = (0.5*RandomBilateral(&RandomState) + 0.5)*Size.X*0.2 + Size.X/2.0;
		f32 Y = (0.5*RandomBilateral(&RandomState) + 0.5)*Size.Y*0.2 + Size.Y/5.0;
		Info.Nodes[It.Index].TargetPosition = v2f(X, Y);
		Info.Nodes[It.Index].TargetPosition = Info.Nodes[It.Index].TargetPosition;
		Info.Nodes[It.Index].Position = Info.Nodes[It.Index].TargetPosition;
		Info.Nodes[It.Index].Name = It.Current->PathAndName;
		Info.Nodes[It.Index].IsFolder = It.Current->Type == OS_DIR_ENTRY_FOLDER;
	}

	for(int I = 0; I < Info.Tree.Count; I++)
	{
		LogInfo("ENTRY: ?S", Info.Nodes[I].Name);
	}

	Info.NodeCount = Info.Tree.Count;

	int LinkCount = 0;
	for(os_directory_tree_iterator It = {0};
		OS_DirectoryTreeIterateWithStack(Arena, Info.Tree, &It);)
	{
		Info.Links[LinkCount].From = It.ParentIndex;
		Info.Links[LinkCount].To = It.Index;
		LinkCount += 1;
	}
	Info.LinkCount = LinkCount;

	ScratchEnd(Scratch);
	return Info;
}

void UI_TreeView(ui_context *UI, draw_context *Draw, shared_wave_info *Shared,
				 m_arena *Arena, ui_tree *TreeView, str8 Path, v2f Size)
{
	u64 Hash = M_FNVHashString(M_FNV_HASH_START, Path);
	if(Hash != TreeView->TreeHash)
	{
		if(!TreeView->Arena)
		{
			TreeView->Arena = ArenaInitialise(MegaBytes(256));
		}
		//ArenaReset(TreeView->Arena);
		TreeView->Info = TreeInfoFromPath(TreeView->Arena, Path, Size);
		TreeView->TreeHash = Hash;
		TreeView->Temperature = 100.0;
	}

	quadtree_header *QuadTree = ArenaPushArrayZero(Arena, quadtree_header, 1);
	QuadTree->Bounds = ArenaPushArrayZero(Arena, bounds, TreeView->Info.NodeCount*4);
	QuadTree->ChildrenCounts = ArenaPushArrayZero(Arena, int, TreeView->Info.NodeCount*4);
	QuadTree->Nodes = ArenaPushArrayZero(Arena, quadrant_node*, TreeView->Info.NodeCount*4);
	QuadTree->Masses = ArenaPushArrayZero(Arena, v2f, TreeView->Info.NodeCount*4);
	QuadTree->Quadrants = ArenaPushArrayZero(Arena, Quadrant, TreeView->Info.NodeCount*4);

	QuadTree->Bounds[0] = GetBounds(TreeView->Info.Nodes, TreeView->Info.NodeCount);
	QuadTree->NextQuadrantIndex = 1;

	for(int I = 0; I < TreeView->Info.NodeCount; I++)
	{
		QuadTreeBuild(Arena, QuadTree, 0, &TreeView->Info.Nodes[I], 0);
	}
	
	QuadTree->Masses[0] = QuadTreeCalculateAttributes(QuadTree, 0);

	SimulateForceDirectedGraphQuadTree(Arena, 
									   TreeView->Info.Nodes,
									   TreeView->Info.NodeCount,
									   TreeView->Info.Links,
									   TreeView->Info.LinkCount,
									   &TreeView->Temperature,
									   QuadTree,
									   Shared);

	

	v3f *Positions = (v3f*)ArenaPushArrayZero(Arena, v3f, TreeView->Info.NodeCount);
	for(int I = 0; I < TreeView->Info.NodeCount; I++)
	{
		Positions[I] = v3f(TreeView->Info.Nodes[I].Position.X,
						   TreeView->Info.Nodes[I].Position.Y, 0.0);
	}

	v3f *LinePositions = ArenaPushArrayZero(Arena, v3f, TreeView->Info.LinkCount*2);
	int LineIndex = 0;
	for(int I = 0; I < TreeView->Info.LinkCount; I++)
	{
		LinePositions[LineIndex + 0] = v3f(TreeView->Info.Nodes[TreeView->Info.Links[I].From].Position.X,
										   TreeView->Info.Nodes[TreeView->Info.Links[I].From].Position.Y,
										   0.0);
		LinePositions[LineIndex + 1] = v3f(TreeView->Info.Nodes[TreeView->Info.Links[I].To].Position.X,
										   TreeView->Info.Nodes[TreeView->Info.Links[I].To].Position.Y,
										   0.0);
		LineIndex += 2;
	}


	D_DrawPointsArray(Draw, Arena, Positions, TreeView->Info.NodeCount, v4f(1, 0, 0, 1));
	D_DrawLinesArray(Draw, Arena, LinePositions, TreeView->Info.LinkCount*2, v4f(0.5, 0.1, 0, 1));

	bounds TotalBounds = GetBounds(TreeView->Info.Nodes, TreeView->Info.NodeCount);
	v2f Centre = DivV2F(AddV2F(TotalBounds.Min, TotalBounds.Max), v2f(2.0, 2.0));

	DrawQuadTree(Draw, Arena, QuadTree, 0);
	
	for(int I = 0; I < TreeView->Info.NodeCount; I++)
	{
		TreeView->Info.Nodes[I].Position = LerpV2F(TreeView->Info.Nodes[I].Position, TreeView->Info.Nodes[I].TargetPosition, 0.1);
	}
}


ui_tree_view_signal UI_TreeViewFolderLabels(ui_context *UI, draw_context *Draw,
							 				m_arena *Arena,
											ui_tree *TreeView, str8 CurrentPath)
{
	ui_tree_view_signal Result = {0};
	 
	for(os_directory_tree_iterator It = {0};
		OS_DirectoryTreeIterate(TreeView->Info.Tree, &It);)
	{
		f32 FontSize = 40;
		if(TreeView->Info.Nodes[It.Index].IsFolder)
		{
			
			b32 Clicked = UI_TextButton(UI, Draw,
				Arena, Str8PushF(Arena, "?S##?u32", Str8FromLastCharacter(Arena, TreeView->Info.Nodes[It.Index].Name, '/'), It.Index),
				FontSize,
				TreeView->Info.Nodes[It.Index].Position, v4f(0.5, 0.1, 0.0, 1));
			if(Clicked)
			{
				Result.Clicked = True;
				Result.NewPath = Str8Copy(TreeView->Arena, TreeView->Info.Nodes[It.Index].Name);
			}
		}
		if(It.Depth == 1)
		{		
			if(!TreeView->Info.Nodes[It.Index].IsFolder)
			{
				b32 Clicked = UI_TextButton(UI, Draw,
					Arena, Str8PushF(Arena, "?S##?u32", Str8FromLastCharacter(Arena, TreeView->Info.Nodes[It.Index].Name, '/'), It.Index),
					FontSize,
					TreeView->Info.Nodes[It.Index].Position, v4f(0.5, 0.1, 0.0, 1));
				if(Clicked)
				{
					Result.Clicked = True;
					Result.FileToLoad = Str8Copy(TreeView->Arena, TreeView->Info.Nodes[It.Index].Name);
				}
			}
		}
	}

	return Result;
}