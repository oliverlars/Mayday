#pragma once
#include "maths.h"
#include "memory.h"


#include "draw.h"

#include "wave.h"


// Force directed graph node
typedef struct fdg_node
{
	v2f TargetPosition;
	v2f Position;
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
	v2f Min;
	v2f Max;
} bounds;


typedef u32 QuadTree_index;

typedef struct Quadrant
{
	QuadTree_index Children[4];
}
Quadrant;

typedef struct Quadrant_node
{
	fdg_node *Node;
	struct Quadrant_node *Next;
} quadrant_node;

typedef struct quadtree_header
{
	v2f          *Masses;
	int          *ChildrenCounts;
	quadrant_node **Nodes;
	bounds       *Bounds;
	Quadrant       *Quadrants;
	int           Count;
	int           NextQuadrantIndex;
} quadtree_header;

void SimulateForceDirectedGraphQuadTree(m_arena *Arena, fdg_node *Nodes, int NodeCount,
								fdg_link *Links, int LinkCount,
								f32 *Temperature,
								quadtree_header *QuadTree, shared_wave_info *Shared);

bounds GetBounds(fdg_node *Nodes, int NodeCount);

void QuadTreeBuild(m_arena *Arena, quadtree_header *QuadTree, QuadTree_index QuadrantIndex, fdg_node *Node, int Depth);
v2f QuadTreeCalculateAttributes(quadtree_header *QuadTree, int QuadrantIndex);

void DrawBounds(draw_context *Draw, m_arena *Arena, bounds Bounds);

void DrawQuadTree(draw_context *Draw, m_arena *Arena, quadtree_header *QuadTree, int QuadTreeIndex);