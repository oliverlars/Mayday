#include "friday.h"

Entry(InputWaveInfo)
{

	wave_info *WaveInfo = (wave_info*)InputWaveInfo;
	shared_wave_info *Shared = WaveInfo->Shared;

	memory_arena Arena = {0};
	Arena.Start = MemoryReserve(GigaBytes(64));

	operating_system OS = GetOperatingSystem();
	build_options BuildOptions = {0};
	b32 PrintHelpMenu = False;
	if(WaveIsFirstLane(WaveInfo))
	{
		b32 OptimisedBuild = False;
		b32 Rebuild = False;
		compiler Compiler = COMPILER_PLATFORM_DEFAULT;
		for(s64 ArgIndex = 1; ArgIndex < Shared->ArgCount; ArgIndex++)
		{
			str8 Arg = Str8PushF(&Arena, "%s", Shared->Args[ArgIndex]);
			if(0) {}
			else if(Str8Equal(Arg, Str8("release"))) { OptimisedBuild = True; }
			else if(Str8Equal(Arg, Str8("rebuild"))) { Rebuild = True; }
			else if(Str8Equal(Arg, Str8("msvc")))
			{
				if(OS == OS_WINDOWS)
				{
					Compiler = COMPILER_MSVC;
				}
				else
				{
					printf(STR_COLOUR_YELLOW "WARNING " STR_COLOUR_RESET);
					printf("This compiler choice is only supported on Windows...\n");
					printf("Using platform default\n\n");
				}
			}
			else if(Str8Equal(Arg, Str8("clang")))
			{
				Compiler = COMPILER_CLANG;
			}
			else if(Str8Equal(Arg, Str8("gcc")))
			{
				if(OS == OS_DARWIN || OS == OS_LINUX)
				{
					Compiler = COMPILER_GCC;
				}
				else
				{
					printf(STR_COLOUR_RED "WARNING " STR_COLOUR_RESET);
					printf("This compiler choice is only supported on Mac and Linux...\n");
					printf("Using platform default\n\n");
				}
			}
			else if(Str8Equal(Arg, Str8("help"))) { 
				str8_list Help = {0};
				Str8ListAppend(&Arena, &Help, Str8PushF(&Arena, "\nFriday MK%s\n\n", FRIDAY_VERSION));
				Str8ListAppend(&Arena, &Help, Str8("Flags: \n"));
				Str8ListAppend(&Arena, &Help, Str8("  - rebuild\n"));
				Str8ListAppend(&Arena, &Help, Str8("      Rebuild all specified source files\n"));
				Str8ListAppend(&Arena, &Help, Str8("  - debug \n"));
				Str8ListAppend(&Arena, &Help, Str8("      The default, builds a non-optimised build with debug information\n"));
				Str8ListAppend(&Arena, &Help, Str8("  - release \n"));
				Str8ListAppend(&Arena, &Help, Str8("      Builds an optimised build without debug information\n"));
				Str8ListAppend(&Arena, &Help, Str8("  - msvc \n"));
				Str8ListAppend(&Arena, &Help, Str8("      Choose to compile with msvc (Windows only)\n"));
				Str8ListAppend(&Arena, &Help, Str8("  - clang \n"));
				Str8ListAppend(&Arena, &Help, Str8("      Choose to compile with clang\n"));
				Str8ListAppend(&Arena, &Help, Str8("  - gcc \n"));
				Str8ListAppend(&Arena, &Help, Str8("      Choose to compile with gcc (Mac and Linux only)\n"));
				Str8ListAppend(&Arena, &Help, Str8("  - help \n"));
				Str8ListAppend(&Arena, &Help, Str8("      Prints out help menu\n"));

				printf("%.*s", Str8ToPrintfArgs(Str8ListJoin(&Arena, Help)));
				PrintHelpMenu = True;
			}
		}

		str8_list IncludePaths = {0};
		str8_list SourceFiles = {0};
		str8_list ExtraLinkFlags = {0};
		Str8ListAppend(&Arena, &IncludePaths, Str8("include/"));
		Str8ListAppend(&Arena, &IncludePaths, Str8("external/"));
		Str8ListAppend(&Arena, &IncludePaths, Str8("embed/"));
		AppendSourceFilesInFolder(&Arena, &SourceFiles, Str8("source"));

		if(OS == OS_WINDOWS)
		{
			AppendSourceFilesInFolder(&Shared->Arena, &SourceFiles, Str8("source/win32"));
		}
		else if(OS == OS_DARWIN)
		{
			AppendSourceFilesInFolder(&Shared->Arena, &SourceFiles, Str8("source/darwin"));
			AppendSourceFilesInFolder(&Shared->Arena, &SourceFiles, Str8("source/sokol"));
			AppendSourceFilesInFolder(&Shared->Arena, &SourceFiles, Str8("external"));
			Str8ListAppend(&Shared->Arena, &ExtraLinkFlags, Str8("-framework Cocoa -framework MetalKit -framework QuartzCore -framework Metal -framework MetalPerformanceShaders"));
		}

		EmbedFile(&Arena, Str8("fonts/BerkeleyMono-Regular.ttf"),
						  Str8("embed/berkeley_mono_font.h"),
						  Str8("G_BerkeleyMonoRegular"));

		str8_list Shaders = {0};
		Str8ListAppend(&Arena, &Shaders, Str8("shaders/triangle.metal"));
		Str8ListAppend(&Arena, &Shaders, Str8("shaders/glyph.metal"));
		Str8ListAppend(&Arena, &Shaders, Str8("shaders/bloom.metal"));

		str8_list ShaderVariableNames = {0};
		Str8ListAppend(&Arena, &ShaderVariableNames, Str8("G_TriangleShader"));
		Str8ListAppend(&Arena, &ShaderVariableNames, Str8("G_GlyphShader"));
		Str8ListAppend(&Arena, &ShaderVariableNames, Str8("G_BloomShader"));

		EmbedFiles(&Arena, Shaders, Str8("embed/shaders.h"), ShaderVariableNames);

		BuildOptions = (build_options){
			.Compiler = Compiler,
			.OutputName = Str8PushF(&Arena, "mayday"),
			.OutputType = OUTPUT_EXECUTABLE,
			.SourceFiles = SourceFiles,
			.IncludePaths = IncludePaths,
			.ExtraLinkFlags = ExtraLinkFlags,
			.WorkingDirectory = GetExecutableDirectory(&Arena),
			.OutputPath = Str8PushF(&Arena, "build/"),
			.Optimise = OptimisedBuild,
			.FullRebuild = Rebuild,
		};

		EmitCompileCommandsJson(&Arena, BuildOptions);
	}

	BroadcastToAllLanes(WaveInfo, BuildOptions);
	BroadcastToAllLanes(WaveInfo, PrintHelpMenu);

	if(!PrintHelpMenu)
	{
		Build(WaveInfo, &Arena, BuildOptions);
	}

	return 0;
}

int main(s32 ArgCount, char **Args)
{
	shared_wave_info Shared = {0};
	Shared.ArgCount = ArgCount;
	Shared.Args = Args;

	memory_arena SharedArena = {0};
	SharedArena.Start = MemoryReserve(GigaBytes(64));
	Shared.Arena = SharedArena;
	Dispatch(&Shared, GetNumberOfCPUs());
}