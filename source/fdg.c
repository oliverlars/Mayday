#include "fdg.h"
#include <MacTypes.h>
#include <assert.h>
#include <float.h>
#include <math.h>

#include "log.h"
#include "maths.h"
#include "memory.h"
#include "os.h"

#include "draw.h"
#include "wave.h"

#include <arm_neon.h>
#include <stdio.h>

void DrawBounds(draw_context *Draw, m_arena *Arena, bounds Bounds)
{
	v3f *Lines = ArenaPushArrayZero(Arena, v3f, 12*2);

	v3f Min = Bounds.Min;
	v3f Max = Bounds.Max;

	Lines[0] = v3f(Min.X, Min.Y, Min.Z);
	Lines[1] = v3f(Min.X, Max.Y, Min.Z);

	Lines[2] = v3f(Min.X, Min.Y, Min.Z);
	Lines[3] = v3f(Max.X, Min.Y, Min.Z);

	Lines[4] = v3f(Min.X, Min.Y, Min.Z);
	Lines[5] = v3f(Min.X, Min.Y, Max.Z);

	Lines[6] = v3f(Max.X, Max.Y, Max.Z);
	Lines[7] = v3f(Max.X, Min.Y, Max.Z);

	Lines[8] = v3f(Max.X, Max.Y, Max.Z);
	Lines[9] = v3f(Min.X, Max.Y, Max.Z);

	Lines[10] = v3f(Max.X, Max.Y, Max.Z);
	Lines[11] = v3f(Max.X, Max.Y, Min.Z);

	Lines[12] = v3f(Min.X, Max.Y, Min.Z);
	Lines[13] = v3f(Max.X, Max.Y, Min.Z);

	Lines[14] = v3f(Min.X, Max.Y, Min.Z);
	Lines[15] = v3f(Min.X, Max.Y, Max.Z);

	Lines[16] = v3f(Max.X, Min.Y, Min.Z);
	Lines[17] = v3f(Max.X, Max.Y, Min.Z);

	Lines[18] = v3f(Max.X, Min.Y, Min.Z);
	Lines[19] = v3f(Max.X, Min.Y, Max.Z);

	Lines[20] = v3f(Min.X, Min.Y, Max.Z);
	Lines[21] = v3f(Min.X, Max.Y, Max.Z);

	Lines[22] = v3f(Min.X, Min.Y, Max.Z);
	Lines[23] = v3f(Max.X, Min.Y, Max.Z);

	D_DrawLinesArray(Draw, Arena, Lines, 24, v4f(0, 0.1, 0, 1));
}

void DrawQuadTree(draw_context *Draw, m_arena *Arena, quadtree_header *QuadTree, int QuadTreeIndex)
{
	DrawBounds(Draw, Arena, QuadTree->Bounds[QuadTreeIndex]);

	for(int I = 0; I < 8; I++)
	{
		if(QuadTree->Quadrants[QuadTreeIndex].Children[I])
		{
			DrawQuadTree(Draw, Arena, QuadTree, QuadTree->Quadrants[QuadTreeIndex].Children[I]);
		}
	}
}

bounds GetBoundsV4F(v4f *Positions, int Count)
{
	v3f Min = v3f(FLT_MAX, FLT_MAX, FLT_MAX);
	v3f Max = v3f(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	for(int I = 0; I < Count; I++)
	{
		Min.X = Min(Min.X, Positions[I].X);
		Min.Y = Min(Min.Y, Positions[I].Y);
		Min.Z = Min(Min.Z, Positions[I].Z);

		Max.X = Max(Max.X, Positions[I].X);
		Max.Y = Max(Max.Y, Positions[I].Y);
		Max.Z = Max(Max.Z, Positions[I].Z);

	}
	return (bounds){Min, Max};
}

bounds GetBoundsV3F(v3f *Positions, int Count)
{
	v3f Min = v3f(FLT_MAX, FLT_MAX, FLT_MAX);
	v3f Max = v3f(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	for(int I = 0; I < Count; I++)
	{
		Min.X = Min(Min.X, Positions[I].X);
		Min.Y = Min(Min.Y, Positions[I].Y);
		Min.Z = Min(Min.Z, Positions[I].Z);

		Max.X = Max(Max.X, Positions[I].X);
		Max.Y = Max(Max.Y, Positions[I].Y);
		Max.Z = Max(Max.Z, Positions[I].Z);

	}
	return (bounds){Min, Max};
}

void QuadTreeBuild(m_arena *Arena, quadtree_header *QuadTree, QuadTree_index QuadrantIndex, v4f NodePosition, int Depth)
{
	v3f Min = QuadTree->Bounds[QuadrantIndex].Min;
	v3f Max = QuadTree->Bounds[QuadrantIndex].Max;

	v3f Centre = DivV3F(AddV3F(Min, Max), v3f(2.0, 2.0, 2.0));


	int Index = 0;
	if(NodePosition.X >= Centre.X) { Index |= 1; }
	if(NodePosition.Y >= Centre.Y) { Index |= 2; }
	if(NodePosition.Z >= Centre.Z) { Index |= 4; }

	bounds ChildBounds[8] =
	{
		{ v3f(Min.X, Min.Y, Min.Z),          v3f(Centre.X, Centre.Y, Centre.Z) },
		{ v3f(Centre.X, Min.Y, Min.Z),       v3f(Max.X, Centre.Y, Centre.Z)    },
		{ v3f(Min.X, Centre.Y, Min.Z),       v3f(Centre.X, Max.Y, Centre.Z)    },
		{ v3f(Centre.X, Centre.Y, Min.Z),    v3f(Max.X, Max.Y, Centre.Z)       },
		{ v3f(Min.X, Min.Y, Centre.Z),       v3f(Centre.X, Centre.Y, Max.Z)    },
		{ v3f(Centre.X, Min.Y, Centre.Z),    v3f(Max.X, Centre.Y, Max.Z)       },
		{ v3f(Min.X, Centre.Y, Centre.Z),    v3f(Centre.X, Max.Y, Max.Z)       },
		{ v3f(Centre.X, Centre.Y, Centre.Z), v3f(Max.X, Max.Y, Max.Z)          },
	};

	int ChildIndex = QuadTree->Quadrants[QuadrantIndex].Children[Index];

	if(!ChildIndex)
	{
		QuadTree->Quadrants[QuadrantIndex].Children[Index] = QuadTree->NextQuadrantIndex++;
		ChildIndex = QuadTree->Quadrants[QuadrantIndex].Children[Index];
		QuadTree->Bounds[ChildIndex] = ChildBounds[Index];
	}

	if(QuadTree->Nodes[ChildIndex] == Null)
	{
		QuadTree->Nodes[ChildIndex] = ArenaPushArrayZero(Arena, quadrant_node, 1);
		QuadTree->Nodes[ChildIndex]->NodePosition = NodePosition;
	}
	else
	{
		if(QuadTree->Nodes[ChildIndex] && ((Depth >= 64) ||
		   (fabs(Max.X - Min.X) < 0.01 || fabs(Max.Y - Min.Y) < 0.01) || fabs(Max.Z - Min.Z) < 0.01))
		{
			quadrant_node *LastNode = QuadTree->Nodes[ChildIndex];
			for(;LastNode->Next; LastNode = LastNode->Next);

			LastNode->Next = ArenaPushArrayZero(Arena, quadrant_node, 1);
			LastNode->Next->NodePosition = NodePosition;
		}
		else
		{
			quadrant_node *Existing = QuadTree->Nodes[ChildIndex];
			QuadTree->Nodes[ChildIndex] = Null;
			QuadTreeBuild(Arena, QuadTree, ChildIndex, Existing->NodePosition, Depth + 1);
			QuadTreeBuild(Arena, QuadTree, ChildIndex, NodePosition, Depth + 1);
		}
	}
}

v4f QuadTreeCalculateAttributes(quadtree_header *QuadTree, int QuadrantIndex)
{
	int ChildrenInThisQuadrant = 0;
	for(int I = 0; I < 8; I++)
	{
		if(QuadTree->Quadrants[QuadrantIndex].Children[I])
		{
			ChildrenInThisQuadrant++;
		}
	}

	v4f CentreOfMass = v4f(0.0, 0.0, 0.0);
	if(ChildrenInThisQuadrant == 0)
	{
		if(QuadTree->Nodes[QuadrantIndex])
		{
			QuadTree->ChildrenCounts[QuadrantIndex] = 1;
			quadrant_node *Node = QuadTree->Nodes[QuadrantIndex];
			int Count = 0;
			for(;Node; Node = Node->Next)
			{
				CentreOfMass = AddV4F(Node->NodePosition, CentreOfMass);
				Count++;
			}
			CentreOfMass = DivV4F(CentreOfMass, v4f((f32)Count, (f32)Count, (f32)Count, (f32)Count));
		}
		else
		{
			QuadTree->ChildrenCounts[QuadrantIndex] = 0;
		}
	}
	else
	{
		int TotalChildren = 0;
		for(int I = 0; I < 8; I++)
		{
			if(QuadTree->Quadrants[QuadrantIndex].Children[I])
			{
				v4f ChildCentreOfMass = QuadTreeCalculateAttributes(QuadTree, QuadTree->Quadrants[QuadrantIndex].Children[I]);

				int ChildIndex = QuadTree->Quadrants[QuadrantIndex].Children[I];
				int ChildrenCount = QuadTree->ChildrenCounts[ChildIndex];
				TotalChildren += ChildrenCount;


				CentreOfMass.X += ChildCentreOfMass.X * (f32)ChildrenCount;
				CentreOfMass.Y += ChildCentreOfMass.Y * (f32)ChildrenCount;
				CentreOfMass.Z += ChildCentreOfMass.Z * (f32)ChildrenCount;

			}
		}
		CentreOfMass = DivV4F(CentreOfMass, v4f((f32)TotalChildren, (f32)TotalChildren, (f32)TotalChildren, (f32)TotalChildren));
		QuadTree->ChildrenCounts[QuadrantIndex] = TotalChildren;
	}
	QuadTree->Masses[QuadrantIndex] = v4f(CentreOfMass.X, CentreOfMass.Y, CentreOfMass.Z, 0.0);
	return QuadTree->Masses[QuadrantIndex];
}

// https://patterns.eecs.berkeley.edu/?page_id=193
v4f ForceDisplacementFromPosition(quadtree_header *QuadTree, v4f NodePosition, f32 K)
{
	v4f Result = v4f(0.0);

	int QuadrantStack[1024];
	int StackTop = 0;
	int QuadrantIndex = 0;
	QuadrantStack[StackTop++] = QuadrantIndex;

	for(;StackTop > 0;)
	{
		QuadrantIndex = QuadrantStack[--StackTop];
		v4f Centre = QuadTree->Masses[QuadrantIndex];
		v3f PositionToCentre = v3f(NodePosition.X - Centre.X, NodePosition.Y - Centre.Y, NodePosition.Z - Centre.Z);
		f64 DistanceSquared = DotV3F(PositionToCentre, PositionToCentre);
		f64 Distance = sqrt(DistanceSquared);
		f32 TemporaryRadius = 0.0;
		f64 EffectiveDistance = Max((f32)Distance - TemporaryRadius, 0.1);
		if(QuadTree->ChildrenCounts[QuadrantIndex] == 1)
		{
			f64 ForceMagnitude = (K) / (EffectiveDistance*EffectiveDistance + 0.01);

			Result.X += ForceMagnitude * PositionToCentre.X;
			Result.Y += ForceMagnitude * PositionToCentre.Y;
			Result.Z += ForceMagnitude * PositionToCentre.Z;
		}
		else
		{
			v3f MaxSubMin = SubV3F(QuadTree->Bounds[QuadrantIndex].Max, QuadTree->Bounds[QuadrantIndex].Min);
			f32 QuadrantRadiusSquared = DotV3F(MaxSubMin, MaxSubMin);
			if(QuadrantRadiusSquared/(EffectiveDistance*EffectiveDistance + 0.01) < 0.5)
			{
				f32 ForceMagnitude = QuadTree->ChildrenCounts[QuadrantIndex] * (K) / (EffectiveDistance*EffectiveDistance + 0.01);

				Result.X += ForceMagnitude * PositionToCentre.X;
				Result.Y += ForceMagnitude * PositionToCentre.Y;
				Result.Z += ForceMagnitude * PositionToCentre.Z;
			}
			else
			{
				for(int I = 0; I < 8; I++)
				{
					if(QuadTree->Quadrants[QuadrantIndex].Children[I])
					{
						QuadrantStack[StackTop++] = QuadTree->Quadrants[QuadrantIndex].Children[I];
					}
				}
			}
		}
	}

	return Result;
}

#define CACHE_LINE_SIZE (128)

typedef struct parallel_force_displacement
{
	v4f *DisplacementsMerged;
	v4f **Displacements;
	f32 K;
	v4f *NodePositions;
	int NodeCount;
	int *Froms;
	int *Tos;
	int LinkCount;
	f32 Temperature;
	quadtree_header *QuadTree;
} parallel_force_displacement;

void ParallelForceDisplacement(wave_info *WaveInfo, void* Input)
{
	parallel_force_displacement *P = Input;
	int NodeCount = LaneValueFromTotal(WaveInfo, P->NodeCount);
	int NodeIndex = ExclusiveWavePrefixSum(WaveInfo, NodeCount);

	int LinkCount = LaneValueFromTotal(WaveInfo, P->LinkCount);
	int LinkIndex = ExclusiveWavePrefixSum(WaveInfo, LinkCount);

	u64 Start = OS_TimeStampNow();

	m_scratch_arena Scratch = ScratchBegin(0, 0);

	v4f *Displacements = P->Displacements[WaveInfo->Lane];

	Start = OS_TimeStampNow();
	for(int I = 0; I < NodeCount; I++)
	{
		P->DisplacementsMerged[I + NodeIndex] = ForceDisplacementFromPosition(P->QuadTree,
														 P->NodePositions[I + NodeIndex],
														 P->K);

	}

	WaveBarrier(WaveInfo->Shared->Barrier);

	if(WaveIsFirstLane(WaveInfo))
	{
		for(int I = 0; I < P->LinkCount; I++)
		{
			int *Froms = P->Froms + LinkIndex;
			int *Tos = P->Tos + LinkIndex;
			v4f *NodePositions = P->NodePositions;
			int A = Froms[I];
			int B = Tos[I];

			f32 DX = NodePositions[A].X - NodePositions[B].X;
			f32 DY = NodePositions[A].Y - NodePositions[B].Y;
			f32 DZ = NodePositions[A].Z - NodePositions[B].Z;
			f32 DistanceSquared = DX*DX + DY*DY + DZ*DZ;
			f32 Distance = sqrtf(DistanceSquared);

			f32 ReciprocalDistance = 1.0 / (Distance + 0.01);
			f32 NormDX = DX * ReciprocalDistance;
			f32 NormDY = DY * ReciprocalDistance;
			f32 NormDZ = DZ * ReciprocalDistance;

			f32 AttractiveForce = (DistanceSquared) / P->K;

			P->DisplacementsMerged[A].X -= NormDX * AttractiveForce;
			P->DisplacementsMerged[A].Y -= NormDY * AttractiveForce;
			P->DisplacementsMerged[A].Z -= NormDZ * AttractiveForce;

			P->DisplacementsMerged[B].X += NormDX * AttractiveForce;
			P->DisplacementsMerged[B].Y += NormDY * AttractiveForce;
			P->DisplacementsMerged[B].Z += NormDZ * AttractiveForce;

		}
	}

	WaveBarrier(WaveInfo->Shared->Barrier);

	for(int I = 0; I < NodeCount; I++)
	{
		v4f D = P->DisplacementsMerged[I + NodeIndex];
		f32 Norm = sqrtf(D.X * D.X + D.Y * D.Y + D.Z * D.Z);
		if(Norm > 0)
		{
			P->NodePositions[I + NodeIndex].X += (D.X / Norm) * Min(Norm, P->Temperature);
			P->NodePositions[I + NodeIndex].Y += (D.Y / Norm) * Min(Norm, P->Temperature);
			P->NodePositions[I + NodeIndex].Z += (D.Z / Norm) * Min(Norm, P->Temperature);
		}
	}

	ScratchEnd(Scratch);
}
#define PARALLEL_QUADTREE
//#define QUADTREE
//#define NSQUARED

void SimulateForceDirectedGraphQuadTree(m_arena *Arena, v4f *NodePositions, int NodeCount,
									  int *Froms, int *Tos, int LinkCount,
									  f32 *Temperature,
									  quadtree_header *QuadTree, shared_wave_info *Shared)
{
	u64 Start = OS_TimeStampNow();
	f32 K = 5;

	v4f *Displacements = ArenaPushArrayZero(Arena, v4f, NodeCount);


#if defined(PARALLEL_QUADTREE)
	parallel_force_displacement P = {0};
	P.DisplacementsMerged = ArenaPushArrayZero(Arena, v4f, NodeCount);
	P.Displacements = ArenaPushArrayZero(Arena, v4f*, Shared->LaneCount);
	for(int I = 0; I < Shared->LaneCount; I++)
	{
		int CacheLineSizeInBytes = 128;
		int Bytes = sizeof(v4f)*(NodeCount/Shared->LaneCount);
		P.Displacements[I] = (v4f*)ArenaPushZero(Arena, Max(CacheLineSizeInBytes, Bytes));
	}
	P.K = K;
	P.NodeCount = NodeCount;
	P.NodePositions = NodePositions;
	P.QuadTree = QuadTree;
	P.Froms = Froms;
	P.Tos = Tos;
	P.LinkCount = LinkCount;
	P.Temperature = Temperature[0];

	Dispatch(Shared, ParallelForceDisplacement, &P);

#elif defined(QUADTREE)

	for(int I = 0; I < NodeCount; I++)
	{
		Displacements[I] = ForceDisplacementFromPosition(QuadTree,
														 Nodes + I,
														 K);
	}
#elif defined(NSQUARED)

	for(int I = 0; I < NodeCount; I++)
	{
		for(int J = I+1; J < NodeCount; J++)
		{
			f32 DX = Nodes[J].TargetPosition.X - Nodes[I].TargetPosition.X;
			f32 DY = Nodes[J].TargetPosition.Y - Nodes[I].TargetPosition.Y;
			f32 DZ = Nodes[J].TargetPosition.Z - Nodes[I].TargetPosition.Z;
			f32 DistanceSquared = DX*DX + DY*DY + DZ*DZ;

			Displacements[I].X -= (K) * DX / Max(DistanceSquared, 0.01);
			Displacements[I].Y -= (K) * DY / Max(DistanceSquared, 0.01);
			Displacements[I].Z -= (K) * DZ / Max(DistanceSquared, 0.01);

			Displacements[J].X += (K) * DX / Max(DistanceSquared, 0.01);
			Displacements[J].Y += (K) * DY / Max(DistanceSquared, 0.01);
			Displacements[J].Z += (K) * DZ / Max(DistanceSquared, 0.01);
		}
	}
#endif

	Temperature[0] = Lerp(Temperature[0], 0.0, 0.05);

	//LogInfo("Time to simulate: ?f32", OS_MilliSecondsFromTimeStamp(OS_TimeStampNow() - Start));
}
