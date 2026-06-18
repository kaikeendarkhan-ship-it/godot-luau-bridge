/**************************************************************************/
/*  luau_variant.cpp                                                      */
/**************************************************************************/

#include "luau_variant.h"

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/variant/callable.h"

#include <lua.h>
#include <lualib.h>

static const char *GODOT_OBJECT_METATABLE = "GodotObject";
static const char *GODOT_CALLABLE_METATABLE = "GodotCallable";

static bool table_is_array(lua_State *L, int index) {
	int abs_index = lua_absindex(L, index);
	int len = (int)lua_objlen(L, abs_index);

	if (len == 0) {
		return false;
	}

	// Verify all keys from 1 to len exist
	for (int i = 1; i <= len; i++) {
		lua_rawgeti(L, abs_index, i);
		if (lua_isnil(L, -1)) {
			lua_pop(L, 1);
			return false;
		}
		lua_pop(L, 1);
	}

	return true;
}

static Variant::Type detect_vector_type(lua_State *L, int index) {
	int abs_index = lua_absindex(L, index);
	int len = (int)lua_objlen(L, abs_index);

	if (len < 2 || len > 4) {
		return Variant::NIL;
	}

	// Check if all values are numbers
	for (int i = 1; i <= len; i++) {
		lua_rawgeti(L, abs_index, i);
		if (!lua_isnumber(L, -1)) {
			lua_pop(L, 1);
			return Variant::NIL;
		}
		lua_pop(L, 1);
	}

	// Check for named fields that indicate non-vector tables
	lua_pushnil(L);
	while (lua_next(L, abs_index) != 0) {
		if (lua_type(L, -2) != LUA_TNUMBER) {
			lua_pop(L, 2);
			return Variant::NIL;
		}
		lua_pop(L, 1);
	}

	switch (len) {
		case 2:
			return Variant::VECTOR2;
		case 3:
			return Variant::VECTOR3;
		case 4:
			return Variant::VECTOR4;
		default:
			return Variant::NIL;
	}
}

static bool table_is_color(lua_State *L, int index) {
	int abs_index = lua_absindex(L, index);

	lua_getfield(L, abs_index, "r");
	bool has_r = !lua_isnil(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, abs_index, "g");
	bool has_g = !lua_isnil(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, abs_index, "b");
	bool has_b = !lua_isnil(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, abs_index, "a");
	bool has_a = !lua_isnil(L, -1);
	lua_pop(L, 1);

	return has_r && has_g && has_b && has_a;
}

static bool table_is_rect2(lua_State *L, int index) {
	int abs_index = lua_absindex(L, index);

	lua_getfield(L, abs_index, "position");
	bool has_position = !lua_isnil(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, abs_index, "size");
	bool has_size = !lua_isnil(L, -1);
	lua_pop(L, 1);

	return has_position && has_size;
}

// --- Luau → Godot ---

Variant LuauVariant::to_variant(lua_State *L, int index) {
	int type = lua_type(L, index);

	if (type == LUA_TNONE) {
		return Variant();
	}

	switch (type) {
		case LUA_TNIL:
			return Variant();

		case LUA_TBOOLEAN:
			return Variant(lua_toboolean(L, index) != 0);

		case LUA_TNUMBER:
			return Variant(static_cast<double>(lua_tonumber(L, index)));

		case LUA_TSTRING: {
			size_t len = 0;
			const char *str = lua_tolstring(L, index, &len);
			return Variant(String::utf8(str, len));
		}

		case LUA_TTABLE: {
			int abs_index = lua_absindex(L, index);

			// Check for Color
			if (table_is_color(L, index)) {
				lua_getfield(L, abs_index, "r");
				double r = lua_tonumber(L, -1);
				lua_pop(L, 1);
				lua_getfield(L, abs_index, "g");
				double g = lua_tonumber(L, -1);
				lua_pop(L, 1);
				lua_getfield(L, abs_index, "b");
				double b = lua_tonumber(L, -1);
				lua_pop(L, 1);
				lua_getfield(L, abs_index, "a");
				double a = lua_tonumber(L, -1);
				lua_pop(L, 1);
				return Variant(Color(r, g, b, a));
			}

			// Check for Rect2
			if (table_is_rect2(L, index)) {
				lua_getfield(L, abs_index, "position");
				Vector2 pos;
				if (lua_istable(L, -1)) {
					lua_rawgeti(L, -1, 1);
					pos.x = lua_tonumber(L, -1);
					lua_pop(L, 1);
					lua_rawgeti(L, -1, 2);
					pos.y = lua_tonumber(L, -1);
					lua_pop(L, 1);
				}
				lua_pop(L, 1);

				lua_getfield(L, abs_index, "size");
				Vector2 size;
				if (lua_istable(L, -1)) {
					lua_rawgeti(L, -1, 1);
					size.x = lua_tonumber(L, -1);
					lua_pop(L, 1);
					lua_rawgeti(L, -1, 2);
					size.y = lua_tonumber(L, -1);
					lua_pop(L, 1);
				}
				lua_pop(L, 1);
				return Variant(Rect2(pos, size));
			}

			// Check for Vector type
			Variant::Type vec_type = detect_vector_type(L, index);
			if (vec_type != Variant::NIL) {
				double vals[4] = { 0, 0, 0, 0 };
				int len = (int)lua_objlen(L, abs_index);
				for (int i = 0; i < len; i++) {
					lua_rawgeti(L, abs_index, i + 1);
					vals[i] = lua_tonumber(L, -1);
					lua_pop(L, 1);
				}
				switch (vec_type) {
					case Variant::VECTOR2:
						return Variant(Vector2(vals[0], vals[1]));
					case Variant::VECTOR3:
						return Variant(Vector3(vals[0], vals[1], vals[2]));
					case Variant::VECTOR4:
						return Variant(Vector4(vals[0], vals[1], vals[2], vals[3]));
					default:
						break;
				}
			}

			// Check for Array vs Dictionary
			if (table_is_array(L, index)) {
				int len = (int)lua_objlen(L, abs_index);
				Array arr;
				arr.resize(len);
				for (int i = 0; i < len; i++) {
					lua_rawgeti(L, abs_index, i + 1);
					arr[i] = to_variant(L, -1);
					lua_pop(L, 1);
				}
				return arr;
			} else {
				Dictionary dict;
				lua_pushnil(L);
				while (lua_next(L, abs_index) != 0) {
					Variant key = to_variant(L, -2);
					Variant val = to_variant(L, -1);
					dict[key] = val;
					lua_pop(L, 1);
				}
				return dict;
			}
		}

		case LUA_TFUNCTION:
			return Variant();

		case LUA_TUSERDATA:
			return _to_userdata_internal(L, index);

		case LUA_TVECTOR: {
			const float *v = lua_tovector(L, index);
			if (v) {
				// Luau vectors are always 3-component
				return Variant(Vector3(v[0], v[1], v[2]));
			}
			return Variant();
		}

		default:
			return Variant();
	}
}

Variant LuauVariant::_to_userdata_internal(lua_State *L, int index) {
	void *ud = lua_touserdata(L, index);
	if (!ud) {
		return Variant();
	}

	if (lua_getmetatable(L, index)) {
		lua_getfield(L, LUA_REGISTRYINDEX, GODOT_OBJECT_METATABLE);
		if (lua_rawequal(L, -1, -2)) {
			lua_pop(L, 2);
			ObjectID *id_ptr = (ObjectID *)ud;
			Object *obj = ObjectDB::get_instance(*id_ptr);
			if (obj) {
				return Variant(obj);
			}
			return Variant();
		}
		lua_pop(L, 2);

		lua_getfield(L, LUA_REGISTRYINDEX, GODOT_CALLABLE_METATABLE);
		if (lua_rawequal(L, -1, -2)) {
			lua_pop(L, 2);
			Callable *callable_ptr = (Callable *)ud;
			if (callable_ptr) {
				Variant v = *callable_ptr;
				return v;
			}
			return Variant();
		}
		lua_pop(L, 2);
	}

	return Variant();
}

// --- Godot → Luau ---

bool LuauVariant::push_variant(lua_State *L, const Variant &value) {
	switch (value.get_type()) {
		case Variant::NIL:
			lua_pushnil(L);
			return true;

		case Variant::BOOL:
			lua_pushboolean(L, value.operator bool() ? 1 : 0);
			return true;

		case Variant::INT:
			lua_pushinteger(L, value);
			return true;

		case Variant::FLOAT:
			lua_pushnumber(L, value);
			return true;

		case Variant::STRING: {
			CharString utf8 = value.stringify().utf8();
			lua_pushlstring(L, utf8.get_data(), utf8.length());
			return true;
		}

		case Variant::VECTOR2: {
			Vector2 v = value;
			lua_createtable(L, 2, 0);
			lua_pushnumber(L, v.x);
			lua_rawseti(L, -2, 1);
			lua_pushnumber(L, v.y);
			lua_rawseti(L, -2, 2);
			return true;
		}

		case Variant::VECTOR2I: {
			Vector2i v = value;
			lua_createtable(L, 2, 0);
			lua_pushinteger(L, v.x);
			lua_rawseti(L, -2, 1);
			lua_pushinteger(L, v.y);
			lua_rawseti(L, -2, 2);
			return true;
		}

		case Variant::VECTOR3: {
			Vector3 v = value;
			lua_createtable(L, 3, 0);
			lua_pushnumber(L, v.x);
			lua_rawseti(L, -2, 1);
			lua_pushnumber(L, v.y);
			lua_rawseti(L, -2, 2);
			lua_pushnumber(L, v.z);
			lua_rawseti(L, -2, 3);
			return true;
		}

		case Variant::VECTOR3I: {
			Vector3i v = value;
			lua_createtable(L, 3, 0);
			lua_pushinteger(L, v.x);
			lua_rawseti(L, -2, 1);
			lua_pushinteger(L, v.y);
			lua_rawseti(L, -2, 2);
			lua_pushinteger(L, v.z);
			lua_rawseti(L, -2, 3);
			return true;
		}

		case Variant::VECTOR4: {
			Vector4 v = value;
			lua_createtable(L, 4, 0);
			lua_pushnumber(L, v.x);
			lua_rawseti(L, -2, 1);
			lua_pushnumber(L, v.y);
			lua_rawseti(L, -2, 2);
			lua_pushnumber(L, v.z);
			lua_rawseti(L, -2, 3);
			lua_pushnumber(L, v.w);
			lua_rawseti(L, -2, 4);
			return true;
		}

		case Variant::VECTOR4I: {
			Vector4i v = value;
			lua_createtable(L, 4, 0);
			lua_pushinteger(L, v.x);
			lua_rawseti(L, -2, 1);
			lua_pushinteger(L, v.y);
			lua_rawseti(L, -2, 2);
			lua_pushinteger(L, v.z);
			lua_rawseti(L, -2, 3);
			lua_pushinteger(L, v.w);
			lua_rawseti(L, -2, 4);
			return true;
		}

		case Variant::RECT2: {
			Rect2 v = value;
			lua_createtable(L, 0, 2);
			lua_createtable(L, 2, 0);
			lua_pushnumber(L, v.position.x);
			lua_rawseti(L, -2, 1);
			lua_pushnumber(L, v.position.y);
			lua_rawseti(L, -2, 2);
			lua_setfield(L, -2, "position");
			lua_createtable(L, 2, 0);
			lua_pushnumber(L, v.size.x);
			lua_rawseti(L, -2, 1);
			lua_pushnumber(L, v.size.y);
			lua_rawseti(L, -2, 2);
			lua_setfield(L, -2, "size");
			return true;
		}

		case Variant::RECT2I: {
			Rect2i v = value;
			lua_createtable(L, 0, 2);
			lua_createtable(L, 2, 0);
			lua_pushinteger(L, v.position.x);
			lua_rawseti(L, -2, 1);
			lua_pushinteger(L, v.position.y);
			lua_rawseti(L, -2, 2);
			lua_setfield(L, -2, "position");
			lua_createtable(L, 2, 0);
			lua_pushinteger(L, v.size.x);
			lua_rawseti(L, -2, 1);
			lua_pushinteger(L, v.size.y);
			lua_rawseti(L, -2, 2);
			lua_setfield(L, -2, "size");
			return true;
		}

		case Variant::TRANSFORM2D: {
			Transform2D v = value;
			lua_createtable(L, 6, 0);
			int idx = 1;
			for (int i = 0; i < 2; i++) {
				for (int j = 0; j < 2; j++) {
					lua_pushnumber(L, v.columns[i][j]);
					lua_rawseti(L, -2, idx++);
				}
			}
			return true;
		}

		case Variant::TRANSFORM3D: {
			Transform3D v = value;
			lua_createtable(L, 12, 0);
			int idx = 1;
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 3; j++) {
					lua_pushnumber(L, v.basis[i][j]);
					lua_rawseti(L, -2, idx++);
				}
			}
			lua_pushnumber(L, v.origin.x);
			lua_rawseti(L, -2, idx++);
			lua_pushnumber(L, v.origin.y);
			lua_rawseti(L, -2, idx++);
			lua_pushnumber(L, v.origin.z);
			lua_rawseti(L, -2, idx++);
			return true;
		}

		case Variant::BASIS: {
			Basis v = value;
			lua_createtable(L, 9, 0);
			int idx = 1;
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 3; j++) {
					lua_pushnumber(L, v[i][j]);
					lua_rawseti(L, -2, idx++);
				}
			}
			return true;
		}

		case Variant::QUATERNION: {
			Quaternion v = value;
			lua_createtable(L, 4, 0);
			lua_pushnumber(L, v.x);
			lua_rawseti(L, -2, 1);
			lua_pushnumber(L, v.y);
			lua_rawseti(L, -2, 2);
			lua_pushnumber(L, v.z);
			lua_rawseti(L, -2, 3);
			lua_pushnumber(L, v.w);
			lua_rawseti(L, -2, 4);
			return true;
		}

		case Variant::COLOR: {
			Color v = value;
			lua_createtable(L, 0, 4);
			lua_pushnumber(L, v.r);
			lua_setfield(L, -2, "r");
			lua_pushnumber(L, v.g);
			lua_setfield(L, -2, "g");
			lua_pushnumber(L, v.b);
			lua_setfield(L, -2, "b");
			lua_pushnumber(L, v.a);
			lua_setfield(L, -2, "a");
			return true;
		}

		case Variant::AABB: {
			AABB v = value;
			lua_createtable(L, 0, 2);
			lua_createtable(L, 3, 0);
			lua_pushnumber(L, v.position.x);
			lua_rawseti(L, -2, 1);
			lua_pushnumber(L, v.position.y);
			lua_rawseti(L, -2, 2);
			lua_pushnumber(L, v.position.z);
			lua_rawseti(L, -2, 3);
			lua_setfield(L, -2, "position");
			lua_createtable(L, 3, 0);
			lua_pushnumber(L, v.size.x);
			lua_rawseti(L, -2, 1);
			lua_pushnumber(L, v.size.y);
			lua_rawseti(L, -2, 2);
			lua_pushnumber(L, v.size.z);
			lua_rawseti(L, -2, 3);
			lua_setfield(L, -2, "size");
			return true;
		}

		case Variant::PROJECTION: {
			Projection v = value;
			lua_createtable(L, 16, 0);
			int idx = 1;
			for (int i = 0; i < 4; i++) {
				for (int j = 0; j < 4; j++) {
					lua_pushnumber(L, v.columns[i][j]);
					lua_rawseti(L, -2, idx++);
				}
			}
			return true;
		}

		case Variant::NODE_PATH: {
			NodePath v = value;
			CharString utf8 = String(v).utf8();
			lua_pushlstring(L, utf8.get_data(), utf8.length());
			return true;
		}

		case Variant::STRING_NAME: {
			StringName v = value;
			CharString utf8 = String(v).utf8();
			lua_pushlstring(L, utf8.get_data(), utf8.length());
			return true;
		}

		case Variant::RID: {
			RID rid = value;
			lua_pushlightuserdata(L, reinterpret_cast<void *>(rid.get_id()));
			return true;
		}

		case Variant::OBJECT: {
			Object *obj = value;
			push_godot_object(L, obj);
			return true;
		}

		case Variant::CALLABLE: {
			Callable callable = value;
			push_callable(L, callable);
			return true;
		}

		case Variant::SIGNAL: {
			Signal sig = value;
			CharString utf8 = String(sig.get_name()).utf8();
			lua_pushlstring(L, utf8.get_data(), utf8.length());
			return true;
		}

		case Variant::ARRAY: {
			Array arr = value;
			int size = arr.size();
			lua_createtable(L, size, 0);
			for (int i = 0; i < size; i++) {
				push_variant(L, arr[i]);
				lua_rawseti(L, -2, i + 1);
			}
			return true;
		}

		case Variant::PACKED_BYTE_ARRAY: {
			PackedByteArray arr = value;
			int size = arr.size();
			lua_createtable(L, size, 0);
			for (int i = 0; i < size; i++) {
				lua_pushinteger(L, arr[i]);
				lua_rawseti(L, -2, i + 1);
			}
			return true;
		}

		case Variant::PACKED_INT32_ARRAY: {
			PackedInt32Array arr = value;
			int size = arr.size();
			lua_createtable(L, size, 0);
			for (int i = 0; i < size; i++) {
				lua_pushinteger(L, arr[i]);
				lua_rawseti(L, -2, i + 1);
			}
			return true;
		}

		case Variant::PACKED_INT64_ARRAY: {
			PackedInt64Array arr = value;
			int size = arr.size();
			lua_createtable(L, size, 0);
			for (int i = 0; i < size; i++) {
				lua_pushinteger(L, arr[i]);
				lua_rawseti(L, -2, i + 1);
			}
			return true;
		}

		case Variant::PACKED_FLOAT32_ARRAY: {
			PackedFloat32Array arr = value;
			int size = arr.size();
			lua_createtable(L, size, 0);
			for (int i = 0; i < size; i++) {
				lua_pushnumber(L, arr[i]);
				lua_rawseti(L, -2, i + 1);
			}
			return true;
		}

		case Variant::PACKED_FLOAT64_ARRAY: {
			PackedFloat64Array arr = value;
			int size = arr.size();
			lua_createtable(L, size, 0);
			for (int i = 0; i < size; i++) {
				lua_pushnumber(L, arr[i]);
				lua_rawseti(L, -2, i + 1);
			}
			return true;
		}

		case Variant::PACKED_STRING_ARRAY: {
			PackedStringArray arr = value;
			int size = arr.size();
			lua_createtable(L, size, 0);
			for (int i = 0; i < size; i++) {
				CharString utf8 = arr[i].utf8();
				lua_pushlstring(L, utf8.get_data(), utf8.length());
				lua_rawseti(L, -2, i + 1);
			}
			return true;
		}

		case Variant::PACKED_VECTOR2_ARRAY: {
			PackedVector2Array arr = value;
			int size = arr.size();
			lua_createtable(L, size, 0);
			for (int i = 0; i < size; i++) {
				lua_createtable(L, 2, 0);
				lua_pushnumber(L, arr[i].x);
				lua_rawseti(L, -2, 1);
				lua_pushnumber(L, arr[i].y);
				lua_rawseti(L, -2, 2);
				lua_rawseti(L, -2, i + 1);
			}
			return true;
		}

		case Variant::PACKED_VECTOR3_ARRAY: {
			PackedVector3Array arr = value;
			int size = arr.size();
			lua_createtable(L, size, 0);
			for (int i = 0; i < size; i++) {
				lua_createtable(L, 3, 0);
				lua_pushnumber(L, arr[i].x);
				lua_rawseti(L, -2, 1);
				lua_pushnumber(L, arr[i].y);
				lua_rawseti(L, -2, 2);
				lua_pushnumber(L, arr[i].z);
				lua_rawseti(L, -2, 3);
				lua_rawseti(L, -2, i + 1);
			}
			return true;
		}

		case Variant::PACKED_COLOR_ARRAY: {
			PackedColorArray arr = value;
			int size = arr.size();
			lua_createtable(L, size, 0);
			for (int i = 0; i < size; i++) {
				lua_createtable(L, 0, 4);
				lua_pushnumber(L, arr[i].r);
				lua_setfield(L, -2, "r");
				lua_pushnumber(L, arr[i].g);
				lua_setfield(L, -2, "g");
				lua_pushnumber(L, arr[i].b);
				lua_setfield(L, -2, "b");
				lua_pushnumber(L, arr[i].a);
				lua_setfield(L, -2, "a");
				lua_rawseti(L, -2, i + 1);
			}
			return true;
		}

		case Variant::PACKED_VECTOR4_ARRAY: {
			PackedVector4Array arr = value;
			int size = arr.size();
			lua_createtable(L, size, 0);
			for (int i = 0; i < size; i++) {
				lua_createtable(L, 4, 0);
				lua_pushnumber(L, arr[i].x);
				lua_rawseti(L, -2, 1);
				lua_pushnumber(L, arr[i].y);
				lua_rawseti(L, -2, 2);
				lua_pushnumber(L, arr[i].z);
				lua_rawseti(L, -2, 3);
				lua_pushnumber(L, arr[i].w);
				lua_rawseti(L, -2, 4);
				lua_rawseti(L, -2, i + 1);
			}
			return true;
		}

		case Variant::DICTIONARY: {
			Dictionary dict = value;
			LocalVector<Variant> keys = dict.get_key_list();
			lua_createtable(L, 0, keys.size());
			for (const Variant &key : keys) {
				push_variant(L, key);
				push_variant(L, dict[key]);
				lua_rawset(L, -3);
			}
			return true;
		}

		default:
			lua_pushnil(L);
			return false;
	}
}

bool LuauVariant::is_godot_object(lua_State *L, int index) {
	if (!lua_isuserdata(L, index)) {
		return false;
	}

	if (lua_getmetatable(L, index)) {
		lua_getfield(L, LUA_REGISTRYINDEX, GODOT_OBJECT_METATABLE);
		bool is_godot = lua_rawequal(L, -1, -2);
		lua_pop(L, 2);
		return is_godot;
	}

	return false;
}

Variant::Type LuauVariant::get_variant_type(lua_State *L, int index) {
	int type = lua_type(L, index);

	switch (type) {
		case LUA_TNIL:
			return Variant::NIL;
		case LUA_TBOOLEAN:
			return Variant::BOOL;
		case LUA_TNUMBER:
			return Variant::FLOAT;
		case LUA_TSTRING:
			return Variant::STRING;
		case LUA_TTABLE:
			return get_table_variant_type(L, index);
		case LUA_TFUNCTION:
			return Variant::CALLABLE;
		case LUA_TUSERDATA:
			if (is_godot_object(L, index)) {
				return Variant::OBJECT;
			}
			return Variant::NIL;
		case LUA_TVECTOR:
			return Variant::VECTOR3;
		default:
			return Variant::NIL;
	}
}

Variant::Type LuauVariant::get_table_variant_type(lua_State *L, int index) {
	if (table_is_color(L, index)) {
		return Variant::COLOR;
	}
	if (table_is_rect2(L, index)) {
		return Variant::RECT2;
	}

	Variant::Type vec_type = detect_vector_type(L, index);
	if (vec_type != Variant::NIL) {
		return vec_type;
	}

	if (table_is_array(L, index)) {
		return Variant::ARRAY;
	}

	return Variant::DICTIONARY;
}

static int luau_object_native_call(lua_State *L) {
	const char *method_name = lua_tostring(L, lua_upvalueindex(1));
	ObjectID *id_ptr = (ObjectID *)lua_touserdata(L, lua_upvalueindex(2));

	if (!id_ptr) {
		lua_pushnil(L);
		return 1;
	}

	Object *obj = ObjectDB::get_instance(*id_ptr);
	if (!obj) {
		luaL_error(L, "Attempt to call method '%s' on a freed or invalid Godot Object instance.", method_name);
		return 0;
	}
	int total_top = lua_gettop(L);

	int args_start = 1;
	if (total_top >= 1 && lua_isuserdata(L, 1)) {
		args_start = 2;
	}

	int godot_args_count = total_top - args_start + 1;
	if (godot_args_count < 0) {
		godot_args_count = 0;
	}

	Vector<Variant> godot_args;
	for (int i = args_start; i <= total_top; i++) {
		godot_args.push_back(LuauVariant::to_variant(L, i));
	}

	Vector<const Variant *> argptrs;
	if (godot_args_count > 0) {
		argptrs.resize(godot_args_count);
		for (int i = 0; i < godot_args_count; i++) {
			argptrs.write[i] = &godot_args[i];
		}
	}

	StringName class_name = obj->get_class_name();
	StringName method_key(method_name);
	MethodBind *method_bind = ClassDB::get_method(class_name, method_key);

	if (!method_bind) {
		ERR_PRINT("Luau: MethodBind not found — " + String(method_name));
		lua_pushnil(L);
		return 1;
	}

	Callable::CallError call_error;
	Variant result = method_bind->call(obj, (const Variant **)argptrs.ptr(), godot_args_count, call_error);

	if (call_error.error != Callable::CallError::CALL_OK) {
		ERR_PRINT("Luau: native call failed — " + String(method_name));
		lua_pushnil(L);
		return 1;
	}

	LuauVariant::push_variant(L, result);
	return 1;
}

static int luau_object_index(lua_State *L) {
	if (!lua_isuserdata(L, 1) || !lua_isstring(L, 2)) {
		lua_pushnil(L);
		return 1;
	}

	ObjectID *id_ptr = (ObjectID *)lua_touserdata(L, 1);
	if (!id_ptr) {
		lua_pushnil(L);
		return 1;
	}

	Object *obj = ObjectDB::get_instance(*id_ptr);
	if (!obj) {
		luaL_error(L, "Attempt to access a freed or invalid Godot Object instance.");
		return 0;
	}

	const char *key = lua_tostring(L, 2);

	StringName class_name = obj->get_class_name();
	StringName method_key(key);

	if (ClassDB::has_method(class_name, method_key, false)) {
		lua_pushstring(L, key);
		lua_pushvalue(L, 1);
		lua_pushcclosure(L, luau_object_native_call, "object_native_call", 2);
		return 1;
	}

	lua_pushnil(L);
	return 1;
}

void LuauVariant::push_godot_object(lua_State *L, Object *p_object) {
	if (!p_object) {
		lua_pushnil(L);
		return;
	}

	ObjectID *ud = (ObjectID *)lua_newuserdata(L, sizeof(ObjectID));
	*ud = p_object->get_instance_id();

	lua_getfield(L, LUA_REGISTRYINDEX, GODOT_OBJECT_METATABLE);
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		lua_createtable(L, 0, 1);
		lua_pushcfunction(L, luau_object_index, "GodotObject.__index");
		lua_setfield(L, -2, "__index");
		lua_pushvalue(L, -1);
		lua_setfield(L, LUA_REGISTRYINDEX, GODOT_OBJECT_METATABLE);
	}
	lua_setmetatable(L, -2);
}

Object *LuauVariant::get_godot_object(lua_State *L, int index) {
	if (!lua_isuserdata(L, index)) {
		return nullptr;
	}

	void *ud = lua_touserdata(L, index);
	if (!ud) {
		return nullptr;
	}

	if (lua_getmetatable(L, index)) {
		lua_getfield(L, LUA_REGISTRYINDEX, GODOT_OBJECT_METATABLE);
		bool is_godot = lua_rawequal(L, -1, -2);
		lua_pop(L, 2);

		if (is_godot) {
			ObjectID *id_ptr = (ObjectID *)ud;
			return ObjectDB::get_instance(*id_ptr);
		}
	}

	return nullptr;
}

static int luau_callable_gc(lua_State *L) {
	Callable *c = (Callable *)lua_touserdata(L, 1);
	if (c) {
		c->~Callable();
	}
	return 0;
}

void LuauVariant::push_callable(lua_State *L, const Callable &p_callable) {
	Callable *ud = (Callable *)lua_newuserdata(L, sizeof(Callable));
	new (ud) Callable(p_callable);

	lua_getfield(L, LUA_REGISTRYINDEX, GODOT_CALLABLE_METATABLE);
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		lua_createtable(L, 0, 1);
		lua_pushcfunction(L, luau_callable_gc, "GodotCallable.__gc");
		lua_setfield(L, -2, "__gc");
		lua_pushvalue(L, -1);
		lua_setfield(L, LUA_REGISTRYINDEX, GODOT_CALLABLE_METATABLE);
	}
	lua_setmetatable(L, -2);
}

Callable LuauVariant::get_callable(lua_State *L, int index) {
	if (!lua_isuserdata(L, index)) {
		return Callable();
	}

	void *ud = lua_touserdata(L, index);
	if (!ud) {
		return Callable();
	}

	if (lua_getmetatable(L, index)) {
		lua_getfield(L, LUA_REGISTRYINDEX, GODOT_CALLABLE_METATABLE);
		bool is_callable = lua_rawequal(L, -1, -2);
		lua_pop(L, 2);

		if (is_callable) {
			Callable *callable_ptr = (Callable *)ud;
			return callable_ptr ? *callable_ptr : Callable();
		}
	}

	return Callable();
}

void LuauVariant::set_global(lua_State *L, const char *p_name, const Variant &p_value) {
	push_variant(L, p_value);
	lua_setglobal(L, p_name);
}
