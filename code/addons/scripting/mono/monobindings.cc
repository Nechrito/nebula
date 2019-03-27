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

#include "conversion/vector4.h"
#include "conversion/vector3.h"
#include "conversion/vector2.h"
#include "conversion/quaternion.h"
#include "conversion/bbox.h"
#include "conversion/entity.h"

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
	MonoAssembly* assembly;
	MonoImage* image;
	// setup from main assembly
	image = mono_assembly_get_image(mono_assembly_get_main());
	Mono::Entity::Setup(image);

	MonoBindings::SetupInternalCalls();

	IO::URI uri = IO::URI("bin:xna_math.dll");
	Util::String path = uri.AsString();
	
	assembly = mono_domain_assembly_open(mono_domain_get(), path.AsCharPtr());
	if (!assembly)
		n_error("xna_math.dll initialization: Could not load assembly!");
	
	char* argc[1] = { "xna_math.dll" };
	
	image = mono_assembly_get_image(assembly);

	Mono::Matrix44::Setup(image);
	Mono::Vector4::Setup(image);
	Mono::Vector3::Setup(image);
	Mono::Vector2::Setup(image);
	Mono::Quaternion::Setup(image);
	Mono::BoundingBox::Setup(image);
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
}

//------------------------------------------------------------------------------
/**
*/
Math::matrix44
MonoBindings::GetTransform(unsigned int entity)
{
	return Game::TransformComponent::GetWorldTransform(Game::Entity(entity));
}

//------------------------------------------------------------------------------
/**
*/
void
MonoBindings::SetTransform(Game::Entity* entity, MonoObject* mat)
{
	Game::TransformComponent::SetLocalTransform(*entity, Matrix44::Convert(mat));
}

//------------------------------------------------------------------------------
/**
*/
bool
MonoBindings::EntityIsValid(unsigned int entity)
{
	return Game::EntityManager::Instance()->IsAlive(entity);
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
