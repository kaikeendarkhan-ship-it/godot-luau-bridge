/**************************************************************************/
/*  luau_script.h                                                         */
/**************************************************************************/

#ifndef LUAU_SCRIPT_H
#define LUAU_SCRIPT_H

#include "core/io/resource.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/script_language.h"

#include <lua.h>

class LuauLanguage;
class LuauInstance;

class LuauScript : public Script {
	GDCLASS(LuauScript, Script);

public:
	struct CompilerOptions {
		int optimization_level = 1;
		int debug_level = 1;
		bool coverage_enabled = false;
		bool concurrent_compile = false;
	};

	struct Opts {
		Vector<uint8_t> bytecode;
	};

private:
	String source_code;

	Vector<uint8_t> compiled_bytecode;
	bool is_compiled = false;

	HashSet<LuauInstance *> instances;

public:
	bool is_script_compiled() const { return is_compiled; }
	const Vector<uint8_t> &get_compiled_bytecode() const { return compiled_bytecode; }

protected:
	static void _bind_methods();

public:
	bool can_instantiate() const override;
	Ref<Script> get_base_script() const override;
	StringName get_global_name() const override;
	bool inherits_script(const Ref<Script> &p_script) const override;
	StringName get_instance_base_type() const override;

	ScriptInstance *instance_create(Object *p_this) override;
	PlaceHolderScriptInstance *placeholder_instance_create(Object *p_this) override;
	bool instance_has(const Object *p_this) const override;

	bool has_source_code() const override;
	String get_source_code() const override;
	void set_source_code(const String &p_code) override;
	Error reload(bool p_keep_state = false) override;

#ifdef TOOLS_ENABLED
	StringName get_doc_class_name() const override;
	Vector<DocData::ClassDoc> get_documentation() const override;
	String get_class_icon_path() const override;
#endif

	bool has_method(const StringName &p_method) const override;
	bool has_static_method(const StringName &p_method) const override;
	int get_script_method_argument_count(const StringName &p_method, bool *r_is_valid = nullptr) const override;
	MethodInfo get_method_info(const StringName &p_method) const override;

	bool is_tool() const override;
	bool is_valid() const override;
	bool is_abstract() const override;

	ScriptLanguage *get_language() const override;

	bool has_script_signal(const StringName &p_signal) const override;
	void get_script_signal_list(List<MethodInfo> *r_signals) const override;

	bool get_property_default_value(const StringName &p_property, Variant &r_value) const override;

	void get_script_method_list(List<MethodInfo> *p_list) const override;
	void get_script_property_list(List<PropertyInfo> *p_list) const override;

	const Variant get_rpc_config() const override;

	static bool compile_code(const String &p_code, const String &p_path, const CompilerOptions &p_opts, Opts &r_opts, String &r_error);

	void _add_instance(LuauInstance *p_instance);
	void _remove_instance(LuauInstance *p_instance);

	LuauLanguage *get_luau_language() const;

	LuauScript();
	~LuauScript();
};

class ResourceFormatLoaderLuauScript : public ResourceFormatLoader {
	GDCLASS(ResourceFormatLoaderLuauScript, ResourceFormatLoader);

public:
	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual bool handles_type(const String &p_type) const override;
	virtual String get_resource_type(const String &p_path) const override;
};

class ResourceFormatSaverLuauScript : public ResourceFormatSaver {
	GDCLASS(ResourceFormatSaverLuauScript, ResourceFormatSaver);

public:
	virtual Error save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags = 0) override;
	virtual void get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const override;
	virtual bool recognize(const Ref<Resource> &p_resource) const override;
};

#endif // LUAU_SCRIPT_H
