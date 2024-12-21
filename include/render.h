#pragma once
#include "memory.h"
#include "log.h"
#include "maths.h"

typedef enum r_vertex_format
{
	R_VF_POSITION = 1 << 0,
	R_VF_COLOUR   = 1 << 1,
	R_VF_NORMAL   = 1 << 2,
	R_VF_UV       = 1 << 3,
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

typedef enum r_command_type
{
	R_COMMAND_GLYPH,
	R_COMMAND_TRIANGLE,
	R_COMMAND_BLOOM,
	R_COMMAND_POINTS,
} r_command_type;

typedef struct r_command
{
	//NOTE(Oliver): not sure this is really necessary
	r_command_type Type;

	r_vertex *Vertices;
	int VerticesCount;
	r_vertex_format VerticesFormat;

	s32 *Indices;
	int IndexCount;

	r_texture Texture;

	m44f Transform;

	struct r_command *Next;
}
r_command;

typedef struct render_context
{
	memory_arena *Arena;
	logger *Logger;

	r_command *CommandsFirst;
	r_command *CommandsLast;

	void *Backend;
}
render_context;

typedef struct r_options
{
}
r_options;

void R_Init(render_context *R);
void R_Update(render_context *R, memory_arena *FrameArena);

void R_TextureFromBytes(render_context *R, u8 *Bytes, s32 Width, s32 Height);

void R_PushCommand(render_context *R, memory_arena *Arena, r_command Command);