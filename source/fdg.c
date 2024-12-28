#include "fdg.h"

f32 Fr(f32 X, f32 K) {
	return (K * K) / X;
}

f32 Fa(f32 X, f32 K) {
	return (X * X) / K;
}


void SimulateForceDirectedGraph(m_arena *Arena, fdg_node *Nodes, int NodeCount,
								fdg_link *Links, int LinkCount,
								f32 *Temperature)
{
	f32 Area = 5.0;
	f32 K = sqrtf(5.0 / NodeCount);

	v2f *Displacements = (v2f*)ArenaPushZero(Arena, sizeof(v2f) * NodeCount);
	for(int I = 0; I < NodeCount; I++)
	{
		for(int J = 0; J < NodeCount; J++)
		{
			if(I == J) continue;

			f32 DX = Nodes[I].TargetPosition.X - Nodes[J].TargetPosition.X;
			f32 DY = Nodes[I].TargetPosition.Y - Nodes[J].TargetPosition.Y;
			f32 Distance = sqrtf(DX * DX + DY * DY);

			if(Distance > 0)
			{
				f32 RepulsiveForce = Fr(Distance, K);
				Displacements[I].X += (DX / Distance) * RepulsiveForce;
				Displacements[I].Y += (DY / Distance) * RepulsiveForce;
			}
		}
	}

	for(int I = 0; I < LinkCount; I++)
	{
		int A = Links[I].From;
		int B = Links[I].To;

		f32 DX = Nodes[A].TargetPosition.X - Nodes[B].TargetPosition.X;
		f32 DY = Nodes[A].TargetPosition.Y - Nodes[B].TargetPosition.Y;
		f32 Distance = sqrtf(DX * DX + DY * DY);

		if(Distance > 0)
		{
			f32 AttractiveForce = Fa(Distance, K);
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
			Nodes[I].TargetPosition.X += (Displacements[I].X / Norm) * fmin(Norm, Temperature[0]);
			Nodes[I].TargetPosition.Y += (Displacements[I].Y / Norm) * fmin(Norm, Temperature[0]);
		}
	}

	Temperature[0] *= 0.95;
}