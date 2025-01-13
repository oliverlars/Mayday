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

bounds GetBounds(fdg_node *Nodes, int NodeCount)
{
	v3f Min = v3f(FLT_MAX, FLT_MAX, FLT_MAX);
	v3f Max = v3f(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	for(int I = 0; I < NodeCount; I++)
	{
		Min.X = Min(Min.X, Nodes[I].TargetPosition.X);
		Min.Y = Min(Min.Y, Nodes[I].TargetPosition.Y);
		Min.Z = Min(Min.Z, Nodes[I].TargetPosition.Z);

		Max.X = Max(Max.X, Nodes[I].TargetPosition.X);
		Max.Y = Max(Max.Y, Nodes[I].TargetPosition.Y);
		Max.Z = Max(Max.Z, Nodes[I].TargetPosition.Z);

	}
	return (bounds){Min, Max};
}

void QuadTreeBuild(m_arena *Arena, quadtree_header *QuadTree, QuadTree_index QuadrantIndex, fdg_node *Node, int Depth)
{
	v3f Min = QuadTree->Bounds[QuadrantIndex].Min;
	v3f Max = QuadTree->Bounds[QuadrantIndex].Max;

	v3f Centre = DivV3F(AddV3F(Min, Max), v3f(2.0, 2.0, 2.0));


	int Index = 0;
	if(Node->TargetPosition.X >= Centre.X) { Index |= 1; }
	if(Node->TargetPosition.Y >= Centre.Y) { Index |= 2; }
	if(Node->TargetPosition.Z >= Centre.Z) { Index |= 4; }

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
		QuadTree->Nodes[ChildIndex]->Node = Node;
	}
	else
	{
		if(QuadTree->Nodes[ChildIndex] && ((Depth >= 64) ||
		   (fabs(Max.X - Min.X) < 0.1 || fabs(Max.Y - Min.Y) < 0.1) || fabs(Max.Z - Min.Z) < 0.1))
		{
			quadrant_node *LastNode = QuadTree->Nodes[ChildIndex];
			for(;LastNode->Next; LastNode = LastNode->Next);

			LastNode->Next = ArenaPushArrayZero(Arena, quadrant_node, 1);
			LastNode->Next->Node = Node;
		}
		else
		{
			quadrant_node *Existing = QuadTree->Nodes[ChildIndex];
			QuadTree->Nodes[ChildIndex] = Null;
			QuadTreeBuild(Arena, QuadTree, ChildIndex, Existing->Node, Depth + 1);
			QuadTreeBuild(Arena, QuadTree, ChildIndex, Node, Depth + 1);
		}
	}
}

v3f QuadTreeCalculateAttributes(quadtree_header *QuadTree, int QuadrantIndex)
{
	int ChildrenInThisQuadrant = 0;
	for(int I = 0; I < 8; I++)
	{
		if(QuadTree->Quadrants[QuadrantIndex].Children[I])
		{
			ChildrenInThisQuadrant++;
		}
	}

	v3f CentreOfMass = v3f(0.0, 0.0, 0.0);
	if(ChildrenInThisQuadrant == 0)
	{
		if(QuadTree->Nodes[QuadrantIndex])
		{
			QuadTree->ChildrenCounts[QuadrantIndex] = 1;
			quadrant_node *Node = QuadTree->Nodes[QuadrantIndex];
			int Count = 0;
			for(;Node; Node = Node->Next)
			{
				CentreOfMass = AddV3F(Node->Node->TargetPosition, CentreOfMass);
				Count++;
			}
			CentreOfMass = DivV3F(CentreOfMass, v3f((f32)Count, (f32)Count, (f32)Count));
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
				v3f ChildCentreOfMass = QuadTreeCalculateAttributes(QuadTree, QuadTree->Quadrants[QuadrantIndex].Children[I]);

				int ChildIndex = QuadTree->Quadrants[QuadrantIndex].Children[I];
				int ChildrenCount = QuadTree->ChildrenCounts[ChildIndex];
				TotalChildren += ChildrenCount;


				CentreOfMass.X += ChildCentreOfMass.X * (f32)ChildrenCount;
				CentreOfMass.Y += ChildCentreOfMass.Y * (f32)ChildrenCount;
				CentreOfMass.Z += ChildCentreOfMass.Z * (f32)ChildrenCount;

			}
		}
		CentreOfMass = DivV3F(CentreOfMass, v3f((f32)TotalChildren, (f32)TotalChildren, (f32)TotalChildren));
		QuadTree->ChildrenCounts[QuadrantIndex] = TotalChildren;
	}
	QuadTree->Masses[QuadrantIndex] = CentreOfMass;
	return CentreOfMass;
}

// https://patterns.eecs.berkeley.edu/?page_id=193
v3f ForceDisplacementFromPosition(quadtree_header *QuadTree, fdg_node *Node, f32 K)
{
	v3f Result = v3f(0.0);

	int QuadrantStack[1024];
	int StackTop = 0;
	int QuadrantIndex = 0;
	QuadrantStack[StackTop++] = QuadrantIndex;

	v3f Position = Node->TargetPosition;

	for(;StackTop > 0;)
	{
		QuadrantIndex = QuadrantStack[--StackTop];
		v3f Centre = QuadTree->Masses[QuadrantIndex];
		v3f PositionToCentre = v3f(Position.X - Centre.X, Position.Y - Centre.Y, Position.Z - Centre.Z);
		f64 DistanceSquared = DotV3F(PositionToCentre, PositionToCentre);
		f64 Distance = sqrt(DistanceSquared);
		f64 EffectiveDistance = Max((f32)Distance - Node->Radius, 0.1);
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
			if(QuadrantRadiusSquared/(EffectiveDistance*EffectiveDistance + 0.01) < 0.1)
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

typedef struct parallel_force_displacement
{
	v3f *Displacements;
	f32 K;
	fdg_node *Nodes;
	int NodeCount;

	quadtree_header *QuadTree;
} parallel_force_displacement;

void ParallelForceDisplacement(wave_info *WaveInfo, void* Input)
{

	parallel_force_displacement *P = Input;
	int Count = LaneValueFromTotal(WaveInfo, P->NodeCount);
	int Index = ExclusiveWavePrefixSum(WaveInfo, Count);

	for(int I = Index; I < Index + Count; I++)
	{
		P->Displacements[I] = ForceDisplacementFromPosition(P->QuadTree,
															P->Nodes + I,
															P->K);

	}
}
#define PARALLEL_QUADTREE
//#define QUADTREE
//#define NSQUARED

void SimulateForceDirectedGraphQuadTree(m_arena *Arena, fdg_node *Nodes, int NodeCount,
									  fdg_link *Links, int LinkCount,
									  f32 *Temperature,
									  quadtree_header *QuadTree, shared_wave_info *Shared)
{
	u64 Start = OS_TimeStampNow();
	f32 K = 5;

	v3f *Displacements = ArenaPushArrayZero(Arena, v3f, NodeCount);


#if defined(PARALLEL_QUADTREE)
	parallel_force_displacement P = {0};
	P.Displacements = Displacements;
	P.K = K;
	P.NodeCount = NodeCount;
	P.Nodes = Nodes;
	P.QuadTree = QuadTree;

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

	for(int I = 0; I < LinkCount; I++)
	{
		int A = Links[I].From;
		int B = Links[I].To;

		f32 DX = Nodes[A].TargetPosition.X - Nodes[B].TargetPosition.X;
		f32 DY = Nodes[A].TargetPosition.Y - Nodes[B].TargetPosition.Y;
		f32 DZ = Nodes[A].TargetPosition.Z - Nodes[B].TargetPosition.Z;
		f32 DistanceSquared = DX*DX + DY*DY + DZ*DZ;
		f32 Distance = sqrtf(DistanceSquared);

		f32 ReciprocalDistance = 1.0 / (Distance + 0.01);
		f32 NormDX = DX * ReciprocalDistance;
		f32 NormDY = DY * ReciprocalDistance;
		f32 NormDZ = DZ * ReciprocalDistance;

		f32 AttractiveForce = (DistanceSquared) / K;

		Displacements[A].X -= NormDX * AttractiveForce;
		Displacements[A].Y -= NormDY * AttractiveForce;
		Displacements[A].Z -= NormDZ * AttractiveForce;

		Displacements[B].X += NormDX * AttractiveForce;
		Displacements[B].Y += NormDY * AttractiveForce;
		Displacements[B].Z += NormDZ * AttractiveForce;

	}

	for(int I = 0; I < NodeCount; I++)
	{
		f32 Norm = sqrtf(Displacements[I].X * Displacements[I].X + Displacements[I].Y * Displacements[I].Y + Displacements[I].Z * Displacements[I].Z);
		if(Norm > 0)
		{
			Nodes[I].TargetPosition.X += (Displacements[I].X / Norm) * Min(Norm, Temperature[0]);
			Nodes[I].TargetPosition.Y += (Displacements[I].Y / Norm) * Min(Norm, Temperature[0]);
			Nodes[I].TargetPosition.Z += (Displacements[I].Z / Norm) * Min(Norm, Temperature[0]);
		}
	}

	Temperature[0] = LerpF32(Temperature[0], 0.0, 0.05);

	LogInfo("Time to simulate: ?f32", OS_MilliSecondsFromTimeStamp(OS_TimeStampNow() - Start));
}
