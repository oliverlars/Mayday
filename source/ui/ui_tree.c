#include "maths.h"
#include "memory.h"
#include "misc.h"
#include "os.h"
#include "ui.h"
#include "wave.h"


ui_tree_info TreeInfoFromPath(m_arena *Arena, str8 Path, v2f Size)
{
	ArenaReset(Arena);
	ui_tree_info Info = {0};
	Info.Tree = OS_DirectoryTreeFromPath(Arena, Str8PushF(Arena, "?S", Path));

	int TotalCount = OS_CountAllEntriesInDirectoryTree(Info.Tree);

	Info.Nodes = (fdg_node*)ArenaPushArrayZero(Arena, fdg_node, TotalCount);
	Info.Links = (fdg_link*)ArenaPushArrayZero(Arena, fdg_link, TotalCount);

	os_directory_tree_iterator Iter = {.Tree = Info.Tree, .Current = Info.Tree.Root };

	Info.NodeCount = TotalCount;
	Info.Nodes[0].TargetPosition = v2f(0, 0);
	Info.Nodes[0].Position = v2f(0.0, 0.0);
	Info.Nodes[0].Name = Info.Tree.Root->PathAndName;
	Info.Nodes[0].IsFolder = True;
	int Index = 1;
	random32_state RandomState = {0};
	RandomState.State = (u32)M_FNV_HASH_START;
	for(;OS_DirectoryTreeIterate(&Iter);)
	{

		f32 X = (0.5*RandomBilateral(&RandomState) + 0.5)*Size.X + Size.X/2.0;
		f32 Y = (0.5*RandomBilateral(&RandomState) + 0.5)*Size.Y + Size.Y/2.0;
		Info.Nodes[Index].TargetPosition = v2f(X, Y);
		Info.Nodes[Index].TargetPosition = Info.Nodes[Index].TargetPosition;
		Info.Nodes[Index].Position = Info.Nodes[Index].TargetPosition;
		Info.Nodes[Index].Name = Iter.Current->PathAndName;
		Info.Nodes[Index].IsFolder = Iter.Current->Type == OS_DIR_ENTRY_FOLDER;
		Index++;
	}

	Iter.Current = Info.Tree.Root;
	int From = 0;
	int To = 1;
	Index = 0;

	int Stack[1024];
	int Top = 0;
	Stack[Top++] = To;
	for(;OS_DirectoryTreeIterate(&Iter);)
	{
		Info.Links[Index].From = From;
		Info.Links[Index].To = To;
		if(Iter.Current->Type == OS_DIR_ENTRY_FOLDER)
		{
			From = To;
		}
		To++;
		Index++;
	}
	Info.LinkCount = Index;

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

	m_scratch_arena Scratch = ScratchBegin(0, 0);

	v3f *Positions = (v3f*)ArenaPushArrayZero(Scratch.Arena, v3f, TreeView->Info.NodeCount);
	for(int I = 0; I < TreeView->Info.NodeCount; I++)
	{
		Positions[I] = v3f(TreeView->Info.Nodes[I].Position.X,
						   TreeView->Info.Nodes[I].Position.Y, 0.0);
	}

	v3f *LinePositions = ArenaPushArrayZero(Scratch.Arena, v3f, TreeView->Info.LinkCount*2);
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
		f32 FontSize = 40;//ZoomedIn ? 40 : 35;
		if(TreeView->Info.Nodes[I].IsFolder)
		{
			
			b32 Clicked = UI_TextButton(UI, Draw,
				Arena, Str8PushF(Arena, "?S##?u32", Str8FromLastCharacter(Arena, TreeView->Info.Nodes[I].Name, '/'), I),
				FontSize,
				TreeView->Info.Nodes[I].Position, v4f(0.5, 0.1, 0.0, 1));
			if(Clicked)
			{
					
			}
		}
		
		if(0 && !TreeView->Info.Nodes[I].IsFolder)
		{
			b32 Clicked = UI_TextButton(UI, Draw,
				Arena, Str8PushF(Arena, "?S##?u32", Str8FromLastCharacter(Arena, TreeView->Info.Nodes[I].Name, '/'), I),
				FontSize,
				TreeView->Info.Nodes->Position, v4f(0.5, 0.1, 0.0, 1));
		}
	}

	D_DrawStr8(Draw, Arena, UI->Font, v2f(50, 50), v4f(1.0, 0.05, 0.0, 1), 100, Str8("MAYDAY!"));
	
	for(int I = 0; I < TreeView->Info.NodeCount; I++)
	{
		TreeView->Info.Nodes[I].Position = LerpV2F(TreeView->Info.Nodes[I].Position, TreeView->Info.Nodes[I].TargetPosition, 0.1);
	}

	ScratchEnd(Scratch);
}