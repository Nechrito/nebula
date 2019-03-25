//------------------------------------------------------------------------------
//  monobindings.cc
//  (C) 2019 Individual contributors, see AUTHORS file
//------------------------------------------------------------------------------
#include "foundation/stdneb.h"
#include "monobindings.h"
#include "core/debug.h"
#include "mono/metadata/loader.h"
#include "conversion/matrix44.h"
#include "mono/jit/jit.h"
#include "mono/metadata/assembly.h"
#include "mono/metadata/debug-helpers.h"

#include "basegamefeature\components\transformcomponent.h"


namespace Mono
{
// preloaded classes and methods used in internal calls
static MonoClass* vector2Class;
static MonoClass* vector3Class;
static MonoClass* vector4Class;
static MonoClass* quaternionClass;

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

	IO::URI uri = IO::URI("bin:xna_math.dll");
	Util::String path = uri.AsString();

	assembly = mono_domain_assembly_open(mono_domain_get(), path.AsCharPtr());
	if (!assembly)
		n_error("xna_math.dll initialization: Could not load assembly!");
	
	char* argc[1] = { "xna_math.dll" };

	MonoImage* image = mono_assembly_get_image(assembly);

	Mono::Matrix44::Setup(image);

	vector2Class = mono_class_from_name(image, "Microsoft.Xna.Framework", "Vector2");
	vector3Class = mono_class_from_name(image, "Microsoft.Xna.Framework", "Vector3");
	vector4Class = mono_class_from_name(image, "Microsoft.Xna.Framework", "Vector4");
	quaternionClass = mono_class_from_name(image, "Microsoft.Xna.Framework", "Quaternion");

	MonoBindings::SetupInternalCalls();
}

//------------------------------------------------------------------------------
/**
*/
void
MonoBindings::SetupInternalCalls()
{
	mono_add_internal_call("Game::GetTransform", MonoBindings::GetTransform);
}

//------------------------------------------------------------------------------
/**
*/
MonoObject*
MonoBindings::GetTransform(unsigned int entity)
{
	Math::matrix44 mat = Game::TransformComponent::GetWorldTransform(Game::Entity(entity));
	return Mono::Matrix44::Convert(mat);
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
