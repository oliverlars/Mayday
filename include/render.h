

typedef struct r_command
{

} r_command;

typedef struct r_context
{
	r_command *Commands;
	int MaxCommands;
};

typedef struct r_options
{
}
r_options;

void R_Init();
void R_Update();