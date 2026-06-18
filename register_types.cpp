/**************************************************************************/
/*  register_types.cpp                                                    */
/**************************************************************************/

#include "register_types.h"

#include "luau_language.h"
#include "luau_script.h"

#include "core/config/engine.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"

LuauLanguage *luau_language = nullptr;

Ref<ResourceFormatLoaderLuauScript> resource_loader_luau;
Ref<ResourceFormatSaverLuauScript> resource_saver_luau;

void initialize_luau_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		ClassDB::register_class<LuauScript>();

		luau_language = memnew(LuauLanguage);
		ScriptServer::register_language(luau_language);

		resource_loader_luau.instantiate();
		ResourceLoader::add_resource_format_loader(resource_loader_luau);

		resource_saver_luau.instantiate();
		ResourceSaver::add_resource_format_saver(resource_saver_luau);
	}
}

void uninitialize_luau_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		ScriptServer::unregister_language(luau_language);

		if (luau_language) {
			memdelete(luau_language);
			luau_language = nullptr;
		}

		ResourceLoader::remove_resource_format_loader(resource_loader_luau);
		resource_loader_luau.unref();

		ResourceSaver::remove_resource_format_saver(resource_saver_luau);
		resource_saver_luau.unref();
	}
}
