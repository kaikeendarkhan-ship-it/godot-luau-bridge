/**************************************************************************/
/*  register_types.h                                                      */
/**************************************************************************/

#ifndef LUAU_REGISTER_TYPES_H
#define LUAU_REGISTER_TYPES_H

#include "modules/register_module_types.h"

void initialize_luau_module(ModuleInitializationLevel p_level);
void uninitialize_luau_module(ModuleInitializationLevel p_level);

#endif // LUAU_REGISTER_TYPES_H
