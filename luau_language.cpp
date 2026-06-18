/**************************************************************************/
/*  luau_language.cpp                                                     */
/**************************************************************************/

#include "luau_language.h"

#include "luau_script.h"

#include "core/config/project_settings.h"
#include "core/io/file_access.h"

void LuauLanguage::_bind_methods() {
}

String LuauLanguage::get_name() const {
	return "Luau";
}

void LuauLanguage::init() {
}

String LuauLanguage::get_type() const {
	return "Luau";
}

String LuauLanguage::get_extension() const {
	return "luau";
}

void LuauLanguage::finish() {
}

Vector<String> LuauLanguage::get_reserved_words() const {
	static const char *keywords[] = {
		"and", "break", "do", "else", "elseif", "end",
		"false", "for", "function", "if", "in",
		"local", "nil", "not", "or", "repeat",
		"return", "then", "true", "until", "while",
		"continue", "type", "typeof", "assert", "require",
		nullptr
	};

	Vector<String> words;
	const char **kw = keywords;
	while (*kw) {
		words.push_back(*kw);
		kw++;
	}
	return words;
}

bool LuauLanguage::is_control_flow_keyword(const String &p_string) const {
	return p_string == "if" || p_string == "else" || p_string == "elseif" ||
			p_string == "for" || p_string == "while" || p_string == "repeat" ||
			p_string == "until" || p_string == "break" || p_string == "return" ||
			p_string == "do" || p_string == "end" || p_string == "in";
}

Vector<String> LuauLanguage::get_comment_delimiters() const {
	Vector<String> delimiters;
	delimiters.push_back("--");
	return delimiters;
}

Vector<String> LuauLanguage::get_doc_comment_delimiters() const {
	Vector<String> delimiters;
	delimiters.push_back("---");
	return delimiters;
}

Vector<String> LuauLanguage::get_string_delimiters() const {
	Vector<String> delimiters;
	delimiters.push_back("\"");
	delimiters.push_back("'");
	delimiters.push_back("[[");
	return delimiters;
}

Ref<Script> LuauLanguage::make_template(const String &p_template, const String &p_class_name, const String &p_base_class_name) const {
	Ref<LuauScript> script;
	script.instantiate();

	String code = "local " + p_class_name + " = {}\n\nfunction " + p_class_name + ".new()\n\tlocal self = setmetatable({}, " + p_class_name + ")\n\treturn self\nend\n\nreturn " + p_class_name;
	script->set_source_code(code);
	return script;
}

bool LuauLanguage::validate(const String &p_script, const String &p_path, List<String> *r_functions, List<ScriptError> *r_errors, List<Warning> *r_warnings, HashSet<int> *r_safe_lines) const {
	return true;
}

String LuauLanguage::validate_path(const String &p_path) const {
	return "";
}

Script *LuauLanguage::create_script() const {
	return memnew(LuauScript);
}

bool LuauLanguage::supports_builtin_mode() const {
	return false;
}

int LuauLanguage::find_function(const String &p_function, const String &p_code) const {
	return -1;
}

String LuauLanguage::make_function(const String &p_class, const String &p_name, const PackedStringArray &p_args) const {
	String args;
	for (int i = 0; i < p_args.size(); i++) {
		if (i > 0) {
			args += ", ";
		}
		args += p_args[i];
	}
	return "function " + p_class + "." + p_name + "(" + args + ")\n\t-- TODO\nend";
}

void LuauLanguage::auto_indent_code(String &p_code, int p_from_line, int p_to_line) const {
}

void LuauLanguage::add_global_constant(const StringName &p_variable, const Variant &p_value) {
}

void LuauLanguage::thread_enter() {
}

void LuauLanguage::thread_exit() {
}

String LuauLanguage::debug_get_error() const {
	return "";
}

int LuauLanguage::debug_get_stack_level_count() const {
	return 0;
}

int LuauLanguage::debug_get_stack_level_line(int p_level) const {
	return 0;
}

String LuauLanguage::debug_get_stack_level_function(int p_level) const {
	return "";
}

String LuauLanguage::debug_get_stack_level_source(int p_level) const {
	return "";
}

void LuauLanguage::debug_get_stack_level_locals(int p_level, List<String> *p_locals, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
}

void LuauLanguage::debug_get_stack_level_members(int p_level, List<String> *p_members, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
}

void LuauLanguage::debug_get_globals(List<String> *p_globals, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
}

String LuauLanguage::debug_parse_stack_level_expression(int p_level, const String &p_expression, int p_max_subitems, int p_max_depth) {
	return "";
}

void LuauLanguage::reload_all_scripts() {
}

void LuauLanguage::reload_scripts(const Array &p_scripts, bool p_soft_reload) {
}

void LuauLanguage::reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) {
}

void LuauLanguage::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("luau");
}

void LuauLanguage::get_public_functions(List<MethodInfo> *p_functions) const {
}

void LuauLanguage::get_public_constants(List<Pair<String, Variant>> *p_constants) const {
}

void LuauLanguage::get_public_annotations(List<MethodInfo> *p_annotations) const {
}

void LuauLanguage::profiling_start() {
}

void LuauLanguage::profiling_stop() {
}

void LuauLanguage::profiling_set_save_native_calls(bool p_enable) {
}

int LuauLanguage::profiling_get_accumulated_data(ProfilingInfo *p_info_arr, int p_info_max) {
	return 0;
}

int LuauLanguage::profiling_get_frame_data(ProfilingInfo *p_info_arr, int p_info_max) {
	return 0;
}

void LuauLanguage::frame() {
}

LuauLanguage::LuauLanguage() {
}

LuauLanguage::~LuauLanguage() {
}
