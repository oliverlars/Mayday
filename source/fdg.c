#include "fdg.h"
#include <assert.h>
#include <float.h>
#include <math.h>

#include "log.h"
#include "maths.h"
#include "memory.h"
#include "os.h"

#include "draw.h"
#include "wave.h"
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

	D_LinesArray(Draw, Arena, Lines, 24, 1.0, v4f(0, 0.1, 0, 1));
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
		bounds CB = QuadTree->Bounds[ChildIndex];
		if(QuadTree->Nodes[ChildIndex] && (
		  (Depth >= 64) ||
		  (fabsf(CB.Max.X - CB.Min.X) < 0.01f) ||
		  (fabsf(CB.Max.Y - CB.Min.Y) < 0.01f) ||
		  (fabsf(CB.Max.Z - CB.Min.Z) < 0.01f)))
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
			quadrant_node *Node = QuadTree->Nodes[QuadrantIndex];
			int Count = 0;
			for(;Node; Node = Node->Next)
			{
				CentreOfMass = AddV4F(Node->NodePosition, CentreOfMass);
				Count++;
			}
			QuadTree->ChildrenCounts[QuadrantIndex] = Count;
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

		v3f PositionToCentre = v3f(
			NodePosition.X - Centre.X,
			NodePosition.Y - Centre.Y,
			NodePosition.Z - Centre.Z
		);

		f32 DistanceSquared = (f32)DotV3F(PositionToCentre, PositionToCentre);
		f32 Distance = sqrtf(DistanceSquared);
		f32 EffectiveDistance = Max(Distance - 0.0f, 0.1f);

		int ChildCount = QuadTree->ChildrenCounts[QuadrantIndex];

		int HasChildren = 0;
		for(int I = 0; I < 8; I++) { if(QuadTree->Quadrants[QuadrantIndex].Children[I]) { HasChildren = 1; break; } }

		if(!HasChildren && QuadTree->Nodes[QuadrantIndex])
		{
			for(quadrant_node *N = QuadTree->Nodes[QuadrantIndex]; N; N = N->Next)
			{
				v3f Delta = v3f(
					NodePosition.X - N->NodePosition.X,
					NodePosition.Y - N->NodePosition.Y,
					NodePosition.Z - N->NodePosition.Z
				);

				f32 DistanceSquared = (f32)DotV3F(Delta, Delta);
				f32 Distance  = sqrtf(DistanceSquared) + 0.01;
				f32 RepulsiveForce = (K*K) / (Distance + 0.01);

				Result.X += RepulsiveForce * Delta.X / Distance;
				Result.Y += RepulsiveForce * Delta.Y / Distance;
				Result.Z += RepulsiveForce * Delta.Z / Distance;
			}
			continue;
		}

		if(ChildCount == 1)
		{
			f32 ForceMagnitude  = (K*K) / EffectiveDistance;
			Result.X += ForceMagnitude * PositionToCentre.X / EffectiveDistance;
			Result.Y += ForceMagnitude * PositionToCentre.Y / EffectiveDistance;
			Result.Z += ForceMagnitude * PositionToCentre.Z / EffectiveDistance;
		}
		else
		{
			v3f Extents = SubV3F(QuadTree->Bounds[QuadrantIndex].Max, QuadTree->Bounds[QuadrantIndex].Min);
			f32 HalfDiagonal = 0.5f * sqrtf(DotV3F(Extents, Extents));
			f32 Theta = 0.9f;

			if((HalfDiagonal / EffectiveDistance) < Theta)
			{
				f32 InverseDistance = 1.0f / EffectiveDistance;
				f32 ForceMagnitude  = (K*K) * InverseDistance * (f32)ChildCount;
				Result.X += ForceMagnitude * PositionToCentre.X / EffectiveDistance;
				Result.Y += ForceMagnitude * PositionToCentre.Y / EffectiveDistance;
				Result.Z += ForceMagnitude * PositionToCentre.Z / EffectiveDistance;
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
	v4f *NodeVelocities;
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

	typedef struct link_info
	{
		f32 AttractiveForce;
		f32 RepulsiveForce;
		v3f Delta;
		v3f Norm;
		f32 Distance;
	}
	link_info;

	typedef struct link_infos
	{
		link_info *Infos;
	}
	link_infos;

	link_info *LinkInfos = NULL;
	if(WaveIsFirstLane(WaveInfo))
	{
		LinkInfos = ArenaPushArrayZero(WaveInfo->Shared->Arena, link_info, P->LinkCount);
	}


	BroadcastToAllLanes(WaveInfo, LinkInfos);


	WaveBarrier(WaveInfo->Shared->Barrier);

	for(int I = LinkIndex; I < LinkIndex + LinkCount; I++)
	{
		v4f *NodePositions = P->NodePositions;
		int A = P->Froms[I];
		int B = P->Tos[I];

		f32 DX = NodePositions[A].X - NodePositions[B].X;
		f32 DY = NodePositions[A].Y - NodePositions[B].Y;
		f32 DZ = NodePositions[A].Z - NodePositions[B].Z;
		f32 DistanceSquared = DX*DX + DY*DY + DZ*DZ;
		f32 Distance = sqrtf(DistanceSquared) + 0.1;

		f32 ReciprocalDistance = 1.0 / (Distance + 0.01);
		f32 NormDX = DX * ReciprocalDistance;
		f32 NormDY = DY * ReciprocalDistance;
		f32 NormDZ = DZ * ReciprocalDistance;

		f32 AttractiveForce = DistanceSquared / P->K;

		f32 RepulsiveForce = P->K * P->K / Distance;

		LinkInfos[I].AttractiveForce = AttractiveForce;
		LinkInfos[I].RepulsiveForce = RepulsiveForce;
		LinkInfos[I].Norm = v3f(NormDX, NormDY, NormDZ);
		LinkInfos[I].Delta = v3f(DX, DY, DZ);
		LinkInfos[I].Distance = Distance;
	}

	WaveBarrier(WaveInfo->Shared->Barrier);

	if(WaveIsFirstLane(WaveInfo))
	{
		for(int I = 0; I < P->LinkCount; I++)
		{
			int A = P->Froms[I];
			int B = P->Tos[I];

			link_info LinkInfo = LinkInfos[I];

			P->DisplacementsMerged[A].X -= LinkInfo.Delta.X / LinkInfo.Distance * LinkInfo.AttractiveForce;
			P->DisplacementsMerged[A].Y -= LinkInfo.Delta.Y / LinkInfo.Distance * LinkInfo.AttractiveForce;
			P->DisplacementsMerged[A].Z -= LinkInfo.Delta.Z / LinkInfo.Distance * LinkInfo.AttractiveForce;

			P->DisplacementsMerged[B].X += LinkInfo.Delta.X / LinkInfo.Distance * LinkInfo.AttractiveForce;
			P->DisplacementsMerged[B].Y += LinkInfo.Delta.Y / LinkInfo.Distance * LinkInfo.AttractiveForce;
			P->DisplacementsMerged[B].Z += LinkInfo.Delta.Z / LinkInfo.Distance * LinkInfo.AttractiveForce;
		}
	}

	WaveBarrier(WaveInfo->Shared->Barrier);

	for(int I = 0; I < NodeCount; I++)
	{
		v4f D = P->DisplacementsMerged[I + NodeIndex];
		P->NodeVelocities[I + NodeIndex].X += P->DisplacementsMerged[I + NodeIndex].X;
		P->NodeVelocities[I + NodeIndex].Y += P->DisplacementsMerged[I + NodeIndex].Y;
		P->NodeVelocities[I + NodeIndex].Z += P->DisplacementsMerged[I + NodeIndex].Z;


		// HACK(Oliver): Figure out a better constant here
		// Likely should include dt
		// Okay new constant is better but not sure, i feel it should work
		// regardless of the constant?
		P->NodeVelocities[I + NodeIndex].X *= 5.0f/(f32)(P->NodeCount*P->NodeCount);
		P->NodeVelocities[I + NodeIndex].Y *= 5.0f/(f32)(P->NodeCount*P->NodeCount);
		P->NodeVelocities[I + NodeIndex].Z *= 5.0f/(f32)(P->NodeCount*P->NodeCount);

		P->NodePositions[I + NodeIndex].X += P->NodeVelocities[I + NodeIndex].X;
		P->NodePositions[I + NodeIndex].Y += P->NodeVelocities[I + NodeIndex].Y;
		P->NodePositions[I + NodeIndex].Z += P->NodeVelocities[I + NodeIndex].Z;

	}

	ScratchEnd(Scratch);
}
#define PARALLEL_QUADTREE
//#define QUADTREE
//#define NSQUARED

void SimulateForceDirectedGraphQuadTree
(
	m_arena *Arena, v4f *NodeVelocities,
	v4f *NodePositions, int NodeCount,
	int *Froms, int *Tos, int LinkCount,
	f32 *Temperature,
	quadtree_header *QuadTree, 
	shared_wave_info *Shared
)
{
	u64 Start = OS_TimeStampNow();
	f32 K = 1.0f/sqrtf((f32)NodeCount);

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
	P.NodeVelocities = NodeVelocities;
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
			f32 DX = NodePositions[J].X - NodePositions[I].X;
			f32 DY = NodePositions[J].Y - NodePositions[I].Y;
			f32 DZ = NodePositions[J].Z - NodePositions[I].Z;
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

	Temperature[0] *= 0.99;

	//LogInfo("Time to simulate: ?f32", OS_MilliSecondsFromTimeStamp(OS_TimeStampNow() - Start));
}
