#include "maths.h"
#include "memory.h"


#include "draw.h"

#include "wave.h"


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


typedef u32 octree_index;

typedef struct octant
{
	octree_index Children[8];
}
octant;

typedef struct octant_node
{
	fdg_node *Node;
	struct octant_node *Next;
} octant_node;

typedef struct octree_header
{
	v3f          *Masses;
	int          *ChildrenCounts;
	octant_node **Nodes;
	bounds       *Bounds;
	octant       *Octants;
	int           Count;
	int           NextOctantIndex;
} octree_header;

void SimulateForceDirectedGraphOctree(m_arena *Arena, fdg_node *Nodes, int NodeCount,
								fdg_link *Links, int LinkCount,
								f32 *Temperature,
								octree_header *Octree, shared_wave_info *Shared);

void SimulateForceDirectedGraphFast(m_arena *Arena, fdg_node *Nodes, int NodeCount,
								fdg_link *Links, int LinkCount,
								f32 *Temperature);

void SimulateForceDirectedGraphSIMD(m_arena *Arena, fdg_node *Nodes, int NodeCount,
								fdg_link *Links, int LinkCount,
								f32 *Temperature);

bounds GetBounds(fdg_node *Nodes, int NodeCount);

void OctreeBuild(m_arena *Arena, octree_header *Octree, octree_index OctantIndex, fdg_node *Node, int Depth);
v3f OctreeCalculateAttributes(octree_header *Octree, int OctantIndex);

void DrawBounds(draw_context *Draw, m_arena *Arena, bounds Bounds);

void DrawOctree(draw_context *Draw, m_arena *Arena, octree_header *Octree, int OctreeIndex);