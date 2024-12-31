#include "maths.h"
#include "memory.h"


#include "draw.h"


// Force directed graph node
typedef struct fdg_node
{
	v3f TargetPosition;
	v3f Position;
	// These should probably not be stored here
	str8 Name;
	b32 IsFolder;
} fdg_node;

typedef struct fdg_link
{
	int From;
	int To;
} fdg_link;

typedef struct bounds
{
	v3f Min;
	v3f Max;
} bounds;

typedef struct octree
{
	struct octree  *Children[8];
	fdg_node *Node;
	bounds Bounds;
}
octree;

void SimulateForceDirectedGraphFast(m_arena *Arena, fdg_node *Nodes, int NodeCount,
								fdg_link *Links, int LinkCount,
								f32 *Temperature);

void SimulateForceDirectedGraphSIMD(m_arena *Arena, fdg_node *Nodes, int NodeCount,
								fdg_link *Links, int LinkCount,
								f32 *Temperature);

bounds GetBounds(fdg_node *Nodes, int NodeCount);

void OctreeBuild(m_arena *Arena, octree *Octant, fdg_node *Node);

void DrawBounds(draw_context *Draw, m_arena *Arena, bounds Bounds);

void DrawOctant(draw_context *Draw, m_arena *Arena, octree *Octant);