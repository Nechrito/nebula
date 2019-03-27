#pragma once
//------------------------------------------------------------------------------
/**
	@file monobindings.h

	Bindings specifically for mono scripting

	(C) 2019 Individual contributors, see AUTHORS file
*/
//------------------------------------------------------------------------------
#include "scripting/scriptconfig.h"
#include "mono/metadata/object.h"
#include "game/entity.h"
#include "math/matrix44.h"

namespace Mono
{


class MonoBindings
{
public:
	MonoBindings();
	~MonoBindings();

	void Initialize();

private:
	static void SetupInternalCalls();

	static Math::matrix44 GetTransform(unsigned int entity);
	static bool EntityIsValid(unsigned int entity);
	static MonoObject* CreateEntity();
};

//------------------------------------------------------------------------------
/**
*/
NEBULA_EXPORT void N_Print(const char *string, int32_t is_stdout);

//------------------------------------------------------------------------------
/**
*/
NEBULA_EXPORT void N_Error(const char *string, int32_t is_stdout);

//------------------------------------------------------------------------------
/**
*/
NEBULA_EXPORT void N_Log(const char *log_domain, const char *log_level, const char *message, int32_t fatal, void *user_data);


} // namespace Mono
