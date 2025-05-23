#define _USE_MATH_DEFINES
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

/*

BIG NOTE!!!

There's a clear algorithm here that should essentially look like this:

- We open a folder that the user requested
- We render the root folder initially at (0, 0, 0)
- All of its immediate children follow the fibonacci lattice sequence on the surface
- If one of these children are clicked either open the file or enter the subdirectory
- If a subdirectory is selected, show the immediate children of that subdirectory as well as the siblings
- This is recursive


We essentially need a way to say:
- Get orbital/fibonacci positions for a given depth
- Increase depth for every subdir visited
- Decrease depth for every scroll up
- Orbit selected (index + depth) look up

Using OS directory code for this seems like a problem, but maybe it's fine. Ideally
we only need to process Depth +/- 2 levels and not need to traverse the entire tree everytime


*/

ui_orbit_node_array UI_OrbitNodesFromTreeDepth(m_arena *Arena, os_directory_tree Tree, int Depth, f32 Scale)
{
	int DepthCount = 0;
	for(os_directory_tree_iterator It = {0}; OS_DirectoryTreeIterate(Tree, &It);)
	{
		if(It.Depth == Depth)
		{
			DepthCount++;
		}
	}

	ui_orbit_node_array Result = {0};
	Result.Items = ArenaPushArrayZero(Arena, ui_orbit_node, DepthCount);
	Result.Count = DepthCount;

	int Index = 0;
	for(os_directory_tree_iterator It = {0}; OS_DirectoryTreeIterate(Tree, &It);)
	{
		if(It.Depth == Depth)
		{
			ui_orbit_node Node = {0};

			f32 Phi = (1.0 + sqrt(5.0)) / 2.0;
			f32 GoldenAngle = 2.0 * M_PI * (1.0 - 1.0 / Phi);
			f32 Z = 1.0 - 2.0 * (((f32)Index + 0.5) / (f32)DepthCount);
			f32 R = sqrt(1.0 - Z * Z);
			f32 theta = (f32)Index * GoldenAngle;
			Node.Position.X = Scale * R * cos(theta);
			Node.Position.Y = Scale * R * sin(theta);
			Node.Position.Z = Scale * Z;
			Node.Path = Str8Copy(Arena, It.Current->PathAndName);
			Result.Items[Index++] = Node;
		}
	}

	return Result;
}

ui_orbit UI_OrbitView(ui_context *UI, draw_context *Draw, m_arena *Arena,
					  os_directory_tree Tree, int ActiveIndex, int Depth, f32 Radius)
{
	ui_orbit Result = {0};
	if(Tree.Count == 0) return Result;

	ui_orbit_node_array OrbitNodes= UI_OrbitNodesFromTreeDepth(Arena, Tree, Depth, Radius);


	int PositionIndex = 0;
	v3f PositionOfActive = v3f(0);
	for(os_directory_tree_iterator It = {0}; OS_DirectoryTreeIterate(Tree, &It);)
	{
		if(It.Depth == Depth)
		{
			if(It.Index == ActiveIndex)
			{
				PositionOfActive = OrbitNodes.Items[PositionIndex].Position;
				break;
			}
			PositionIndex++;
		}
	}

	Result.ActivePosition = PositionOfActive;

	v3f *Positions =  ArenaPushArrayZero(Arena, v3f, OrbitNodes.Count);
	for(int I = 0; I < OrbitNodes.Count; I++)
	{
		Positions[I] = OrbitNodes.Items[I].Position;
	}

	D_DrawPointsArray(Draw, Arena, Positions, OrbitNodes.Count, 20, v4f(1, 0, 0, 1));

	Result.Positions = Positions;
	Result.Count = OrbitNodes.Count;

	return Result;
}

void UI_Slider(ui_context *UI, draw_context *Draw, m_arena *Arena, str8 String,
				  v2f Position, f32 Width, f32 *Value, v4f Colour)
{
	ui_widget *Widget = UI_WidgetFromString(UI, String);
	Assert(Value);
	u64 Hash = Widget->Hash;

	f32 Height = Width*0.2;

	v2f Min = v2f(Position.X, Position.Y - Height);
	v2f Max = v2f(Min.X + Width, Position.Y);

	v4f MouseNDC = v4f(UI->MousePosition.X, UI->MousePosition.Y, 0, 1);
	MouseNDC.X = (MouseNDC.X/sapp_widthf())*2.0 - 1.0;
	MouseNDC.Y = (MouseNDC.Y/sapp_heightf())*2.0 -1.0;

	v4f MouseWorld = MulM44FV4F(M44FTranspose(R_CurrentInverseTransform(Draw->Render)), MouseNDC);
	v2f MP = v2f(MouseWorld.X / MouseWorld.W, MouseWorld.Y / MouseWorld.W);

	if(MP.X >= Min.X && MP.X <= (Max.X) &&
	   MP.Y >= Min.Y && MP.Y <= (Max.Y))
	{
		UI->Hot = Hash;
	}

	if(UI->Hot == Hash)
	{
		f32 Offset = (MP.X - Min.X)/(Max.X - Min.X);
		D_DrawQuad(Draw, Arena, v2f(Position.X + Offset*Width, Position.Y - Height*0.8), 1, Height*0.8, v4f(1, 1, 1, 1));
	}
	else
	{
		f32 Offset = Value[0];
		D_DrawQuad(Draw, Arena, v2f(Position.X + Offset*Width, Position.Y - Height*0.8), 1, Height*0.8, v4f(0.3, 0.3, .3, 1.0));
	}

	if(UI->Active && UI->MouseIsDragging)
	{
		Value[0] = Clamp((MP.X - Min.X)/(Max.X - Min.X), 0, 1);
	}

	D_DrawQuad(Draw, Arena, Position, Width, 1, Colour);
	D_DrawQuad(Draw, Arena, v2f(Position.X, Position.Y - Height), 1, Height, Colour);
	D_DrawQuad(Draw, Arena, v2f(Position.X + Width, Position.Y - Height), 1, Height, Colour);

	D_DrawQuad(Draw, Arena, v2f(Position.X + Width/2.0, Position.Y - Height/2.0), 1, Height/2.0, Colour);

	D_DrawQuad(Draw, Arena, v2f(Position.X + 3*Width/4.0, Position.Y - Height/2.0), 1, Height/2.0, Colour);
	D_DrawQuad(Draw, Arena, v2f(Position.X + Width/4.0, Position.Y - Height/2.0), 1, Height/2.0, Colour);


}

ui_orbit_signal UI_OrbitLabels(ui_context *UI, draw_context *Draw, edit_context *Edit,
					d_font Font, d_font LogoFont, m_arena *Arena, ui_orbit Orbit, os_directory_tree Tree,
					v3f CameraPosition, v3f CameraForward, f32 MinDepth, f32 MaxDepth,
					int ActiveItem, int ActiveDepth,
					m44f MVP, m44f ModelTransform, editable_buffer* SearchBuffer)
{
	ui_textbox_signal SearchSignal = UI_TextBox(Arena, Font, v2f(sapp_widthf()/2.0 - 500, 100), 1000,
												UI, Draw, Edit, 60, Str8("TreeViewTextBox"), SearchBuffer);

	ui_orbit_signal Result = {0};

	str8 Filter = SearchSignal.String;

	f32 FontSizes[2] = {10, 30};
	v3f Colours[2] = { v3f(0.8, 0.3, 0.0), v3f(0.4, 0.1, 0.0) };

	m_scratch_arena Scratch = ScratchBegin(0, 0);
	int Index = 0;
	for(os_directory_tree_iterator It = {0};
		OS_DirectoryTreeIterate(Tree, &It);)
	{
		if(It.Depth != ActiveDepth) continue;
		f32 FontSize = 30;
		v3f P = Orbit.Positions[Index++];

		v4f P4D = v4f(P.X, P.Y, P.Z, 1.0);
		P4D = Mul(ModelTransform, P4D);
		v3f P3D = v3f(P4D.X, P4D.Y, P4D.Z);

		f32 Depth = Dot(Sub(P3D, CameraPosition), CameraForward);
		f32 HalfWayDepth = (MaxDepth - MinDepth) / 2.0;

		f32 NormalizedDistance = ClampF32((Depth - MinDepth) / (MaxDepth - MinDepth), 0.0, 1.0);

		v4f Position4D = v4f(P.X, P.Y, P.Z, 1.0);
		v4f TransformedPoint = Mul(M44FTranspose(MVP), Position4D);
		TransformedPoint.X /= TransformedPoint.W;
		TransformedPoint.Y /= TransformedPoint.W;
		TransformedPoint.X = TransformedPoint.X*0.5 + 0.5;
		TransformedPoint.Y = TransformedPoint.Y*0.5 + 0.5;

		TransformedPoint.X *= sapp_widthf();
		TransformedPoint.Y *= sapp_heightf();

		v4f Colour = v4f(0.8, 0.3, 0.0, 1);
		v3f C = LerpV3F(v3f(0, 0, 0), v3f(0.5, 0.2, 0.0), NormalizedDistance*NormalizedDistance);
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
				ui_signal Button = {0};
				if(NormalizedDistance > 0.5)
				{
					Button = UI_TextButton(UI, Draw,
						Arena, Str8PushF(Arena, "?S##?u32", FolderName, It.Index),
						FontSize,
						v2f(TransformedPoint.X, TransformedPoint.Y), Colour);
				}

				if((Button.Hover || Button.HotAnimation > 0))
				{
					Result.AnyHovering |= Button.Hover;
					str8 LastModified = OS_GetLastModifiedTime(Arena, It.Current->PathAndName);
					UI_PushStyle(UI, (ui_style){.Font = LogoFont});
					UI_Label(UI, Draw,
						Arena, Str8PushF(Arena, "?S##FolderHover ?u32", FolderName, It.Index),
						200,
						v2f(100, 1000), v4f(0.02, 0.005, 0.0, 1.0*Button.HotAnimation));
					UI_Label(UI, Draw,
						Arena, Str8PushF(Arena, "?S##?u32", LastModified, It.Index),
						80,
						v2f(120, 900), v4f(0.5, 0.1, 0.0, 1*Button.HotAnimation));
					UI_PopStyle(UI);
				}

				if(Button.Clicked || (SearchSignal.Submitted && Filter.Size > 0))
				{
					Result.ActiveEntry = It.Current;
					Result.ActiveItem = Index;
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

					ui_signal Button = {0};
					if(NormalizedDistance > 0.5)
					{
						Button = UI_TextButton(UI, Draw, Arena,
											   Str8PushF(Arena, "?S##?u32", FileName, It.Index),
											   FontSize, v2f(TransformedPoint.X, TransformedPoint.Y), Colour);
					}
					else
					{
						UI_Label(UI, Draw, Arena,
								 Str8PushF(Arena, "?S##?u32", FileName, It.Index),
								 FontSize, v2f(TransformedPoint.X, TransformedPoint.Y), Colour);
					}

					if((Button.Hover || Button.HotAnimation > 0))
					{
						Result.AnyHovering |= Button.Hover;
						str8 LastModified = OS_GetLastModifiedTime(Arena, It.Current->PathAndName);
						UI_PushStyle(UI, (ui_style){.Font = LogoFont});
						UI_Label(UI, Draw,
							Arena, Str8PushF(Arena, "?S##Hover ?u32", FileName, It.Index),
							200,
							v2f(100, 1000), v4f(0.02, 0.005, 0.0, 1.0*Button.HotAnimation));
						UI_Label(UI, Draw,
							Arena, Str8PushF(Arena, "?S##?u32", LastModified, It.Index),
							80,
							v2f(120, 900), v4f(0.5, 0.1, 0.0, 1*Button.HotAnimation));
						UI_PopStyle(UI);
					}

					if(Button.Clicked || (SearchSignal.Submitted && Filter.Size > 0))
					{
						Result.Clicked = True;
						Result.FileToLoad = Str8Copy(Arena, It.Current->PathAndName);
					}
				}
			}
		}
	}

	if(UI_KeyDown(UI, SAPP_KEYCODE_RIGHT, 0))
	{
		Result.Clicked = True;
		//Result.ActiveItem = ActiveItem + 1;
	}

	if(UI_KeyDown(UI, SAPP_KEYCODE_LEFT, 0))
	{
		Result.Clicked = True;
		//Result.ActiveItem = Max(ActiveItem - 1, 0);
	}

	ScratchEnd(Scratch);

	return Result;
}
