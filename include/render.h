#pragma once
#include "memory.h"
#include "log.h"
#include "maths.h"

#include "render_common.h"


void R_Init(render_context *R, m_arena *Arena, v2i Size);
void R_Update(render_context *R);
