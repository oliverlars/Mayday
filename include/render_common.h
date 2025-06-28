#include "maths.h"


typedef struct r_vertex
{
	v4f Position;
	v4f Colour;
	v4f Normal;
	v2f UV;
	f32 PointSize;
	v2f Start;
	v2f End;
	f32 Width;
	f32 Height;
} r_vertex;

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

	struct r_command *Next;
}
r_command;

#define MAX_TRANSFORMS (4096)
#define MAX_SCISSORS   (4096)

typedef struct render_context
{
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

void R_TextureFromBytes(render_context *R, u8 *Bytes, s32 Width, s32 Height);

void R_PushCommand(render_context *R, m_arena *Arena, r_command Command);

void R_PushTransform(render_context *Render, m44f Transform);
void R_PopTransform(render_context *Render);

void R_PushScissor(render_context *Render, v4i Scissor);
void R_PopScissor(render_context *Render);
v4i R_CurrentScissor(render_context *Render);

m44f R_CurrentTransform(render_context *Render);
m44f R_CurrentInverseTransform(render_context *Render);