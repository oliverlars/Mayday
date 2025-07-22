#include "render_common.h"
#include "misc.h"


s64 R_StrideFromAttributes(r_vertex_attributes Attributes)
{

	s64 Stride = 0;
	for(int I = 0; I < ArrayCount(G_VertexAttributeSizes); I++)
	{
		if(Attributes & G_VertexAttributes[I])
		{
			Stride += G_VertexAttributeSizes[I];
		}
	}
	return Stride;
}


r_vertex_attributes R_AttributesFromCommandType(r_command_type CommandType)
{
	r_vertex_attributes Result = 0;
	switch(CommandType)
	{
		case R_COMMAND_TRIANGLE:
		{
			Result |= R_VA_POSITION;
			Result |= R_VA_COLOUR;
		} break;
		case R_COMMAND_POINTS:
		{
			Result |= R_VA_POSITION;
			Result |= R_VA_COLOUR;
		} break;
		case R_COMMAND_BEZIER:
		{
			Result |= R_VA_POSITION;
			Result |= R_VA_COLOUR;
			Result |= R_VA_START;
			Result |= R_VA_CONTROL;
			Result |= R_VA_END;
			Result |= R_VA_THICKNESS;
		} break;
		case R_COMMAND_GLYPH:
		{
			Result |= R_VA_POSITION;
			Result |= R_VA_COLOUR;
			Result |= R_VA_UV;
			Result |= R_VA_CURVE_START;
			Result |= R_VA_CURVE_COUNT;
		} break;
		case R_COMMAND_BLOOM:
		{

		} break;
		case R_COMMAND_LINES:
		{
			Result |= R_VA_POSITION;
			Result |= R_VA_COLOUR;
			Result |= R_VA_START;
			Result |= R_VA_END;
			Result |= R_VA_THICKNESS;
		} break;
	}

	return Result;
}

void R_PushCommand(render_context *R, m_arena *Arena, r_command Command)
{
	r_command *NextCommand = (r_command*)ArenaPushZero(Arena, sizeof(r_command));
	NextCommand[0] = Command;
	if(!R->CommandsLast)
	{
		R->CommandsFirst = NextCommand;
		R->CommandsLast = NextCommand;
	}
	else
	{
		R->CommandsLast->Next = NextCommand;
		R->CommandsLast = NextCommand;
	}
	assert(R->TransformCount >= 1);
	NextCommand->Transform = R->TransformStack[R->TransformCount - 1];
	NextCommand->Scissor = R->ScissorStack[R->ScissorCount - 1];
}

void R_PushTransform(render_context *Render, m44f Transform)
{
	assert(Render->TransformCount < MAX_TRANSFORMS);
	Render->TransformStack[Render->TransformCount] = Transform;
	Render->InverseTransformStack[Render->TransformCount] = M44FInverse(Transform);
	Render->TransformCount += 1;
}

void R_PopTransform(render_context *Render)
{
	assert(Render->TransformCount >= 1);
	--Render->TransformCount;
}

void R_PushScissor(render_context *Render, v4i Scissor)
{
	assert(Render->ScissorCount < MAX_SCISSORS);
	Render->ScissorStack[Render->ScissorCount] = Scissor;
	Render->ScissorCount += 1;
}

void R_PopScissor(render_context *Render)
{
	assert(Render->ScissorCount >= 1);
	--Render->ScissorCount;
}

v4i R_CurrentScissor(render_context *Render)
{
	assert(Render->ScissorCount >= 1);
	return Render->ScissorStack[Render->ScissorCount - 1];
}

m44f R_CurrentTransform(render_context *Render)
{
	assert(Render->TransformCount >= 1);
	return Render->TransformStack[Render->TransformCount - 1];
}

m44f R_CurrentInverseTransform(render_context *Render)
{
	assert(Render->TransformCount >= 1);
	return Render->InverseTransformStack[Render->TransformCount - 1];
}
