/**************************************************************************/
/*  luau_instance.cpp                                                     */
/**************************************************************************/

#include "luau_instance.h"

#include "luau_language.h"
#include "luau_script.h"
#include "luau_variant.h"

#include "core/object/class_db.h"
#include "core/string/print_string.h"
#include "scene/main/node.h"

#include <lua.h>
#include <lualib.h>

// --- Godot bridge functions callable from Luau ---

static int luau_godot_call(lua_State *L) {
	Object *obj = LuauVariant::get_godot_object(L, 1);
	if (!obj) {
		lua_pushnil(L);
		return 1;
	}

	int top = lua_gettop(L);
	if (top < 2) {
		lua_pushnil(L);
		return 1;
	}

	const char *method = lua_tostring(L, 2);
	if (!method) {
		lua_pushnil(L);
		return 1;
	}

	int argcount = top - 2;
	Vector<Variant> args;
	args.resize(argcount);
	for (int i = 0; i < argcount; i++) {
		args.write[i] = LuauVariant::to_variant(L, i + 3);
	}

	Variant result;
	Callable::CallError error;
	Vector<Variant *> argptrs;
	argptrs.resize(argcount);
	for (int i = 0; i < argcount; i++) {
		argptrs.write[i] = &args.write[i];
	}

	result = obj->callp(StringName(method), (const Variant **)argptrs.ptr(), argcount, error);

	if (error.error != Callable::CallError::CALL_OK) {
		ERR_PRINT("Luau godot.call error on method: " + String(method));
		lua_pushnil(L);
		return 1;
	}

	LuauVariant::push_variant(L, result);
	return 1;
}

// --- print() override: redirect Lua print() to Godot print_line() ---

static int luau_print(lua_State *L) {
	int nargs = lua_gettop(L);
	String result;

	for (int i = 1; i <= nargs; i++) {
		if (i > 1) {
			result += "\t";
		}
		int type = lua_type(L, i);
		switch (type) {
			case LUA_TNIL:
				result += "nil";
				break;
			case LUA_TBOOLEAN:
				result += lua_toboolean(L, i) ? "true" : "false";
				break;
			case LUA_TNUMBER:
				result += String::num(lua_tonumber(L, i));
				break;
			case LUA_TSTRING: {
				size_t len = 0;
				const char *str = lua_tolstring(L, i, &len);
				result += String::utf8(str, len);
			} break;
			case LUA_TVECTOR: {
				const float *v = lua_tovector(L, i);
				if (v) {
					result += "Vector3(" + String::num(v[0]) + ", " + String::num(v[1]) + ", " + String::num(v[2]) + ")";
				}
			} break;
			default: {
				// For userdata (Godot objects), try to get class name
				if (lua_isuserdata(L, i)) {
					Object *obj = LuauVariant::get_godot_object(L, i);
					if (obj) {
						result += obj->get_class() + ":" + String::num_int64((int64_t)obj);
					} else {
						result += "[userdata]";
					}
				} else {
					result += "[" + String(lua_typename(L, type)) + "]";
				}
			} break;
		}
	}

	print_line(result);
	return 0;
}

// --- __index metamethod: enables self.method_name() syntax ---

// C closure that calls a native Godot method on the owner
static int luau_native_method_call(lua_State *L) {
	const char *method_name = lua_tostring(L, lua_upvalueindex(1));
	LuauInstance *instance = static_cast<LuauInstance *>(lua_touserdata(L, lua_upvalueindex(2)));

	if (!instance) {
		return 0;
	}

	Object *owner = instance->get_owner();
	if (!owner) {
		return 0;
	}

	int total_top = lua_gettop(L);

	int args_start = 1;
	if (total_top >= 1 && lua_istable(L, 1)) {
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

	StringName class_name = owner->get_class_name();
	StringName method_key(method_name);
	MethodBind *method_bind = ClassDB::get_method(class_name, method_key);

	if (!method_bind) {
		ERR_PRINT("Luau: MethodBind not found — " + String(method_name));
		lua_pushnil(L);
		return 1;
	}

	Callable::CallError call_error;
	Variant result = method_bind->call(owner, (const Variant **)argptrs.ptr(), godot_args_count, call_error);

	if (call_error.error != Callable::CallError::CALL_OK) {
		ERR_PRINT("Luau: native call failed — " + String(method_name));
		lua_pushnil(L);
		return 1;
	}

	owner->notification(30);

	LuauVariant::push_variant(L, result);
	return 1;
}

// __index metamethod: called when accessing self.key
// 1. If key exists in the table → return it (normal behavior)
// 2. If key is a native method on the owner → return a C closure
static int luau_index(lua_State *L) {
	if (!lua_istable(L, 1) || !lua_isstring(L, 2)) {
		lua_pushnil(L);
		return 1;
	}

	const char *key = lua_tostring(L, 2);

	// Extract LuauInstance from self table _instance field
	lua_pushstring(L, "_instance");
	lua_rawget(L, 1);
	LuauInstance *instance_ptr = static_cast<LuauInstance *>(lua_touserdata(L, -1));
	lua_pop(L, 1);

	// Fallback: if _instance is nil, try userdata at index 1 directly
	if (!instance_ptr) {
		instance_ptr = static_cast<LuauInstance *>(lua_touserdata(L, 1));
	}

	if (!instance_ptr) {
		lua_pushnil(L);
		return 1;
	}

	Object *owner = instance_ptr->get_owner();
	if (!owner) {
		lua_pushnil(L);
		return 1;
	}

	// Check native C++ method via ClassDB — pure static lookup, no object callbacks
	StringName method_key(key);
	StringName native_class = owner->get_class();

	if (ClassDB::has_method(native_class, method_key, false)) {
		lua_pushstring(L, key);
		lua_pushlightuserdata(L, instance_ptr);
		lua_pushcclosure(L, luau_native_method_call, "native_call", 2);
		return 1;
	}

	// Check Luau script table via rawget — no metamethod re-triggering
	lua_pushstring(L, key);
	lua_rawget(L, 1);
	if (!lua_isnil(L, -1)) {
		return 1;
	}
	lua_pop(L, 1);

	lua_pushnil(L);
	return 1;
}

static int luau_godot_get(lua_State *L) {
	Object *obj = LuauVariant::get_godot_object(L, 1);
	if (!obj) {
		lua_pushnil(L);
		return 1;
	}

	const char *prop = lua_tostring(L, 2);
	if (!prop) {
		lua_pushnil(L);
		return 1;
	}

	Variant value = obj->get(StringName(prop));
	LuauVariant::push_variant(L, value);
	return 1;
}

static int luau_godot_set(lua_State *L) {
	Object *obj = LuauVariant::get_godot_object(L, 1);
	if (!obj) {
		lua_pushboolean(L, 0);
		return 1;
	}

	const char *prop = lua_tostring(L, 2);
	if (!prop) {
		lua_pushboolean(L, 0);
		return 1;
	}

	Variant value = LuauVariant::to_variant(L, 3);
	obj->set(StringName(prop), value);
	lua_pushboolean(L, 1);
	return 1;
}

static int luau_godot_create(lua_State *L) {
	const char *classname = lua_tostring(L, 1);
	if (!classname) {
		lua_pushnil(L);
		return 1;
	}

	StringName cls(classname);
	if (!ClassDB::class_exists(cls)) {
		ERR_PRINT("Luau godot.create: class not found: " + String(classname));
		lua_pushnil(L);
		return 1;
	}

	Object *obj = ClassDB::instantiate(cls);
	if (!obj) {
		lua_pushnil(L);
		return 1;
	}

	LuauVariant::push_godot_object(L, obj);
	return 1;
}

void LuauInstance::_create_lua_state() {
	if (L) {
		lua_close(L);
	}

	L = luaL_newstate();
	if (!L) {
		ERR_PRINT("Failed to create Luau state for instance");
		return;
	}

	luaL_openlibs(L);

	// Override print() to redirect to Godot print_line()
	lua_pushcfunction(L, luau_print, "print");
	lua_setglobal(L, "print");

	// Register godot bridge table
	lua_createtable(L, 0, 5);

	lua_pushcfunction(L, luau_godot_call, "godot.call");
	lua_setfield(L, -2, "call");

	lua_pushcfunction(L, luau_godot_get, "godot.get");
	lua_setfield(L, -2, "get");

	lua_pushcfunction(L, luau_godot_set, "godot.set");
	lua_setfield(L, -2, "set");

	lua_pushcfunction(L, luau_godot_create, "godot.create");
	lua_setfield(L, -2, "create");

	lua_setglobal(L, "godot");

	lua_pushlightuserdata(L, this);
	lua_setfield(L, LUA_REGISTRYINDEX, "LuauInstance");
}

void LuauInstance::_setup_metatable() {
	if (!L || !owner) {
		return;
	}

	// Create self table
	lua_createtable(L, 0, 0);

	// Store instance pointer in self for raw access by __index metamethod
	lua_pushlightuserdata(L, this);
	lua_setfield(L, -2, "_instance");

	// Store owner as a Godot object userdata
	LuauVariant::push_godot_object(L, owner);
	lua_setfield(L, -2, "owner");

	// Store godot bridge on self for easy access
	lua_getglobal(L, "godot");
	lua_setfield(L, -2, "godot");

	// Store owner properties in self
	List<PropertyInfo> props;
	owner->get_property_list(&props);
	for (const PropertyInfo &E : props) {
		if (E.usage & PROPERTY_USAGE_SCRIPT_VARIABLE) {
			Variant val = owner->get(E.name);
			LuauVariant::push_variant(L, val);
			CharString name_utf8 = String(E.name).utf8();
			lua_setfield(L, -2, name_utf8.get_data());
		}
	}

	// Set __index metamethod on self table for self:method() syntax
	// Create metatable: { __index = luau_index }
	lua_createtable(L, 0, 1);
	lua_pushcfunction(L, luau_index, "self.__index");
	lua_setfield(L, -2, "__index");
	lua_setmetatable(L, -2);

	lua_setglobal(L, "self");
}

void LuauInstance::_push_owner() {
	if (!L || !owner) {
		return;
	}

	ObjectID *ud = (ObjectID *)lua_newuserdata(L, sizeof(ObjectID));
	*ud = owner->get_instance_id();
	lua_newtable(L);
	lua_setmetatable(L, -2);
}

void LuauInstance::_execute_bytecode() {
	if (!L || !script.is_valid()) {
		return;
	}

	const Vector<uint8_t> &bytecode = script->get_compiled_bytecode();
	if (bytecode.is_empty()) {
		return;
	}

	String path = script->get_path();
	CharString path_utf8 = path.utf8();

	int status = luau_load(L, path_utf8.get_data(), (const char *)bytecode.ptr(), bytecode.size(), 0);
	if (status != LUA_OK) {
		const char *error = lua_tostring(L, -1);
		ERR_PRINT("Luau load error: " + String(error ? error : "unknown"));
		lua_pop(L, 1);
		return;
	}

	_setup_metatable();

	// Execute bytecode — may return a module table (e.g. `return M`)
	status = lua_pcall(L, 0, 1, 0);
	if (status != LUA_OK) {
		const char *error = lua_tostring(L, -1);
		ERR_PRINT("Luau execution error: " + String(error ? error : "unknown"));
		lua_pop(L, 1);
		return;
	}

	// If the script returned a table, copy its functions into both _G and self
	// has_method()/callp() look in self, so functions must be there
	if (lua_istable(L, -1)) {
		lua_getglobal(L, "self"); // push self table

		lua_pushnil(L);
		while (lua_next(L, -3) != 0) {
			if (lua_type(L, -2) == LUA_TSTRING && lua_isfunction(L, -1)) {
				const char *key = lua_tostring(L, -2);
				// Register in _G
				lua_pushvalue(L, -1);
				lua_setglobal(L, key);
				// Register in self (for has_method/callp lookup)
				lua_pushvalue(L, -1);
				lua_setfield(L, -4, key);
			}
			lua_pop(L, 1);
		}
		lua_pop(L, 2); // pop module table + self table
	} else {
		lua_pop(L, 1);
	}

	// Enable processing so Godot calls _process() each frame
	Node *node = Object::cast_to<Node>(owner);
	if (node) {
		node->set_process(true);
	}
}

bool LuauInstance::set(const StringName &p_name, const Variant &p_value) {
	if (!L) {
		return false;
	}

	int top = lua_gettop(L);

	lua_getglobal(L, "self");
	if (lua_istable(L, -1)) {
		LuauVariant::push_variant(L, p_value);
		CharString name_utf8 = String(p_name).utf8();
		lua_setfield(L, -2, name_utf8.get_data());
		lua_pop(L, 1);

		property_values[p_name] = p_value;

		if (lua_gettop(L) != top) {
			lua_settop(L, top);
		}
		return true;
	}
	lua_pop(L, 1);

	if (lua_gettop(L) != top) {
		lua_settop(L, top);
	}
	return false;
}

bool LuauInstance::get(const StringName &p_name, Variant &r_ret) const {
	if (!L) {
		return false;
	}

	int top = lua_gettop(L);

	const Variant *cached = property_values.getptr(p_name);
	if (cached) {
		r_ret = *cached;
		return true;
	}

	lua_getglobal(L, "self");
	if (lua_istable(L, -1)) {
		CharString name_utf8 = String(p_name).utf8();
		lua_getfield(L, -1, name_utf8.get_data());
		if (!lua_isnil(L, -1)) {
			r_ret = LuauVariant::to_variant(L, -1);
			lua_settop(L, top);
			return true;
		}
		lua_pop(L, 1);
	}
	lua_pop(L, 1);

	CharString name_utf8 = String(p_name).utf8();
	lua_getglobal(L, name_utf8.get_data());
	if (!lua_isnil(L, -1)) {
		r_ret = LuauVariant::to_variant(L, -1);
		lua_settop(L, top);
		return true;
	}
	lua_pop(L, 1);

	lua_settop(L, top);
	return false;
}

void LuauInstance::get_property_list(List<PropertyInfo> *p_properties) const {
	if (script.is_valid()) {
		script->get_script_property_list(p_properties);
	}

	for (const auto &E : property_values) {
		PropertyInfo pi;
		pi.name = E.key;
		pi.type = E.value.get_type();
		p_properties->push_back(pi);
	}
}

Variant::Type LuauInstance::get_property_type(const StringName &p_name, bool *r_is_valid) const {
	const Variant *cached = property_values.getptr(p_name);
	if (cached) {
		if (r_is_valid) {
			*r_is_valid = true;
		}
		return cached->get_type();
	}

	if (r_is_valid) {
		*r_is_valid = false;
	}
	return Variant::NIL;
}

void LuauInstance::validate_property(PropertyInfo &p_property) const {
}

bool LuauInstance::property_can_revert(const StringName &p_name) const {
	return false;
}

bool LuauInstance::property_get_revert(const StringName &p_name, Variant &r_ret) const {
	return false;
}

Object *LuauInstance::get_owner() {
	return owner;
}

void LuauInstance::get_method_list(List<MethodInfo> *p_list) const {
	if (!L) {
		return;
	}

	lua_getglobal(L, "self");
	if (lua_istable(L, -1)) {
		lua_pushnil(L);
		while (lua_next(L, -2) != 0) {
			if (lua_type(L, -2) == LUA_TSTRING && lua_type(L, -1) == LUA_TFUNCTION) {
				const char *name = lua_tostring(L, -2);
				MethodInfo mi;
				mi.name = String(name);
				p_list->push_back(mi);
			}
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);
}

bool LuauInstance::has_method(const StringName &p_method) const {
	if (!L) {
		return false;
	}

	lua_getglobal(L, "self");
	if (lua_istable(L, -1)) {
		CharString name_utf8 = String(p_method).utf8();
		lua_getfield(L, -1, name_utf8.get_data());
		bool exists = lua_isfunction(L, -1);
		lua_pop(L, 2);
		return exists;
	}
	lua_pop(L, 1);

	return false;
}

int LuauInstance::get_method_argument_count(const StringName &p_method, bool *r_is_valid) const {
	if (r_is_valid) {
		*r_is_valid = has_method(p_method);
	}
	return 0;
}

Variant LuauInstance::callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	if (!L) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return Variant();
	}

	lua_getglobal(L, "self");
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return Variant();
	}

	CharString method_utf8 = String(p_method).utf8();
	lua_getfield(L, -1, method_utf8.get_data());
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 2);
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return Variant();
	}

	for (int i = 0; i < p_argcount; i++) {
		LuauVariant::push_variant(L, *p_args[i]);
	}

	int status;
	status = lua_pcall(L, p_argcount, 1, 0);
	if (status != LUA_OK) {
		const char *error = lua_tostring(L, -1);
		ERR_PRINT("Luau call error: " + String(error ? error : "unknown"));
		lua_pop(L, 2); // pop error + self table
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return Variant();
	}

	Variant result;
	if (!lua_isnil(L, -1)) {
		result = LuauVariant::to_variant(L, -1);
	}
	lua_pop(L, 2); // pop result + self table

	return result;
}

void LuauInstance::notification(int p_notification, bool p_reversed) {
	if (!L || !script.is_valid() || !script->is_script_compiled()) {
		return;
	}

	int top = lua_gettop(L);
	if (top < 0) {
		return;
	}

	static StringName _notification_name("_notification");
	if (has_method(_notification_name)) {
		Callable::CallError error;
		Variant arg = (int64_t)p_notification;
		const Variant *argp = &arg;
		callp(_notification_name, &argp, 1, error);
	}

	if (lua_gettop(L) != top) {
		lua_settop(L, top);
	}
}

Ref<Script> LuauInstance::get_script() const {
	return script;
}

ScriptLanguage *LuauInstance::get_language() {
	if (script.is_valid()) {
		return script->get_luau_language();
	}
	return nullptr;
}

bool LuauInstance::is_placeholder() const {
	return false;
}

const Variant LuauInstance::get_rpc_config() const {
	return Variant();
}

void LuauInstance::set_script(const Ref<LuauScript> &p_script) {
	script = p_script;

	if (script.is_valid() && script->is_script_compiled()) {
		_create_lua_state();
		_execute_bytecode();
	}
}

void LuauInstance::set_owner(Object *p_owner) {
	owner = p_owner;
}

void LuauInstance::_reload() {
	if (script.is_valid() && script->is_script_compiled()) {
		if (L) {
			_execute_bytecode();
		}
	}
}

void LuauInstance::update_exports() {
}

LuauInstance::LuauInstance() {
}

LuauInstance::~LuauInstance() {
	if (script.is_valid()) {
		script->_remove_instance(this);
	}

	if (L) {
		lua_close(L);
		L = nullptr;
	}
}
