#include "maths.h"
#include "memory.h"
#include <math.h>


// Force directed graph node
typedef struct fdg_node
{
	v3f TargetPosition;
	v3f Position;
	str8 Name;
} fdg_node;

typedef struct fdg_link
{
	int From;
	int To;
} fdg_link;

f32 Fr(f32 X, f32 K);

f32 Fa(f32 X, f32 K);


void SimulateForceDirectedGraph(m_arena *Arena, fdg_node *Nodes, int NodeCount,
												fdg_link *Links, int LinkCount,
												f32 *Temperature);