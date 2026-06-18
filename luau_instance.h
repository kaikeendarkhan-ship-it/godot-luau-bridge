/**************************************************************************/
/*  luau_instance.h                                                       */
/**************************************************************************/

#ifndef LUAU_INSTANCE_H
#define LUAU_INSTANCE_H

#include "core/object/script_language.h"

#include <lua.h>

class LuauScript;
class LuauLanguage;

class LuauInstance : public ScriptInstance {
	friend class LuauScript;

	Object *owner = nullptr;
	Ref<LuauScript> script;
	lua_State *L = nullptr;

	// Property cache
	HashMap<StringName, Variant> property_values;

	void _create_lua_state();
	void _setup_metatable();
	void _push_owner();
	void _execute_bytecode();

public:
	virtual bool set(const StringName &p_name, const Variant &p_value) override;
	virtual bool get(const StringName &p_name, Variant &r_ret) const override;
	virtual void get_property_list(List<PropertyInfo> *p_properties) const override;
	virtual Variant::Type get_property_type(const StringName &p_name, bool *r_is_valid = nullptr) const override;
	virtual void validate_property(PropertyInfo &p_property) const override;

	virtual bool property_can_revert(const StringName &p_name) const override;
	virtual bool property_get_revert(const StringName &p_name, Variant &r_ret) const override;

	virtual Object *get_owner() override;
	virtual void get_method_list(List<MethodInfo> *p_list) const override;
	virtual bool has_method(const StringName &p_method) const override;
	virtual int get_method_argument_count(const StringName &p_method, bool *r_is_valid = nullptr) const override;

	virtual Variant callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) override;

	virtual void notification(int p_notification, bool p_reversed = false) override;

	virtual Ref<Script> get_script() const override;

	virtual ScriptLanguage *get_language() override;

	virtual bool is_placeholder() const override;

	virtual const Variant get_rpc_config() const override;

	void set_script(const Ref<LuauScript> &p_script);
	void set_owner(Object *p_owner);
	void _reload();
	void update_exports();

	lua_State *get_lua_state() const { return L; }

	LuauInstance();
	~LuauInstance();
};

#endif // LUAU_INSTANCE_H
