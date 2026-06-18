/**************************************************************************/
/*  luau_variant.h                                                        */
/**************************************************************************/

#ifndef LUAU_VARIANT_H
#define LUAU_VARIANT_H

#include "core/variant/variant.h"

#include <lua.h>

class LuauVariant {
public:
	static Variant to_variant(lua_State *L, int index);
	static bool push_variant(lua_State *L, const Variant &value);

	static bool is_godot_object(lua_State *L, int index);
	static Variant::Type get_variant_type(lua_State *L, int index);
	static Variant::Type get_table_variant_type(lua_State *L, int index);

	static void push_godot_object(lua_State *L, Object *p_object);
	static Object *get_godot_object(lua_State *L, int index);

	static void push_callable(lua_State *L, const Callable &p_callable);
	static Callable get_callable(lua_State *L, int index);

	static void set_global(lua_State *L, const char *p_name, const Variant &p_value);

private:
	static Variant _to_userdata_internal(lua_State *L, int index);
};

#endif // LUAU_VARIANT_H
