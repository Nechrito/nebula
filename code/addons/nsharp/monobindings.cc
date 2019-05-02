//------------------------------------------------------------------------------
//  monobindings.cc
//  (C) 2019 Individual contributors, see AUTHORS file
//------------------------------------------------------------------------------
#include "foundation/stdneb.h"
#include "monobindings.h"
#include "core/debug.h"
#include "mono/metadata/loader.h"
#include "mono/jit/jit.h"
#include "mono/metadata/assembly.h"
#include "mono/metadata/debug-helpers.h"

#include "basegamefeature\components\transformcomponent.h"

namespace Mono
{

MonoBindings::MonoBindings()
{
}

MonoBindings::~MonoBindings()
{
}

//------------------------------------------------------------------------------
/**
*/
void
MonoBindings::Initialize()
{
	MonoBindings::SetupInternalCalls();
}

//------------------------------------------------------------------------------
/**
*/
void
MonoBindings::SetupInternalCalls()
{
	mono_add_internal_call("Nebula.Game.Entity::GetTransform", MonoBindings::GetTransform);
	mono_add_internal_call("Nebula.Game.Entity::SetTransform", MonoBindings::SetTransform);
	mono_add_internal_call("Nebula.Game.Entity::IsAlive", MonoBindings::EntityIsValid);
	mono_add_internal_call("Nebula.EntityManager::CreateEntity", MonoBindings::CreateEntity);
	mono_add_internal_call("Nebula.EntityManager::DeleteEntity", MonoBindings::DeleteEntity);
}

//------------------------------------------------------------------------------
/**
*/
Math::matrix44
MonoBindings::GetTransform(Game::Entity* entity)
{
	return Game::TransformComponent::GetWorldTransform(*entity);
}

//------------------------------------------------------------------------------
/**
*/
void
MonoBindings::SetTransform(Game::Entity* entity, Math::matrix44* object)
{
	// Align memory to 16 bytes since c# can't do it for us.
	// just copy the data to the stack
	static Math::matrix44 mat;
	mat.loadu((Math::scalar*)object);
	Game::TransformComponent::SetLocalTransform(*entity, mat);
}

//------------------------------------------------------------------------------
/**
*/
bool
MonoBindings::EntityIsValid(Game::Entity* entity)
{
	return Game::EntityManager::Instance()->IsAlive(*entity);
}

//------------------------------------------------------------------------------
/**
	Can't pass the entity struct directly, however we can pass
	the id type (blittable) and implicitly convert it to Game.Entity in C#
*/
decltype(Game::Entity::id)
MonoBindings::CreateEntity()
{
	Game::Entity entity = Game::EntityManager::Instance()->NewEntity();
	Game::TransformComponent::RegisterEntity(entity);
	return entity.id;
}

//------------------------------------------------------------------------------
/**
*/
void
MonoBindings::DeleteEntity(Game::Entity* entity)
{
	Game::EntityManager::Instance()->DeleteEntity(*entity);
}

//------------------------------------------------------------------------------
/**
*/
void
N_Print(const char * string, int32_t is_stdout)
{
	n_printf(string);
}

//------------------------------------------------------------------------------
/**
*/
void
N_Error(const char * string, int32_t is_stdout)
{
	n_error(string);
}

//------------------------------------------------------------------------------
/**
*/
void
N_Log(const char * log_domain, const char * log_level, const char * message, int32_t fatal, void * user_data)
{
	if (fatal)
	{
		n_error(message);
	}
	else
	{
		n_warning(message);
	}
}


} // namespace Mono
