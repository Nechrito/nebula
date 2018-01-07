//------------------------------------------------------------------------------
// vktexture.cc
// (C) 2016 Individual contributors, see AUTHORS file
//------------------------------------------------------------------------------
#include "stdneb.h"
#include "vktexture.h"
#include "vulkan/vulkan.h"

namespace Vulkan
{

VkTextureAllocator textureAllocator(0x00FFFFFF);

//------------------------------------------------------------------------------
/**
*/
const VkImage
TextureGetVk(const CoreGraphics::TextureId id)
{
	return textureAllocator.GetSafe<1>(id.id24).img;
}

} // namespace Vulkan
