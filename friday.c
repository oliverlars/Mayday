#include "friday.h"
#include "memory.h"

Entry(InputWaveInfo)
{

	wave_info *WaveInfo = (wave_info*)InputWaveInfo;
	shared_wave_info *Shared = WaveInfo->Shared;

	memory_arena Arena = {0};
	Arena.Start = MemoryReserve(GigaBytes(64));

	operating_system OS = GetOperatingSystem();
	build_options BuildOptions = {0};
	b32 PrintHelpMenu = False;
	b32 ShouldRun = False;
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
			else if(Str8Equal(Arg, Str8("run"))) { ShouldRun = True; }
			else if(Str8Equal(Arg, Str8("msvc")))
			{
				if(OS == OS_WINDOWS)
				{
					Compiler = COMPILER_MSVC;
				}
				else
				{
					printf(F_WARN "This compiler choice is only supported on Windows...\n");
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
		str8_list ExtraCompileFlags = {0};
		Str8ListAppend(&Arena, &IncludePaths, Str8("include/"));
		Str8ListAppend(&Arena, &IncludePaths, Str8("external/"));
		Str8ListAppend(&Arena, &IncludePaths, Str8("embed/"));
		AppendSourceFilesInFolder(&Arena, &SourceFiles, Str8("source"));
		AppendSourceFilesInFolder(&Arena, &SourceFiles, Str8("source/ui"));

		str8_list Fonts = {0};
		Str8ListAppend(&Arena, &Fonts, Str8("fonts/BerkeleyMono-Regular.ttf"));
		Str8ListAppend(&Arena, &Fonts, Str8("fonts/BebasNeue-Regular.ttf"));
		Str8ListAppend(&Arena, &Fonts, Str8("fonts/BasementGrotesque.otf"));
		Str8ListAppend(&Arena, &Fonts, Str8("fonts/Doto.ttf"));
		Str8ListAppend(&Arena, &Fonts, Str8("fonts/cocogoose-pro.block-border.ttf"));
		Str8ListAppend(&Arena, &Fonts, Str8("fonts/cocogoose-pro.block-gradient.ttf"));
		Str8ListAppend(&Arena, &Fonts, Str8("fonts/cocogoose-pro.regular.ttf"));

		str8_list FontVariableNames = {0};
		Str8ListAppend(&Arena, &FontVariableNames, Str8("G_BerkeleyMonoRegular"));
		Str8ListAppend(&Arena, &FontVariableNames, Str8("G_BebasNeueRegular"));
		Str8ListAppend(&Arena, &FontVariableNames, Str8("G_BasementGrotesque"));
		Str8ListAppend(&Arena, &FontVariableNames, Str8("G_Doto"));
		Str8ListAppend(&Arena, &FontVariableNames, Str8("G_CocogooseBorder"));
		Str8ListAppend(&Arena, &FontVariableNames, Str8("G_CocogooseGradient"));
		Str8ListAppend(&Arena, &FontVariableNames, Str8("G_CocogooseRegular"));


		EmbedFiles(&Arena, Fonts, Str8("embed/fonts.c"), FontVariableNames);

		str8_list Shaders = {0};
		str8_list ShaderVariableNames = {0};

		if(OS == OS_WINDOWS)
		{
			str8_list Entries = ListOfDirectoryEntriesFromPath(&Arena, Str8("shaders/hlsl/"));
			for(str8_list_node *File = Entries.First; File; File = File->Next)
			{
				if(Str8EndsWith(File->String, Str8(".hlsl")))
				{
					str8 FileName = Str8UpToLastCharacter(&Arena, File->String, '.');
					Str8ListAppend(&Arena, &Shaders, Str8PushF(
						&Arena, "%.*s/%.*s",
						Str8ToPrintfArgs(Str8("shaders/hlsl/")),
						Str8ToPrintfArgs(File->String)
					));
					str8 CapitalFileName = FileName;
					CapitalFileName.Bytes[0] = CapitalFileName.Bytes[0] - 'a' + 'A';
					Str8ListAppend(&Arena, &ShaderVariableNames, Str8PushF(
						&Arena,
						"G_%.*sShader", Str8ToPrintfArgs(CapitalFileName))
					);
				}
			}
		}
		else
		{
			str8_list Entries = ListOfDirectoryEntriesFromPath(&Arena, Str8("shaders/msl/"));
			for(str8_list_node *File = Entries.First; File; File = File->Next)
			{
				if(Str8EndsWith(File->String, Str8(".metal")))
				{
					str8 FileName = Str8UpToLastCharacter(&Arena, File->String, '.');
					Str8ListAppend(&Arena, &Shaders, Str8PushF(
						&Arena, "%.*s/%.*s",
						Str8ToPrintfArgs(Str8("shaders/msl/")),
						Str8ToPrintfArgs(File->String)
					));
					str8 CapitalFileName = FileName;
					CapitalFileName.Bytes[0] = CapitalFileName.Bytes[0] - 'a' + 'A';
					Str8ListAppend(&Arena, &ShaderVariableNames, Str8PushF(
						&Arena,
						"G_%.*sShader", Str8ToPrintfArgs(CapitalFileName))
					);
				}
			}
		}

		EmbedFiles(&Arena, Shaders, Str8("embed/shaders.c"), ShaderVariableNames);

		AppendSourceFilesInFolder(&Shared->Arena, &SourceFiles, Str8("embed/"));

		if(OS == OS_WINDOWS)
		{
			AppendSourceFilesInFolder(&Shared->Arena, &SourceFiles, Str8("source/win32"));
			AppendSourceFilesInFolder(&Shared->Arena, &SourceFiles, Str8("external"));
			AppendSourceFilesInFolder(&Shared->Arena, &SourceFiles, Str8("external/win32"));
			Str8ListAppend(&Shared->Arena, &ExtraCompileFlags, Str8(" /D_WIN64 /experimental:c11atomics /std:c17 "));
			Str8ListAppend(&Shared->Arena, &ExtraLinkFlags, Str8(" user32.lib gdi32.lib kernel32.lib opengl32.lib Shell32.lib Synchronization.lib dxguid.lib d3dcompiler.lib "));
		}
		else if(OS == OS_DARWIN)
		{
			AppendSourceFilesInFolder(&Shared->Arena, &SourceFiles, Str8("source/darwin"));
			AppendSourceFilesInFolder(&Shared->Arena, &SourceFiles, Str8("external"));
			AppendSourceFilesInFolder(&Shared->Arena, &SourceFiles, Str8("external/darwin"));
			Str8ListAppend(&Shared->Arena, &ExtraLinkFlags, Str8("-framework Cocoa -framework MetalKit -framework QuartzCore -framework Metal -framework MetalPerformanceShaders"));
		}

		BuildOptions = (build_options){
			.Compiler = Compiler,
			.OutputName = Str8PushF(&Arena, "mayday"),
			.OutputType = OUTPUT_EXECUTABLE,
			.SourceFiles = SourceFiles,
			.IncludePaths = IncludePaths,
			.ExtraLinkFlags = ExtraLinkFlags,
			.ExtraCompileFlags = ExtraCompileFlags,
			.WorkingDirectory = GetExecutableDirectory(&Arena),
			.OutputPath = Str8PushF(&Arena, "build/"),
			.Optimise = OptimisedBuild,
			.FullRebuild = Rebuild,
		};

		EmitCompileCommandsJson(&Arena, BuildOptions);
	}

	BroadcastToAllLanes(WaveInfo, BuildOptions);
	BroadcastToAllLanes(WaveInfo, PrintHelpMenu);
	BroadcastToAllLanes(WaveInfo, ShouldRun);

	b32 FailedToBuild = False;
	if(!PrintHelpMenu)
	{
		FailedToBuild = Build(WaveInfo, &Arena, BuildOptions);
	}

	if(WaveIsFirstLane(WaveInfo))
	{
		str8 PathToExecutable = Str8PushF(&Arena, "%.*s/%.*s",
			Str8ToPrintfArgs(BuildOptions.OutputPath),
			Str8ToPrintfArgs(BuildOptions.OutputName));
		
		if(OS == OS_DARWIN)
		{
			str8 NewPathToExecutable = Str8("build/Mayday.app/Contents/MacOS/Mayday");
			MoveFileOnDisk(&Arena, PathToExecutable, NewPathToExecutable);
			PathToExecutable = NewPathToExecutable;
		}
		
		str8 PathToBundle = Str8("build/Mayday");

		if(ShouldRun && !FailedToBuild)
		{
			StartApplication(&Arena, PathToExecutable);
		}

		if(!FailedToBuild && OS == OS_DARWIN)
		{
			if(PathExists(&Arena, Str8("build/Icon.iconset")))
			{
				b32 FailedToExecute = False;
				b32 ProcessSucceeded = ExecuteCommand(&Arena, Str8("iconutil -c icns build/Icon.iconset"), &FailedToExecute);
				if(!FailedToExecute && ProcessSucceeded)
				{
					MoveFileOnDisk(&Arena, Str8("build/Icon.icns"), Str8("build/Mayday.app/Contents/Resources/Icon.icns"));
				}
			}
			else
			{
				printf(F_ERROR "build/Icon.iconset folder not found. Put \n");
			}
		}
	}

	return 0;
}

int main(s32 ArgCount, char **Args)
{


	memory_arena SharedArena = {0};
	SharedArena.Start = MemoryReserve(GigaBytes(64));

	shared_wave_info *Shared = (shared_wave_info*)ArenaPush(&SharedArena, sizeof(shared_wave_info));
	Shared->ArgCount = ArgCount;
	Shared->Args = Args;
	Shared->Arena = SharedArena;
	Dispatch(Shared, GetNumberOfCPUs());
	WaveBarrier(Shared->Barrier);
}
