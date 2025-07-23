#include <stddef.h>

#include "maths.h"

#define VERTEX_ATTRIBUTE_FIELDS         \
    X(v4f, Position, POSITION, 0)   \
    X(v4f, Colour, COLOUR, 1)     \
    X(v4f, Normal, NORMAL, 2)     \
    X(v2f, UV, UV, 3)         \
    X(f32, PointSize, POINT_SIZE, 4)  \
    X(v2f, Start, START, 5)      \
    X(v2f, Control, CONTROL, 6)    \
    X(v2f, End, END, 7)        \
    X(f32, Thickness, THICKNESS, 8)  \
    X(u32, CurveStart, CURVE_START, 9) \
    X(u32, CurveCount, CURVE_COUNT, 10)


#define X(T, NAME, CAPS_NAME, INDEX) T NAME;
typedef struct r_vertex { VERTEX_ATTRIBUTE_FIELDS } r_vertex;
#undef X

#define X(T, NAME, CAPS_NAME, INDEX) R_VA_##CAPS_NAME = 1 << INDEX,
typedef enum r_vertex_attributes { VERTEX_ATTRIBUTE_FIELDS } r_vertex_attributes;
#undef X

#define X(T, NAME, CAPS_NAME, INDEX) R_VA_##CAPS_NAME,
static u64 G_VertexAttributes[] = { VERTEX_ATTRIBUTE_FIELDS };
#undef X

#define X(T, NAME, CAPS_NAME, INDEX) sizeof(T),
static u64 G_VertexAttributeSizes[] = { VERTEX_ATTRIBUTE_FIELDS };
#undef X

#define X(T, NAME, CAPS_NAME, INDEX) offsetof(r_vertex, NAME),
static u64 G_VertexAttributeOffsets[] = { VERTEX_ATTRIBUTE_FIELDS };
#undef X

typedef enum r_texture_format
{
	R_TEXTURE_R8,
	R_TEXTURE_D32,
	R_TEXTURE_RGBA8,
} r_texture_format;

typedef struct r_texture
{
	int Width;
	int Height;
	u8 *Data;
	r_texture_format Format;
} r_texture;

typedef enum r_command_type
{
	R_COMMAND_GLYPH,
	R_COMMAND_TRIANGLE,
	R_COMMAND_BLOOM,
	R_COMMAND_POINTS,
	R_COMMAND_LINES,
	R_COMMAND_BEZIER,
} r_command_type;

typedef struct r_command
{
	//NOTE(Oliver): not sure this is really necessary
	r_command_type Type;

	r_vertex *Vertices;
	int VerticesCount;

	s32 *Indices;
	int IndexCount;

	r_texture Texture;

	m44f Transform;
	v4i Scissor;

	r_vertex_attributes Attributes;

	struct r_command *Next;
}
r_command;

#define MAX_TRANSFORMS (4096)
#define MAX_SCISSORS   (4096)

typedef struct render_context
{
	m_arena *Arena;
	r_command *CommandsFirst;
	r_command *CommandsLast;

	int Frame;

	void *Backend;

	m44f TransformStack[MAX_TRANSFORMS];
	m44f InverseTransformStack[MAX_TRANSFORMS];
	int TransformCount;

	v4i ScissorStack[MAX_SCISSORS];
	int ScissorCount;

} render_context;

r_vertex_attributes R_AttributesFromCommandType(r_command_type CommandType);

void R_TextureFromBytes(render_context *R, u8 *Bytes, s32 Width, s32 Height);

void R_PushCommand(render_context *R, m_arena *Arena, r_command Command);

void R_PushTransform(render_context *Render, m44f Transform);
void R_PopTransform(render_context *Render);

void R_PushScissor(render_context *Render, v4i Scissor);
void R_PopScissor(render_context *Render);
v4i R_CurrentScissor(render_context *Render);

m44f R_CurrentTransform(render_context *Render);
m44f R_CurrentInverseTransform(render_context *Render);

void R_UpdateCurves(render_context *Render, v2f *CurvePoints, int CurvePointsCount, int CurvePointsSoFar);

s64 R_StrideFromAttributes(r_vertex_attributes Attributes);