#include <math.h>

#include "draw.h"
#include "log.h"
#include "maths.h"
#include "memory.h"
#include "misc.h"
#include "os.h"
#include "strings.h"
#include "ui.h"
#include "wave.h"



ui_orbit UI_OrbitView(ui_context *UI, draw_context *Draw, m_arena *Arena, os_directory_tree Tree)
{
	ui_orbit Result = {0};
	if(Tree.Count == 0) return Result;

	int Count = 0;
	for(os_directory_tree_iterator It = {0}; OS_DirectoryTreeIterate(Tree, &It);)
	{
		if(It.Depth == 1)
		{
			Count++;
		}
	}


	f32 Phi = (1.0 + sqrt(5.0)) / 2.0;
	f32 GoldenAngle = 2.0 * M_PI * (1.0 - 1.0 / Phi);


	v3f *Positions = ArenaPushArrayZero(Arena, v3f, Count);

	int Index = 0;
	for(os_directory_tree_iterator It = {0}; OS_DirectoryTreeIterate(Tree, &It);)
	{
		if(It.Depth == 1)
		{
			f32 Z = 1.0 - 2.0 * (((f32)Index + 0.5) / (f32)Count);
			f32 R = sqrt(1.0 - Z * Z);
			f32 theta = (f32)Index * GoldenAngle;
			Positions[Index].X = 15.0 * R * cos(theta);
			Positions[Index].Y = 15.0 * R * sin(theta);
			Positions[Index].Z = 15.0 * Z;
			Index++;
		}
	}

	Result.Count = Count;
	Result.Positions = Positions;

	D_DrawPointsArray(Draw, Arena, Positions, Count, v4f(1, 0, 0, 1));

	return Result;
}

ui_orbit_signal UI_OrbitLabels(ui_context *UI, draw_context *Draw, edit_context *Edit,
					d_font Font, m_arena *Arena, ui_orbit Orbit, os_directory_tree Tree,
					v3f MinPos, v3f MaxPos,
					int ActiveItem, m44f MVP, m44f ModelTransform, editable_buffer* SearchBuffer)
{
	ui_textbox_signal SearchSignal = UI_TextBox(Arena, Font, v2f(sapp_widthf()/2.0 - 500, 100), 1000,
												UI, Draw, Edit, 60, Str8("TreeViewTextBox"), SearchBuffer);

	ui_orbit_signal Result = {0};

	str8 Filter = SearchSignal.String;

	f32 FontSizes[2] = {10, 30};
	v3f Colours[2] = { v3f(0.8, 0.3, 0.0), v3f(0.4, 0.1, 0.0) };

	f32 DistanceBetweenMinAndMax = LengthV3F(SubV3F(MinPos, MaxPos));

	LogInfo("?f32", DistanceBetweenMinAndMax);

	m_scratch_arena Scratch = ScratchBegin(0, 0);
	int Index = 0;
	for(os_directory_tree_iterator It = {0};
		OS_DirectoryTreeIterate(Tree, &It);)
	{
		if(It.Depth != 1) continue;
		f32 FontSize = 30;
		v3f P = Orbit.Positions[Index++];

		v4f P4D = v4f(P.X, P.Y, P.Z, 1.0);
		P4D = MulM44FV4F(ModelTransform, P4D);

		f32 NormalizedDistance = (P4D.Z - MinPos.Z) / (MaxPos.Z - MinPos.Z);
		NormalizedDistance = NormalizedDistance * NormalizedDistance * (3.0f - 2.0f * NormalizedDistance);
		//LogInfo("ND: ?f32", NormalizedDistance);

		v4f Position4D = v4f(P.X, P.Y, P.Z, 1.0);
		v4f TransformedPoint = MulM44FV4F(M44FTranspose(MVP), Position4D);
		TransformedPoint.X /= TransformedPoint.W;
		TransformedPoint.Y /= TransformedPoint.W;
		TransformedPoint.X = TransformedPoint.X*0.5 + 0.5;
		TransformedPoint.Y = TransformedPoint.Y*0.5 + 0.5;

		TransformedPoint.X *= sapp_widthf();
		TransformedPoint.Y *= sapp_heightf();

		v4f Colour = v4f(0.8, 0.3, 0.0, 1);
		v3f C = LerpV3F(v3f(0, 0, 0), v3f(1, 1, 1), NormalizedDistance);
		Colour = v4f(C.X, C.Y, C.Z, 1.0);


		if(It.Index == ActiveItem)
		{
			Colour = v4f(0.8, 0.6, 0.4, 1.0);
			FontSize = 40;
		}


		if(It.Current->Type == OS_DIR_ENTRY_FOLDER)
		{
			str8 FolderName = Str8FromLastCharacter(Arena, It.Current->PathAndName, '/');
			if(Filter.Size == 0 || Str8FuzzyMatch(FolderName, Filter))
			{

				b32 Clicked = UI_TextButton(UI, Draw,
					Arena, Str8PushF(Arena, "?S##?u32", FolderName, It.Index),
					FontSize,
					v2f(TransformedPoint.X, TransformedPoint.Y), Colour);

				if(Clicked || (SearchSignal.Submitted && Filter.Size > 0))
				{
					Result.ActiveItem = It.Index;
					Result.ActiveItemDepth = It.Depth;
					Result.ActiveItemPosition = P;
					Result.Clicked = True;
					Result.Path = Str8Copy(Arena, It.Current->PathAndName);
				}
			}
		}

		{
			FontSize = 30;
			str8 FileName = Str8FromLastCharacter(Arena, It.Current->PathAndName, '/');
			if(It.Current->Type != OS_DIR_ENTRY_FOLDER)
			{
				if(Filter.Size == 0 || Str8FuzzyMatch(FileName, Filter))
				{
					b32 Clicked = UI_TextButton(UI, Draw,
						Arena, Str8PushF(Arena, "?S##?u32", FileName, It.Index),
						FontSize,
						v2f(TransformedPoint.X, TransformedPoint.Y), Colour);
					if(Clicked || (SearchSignal.Submitted && Filter.Size > 0))
					{
						Result.Clicked = True;
						Result.FileToLoad = Str8Copy(Arena, It.Current->PathAndName);
					}
				}
			}
		}
	}

	// if(UI_KeyDown(UI, SAPP_KEYCODE_RIGHT, 0))
	// {
	// 	Result.Clicked = True;
	// 	//Result.ActiveItem = ActiveItem + 1;
	// }

	// if(UI_KeyDown(UI, SAPP_KEYCODE_LEFT, 0))
	// {
	// 	Result.Clicked = True;
	// 	//Result.ActiveItem = Max(ActiveItem - 1, 0);
	// }

	ScratchEnd(Scratch);

	return Result;
}
