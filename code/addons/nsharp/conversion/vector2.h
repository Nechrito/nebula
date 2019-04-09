#pragma once
//------------------------------------------------------------------------------
/**
	Mono::Vector2

	Contains helper functions for creating and converting
	nebula float2's to mono objects.

	(C) 2019 Individual contributors, see AUTHORS file
*/
//------------------------------------------------------------------------------
#include "monoconfig.h"
#include "math/float2.h"
#include "mono/metadata/image.h"
#include "mono/metadata/object.h"

namespace Mono
{

class Vector2
{
public:
	static void Setup(MonoImage* image);
	static MonoObject* Convert(Math::float2 const& vec);
};

} // namespace Mono
