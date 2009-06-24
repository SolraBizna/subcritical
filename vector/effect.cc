#include "subcritical/graphics.h"
#include "vector.h"

using namespace SubCritical;

static uint16_t c16(Scalar in) {
  if(in > 65535) return 65535;
  else if(in < 0) return 0;
  else return (uint16_t)in;
}

#define in4 0
#include "effect_subA.hpp"
#undef in4
#define in4 1
#include "effect_subA.hpp"
#undef in4

SUBCRITICAL_UTILITY(ApplyColorMatrix)(lua_State* L) {
  Drawable* in = lua_toobject(L, 1, Drawable);
  Matrix* mat = lua_toobject(L, 2, Matrix);
  if(mat->r < 3 || mat->c < 3) return luaL_error(L, "Must be a Mat3x3 or larger");
  Scalar plusr = 0.0, plusg = 0.0, plusb = 0.0, plusa = 0.0;
  if(lua_gettop(L) >= 3 && !lua_isnil(L, 3)) {
    Vector* vec = lua_toobject(L, 3, Vector);
    plusr = ((Vec2*)vec)->x * 65535;
    plusg = ((Vec2*)vec)->y * 65535;
    if(vec->n >= 3) {
      plusb = ((Vec3*)vec)->z * 65535;
      if(vec->n >= 4)
	plusa = ((Vec4*)vec)->w * 65535;
    }
  }
  if(in->has_alpha) {
    if(mat->r == 4) {
      if(mat->c == 4)
	return doit44T(L, in, mat, plusr, plusg, plusb, plusa);
      else
	return doit34T(L, in, mat, plusr, plusg, plusb, plusa);
    }
    else {
      if(mat->c == 4)
	return doit43T(L, in, mat, plusr, plusg, plusb, plusa);
      else
	return doit33T(L, in, mat, plusr, plusg, plusb, plusa);
    }
  }
  else {
    if(mat->r == 4) {
      if(mat->c == 4)
	return doit44F(L, in, mat, plusr, plusg, plusb, plusa);
      else
	return doit34F(L, in, mat, plusr, plusg, plusb, plusa);
    }
    else {
      if(mat->c == 4)
	return doit43F(L, in, mat, plusr, plusg, plusb, plusa);
      else
	return doit33F(L, in, mat, plusr, plusg, plusb, plusa);
    }
  }
}
