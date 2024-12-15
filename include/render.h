#include "memory.h"
#include "log.h"
#include "maths.h"

typedef enum r_vertex_format
{
	R_VF_POSITION = 1 << 0,
	R_FV_COLOUR   = 1 << 1,
	R_FV_NORMAL   = 1 << 2,
	R_FV_UV       = 1 << 3,
}
r_vertex_format;
typedef struct r_vertex
{
	v4f Position;
	v4f Colour;
	v4f Normal;
	v2f UV;
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

typedef struct r_command
{
	r_vertex *Vertices;
	int VertexCount;

	s32 *Indices;
	int IndexCount;

	r_texture Texture;

	struct r_command *Next;
}
r_command;

typedef struct r_context
{
	memory_arena *Arena;
	r_command *Commands;
	int MaxCommands;

	logger Logger;
}
r_context;

typedef struct r_options
{
}
r_options;

void R_Init(r_context *RenderContext);
void R_Update(r_context *RenderContext);