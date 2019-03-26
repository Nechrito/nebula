//------------------------------------------------------------------------------
//  vector2.cc
//  (C) 2019 Individual contributors, see AUTHORS file
//------------------------------------------------------------------------------
#include "stdneb.h"
#include "vector2.h"
#include "math/float2.h"
#include "mono/jit/jit.h"
#include "mono/metadata/image.h"
#include "mono/metadata/class.h"
#include "mono/metadata/debug-helpers.h"

namespace Mono
{

static MonoClass* vector2Class;
static MonoMethod* vector2Constructor;

void Vector2::Setup(MonoImage * image)
{
	vector2Class = mono_class_from_name(image, MATH_NAMESPACE, "Vector2");
	MonoMethodDesc* desc = mono_method_desc_new(":.ctor (single,single)", false);
	vector2Constructor = mono_method_desc_search_in_class(desc, vector2Class);
	mono_method_desc_free(desc);

	if (!vector2Constructor)
		n_error("Could not find Vector2 constructor!");
}

//------------------------------------------------------------------------------
/**
*/
MonoObject*
Vector2::Convert(Math::float2 const & vec)
{
	MonoObject* retval = mono_object_new(mono_domain_get(), vector2Class);
	float x = vec.x();
	float y = vec.y();
	void* args[] = {
		(void*)&x,
		(void*)&y
	};
	mono_runtime_invoke(vector2Constructor, retval, args, NULL);
	return retval;
}

} // namespace Mono
