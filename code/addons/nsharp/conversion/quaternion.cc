//------------------------------------------------------------------------------
//  matrix44.cc
//  (C) 2019 Individual contributors, see AUTHORS file
//------------------------------------------------------------------------------
#include "stdneb.h"
#include "quaternion.h"
#include "math/quaternion.h"
#include "mono/jit/jit.h"
#include "mono/metadata/image.h"
#include "mono/metadata/class.h"
#include "mono/metadata/debug-helpers.h"

namespace Mono
{

static MonoClass* quaternionClass;
static MonoMethod* quaternionConstructor;

void Quaternion::Setup(MonoImage * image)
{
	quaternionClass = mono_class_from_name(image, MATH_NAMESPACE, "Quaternion");
	MonoMethodDesc* desc = mono_method_desc_new(":.ctor (single,single,single,single)", false);
	quaternionConstructor = mono_method_desc_search_in_class(desc, quaternionClass);
	mono_method_desc_free(desc);

	if (!quaternionConstructor)
		n_error("Could not find Quaternion constructor!");
}

//------------------------------------------------------------------------------
/**
*/
MonoObject*
Quaternion::Convert(Math::quaternion const & quat)
{
	MonoObject* retval = mono_object_new(mono_domain_get(), quaternionClass);
	float x = quat.x();
	float y = quat.y();
	float z = quat.z();
	float w = quat.w();
	void* args[] = {
		(void*)&x,
		(void*)&y,
		(void*)&z,
		(void*)&w
	};
	mono_runtime_invoke(quaternionConstructor, retval, args, NULL);
	return retval;
}

} // namespace Mono
