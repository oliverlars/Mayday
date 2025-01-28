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
	Info.Tree = OS_DirectoryTreeFromPath(Arena, Str8Copy(Arena, Path));

	Info.NodePositions = ArenaPushArrayZero(Arena, v4f, Info.Tree.Count);
	Info.NodeTargetPositions = ArenaPushArrayZero(Arena, v4f, Info.Tree.Count);
	Info.NodeInfos = ArenaPushArrayZero(Arena, fdg_node_info, Info.Tree.Count);
	Info.Froms = ArenaPushArrayZero(Arena, int, Info.Tree.Count);
	Info.Tos = ArenaPushArrayZero(Arena, int, Info.Tree.Count);

	m_scratch_arena Scratch = ScratchBegin(&Arena, 1);

	random32_state RandomState = {0};
	RandomState.State = (u32)M_FNV_HASH_START;
	for(os_directory_tree_iterator It = {0};
		OS_DirectoryTreeIterate(Info.Tree, &It);)
	{

		f32 X = RandomBilateral(&RandomState);
		f32 Y = RandomBilateral(&RandomState);
		f32 Z = RandomBilateral(&RandomState);
		Info.NodeTargetPositions[It.Index] = v4f(X, Y, Z, 0);
		Info.NodePositions[It.Index] = Info.NodeTargetPositions[It.Index];
		Info.NodeInfos[It.Index].Name = It.Current->PathAndName;
		Info.NodeInfos[It.Index].IsFolder = It.Current->Type == OS_DIR_ENTRY_FOLDER;

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
		//ArenaReset(TreeView->Arena);


		ui_tree_info NextInfo = TreeInfoFromPath(TreeView->Arena, Path, Size);

		/*

		NOTE(Oliver): We do this just so we can avoid the jarring position
		reset. It can probably be done better than this but it's fine really

		*/
		if(TreeView->Info.NodePositions)
		{
			int Index = 0;
			for(int I = 0; I < TreeView->Info.NodeCount; I++)
			{
				if(Str8Equal(TreeView->Info.NodeInfos[I].Name, Path))
				{
					Index = I;
				}
			}

			for(int I = 0; I < NextInfo.NodeCount; I++)
			{
				NextInfo.NodePositions[I] = TreeView->Info.NodePositions[I + Index];
				NextInfo.NodeTargetPositions[I] = TreeView->Info.NodeTargetPositions[I + Index];
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
	QuadTree->Masses = ArenaPushArrayZero(Arena, v4f, TreeView->Info.NodeCount*4);
	QuadTree->Quadrants = ArenaPushArrayZero(Arena, Quadrant, TreeView->Info.NodeCount*4);

	QuadTree->Bounds[0] = GetBounds(TreeView->Info.NodeTargetPositions, TreeView->Info.NodeCount);
	QuadTree->NextQuadrantIndex = 1;

	for(int I = 0; I < TreeView->Info.NodeCount; I++)
	{
		QuadTreeBuild(Arena, QuadTree, 0, TreeView->Info.NodeTargetPositions[I], 0);
	}

	QuadTree->Masses[0] = QuadTreeCalculateAttributes(QuadTree, 0);

	SimulateForceDirectedGraphQuadTree(Arena,
									   TreeView->Info.NodeTargetPositions,
									   TreeView->Info.NodeCount,
									   TreeView->Info.Froms,
									   TreeView->Info.Tos,
									   TreeView->Info.LinkCount,
									   &TreeView->Temperature,
									   QuadTree,
									   Shared);

	v3f *Positions = (v3f*)ArenaPushArrayZero(Arena, v3f, TreeView->Info.NodeCount);
	for(int I = 0; I < TreeView->Info.NodeCount; I++)
	{
		Positions[I].X = TreeView->Info.NodePositions[I].X;
		Positions[I].Y = TreeView->Info.NodePositions[I].Y;
		Positions[I].Z = TreeView->Info.NodePositions[I].Z;
	}

	v3f *LinePositions = ArenaPushArrayZero(Arena, v3f, TreeView->Info.LinkCount*2);
	int LineIndex = 0;
	for(int I = 0; I < TreeView->Info.LinkCount; I++)
	{
		v4f From = TreeView->Info.NodePositions[TreeView->Info.Froms[I]];
		v4f To = TreeView->Info.NodePositions[TreeView->Info.Tos[I]];
		LinePositions[LineIndex + 0] = v3f(From.X, From.Y, From.Z);
		LinePositions[LineIndex + 1] = v3f(To.X, To.Y, To.Z);
		LineIndex += 2;
	}


	D_DrawPointsArray(Draw, Arena, Positions, TreeView->Info.NodeCount, v4f(1, 0, 0, 1));
	D_DrawLinesArray(Draw, Arena, LinePositions, TreeView->Info.LinkCount*2, v4f(0.5, 0.1, 0, 1));

	bounds TotalBounds = GetBounds(TreeView->Info.NodeTargetPositions, TreeView->Info.NodeCount);
	v3f Centre = DivV3F(AddV3F(TotalBounds.Min, TotalBounds.Max), v3f(2.0, 2.0, 2.0));

	//DrawQuadTree(Draw, Arena, QuadTree, 0);

	for(int I = 0; I < TreeView->Info.NodeCount; I++)
	{
		v4f P = TreeView->Info.NodePositions[I];
		v4f Target = TreeView->Info.NodeTargetPositions[I];
		v3f Result = LerpV3F(v3f(P.X, P.Y, P.Z), v3f(Target.X, Target.Y, Target.Z), 0.1);
		TreeView->Info.NodePositions[I] = v4f(Result.X, Result.Y, Result.Z, 0.0);
	}
}


ui_tree_view_signal UI_TreeViewFolderLabels(ui_context *UI, draw_context *Draw, edit_context *Edit, d_font Font,
							 				m_arena *Arena, int ActiveNodeIndex,
											ui_tree *TreeView, m44f TreeTransform, str8 CurrentPath, editable_buffer* SearchBuffer)
{
	ui_tree_view_signal Result = {0};

	if(!OS_ValidPath(CurrentPath)) return Result;

	ui_textbox_signal SearchSignal = UI_TextBox(Arena, Font, v2f(sapp_widthf()/2.0 - 500, 100), 1000,
												UI, Draw, Edit, 60, Str8("TreeViewTextBox"), SearchBuffer);

	str8 Filter = SearchSignal.String;

	m_scratch_arena Scratch = ScratchBegin(0, 0);
	for(os_directory_tree_iterator It = {0};
		OS_DirectoryTreeIterateWithStack(Scratch.Arena, TreeView->Info.Tree, &It);)
	{
		f32 FontSize = 30;
		v4f P = TreeView->Info.NodePositions[It.Index];

		v4f NodePosition4D = v4f(P.X, P.Y, P.Z, 1.0);
		v4f TransformedPoint = MulM44FV4F(M44FTranspose(TreeTransform), NodePosition4D);
		TransformedPoint.X /= TransformedPoint.W;
		TransformedPoint.Y /= TransformedPoint.W;
		TransformedPoint.X = TransformedPoint.X*0.5 + 0.5;
		TransformedPoint.Y = TransformedPoint.Y*0.5 + 0.5;

		TransformedPoint.X *= sapp_widthf();
		TransformedPoint.Y *= sapp_heightf();

		if(TreeView->Info.NodeInfos[It.Index].IsFolder)
		{
			str8 FolderName = Str8FromLastCharacter(Arena, TreeView->Info.NodeInfos[It.Index].Name, '/');
			if(Filter.Size == 0 || Str8FuzzyMatch(FolderName, Filter))
			{
				b32 Clicked = UI_TextButton(UI, Draw,
					Arena, Str8PushF(Arena, "?S##?u32", FolderName, It.Index),
					FontSize,
					v2f(TransformedPoint.X, TransformedPoint.Y), v4f(0.5, 0.8, 0.0, 1));

				if(Clicked || (SearchSignal.Submitted && Filter.Size > 0))
				{
					Result.NodeIndex = It.Index;
					Result.NodeDepth = It.Depth;
					Result.Clicked = True;
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
					b32 Clicked = UI_TextButton(UI, Draw,
						Arena, Str8PushF(Arena, "?S##?u32", FileName, It.Index),
						FontSize,
						v2f(TransformedPoint.X, TransformedPoint.Y), v4f(0.5, 0.4, 0.0, 1));
					if(Clicked || (SearchSignal.Submitted && Filter.Size > 0))
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
		Result.Clicked = True;
		Result.NodeIndex = ActiveNodeIndex + 1;
	}

	if(UI_KeyDown(UI, SAPP_KEYCODE_LEFT, 0))
	{
		Result.Clicked = True;
		Result.NodeIndex = Max(ActiveNodeIndex - 1, 0);
	}

	ScratchEnd(Scratch);

	return Result;
}
