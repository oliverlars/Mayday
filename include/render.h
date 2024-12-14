#include "log.h"

typedef struct r_command
{

}
r_command;

typedef struct r_context
{
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