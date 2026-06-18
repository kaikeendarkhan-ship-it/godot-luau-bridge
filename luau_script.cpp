/**************************************************************************/
/*  luau_script.cpp                                                       */
/**************************************************************************/

#include "luau_script.h"

#include "luau_language.h"
#include "luau_instance.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/file_access.h"

#include <lua.h>
#include <lualib.h>
#include <string>

// Defined in luau_compile_wrapper.mm — catches ObjC exceptions before they enter Luau C++ handlers
std::string luau_compile_safe(const std::string &source, int optimizationLevel, int debugLevel);

void LuauScript::_bind_methods() {
}

bool LuauScript::can_instantiate() const {
#ifdef TOOLS_ENABLED
	return is_compiled && ScriptServer::is_scripting_enabled()
			&& !Engine::get_singleton()->is_recovery_mode_hint();
#else
	return is_compiled;
#endif
}

Ref<Script> LuauScript::get_base_script() const {
	return Ref<Script>();
}

StringName LuauScript::get_global_name() const {
	return StringName();
}

bool LuauScript::inherits_script(const Ref<Script> &p_script) const {
	return false;
}

StringName LuauScript::get_instance_base_type() const {
	return StringName();
}

ScriptInstance *LuauScript::instance_create(Object *p_this) {
	LuauInstance *instance = memnew(LuauInstance);
	instance->set_owner(p_this);
	instance->set_script(Ref<LuauScript>(this));
	_add_instance(instance);
	return instance;
}

PlaceHolderScriptInstance *LuauScript::placeholder_instance_create(Object *p_this) {
	return memnew(PlaceHolderScriptInstance(get_language(), Ref<Script>(this), p_this));
}

bool LuauScript::instance_has(const Object *p_this) const {
	for (LuauInstance *inst : instances) {
		if (inst && inst->get_owner() == p_this) {
			return true;
		}
	}
	return false;
}

bool LuauScript::has_source_code() const {
	return !source_code.is_empty();
}

String LuauScript::get_source_code() const {
	return source_code;
}

void LuauScript::set_source_code(const String &p_code) {
	source_code = p_code;
}

Error LuauScript::reload(bool p_keep_state) {
	if (get_path().is_empty()) {
		return ERR_FILE_NOT_FOUND;
	}

	Error err;
	Ref<FileAccess> file = FileAccess::open(get_path(), FileAccess::READ, &err);
	if (file.is_null()) {
		ERR_PRINT("Failed to open Luau script: " + get_path());
		return err;
	}

	source_code = file->get_as_text();
	file->close();

	CompilerOptions opts;
	Opts result;
	String error;

	bool success = compile_code(source_code, get_path(), opts, result, error);
	if (!success) {
		ERR_PRINT("Luau compilation error: " + error);
		return ERR_COMPILATION_FAILED;
	}

	compiled_bytecode = result.bytecode;
	is_compiled = true;

	for (LuauInstance *inst : instances) {
		if (inst) {
			inst->_reload();
		}
	}

	return OK;
}

#ifdef TOOLS_ENABLED
StringName LuauScript::get_doc_class_name() const {
	return StringName("LuauScript");
}

Vector<DocData::ClassDoc> LuauScript::get_documentation() const {
	return Vector<DocData::ClassDoc>();
}

String LuauScript::get_class_icon_path() const {
	return String();
}
#endif

bool LuauScript::has_method(const StringName &p_method) const {
	return false;
}

bool LuauScript::has_static_method(const StringName &p_method) const {
	return false;
}

int LuauScript::get_script_method_argument_count(const StringName &p_method, bool *r_is_valid) const {
	if (r_is_valid) {
		*r_is_valid = false;
	}
	return 0;
}

MethodInfo LuauScript::get_method_info(const StringName &p_method) const {
	return MethodInfo();
}

bool LuauScript::is_tool() const {
	return false;
}

bool LuauScript::is_valid() const {
	return is_compiled;
}

bool LuauScript::is_abstract() const {
	return false;
}

ScriptLanguage *LuauScript::get_language() const {
	return get_luau_language();
}

bool LuauScript::has_script_signal(const StringName &p_signal) const {
	return false;
}

void LuauScript::get_script_signal_list(List<MethodInfo> *r_signals) const {
}

bool LuauScript::get_property_default_value(const StringName &p_property, Variant &r_value) const {
	return false;
}

void LuauScript::get_script_method_list(List<MethodInfo> *p_list) const {
}

void LuauScript::get_script_property_list(List<PropertyInfo> *p_list) const {
}

const Variant LuauScript::get_rpc_config() const {
	return Variant();
}

bool LuauScript::compile_code(const String &p_code, const String &p_path, const CompilerOptions &p_opts, Opts &r_opts, String &r_error) {
	CharString source_utf8 = p_code.utf8();

	std::string bytecode = luau_compile_safe(
		std::string(source_utf8.get_data(), source_utf8.length()),
		p_opts.optimization_level,
		p_opts.debug_level
	);

	if (bytecode.empty()) {
		r_error = "Compilation failed";
		return false;
	}

	r_opts.bytecode.resize(bytecode.size());
	memcpy(r_opts.bytecode.ptrw(), bytecode.data(), bytecode.size());

	return true;
}

void LuauScript::_add_instance(LuauInstance *p_instance) {
	instances.insert(p_instance);
}

void LuauScript::_remove_instance(LuauInstance *p_instance) {
	instances.erase(p_instance);
}

LuauLanguage *LuauScript::get_luau_language() const {
	for (int i = 0; i < ScriptServer::get_language_count(); i++) {
		ScriptLanguage *lang = ScriptServer::get_language(i);
		if (lang && lang->get_type() == "Luau") {
			return Object::cast_to<LuauLanguage>(lang);
		}
	}
	return nullptr;
}

LuauScript::LuauScript() {
	is_compiled = false;
}

LuauScript::~LuauScript() {
	instances.clear();
}

// ResourceFormatLoaderLuauScript

Ref<Resource> ResourceFormatLoaderLuauScript::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	Ref<LuauScript> script;
	script.instantiate();
	script->set_path(p_path);

	Error err = script->reload(true);

	if (r_error) {
		*r_error = err;
	}

	return script;
}

void ResourceFormatLoaderLuauScript::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("luau");
}

bool ResourceFormatLoaderLuauScript::handles_type(const String &p_type) const {
	return (p_type == "Script" || p_type == "LuauScript");
}

String ResourceFormatLoaderLuauScript::get_resource_type(const String &p_path) const {
	if (p_path.ends_with(".luau")) {
		return "LuauScript";
	}
	return "";
}

// ResourceFormatSaverLuauScript

Error ResourceFormatSaverLuauScript::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	Ref<LuauScript> script = p_resource;
	if (script.is_null()) {
		return ERR_INVALID_PARAMETER;
	}

	Error err;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);
	if (file.is_null()) {
		return err;
	}

	file->store_string(script->get_source_code());
	file->close();

	return OK;
}

void ResourceFormatSaverLuauScript::get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const {
	if (Object::cast_to<LuauScript>(p_resource.ptr())) {
		p_extensions->push_back("luau");
	}
}

bool ResourceFormatSaverLuauScript::recognize(const Ref<Resource> &p_resource) const {
	return Object::cast_to<LuauScript>(p_resource.ptr()) != nullptr;
}
