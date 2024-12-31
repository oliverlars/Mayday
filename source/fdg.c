#include "fdg.h"
#include <float.h>
#include <math.h>

#include "log.h"
#include "maths.h"
#include "memory.h"
#include "os.h"

#include "draw.h"

#include <arm_neon.h>

void DrawBounds(draw_context *Draw, m_arena *Arena, bounds Bounds)
{
	v3f *Lines = (v3f*)ArenaPushZero(Arena, 12*2*sizeof(v3f));

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

	D_DrawLinesArray(Draw, Arena, Lines, 24, v4f(0, 1, 0, 1));
}

void DrawOctant(draw_context *Draw, m_arena *Arena, octree *Octant)
{
	if(!Octant) return;
	DrawBounds(Draw, Arena, Octant->Bounds);
	
	for(int I = 0; I < 8; I++)
	{
		DrawOctant(Draw, Arena, Octant->Children[I]);
	}
}

bounds GetBounds(fdg_node *Nodes, int NodeCount)
{
	v3f Min = v3f(FLT_MAX, FLT_MAX, FLT_MAX);
	v3f Max = v3f(FLT_MIN, FLT_MIN, FLT_MIN);
	for(int I = 0; I < NodeCount; I++)
	{
		if(Nodes[I].TargetPosition.X < Min.X)
		{
			Min.X = Nodes[I].TargetPosition.X;
		}
		if(Nodes[I].TargetPosition.Y < Min.Y)
		{
			Min.Y = Nodes[I].TargetPosition.Y;
		}
		if(Nodes[I].TargetPosition.Z < Min.Z)
		{
			Min.Z = Nodes[I].TargetPosition.Z;
		}

		if(Nodes[I].TargetPosition.X > Max.X)
		{
			Max.X = Nodes[I].TargetPosition.X;
		}
		if(Nodes[I].TargetPosition.Y > Max.Y)
		{
			Max.Y = Nodes[I].TargetPosition.Y;
		}
		if(Nodes[I].TargetPosition.Z > Max.Z)
		{
			Max.Z = Nodes[I].TargetPosition.Z;
		}
	}
	return (bounds){Min, Max};
}

void OctreeBuild(m_arena *Arena, octree *Octant, fdg_node *Node)
{
	v3f Min = Octant->Bounds.Min;
	v3f Max = Octant->Bounds.Max;

	v3f Centre = DivV3F(AddV3F(Min, Max), V3FromF32(2.0));


	int Index = 0;
	if(Node->TargetPosition.X > Centre.X) { Index |= 1; }
	if(Node->TargetPosition.Y > Centre.Y) { Index |= 2; }
	if(Node->TargetPosition.Z > Centre.Z) { Index |= 4; }

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

	if(!Octant->Children[Index])
	{
		Octant->Children[Index] = (octree*)ArenaPushTypeZero(Arena, octree);
		Octant->Children[Index]->Bounds = ChildBounds[Index];
	}

	if(Octant->Children[Index]->Node == Null)
	{
		Octant->Children[Index]->Node = Node;
	}
	else
	{
		fdg_node *Existing = Octant->Children[Index]->Node;
		Octant->Children[Index]->Node = Null;
		OctreeBuild(Arena, Octant->Children[Index], Existing);
		OctreeBuild(Arena, Octant->Children[Index], Node);
	}
}

void SimulateForceDirectedGraphFast(m_arena *Arena, fdg_node *Nodes, int NodeCount,
								fdg_link *Links, int LinkCount,
								f32 *Temperature)
{
	u64 Start = OS_TimeStampNow();
	f32 K = sqrtf(2.0 / NodeCount);

	v3f *Displacements = (v3f*)ArenaPushZero(Arena, sizeof(v3f) * NodeCount);
	for(int I = 0; I < NodeCount; I++)
	{
		for(int J = I + 1; J < NodeCount; J++)
		{

			f32 DX = Nodes[I].TargetPosition.X - Nodes[J].TargetPosition.X;
			f32 DY = Nodes[I].TargetPosition.Y - Nodes[J].TargetPosition.Y;
			f32 DZ = Nodes[I].TargetPosition.Z - Nodes[J].TargetPosition.Z;
			f32 DistanceSquared = DX * DX + DY * DY + DZ * DZ;

			Displacements[I].X += (K * K ) * DX / Max(DistanceSquared, 0.01);
			Displacements[I].Y += (K * K ) * DY / Max(DistanceSquared, 0.01);
			Displacements[I].Z += (K * K ) * DZ / Max(DistanceSquared, 0.01);
			
			Displacements[J].X -= (K * K ) * DX / Max(DistanceSquared, 0.01);
			Displacements[J].Y -= (K * K ) * DY / Max(DistanceSquared, 0.01);
			Displacements[J].Z -= (K * K ) * DZ / Max(DistanceSquared, 0.01);
		}
	}

	for(int I = 0; I < LinkCount; I++)
	{
		int A = Links[I].From;
		int B = Links[I].To;

		f32 DX = Nodes[A].TargetPosition.X - Nodes[B].TargetPosition.X;
		f32 DY = Nodes[A].TargetPosition.Y - Nodes[B].TargetPosition.Y;
		f32 DZ = Nodes[A].TargetPosition.Z - Nodes[B].TargetPosition.Z;
		f32 Distance = sqrtf(DX * DX + DY * DY + DZ * DZ);

		
		{
			f32 AttractiveForce = Distance * Distance / K;
			f32 NormDX = DX / Distance;
			f32 NormDY = DY / Distance;
			f32 NormDZ = DZ / Distance;

			Displacements[A].X -= NormDX * AttractiveForce;
			Displacements[A].Y -= NormDY * AttractiveForce;
			Displacements[A].Z -= NormDZ * AttractiveForce;

			Displacements[B].X += NormDX * AttractiveForce;
			Displacements[B].Y += NormDY * AttractiveForce;
			Displacements[B].Z += NormDZ * AttractiveForce;
		}
	}

	for(int I = 0; I < NodeCount; I++)
	{
		f32 Norm = sqrtf(Displacements[I].X * Displacements[I].X + Displacements[I].Y * Displacements[I].Y + Displacements[I].Z * Displacements[I].Z);
		LogInfo("?s32 ?f32", I, Norm);
		
		{
			Nodes[I].TargetPosition.X += (Displacements[I].X / Norm) * fmin(Norm, Temperature[0]);
			Nodes[I].TargetPosition.Y += (Displacements[I].Y / Norm) * fmin(Norm, Temperature[0]);
			Nodes[I].TargetPosition.Z += (Displacements[I].Z / Norm) * fmin(Norm, Temperature[0]);
		}
	}

	Temperature[0] *= 0.95;

	LogInfo("Time to simulate: ?f32", OS_MilliSecondsFromTimeStamp(OS_TimeStampNow() - Start));
}

#if 0
void SimulateForceDirectedGraphSIMD(m_arena *Arena, fdg_node *AOSNodes, int NodeCount,
									fdg_link *Links, int LinkCount,
									f32 *Temperature)
{
	u64 Start = OS_TimeStampNow();

	float32x4_t K = vdupq_n_f32(sqrtf(2.0 / NodeCount));

	int AlignedNodeCount =  (NodeCount + 3) & ~0x3;
	int AlignedLinkCount = (LinkCount + 3) & ~0x3;

	float32x4_t *PositionsX = (float32x4_t*)ArenaPushZero(Arena, sizeof(float32x4_t) * AlignedNodeCount/4);
	float32x4_t *PositionsY = (float32x4_t*)ArenaPushZero(Arena, sizeof(float32x4_t) * AlignedNodeCount/4);
	float32x4_t *PositionsZ = (float32x4_t*)ArenaPushZero(Arena, sizeof(float32x4_t) * AlignedNodeCount/4);

	for(int I = 0; I < AlignedNodeCount; I += 4)
	{
		v3f Positions[4] = {0};
		Positions[0] = AOSNodes[I + 0].TargetPosition;
		Positions[1] = AOSNodes[I + 1].TargetPosition;
		Positions[2] = AOSNodes[I + 2].TargetPosition;
		Positions[3] = AOSNodes[I + 3].TargetPosition;

		PositionsX[I/4] = (float32x4_t){Positions[0].X, Positions[1].X, Positions[2].X, Positions[3].X};
		PositionsY[I/4] = (float32x4_t){Positions[0].Y, Positions[1].Y, Positions[2].Y, Positions[3].Y};
		PositionsZ[I/4] = (float32x4_t){Positions[0].Z, Positions[1].Z, Positions[2].Z, Positions[3].Z};
	}

	float32x4_t *DisplacementsX = (float32x4_t*)ArenaPushZero(Arena, sizeof(float32x4_t) * AlignedNodeCount/4);
	float32x4_t *DisplacementsY = (float32x4_t*)ArenaPushZero(Arena, sizeof(float32x4_t) * AlignedNodeCount/4);
	float32x4_t *DisplacementsZ = (float32x4_t*)ArenaPushZero(Arena, sizeof(float32x4_t) * AlignedNodeCount/4);
	for(int I = 0; I < AlignedNodeCount/4; I++)
	{
		for(int J = I + 1; J < AlignedNodeCount/4; J++)
		{

			float32x4_t PositionsBX = (float32( ((f32*)PositionsX) + J*4);
			float32x4_t PositionsBY = (float32( ((f32*)PositionsY) + J*4);
			float32x4_t PositionsBZ = (float32( ((f32*)PositionsZ) + J*4);

			float32x4_t PositionsAX = ((float32x4_t*)((f32*)PositionsX))[I];
			float32x4_t PositionsAY = ((float32x4_t*)((f32*)PositionsY))[I];
			float32x4_t PositionsAZ = ((float32x4_t*)((f32*)PositionsZ))[I];

			float32x4_t DX = vsubq_f32(PositionsAX, PositionsBX);
			float32x4_t DY = vsubq_f32(PositionsAY, PositionsBY);
			float32x4_t DZ = vsubq_f32(PositionsAZ, PositionsBZ);
			
			float32x4_t DXSquared = vmulq_f32(DX, DX);
			float32x4_t DYSquared = vmulq_f32(DY, DY);
			float32x4_t DZSquared = vmulq_f32(DZ, DZ);

			float32x4_t DistanceSquared = vaddq_f32(DXSquared, vaddq_f32(DYSquared, DZSquared));
			float32x4_t KSquared = vmulq_f32(K, K);
			DisplacementsX[I] = vaddq_f32(DisplacementsX[I],  vdivq_f32(vmulq_f32(KSquared, DX), vmaxq_f32(DistanceSquared, vdupq_n_f32(0.01))));
			DisplacementsY[I] = vaddq_f32(DisplacementsY[I],  vdivq_f32(vmulq_f32(KSquared, DY), vmaxq_f32(DistanceSquared, vdupq_n_f32(0.01))));
			DisplacementsZ[I] = vaddq_f32(DisplacementsZ[I],  vdivq_f32(vmulq_f32(KSquared, DZ), vmaxq_f32(DistanceSquared, vdupq_n_f32(0.01))));

			DisplacementsX[J] = vsubq_f32(DisplacementsX[J],  vdivq_f32(vmulq_f32(KSquared, DX), vmaxq_f32(DistanceSquared, vdupq_n_f32(0.01))));
			DisplacementsY[J] = vsubq_f32(DisplacementsY[J],  vdivq_f32(vmulq_f32(KSquared, DY), vmaxq_f32(DistanceSquared, vdupq_n_f32(0.01))));
			DisplacementsZ[J] = vsubq_f32(DisplacementsZ[J],  vdivq_f32(vmulq_f32(KSquared, DZ), vmaxq_f32(DistanceSquared, vdupq_n_f32(0.01))));
			
		}
	}

	float32x4_t LaneMasks[4] =
	{
		{1.0f, 0.0f, 0.0f, 0.0f},
		{0.0f, 1.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 1.0f, 0.0f},
		{0.0f, 0.0f, 0.0f, 1.0f} 
	};


	for(int I = 0; I < AlignedLinkCount; I++)
	{
		int VectorA = Links[I].From / 4;
		int LaneA =   Links[I].From % 4;
		int VectorB = Links[I].To / 4;
		int LaneB =   Links[I].To % 4;

		float32x4_t PositionsBX = ((float32x4_t*)((f32*)PositionsX + LaneB))[VectorB];
		float32x4_t PositionsBY = ((float32x4_t*)((f32*)PositionsY + LaneB))[VectorB];
		float32x4_t PositionsBZ = ((float32x4_t*)((f32*)PositionsZ + LaneB))[VectorB];

		float32x4_t PositionsAX = ((float32x4_t*)((f32*)PositionsX + LaneA))[VectorA];
		float32x4_t PositionsAY = ((float32x4_t*)((f32*)PositionsY + LaneA))[VectorA];
		float32x4_t PositionsAZ = ((float32x4_t*)((f32*)PositionsZ + LaneA))[VectorA];

		float32x4_t DX = vsubq_f32(PositionsAX, PositionsBX);
		float32x4_t DY = vsubq_f32(PositionsAY, PositionsBY);
		float32x4_t DZ = vsubq_f32(PositionsAZ, PositionsBZ);
		
		float32x4_t DXSquared = vmulq_f32(DX, DX);
		float32x4_t DYSquared = vmulq_f32(DY, DY);
		float32x4_t DZSquared = vmulq_f32(DZ, DZ);
		float32x4_t DistanceSquared = vaddq_f32(DXSquared, vaddq_f32(DYSquared, DZSquared));
		float32x4_t Distance = vsqrtq_f32(DistanceSquared);
		
		float32x4_t AttractiveForce = vdivq_f32(DistanceSquared, K);
		float32x4_t NormDX = vdivq_f32(DX, Distance);
		float32x4_t NormDY = vdivq_f32(DY, Distance);
		float32x4_t NormDZ = vdivq_f32(DZ, Distance);

		DisplacementsX[VectorA] = vsubq_f32(DisplacementsX[VectorA], vmulq_f32(LaneMasks[LaneA], vmulq_f32(NormDX, AttractiveForce)));
		DisplacementsY[VectorA] = vsubq_f32(DisplacementsY[VectorA], vmulq_f32(LaneMasks[LaneA], vmulq_f32(NormDY, AttractiveForce)));
		DisplacementsZ[VectorA] = vsubq_f32(DisplacementsZ[VectorA], vmulq_f32(LaneMasks[LaneA], vmulq_f32(NormDZ, AttractiveForce)));

		DisplacementsX[VectorB] = vaddq_f32(DisplacementsX[VectorB], vmulq_f32(LaneMasks[LaneB], vmulq_f32(NormDX, AttractiveForce)));
		DisplacementsY[VectorB] = vaddq_f32(DisplacementsY[VectorB], vmulq_f32(LaneMasks[LaneB], vmulq_f32(NormDY, AttractiveForce)));
		DisplacementsZ[VectorB] = vaddq_f32(DisplacementsZ[VectorB], vmulq_f32(LaneMasks[LaneB], vmulq_f32(NormDZ, AttractiveForce)));
	}

	for(int I = 0; I < AlignedNodeCount; I += 4)
	{
		int Index = I / 4;
		float32x4_t DXSquared = vmulq_f32(DisplacementsX[Index], DisplacementsX[Index]);
		float32x4_t DYSquared = vmulq_f32(DisplacementsY[Index], DisplacementsY[Index]);
		float32x4_t DZSquared = vmulq_f32(DisplacementsZ[Index], DisplacementsZ[Index]);

		float32x4_t DSquared = vaddq_f32(DXSquared, vaddq_f32(DYSquared, DZSquared));

		float32x4_t Norm = vsqrtq_f32(vaddq_f32(DXSquared, vaddq_f32(DYSquared, DZSquared)));
		float32x4_t Factor =  vminq_f32(Norm, vdupq_n_f32(Temperature[0]));

		AOSNodes[I + 0].TargetPosition.X += vgetq_lane_f32(vmulq_f32(vdivq_f32(DisplacementsX[Index], Norm), Factor), 0);
		AOSNodes[I + 1].TargetPosition.X += vgetq_lane_f32(vmulq_f32(vdivq_f32(DisplacementsX[Index], Norm), Factor), 1);
		AOSNodes[I + 2].TargetPosition.X += vgetq_lane_f32(vmulq_f32(vdivq_f32(DisplacementsX[Index], Norm), Factor), 2);
		AOSNodes[I + 3].TargetPosition.X += vgetq_lane_f32(vmulq_f32(vdivq_f32(DisplacementsX[Index], Norm), Factor), 3);

		AOSNodes[I + 0].TargetPosition.Y += vgetq_lane_f32(vmulq_f32(vdivq_f32(DisplacementsY[Index], Norm), Factor), 0);
		AOSNodes[I + 1].TargetPosition.Y += vgetq_lane_f32(vmulq_f32(vdivq_f32(DisplacementsY[Index], Norm), Factor), 1);
		AOSNodes[I + 2].TargetPosition.Y += vgetq_lane_f32(vmulq_f32(vdivq_f32(DisplacementsY[Index], Norm), Factor), 2);
		AOSNodes[I + 3].TargetPosition.Y += vgetq_lane_f32(vmulq_f32(vdivq_f32(DisplacementsY[Index], Norm), Factor), 3);

		AOSNodes[I + 0].TargetPosition.Z += vgetq_lane_f32(vmulq_f32(vdivq_f32(DisplacementsZ[Index], Norm), Factor), 0);
		AOSNodes[I + 1].TargetPosition.Z += vgetq_lane_f32(vmulq_f32(vdivq_f32(DisplacementsZ[Index], Norm), Factor), 1);
		AOSNodes[I + 2].TargetPosition.Z += vgetq_lane_f32(vmulq_f32(vdivq_f32(DisplacementsZ[Index], Norm), Factor), 2);
		AOSNodes[I + 3].TargetPosition.Z += vgetq_lane_f32(vmulq_f32(vdivq_f32(DisplacementsZ[Index], Norm), Factor), 3);
	}

	Temperature[0] *= 0.95;

	LogInfo("Time to simulate: ?f32", OS_MilliSecondsFromTimeStamp(OS_TimeStampNow() - Start));
}
#endif