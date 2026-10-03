/*
 * NeoGLOBAL unity translation unit.
 *
 * NeoGLOBAL's implementation and entry point share this translation unit
 * so CFD's header-only arena helpers can access their internal OOM handler.
 * Third-party libraries such as Lua are compiled separately.
 */

#define CFD_LIB_IMPLEMENTATION

#include "problem.c"
#include "lua_problem.c"
#include "main.c"
