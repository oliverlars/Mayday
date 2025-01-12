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
	v3f *Lines = (v3f*)ArenaPushArrayZero(Arena, v3f, 4*2);

	v2f Min = Bounds.Min;
	v2f Max = Bounds.Max;

	Lines[0] = v3f(Min.X, Min.Y, 0.0);
	Lines[1] = v3f(Min.X, Max.Y, 0.0);

	Lines[2] = v3f(Min.X, Min.Y, 0.0);
	Lines[3] = v3f(Max.X, Min.Y, 0.0);

	Lines[4] = v3f(Min.X, Max.Y, 0.0);
	Lines[5] = v3f(Max.X, Max.Y, 0.0);

	Lines[6] = v3f(Max.X, Min.Y, 0.0);
	Lines[7] = v3f(Max.X, Max.Y, 0.0);

	D_DrawLinesArray(Draw, Arena, Lines, 4*2, v4f(0, 0.1, 0, 1));
}

void DrawQuadTree(draw_context *Draw, m_arena *Arena, quadtree_header *QuadTree, int QuadTreeIndex)
{
	DrawBounds(Draw, Arena, QuadTree->Bounds[QuadTreeIndex]);

	for(int I = 0; I < 4; I++)
	{
		if(QuadTree->Quadrants[QuadTreeIndex].Children[I])
		{
			DrawQuadTree(Draw, Arena, QuadTree, QuadTree->Quadrants[QuadTreeIndex].Children[I]);
		}
	}
}

bounds GetBounds(fdg_node *Nodes, int NodeCount)
{
	v2f Min = v2f(FLT_MAX, FLT_MAX);
	v2f Max = v2f(-FLT_MAX, -FLT_MAX);
	for(int I = 0; I < NodeCount; I++)
	{
		Min.X = Min(Min.X, Nodes[I].TargetPosition.X);
		Min.Y = Min(Min.Y, Nodes[I].TargetPosition.Y);

		Max.X = Max(Max.X, Nodes[I].TargetPosition.X);
		Max.Y = Max(Max.Y, Nodes[I].TargetPosition.Y);

	}

	return (bounds){Min, Max};
}

void QuadTreeBuild(m_arena *Arena, quadtree_header *QuadTree, QuadTree_index QuadrantIndex, fdg_node *Node, int Depth)
{
	v2f Min = QuadTree->Bounds[QuadrantIndex].Min;
	v2f Max = QuadTree->Bounds[QuadrantIndex].Max;

	v2f Centre = DivV2F(AddV2F(Min, Max), v2f(2.0, 2.0));


	int Index = 0;
	if(Node->TargetPosition.X >= Centre.X) { Index |= 1; }
	if(Node->TargetPosition.Y >= Centre.Y) { Index |= 2; }

	bounds ChildBounds[4] =
	{
		{ v2f(Min.X, Min.Y),          v2f(Centre.X, Centre.Y) },
		{ v2f(Centre.X, Min.Y),       v2f(Max.X, Centre.Y)    },
		{ v2f(Min.X, Centre.Y),       v2f(Centre.X, Max.Y)    },
		{ v2f(Centre.X, Centre.Y),    v2f(Max.X, Max.Y)       },
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
		if((QuadTree->Nodes[ChildIndex] && (Depth >= 64)) ||
		   ((Max.X - Min.X) < 0.1 || (Max.Y - Min.Y) < 0.1))
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

v2f QuadTreeCalculateAttributes(quadtree_header *QuadTree, int QuadrantIndex)
{
	int ChildrenInThisQuadrant = 0;
	for(int I = 0; I < 4; I++)
	{
		if(QuadTree->Quadrants[QuadrantIndex].Children[I])
		{
			ChildrenInThisQuadrant++;
		}
	}

	v2f CentreOfMass = v2f(0.0, 0.0);
	if(ChildrenInThisQuadrant == 0)
	{
		if(QuadTree->Nodes[QuadrantIndex])
		{
			QuadTree->ChildrenCounts[QuadrantIndex] = 1;
			quadrant_node *Node = QuadTree->Nodes[QuadrantIndex];
			int Count = 0;
			for(;Node; Node = Node->Next)
			{
				CentreOfMass = AddV2F(Node->Node->TargetPosition, CentreOfMass);
				Count++;
			}
			CentreOfMass = DivV2F(CentreOfMass, v2f((f32)Count, (f32)Count));
		}
		else
		{
			QuadTree->ChildrenCounts[QuadrantIndex] = 0;
		}
	}
	else
	{
		int TotalChildren = 0;
		for(int I = 0; I < 4; I++)
		{
			if(QuadTree->Quadrants[QuadrantIndex].Children[I])
			{
				v2f ChildCentreOfMass = QuadTreeCalculateAttributes(QuadTree, QuadTree->Quadrants[QuadrantIndex].Children[I]);

				int ChildIndex = QuadTree->Quadrants[QuadrantIndex].Children[I];
				int ChildrenCount = QuadTree->ChildrenCounts[ChildIndex];
				TotalChildren += ChildrenCount;


				CentreOfMass.X += ChildCentreOfMass.X * (f32)ChildrenCount;
				CentreOfMass.Y += ChildCentreOfMass.Y * (f32)ChildrenCount;

			}
		}
		CentreOfMass = DivV2F(CentreOfMass, v2f((f32)TotalChildren, (f32)TotalChildren));
		QuadTree->ChildrenCounts[QuadrantIndex] = TotalChildren;
	}
	QuadTree->Masses[QuadrantIndex] = CentreOfMass;
	return CentreOfMass;
}

// https://patterns.eecs.berkeley.edu/?page_id=193
v2f ForceDisplacementFromPosition(quadtree_header *QuadTree, int QuadrantIndex, fdg_node *Node, f32 K)
{
	v2f Result = v2f(0.0);

	int QuadrantStack[1024];
	int StackTop = 0;
	QuadrantStack[StackTop++] = QuadrantIndex;

	v2f Position = Node->TargetPosition;

	for(;StackTop > 0;)
	{
		QuadrantIndex = QuadrantStack[--StackTop];
		v2f Centre = QuadTree->Masses[QuadrantIndex];
		v2f PositionToCentre = v2f(Position.X - Centre.X, Position.Y - Centre.Y);
		f64 DistanceSquared = PositionToCentre.X*PositionToCentre.X + PositionToCentre.Y*PositionToCentre.Y;
		f64 Distance = sqrt(DistanceSquared);
		f64 EffectiveDistance = Max((f32)Distance - Node->Radius, 1.0);
		if(QuadTree->ChildrenCounts[QuadrantIndex] == 1)
		{
			f64 ForceMagnitude = (K) / (EffectiveDistance*EffectiveDistance + 0.01);

			Result.X += ForceMagnitude * PositionToCentre.X;
			Result.Y += ForceMagnitude * PositionToCentre.Y;
		}
		else
		{
			v2f MaxSubMin = SubV2F(QuadTree->Bounds[QuadrantIndex].Max, QuadTree->Bounds[QuadrantIndex].Min);
			f32 QuadrantRadiusSquared = MaxSubMin.X*MaxSubMin.X + MaxSubMin.Y*MaxSubMin.Y;
			if(QuadrantRadiusSquared/(EffectiveDistance*EffectiveDistance + 0.01) < 0.5)
			{
				f32 ForceMagnitude = QuadTree->ChildrenCounts[QuadrantIndex] * (K) / (EffectiveDistance*EffectiveDistance + 0.01);

				Result.X += ForceMagnitude * PositionToCentre.X;
				Result.Y += ForceMagnitude * PositionToCentre.Y;
			}
			else
			{
				for(int I = 0; I < 4; I++)
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
	v2f *Displacements;
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
														   0,
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
	f32 K = 5.0;

	v2f *Displacements = ArenaPushArrayZero(Arena, v2f, NodeCount);


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
														 0, Nodes[I].TargetPosition,
														 K);
	}
#elif defined(NSQUARED)

	for(int I = 0; I < NodeCount; I++)
	{
		for(int J = I+1; J < NodeCount; J++)
		{
			f32 DX = Nodes[J].TargetPosition.X - Nodes[I].TargetPosition.X;
			f32 DY = Nodes[J].TargetPosition.Y - Nodes[I].TargetPosition.Y;
			f32 DistanceSquared = DX * DX + DY * DY;

			Displacements[I].X -= (K) * DX / Max(DistanceSquared, 0.01);
			Displacements[I].Y -= (K) * DY / Max(DistanceSquared, 0.01);

			Displacements[J].X += (K) * DX / Max(DistanceSquared, 0.01);
			Displacements[J].Y += (K) * DY / Max(DistanceSquared, 0.01);
		}
	}
#endif

	for(int I = 0; I < LinkCount; I++)
	{
		int A = Links[I].From;
		int B = Links[I].To;

		f32 DX = Nodes[A].TargetPosition.X - Nodes[B].TargetPosition.X;
		f32 DY = Nodes[A].TargetPosition.Y - Nodes[B].TargetPosition.Y;
		v2f Delta = v2f(DX, DY);
		Delta = NormalizeV2F(Delta);;
		f32 Distance = LengthV2F(Delta);

		if(Distance > 0)
		{
			f32 AttractiveForce = Distance * Distance / K;
			f32 NormDX = DX / Distance;
			f32 NormDY = DY / Distance;

			Displacements[A].X -= NormDX * AttractiveForce;
			Displacements[A].Y -= NormDY * AttractiveForce;

			Displacements[B].X += NormDX * AttractiveForce;
			Displacements[B].Y += NormDY * AttractiveForce;
		}
	}

	for(int I = 0; I < NodeCount; I++)
	{
		f32 Norm = sqrtf(Displacements[I].X * Displacements[I].X + Displacements[I].Y * Displacements[I].Y);
		if(Norm > 0)
		{
			Nodes[I].TargetPosition.X += (Displacements[I].X / Norm) * Min(Norm, Temperature[0]);
			Nodes[I].TargetPosition.Y += (Displacements[I].Y / Norm) * Min(Norm, Temperature[0]);
		}
	}

	Temperature[0] = LerpF32(Temperature[0], 0.001, 0.05);

	LogInfo("Time to simulate: ?f32", OS_MilliSecondsFromTimeStamp(OS_TimeStampNow() - Start));
}
