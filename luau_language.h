/**************************************************************************/
/*  luau_language.h                                                       */
/**************************************************************************/

#ifndef LUAU_LANGUAGE_H
#define LUAU_LANGUAGE_H

#include "core/object/script_language.h"

class LuauLanguage : public ScriptLanguage {
	GDCLASS(LuauLanguage, ScriptLanguage);

public:
	String get_name() const override;

	void init() override;
	String get_type() const override;
	String get_extension() const override;
	void finish() override;

	Vector<String> get_reserved_words() const override;
	bool is_control_flow_keyword(const String &p_string) const override;
	Vector<String> get_comment_delimiters() const override;
	Vector<String> get_doc_comment_delimiters() const override;
	Vector<String> get_string_delimiters() const override;
	Ref<Script> make_template(const String &p_template, const String &p_class_name, const String &p_base_class_name) const override;
	bool validate(const String &p_script, const String &p_path, List<String> *r_functions, List<ScriptError> *r_errors, List<Warning> *r_warnings, HashSet<int> *r_safe_lines) const override;
	String validate_path(const String &p_path) const override;
	Script *create_script() const override;
	bool supports_builtin_mode() const override;
	int find_function(const String &p_function, const String &p_code) const override;
	String make_function(const String &p_class, const String &p_name, const PackedStringArray &p_args) const override;
	void auto_indent_code(String &p_code, int p_from_line, int p_to_line) const override;
	void add_global_constant(const StringName &p_variable, const Variant &p_value) override;

	void thread_enter() override;
	void thread_exit() override;

	String debug_get_error() const override;
	int debug_get_stack_level_count() const override;
	int debug_get_stack_level_line(int p_level) const override;
	String debug_get_stack_level_function(int p_level) const override;
	String debug_get_stack_level_source(int p_level) const override;
	void debug_get_stack_level_locals(int p_level, List<String> *p_locals, List<Variant> *p_values, int p_max_subitems, int p_max_depth) override;
	void debug_get_stack_level_members(int p_level, List<String> *p_members, List<Variant> *p_values, int p_max_subitems, int p_max_depth) override;
	void debug_get_globals(List<String> *p_globals, List<Variant> *p_values, int p_max_subitems, int p_max_depth) override;
	String debug_parse_stack_level_expression(int p_level, const String &p_expression, int p_max_subitems, int p_max_depth) override;

	void reload_all_scripts() override;
	void reload_scripts(const Array &p_scripts, bool p_soft_reload) override;
	void reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) override;

	void get_recognized_extensions(List<String> *p_extensions) const override;
	void get_public_functions(List<MethodInfo> *p_functions) const override;
	void get_public_constants(List<Pair<String, Variant>> *p_constants) const override;
	void get_public_annotations(List<MethodInfo> *p_annotations) const override;

	void profiling_start() override;
	void profiling_stop() override;
	void profiling_set_save_native_calls(bool p_enable) override;
	int profiling_get_accumulated_data(ProfilingInfo *p_info_arr, int p_info_max) override;
	int profiling_get_frame_data(ProfilingInfo *p_info_arr, int p_info_max) override;

	void frame() override;

	LuauLanguage();
	~LuauLanguage();

protected:
	static void _bind_methods();
};

#endif // LUAU_LANGUAGE_H
