#pragma once
//------------------------------------------------------------------------------
/**
	Implements a read/write image in Vulkan.
	
	(C) 2016-2018 Individual contributors, see AUTHORS file
*/
//------------------------------------------------------------------------------
#include "ids/idallocator.h"
#include "coregraphics/shaderrwtexture.h"
namespace Vulkan
{

struct VkShaderRWTextureLoadInfo
{
	VkDevice dev;
	VkImage img;
	VkDeviceMemory mem;
	CoreGraphics::TextureDimensions dims;
	SizeT layers;
    float widthScale, heightScale, depthScale;
    bool relativeSize;
};

struct VkShaderRWTextureRuntimeInfo
{
	VkImageView view;
	uint32_t bind;
};

typedef Ids::IdAllocator<
	VkShaderRWTextureLoadInfo,
	VkShaderRWTextureRuntimeInfo,
    CoreGraphicsImageLayout,
    CoreGraphics::ShaderRWTextureInfo
> ShaderRWTextureAllocator;
extern ShaderRWTextureAllocator shaderRWTextureAllocator;

/// get vk image view
const VkImageView ShaderRWTextureGetVkImageView(const CoreGraphics::ShaderRWTextureId id);
/// get vk image
const VkImage ShaderRWTextureGetVkImage(const CoreGraphics::ShaderRWTextureId id);

} // namespace Vulkan