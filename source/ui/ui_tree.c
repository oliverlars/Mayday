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
	Info.Tree = OS_DirectoryTreeFromPath(Arena, Str8Copy(Arena, Path));

	Info.Nodes = (fdg_node*)ArenaPushArrayZero(Arena, fdg_node, Info.Tree.Count);
	Info.Links = (fdg_link*)ArenaPushArrayZero(Arena, fdg_link, Info.Tree.Count);

	m_scratch_arena Scratch = ScratchBegin(&Arena, 1);

	random32_state RandomState = {0};
	RandomState.State = (u32)M_FNV_HASH_START;
	for(os_directory_tree_iterator It = {0};
		OS_DirectoryTreeIterate(Info.Tree, &It);)
	{

		f32 X = RandomBilateral(&RandomState)*Size.X;
		f32 Y = RandomBilateral(&RandomState)*Size.Y;
		f32 Z = RandomBilateral(&RandomState)*Size.Z;
		Info.Nodes[It.Index].TargetPosition = v3f(X, Y, Z);
		Info.Nodes[It.Index].TargetPosition = Info.Nodes[It.Index].TargetPosition;
		Info.Nodes[It.Index].Position = Info.Nodes[It.Index].TargetPosition;
		Info.Nodes[It.Index].Name = It.Current->PathAndName;
		Info.Nodes[It.Index].IsFolder = It.Current->Type == OS_DIR_ENTRY_FOLDER;

		Info.Nodes[It.Index].Radius = 0*Str8UpToLastCharacter(Scratch.Arena, It.Current->PathAndName, '/').Size;

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
				 m_arena *Arena, ui_tree *TreeView, str8 Path, v3f Size)
{
	u64 Hash = M_FNVHashString(M_FNV_HASH_START, Path);
	if(Hash != TreeView->TreeHash)
	{
		if(!TreeView->Arena)
		{
			TreeView->Arena = ArenaInitialise(MegaBytes(256));
		}
		//ArenaReset(TreeView->Arena);


		ui_tree_info NextInfo = TreeInfoFromPath(TreeView->Arena, Path, Size);

		/*

		NOTE(Oliver): We do this just so we can avoid the jarring position
		reset. It can probably be done better than this but it's fine really

		*/
		if(TreeView->Info.Nodes)
		{
			int Index = 0;
			for(int I = 0; I < TreeView->Info.NodeCount; I++)
			{
				if(Str8Equal(TreeView->Info.Nodes[I].Name, Path))
				{
					Index = I;
				}
			}

			for(int I = 0; I < NextInfo.NodeCount; I++)
			{
				NextInfo.Nodes[I].Position = TreeView->Info.Nodes[I + Index].Position;
				NextInfo.Nodes[I].TargetPosition = TreeView->Info.Nodes[I + Index].TargetPosition;
			}
		}
		TreeView->Info = NextInfo;
		TreeView->TreeHash = Hash;
		TreeView->Temperature = 15.0;
	}

	quadtree_header *QuadTree = ArenaPushArrayZero(Arena, quadtree_header, 1);
	QuadTree->Bounds = ArenaPushArrayZero(Arena, bounds, TreeView->Info.NodeCount*4);
	QuadTree->ChildrenCounts = ArenaPushArrayZero(Arena, int, TreeView->Info.NodeCount*4);
	QuadTree->Nodes = ArenaPushArrayZero(Arena, quadrant_node*, TreeView->Info.NodeCount*4);
	QuadTree->Masses = ArenaPushArrayZero(Arena, v3f, TreeView->Info.NodeCount*4);
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
		Positions[I] = TreeView->Info.Nodes[I].Position;
	}

	v3f *LinePositions = ArenaPushArrayZero(Arena, v3f, TreeView->Info.LinkCount*2);
	int LineIndex = 0;
	for(int I = 0; I < TreeView->Info.LinkCount; I++)
	{
		LinePositions[LineIndex + 0] = TreeView->Info.Nodes[TreeView->Info.Links[I].From].Position;
		LinePositions[LineIndex + 1] = TreeView->Info.Nodes[TreeView->Info.Links[I].To].Position;
		LineIndex += 2;
	}


	D_DrawPointsArray(Draw, Arena, Positions, TreeView->Info.NodeCount, v4f(1, 0, 0, 1));
	D_DrawLinesArray(Draw, Arena, LinePositions, TreeView->Info.LinkCount*2, v4f(0.5, 0.1, 0, 1));

	bounds TotalBounds = GetBounds(TreeView->Info.Nodes, TreeView->Info.NodeCount);
	v3f Centre = DivV3F(AddV3F(TotalBounds.Min, TotalBounds.Max), v3f(2.0, 2.0, 2.0));

	//DrawQuadTree(Draw, Arena, QuadTree, 0);

	for(int I = 0; I < TreeView->Info.NodeCount; I++)
	{
		TreeView->Info.Nodes[I].Position = LerpV3F(TreeView->Info.Nodes[I].Position, TreeView->Info.Nodes[I].TargetPosition, 0.1);
	}
}


ui_tree_view_signal UI_TreeViewFolderLabels(ui_context *UI, draw_context *Draw,
							 				m_arena *Arena,
											ui_tree *TreeView, m44f TreeTransform, str8 CurrentPath)
{
	ui_tree_view_signal Result = {0};

	for(os_directory_tree_iterator It = {0};
		OS_DirectoryTreeIterate(TreeView->Info.Tree, &It);)
	{
		f32 FontSize = 30;
		v3f P = TreeView->Info.Nodes[It.Index].Position;

		v4f NodePosition4D = v4f(P.X, P.Y, P.Z, 1.0);
		v4f TransformedPoint = MulM44FV4F(M44FTranspose(TreeTransform), NodePosition4D);
		TransformedPoint.X /= TransformedPoint.W;
		TransformedPoint.Y /= TransformedPoint.W;
		TransformedPoint.X = TransformedPoint.X*0.5 + 0.5;
		TransformedPoint.Y = TransformedPoint.Y*0.5 + 0.5;

		TransformedPoint.X *= sapp_widthf();
		TransformedPoint.Y *= sapp_heightf();

		if(TreeView->Info.Nodes[It.Index].IsFolder)
		{
			b32 Clicked = UI_TextButtonWrapping(UI, Draw,
				Arena, Str8PushF(Arena, "?S##?u32", Str8FromLastCharacter(Arena, TreeView->Info.Nodes[It.Index].Name, '/'), It.Index),
				FontSize,
				v2f(TransformedPoint.X, TransformedPoint.Y), v4f(0.5, 0.1, 0.0, 1));
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
				b32 Clicked = UI_TextButtonWrapping(UI, Draw,
					Arena, Str8PushF(Arena, "?S##?u32", Str8FromLastCharacter(Arena, TreeView->Info.Nodes[It.Index].Name, '/'), It.Index),
					FontSize,
					v2f(TransformedPoint.X, TransformedPoint.Y), v4f(0.5, 0.1, 0.0, 1));
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
