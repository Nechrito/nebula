#pragma once
//------------------------------------------------------------------------------
/**
	Mono::Vector3

	Contains helper functions for creating and converting
	nebula vector3's to mono objects.

	(C) 2019 Individual contributors, see AUTHORS file
*/
//------------------------------------------------------------------------------
#include "monoconfig.h"
#include "math/float4.h"
#include "mono/metadata/image.h"
#include "mono/metadata/object.h"

namespace Mono
{

class Vector3
{
public:
	static void Setup(MonoImage* image);
	static MonoObject* Convert(Math::float4 const& vec);
};

} // namespace Mono
