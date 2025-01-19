#pragma once
#include "maths.h"
#include "memory.h"


#include "draw.h"

#include "wave.h"

typedef struct fdg_node_info
{
	str8 Name;
	b32 IsFolder;
	f32 Radius;
} fdg_node_info;

typedef struct bounds
{
	v3f Min;
	v3f Max;
} bounds;

typedef u32 QuadTree_index;

typedef struct Quadrant
{
	QuadTree_index Children[8];
}
Quadrant;

typedef struct Quadrant_node
{
	v4f NodePosition;
	struct Quadrant_node *Next;
} quadrant_node;

typedef struct quadtree_header
{
	_Alignas(128) v4f            *Masses;
	int            *ChildrenCounts;
	quadrant_node **Nodes;
	bounds         *Bounds;
	Quadrant       *Quadrants;
	int             Count;
	int             NextQuadrantIndex;
} quadtree_header;

void SimulateForceDirectedGraphQuadTree(m_arena *Arena, v4f *NodePositions, int NodeCount,
								int *Froms, int *Tos, int LinkCount,
								f32 *Temperature,
								quadtree_header *QuadTree, shared_wave_info *Shared);

bounds GetBounds(v4f *Positions, int Count);

void QuadTreeBuild(m_arena *Arena, quadtree_header *QuadTree, QuadTree_index QuadrantIndex, v4f NodePosition, int Depth);
v4f QuadTreeCalculateAttributes(quadtree_header *QuadTree, int QuadrantIndex);

void DrawBounds(draw_context *Draw, m_arena *Arena, bounds Bounds);

void DrawQuadTree(draw_context *Draw, m_arena *Arena, quadtree_header *QuadTree, int QuadTreeIndex);
