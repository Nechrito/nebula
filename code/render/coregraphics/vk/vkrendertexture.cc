//------------------------------------------------------------------------------
// vkrendertexture.cc
// (C) 2016-2018 Individual contributors, see AUTHORS file
//------------------------------------------------------------------------------
#include "render/stdneb.h"
#include "vkrendertexture.h"
#include "vktypes.h"
#include "vkgraphicsdevice.h"
#include "vkutilities.h"
#include "coregraphics/window.h"
#include "coregraphics/displaydevice.h"
#include "coregraphics/config.h"
#include "coregraphics/glfw/glfwwindow.h"
#include "coregraphics/shaderserver.h"

namespace Vulkan
{

VkRenderTextureAllocator renderTextureAllocator;

}

namespace CoreGraphics
{

using namespace Vulkan;

//------------------------------------------------------------------------------
/**
*/
void
SetupRenderTexture(const RenderTextureId rtId, const RenderTextureInfo& adjustedInfo)
{
    // RenderTextureInfo adjustedInfo = RenderTextureInfoSetupHelper(info);
    VkRenderTextureLoadInfo& loadInfo = renderTextureAllocator.Get<0>(rtId.id24);
    VkRenderTextureRuntimeInfo& runtimeInfo = renderTextureAllocator.Get<1>(rtId.id24);
    VkRenderTextureMappingInfo& mapInfo = renderTextureAllocator.Get<2>(rtId.id24);
    VkRenderTextureWindowInfo& swapInfo = renderTextureAllocator.Get<3>(rtId.id24);
    CoreGraphicsImageLayout& layout = renderTextureAllocator.Get<4>(rtId.id24);

    // set map to 0
    mapInfo.mapCount = 0;

    // setup dimensions
    loadInfo.dims.width = adjustedInfo.width;
    loadInfo.dims.height = adjustedInfo.height;
    loadInfo.dims.depth = adjustedInfo.depth;
    loadInfo.widthScale = adjustedInfo.widthScale;
    loadInfo.heightScale = adjustedInfo.heightScale;
    loadInfo.depthScale = adjustedInfo.depthScale;
    loadInfo.mips = adjustedInfo.mips;
    loadInfo.layers = adjustedInfo.layers;
    loadInfo.format = adjustedInfo.format;
    loadInfo.relativeSize = adjustedInfo.relativeSize;
    loadInfo.msaa = adjustedInfo.msaa;
    loadInfo.dev = Vulkan::GetCurrentDevice();
    runtimeInfo.type = adjustedInfo.type;
    runtimeInfo.bind = -1;

    // use setup submission
    CoreGraphics::SubmissionContextId sub = CoreGraphics::GetSetupSubmissionContext();

    // if this is a window texture, get the backbuffers from the render device
    if (adjustedInfo.isWindow)
    {
        VkBackbufferInfo& backbufferInfo = CoreGraphics::glfwWindowAllocator.Get<GLFW_Backbuffer>(adjustedInfo.window.id24);
        swapInfo.swapimages = backbufferInfo.backbuffers;
        swapInfo.swapviews = backbufferInfo.backbufferViews;
        VkClearColorValue clear = { 0, 0, 0, 0 };

        VkImageSubresourceRange subres;
        subres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subres.baseArrayLayer = 0;
        subres.baseMipLevel = 0;
        subres.layerCount = 1;
        subres.levelCount = 1;

        // clear textures
        IndexT i;
        for (i = 0; i < swapInfo.swapimages.Size(); i++)
        {
            VkUtilities::ImageBarrier(SubmissionContextGetCmdBuffer(sub), CoreGraphics::BarrierStage::Host, CoreGraphics::BarrierStage::Transfer, VkUtilities::ImageMemoryBarrier(swapInfo.swapimages[i], subres, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
            VkUtilities::ImageColorClear(SubmissionContextGetCmdBuffer(sub), swapInfo.swapimages[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, clear, subres);
            VkUtilities::ImageBarrier(SubmissionContextGetCmdBuffer(sub), CoreGraphics::BarrierStage::Transfer, CoreGraphics::BarrierStage::PassOutput, VkUtilities::ImageMemoryBarrier(swapInfo.swapimages[i], subres, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR));
        }

        n_assert(adjustedInfo.type == Texture2D);
        n_assert(adjustedInfo.mips == 1);
        n_assert(!adjustedInfo.msaa);

        layout = CoreGraphicsImageLayout::Present;
        loadInfo.img = swapInfo.swapimages[0];
        loadInfo.isWindow = true;
        loadInfo.mem = VK_NULL_HANDLE;
        loadInfo.window = adjustedInfo.window;
        runtimeInfo.type = Texture2D;
        runtimeInfo.framebufferView = backbufferInfo.backbufferViews[0];
    }
    else
    {
        VkSampleCountFlagBits sampleCount = adjustedInfo.msaa ? VK_SAMPLE_COUNT_16_BIT : VK_SAMPLE_COUNT_1_BIT;

        VkExtent3D extents;
        extents.width = adjustedInfo.width;
        extents.height = adjustedInfo.height;
        extents.depth = adjustedInfo.depth;

        VkImageViewType viewType;
        switch (adjustedInfo.type)
        {
        case Texture2D:
            viewType = VK_IMAGE_VIEW_TYPE_2D;
            break;
        case Texture2DArray:
            viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            break;
        case TextureCube:
            viewType = VK_IMAGE_VIEW_TYPE_CUBE;
            break;
        case TextureCubeArray:
            viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
            break;
        }

        VkFormat fmt = VkTypes::AsVkFramebufferFormat(adjustedInfo.format);
        if (adjustedInfo.usage == ColorAttachment)
        {
            VkFormatProperties formatProps;
            vkGetPhysicalDeviceFormatProperties(Vulkan::GetCurrentPhysicalDevice(), fmt, &formatProps);
            n_assert(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT &&
                formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT &&
                formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
        }
        VkImageUsageFlags usageFlags = adjustedInfo.usage == ColorAttachment ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        VkImageCreateInfo imgInfo =
        {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            NULL,
            0,
            VK_IMAGE_TYPE_2D,   // if we have a cube map, it's just 2D * 6
            fmt,
            extents,
            1,
            loadInfo.layers,
            sampleCount,
            VK_IMAGE_TILING_OPTIMAL,
            usageFlags | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            0,
            NULL,
            VK_IMAGE_LAYOUT_UNDEFINED
        };

        // create image for rendering
        VkResult res = vkCreateImage(loadInfo.dev, &imgInfo, NULL, &loadInfo.img);
        n_assert(res == VK_SUCCESS);

        // allocate buffer backing and bind to image
        uint32_t size;
        VkUtilities::AllocateImageMemory(loadInfo.dev, loadInfo.img, loadInfo.mem, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, size);
        vkBindImageMemory(loadInfo.dev, loadInfo.img, loadInfo.mem, 0);

        VkImageSubresourceRange subres;
        subres.baseArrayLayer = 0;
        subres.baseMipLevel = 0;
        subres.layerCount = loadInfo.layers;
        subres.levelCount = 1;
        subres.aspectMask = adjustedInfo.usage == ColorAttachment ? VK_IMAGE_ASPECT_COLOR_BIT : VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        VkImageViewCreateInfo viewInfo =
        {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            NULL,
            0,
            loadInfo.img,
            viewType,
            VkTypes::AsVkFramebufferFormat(adjustedInfo.format),
            VkTypes::AsVkMapping(adjustedInfo.format),
            subres
        };

        res = vkCreateImageView(loadInfo.dev, &viewInfo, NULL, &runtimeInfo.framebufferView);
        n_assert(res == VK_SUCCESS);

        if (adjustedInfo.usage == ColorAttachment)
        {
            // clear image and transition layout
            VkClearColorValue clear = { 0, 0, 0, 0 };
            VkUtilities::ImageBarrier(SubmissionContextGetCmdBuffer(sub), CoreGraphics::BarrierStage::Host, CoreGraphics::BarrierStage::Transfer, VkUtilities::ImageMemoryBarrier(loadInfo.img, subres, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
            VkUtilities::ImageColorClear(SubmissionContextGetCmdBuffer(sub), loadInfo.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, clear, subres);
            VkUtilities::ImageBarrier(SubmissionContextGetCmdBuffer(sub), CoreGraphics::BarrierStage::Transfer, CoreGraphics::BarrierStage::PassOutput, VkUtilities::ImageMemoryBarrier(loadInfo.img, subres, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));

            // create binding
            layout = CoreGraphicsImageLayout::ShaderRead;
            runtimeInfo.sampleView = runtimeInfo.framebufferView; // for color formats, these are usually the same (but are they always)?!

            runtimeInfo.bind = VkShaderServer::Instance()->RegisterTexture(rtId, false, runtimeInfo.type);
        }
        else
        {
            // clear image and transition layout
            VkClearDepthStencilValue clear = { 1, 0 };
            VkUtilities::ImageBarrier(SubmissionContextGetCmdBuffer(sub), CoreGraphics::BarrierStage::Host, CoreGraphics::BarrierStage::Transfer, VkUtilities::ImageMemoryBarrier(loadInfo.img, subres, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
            VkUtilities::ImageDepthStencilClear(SubmissionContextGetCmdBuffer(sub), loadInfo.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, clear, subres);
            VkUtilities::ImageBarrier(SubmissionContextGetCmdBuffer(sub), CoreGraphics::BarrierStage::Transfer, CoreGraphics::BarrierStage::PassOutput, VkUtilities::ImageMemoryBarrier(loadInfo.img, subres, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));

            VkImageSubresourceRange subres;
            subres.baseArrayLayer = 0;
            subres.baseMipLevel = 0;
            subres.layerCount = loadInfo.layers;
            subres.levelCount = 1;
            subres.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            VkImageViewCreateInfo viewInfo =
            {
                VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                NULL,
                0,
                loadInfo.img,
                viewType,
                VkTypes::AsVkSampleableFormat(adjustedInfo.format),
                VkTypes::AsVkMapping(adjustedInfo.format),
                subres
            };

            res = vkCreateImageView(loadInfo.dev, &viewInfo, NULL, &runtimeInfo.sampleView);
            n_assert(res == VK_SUCCESS);

            layout = CoreGraphicsImageLayout::DepthStencilRead;
            runtimeInfo.bind = VkShaderServer::Instance()->RegisterTexture(rtId, true, runtimeInfo.type);

        }
    }

    // save render texture info for reuse when resizing
    renderTextureAllocator.Get<5>(rtId.id24) = adjustedInfo;
}


//------------------------------------------------------------------------------
/**
*/
RenderTextureId
CreateRenderTexture(const RenderTextureCreateInfo& info)
{
	Ids::Id32 id = renderTextureAllocator.Alloc();
	RenderTextureInfo adjustedInfo = RenderTextureInfoSetupHelper(info);
	RenderTextureId rtId;
	rtId.id24 = id;
	rtId.id8 = RenderTextureIdType;

    SetupRenderTexture(rtId, adjustedInfo);

#if NEBULA_GRAPHICS_DEBUG
	ObjectSetName(rtId, info.name.Value());
#endif

    CoreGraphics::RegisterRenderTexture(info.name, rtId);
    return rtId;
}

//------------------------------------------------------------------------------
/**
*/
void
CleanupRenderTexture(const RenderTextureId id)
{
    VkRenderTextureLoadInfo& loadInfo = renderTextureAllocator.Get<0>(id.id24);
    VkRenderTextureRuntimeInfo& runtimeInfo = renderTextureAllocator.Get<1>(id.id24);
    VkRenderTextureWindowInfo& swapInfo = renderTextureAllocator.Get<3>(id.id24);

    if (runtimeInfo.bind != -1)
        VkShaderServer::Instance()->UnregisterTexture(runtimeInfo.bind, runtimeInfo.type);

    if (loadInfo.isWindow)
    {
        swapInfo.swapimages.Clear();
        swapInfo.swapviews.Clear();
    }
    else
    {
        vkFreeMemory(loadInfo.dev, loadInfo.mem, nullptr);
        vkDestroyImage(loadInfo.dev, loadInfo.img, nullptr);
        if (runtimeInfo.framebufferView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(loadInfo.dev, runtimeInfo.framebufferView, nullptr);

        }
        else
        {
            vkDestroyImageView(loadInfo.dev, runtimeInfo.sampleView, nullptr);
        }
    }
}

//------------------------------------------------------------------------------
/**
*/
void
DestroyRenderTexture(const RenderTextureId id)
{
    CleanupRenderTexture(id);
    renderTextureAllocator.Dealloc(id.id24);
}

//------------------------------------------------------------------------------
/**
*/
void
RenderTextureResize(const RenderTextureId id, const RenderTextureResizeInfo& info)
{
	RenderTextureInfo adjustedInfo = renderTextureAllocator.Get<5>(id.id24);
    RenderTextureInfoResizeHelper(adjustedInfo, info);
	
    CleanupRenderTexture(id);
    SetupRenderTexture(id, adjustedInfo);

    CoreGraphics::RegisterRenderTexture(adjustedInfo.name, id);
}

//------------------------------------------------------------------------------
/**
*/
IndexT
RenderTextureSwapBuffers(const CoreGraphics::RenderTextureId id)
{
	VkRenderTextureLoadInfo& loadInfo = renderTextureAllocator.Get<0>(id.id24);
	VkRenderTextureRuntimeInfo& runtimeInfo = renderTextureAllocator.Get<1>(id.id24);
	n_assert(loadInfo.isWindow);
	VkWindowSwapInfo& swapInfo = CoreGraphics::glfwWindowAllocator.Get<5>(loadInfo.window.id24);
	VkSemaphore sem = Vulkan::GetPresentSemaphore();
	VkResult res = vkAcquireNextImageKHR(Vulkan::GetCurrentDevice(), swapInfo.swapchain, UINT64_MAX, sem, VK_NULL_HANDLE, &swapInfo.currentBackbuffer);
	Vulkan::WaitForPresent(sem);
	if (res == VK_ERROR_OUT_OF_DATE_KHR)
	{
		// this means our swapchain needs a resize!
	}
	else
	{
		n_assert(res == VK_SUCCESS);
	}

	VkRenderTextureWindowInfo& wnd = renderTextureAllocator.Get<3>(id.id24);
	// set image and update texture
	loadInfo.img = wnd.swapimages[swapInfo.currentBackbuffer];
	runtimeInfo.framebufferView = wnd.swapviews[swapInfo.currentBackbuffer];
	return swapInfo.currentBackbuffer;
	//this->texture->img = this->img;
}

//------------------------------------------------------------------------------
/**
*/
void
RenderTextureWindowResized(const RenderTextureId id)
{
	const VkRenderTextureLoadInfo& loadInfo = renderTextureAllocator.Get<0>(id.id24);
	RenderTextureResizeInfo info;
	info.width = loadInfo.dims.width;
	info.widthScale = loadInfo.widthScale;
	info.height = loadInfo.dims.height;
	info.heightScale = loadInfo.heightScale;
	info.depth = loadInfo.dims.depth;
	info.depthScale = loadInfo.depthScale;
	RenderTextureResize(id, info);
}

//------------------------------------------------------------------------------
/**
*/
const CoreGraphics::TextureDimensions
RenderTextureGetDimensions(const RenderTextureId id)
{
	return renderTextureAllocator.Get<0>(id.id24).dims;
}

//------------------------------------------------------------------------------
/**
*/
const SizeT
RenderTextureGetNumMips(const RenderTextureId id)
{
	return renderTextureAllocator.Get<0>(id.id24).mips;
}

//------------------------------------------------------------------------------
/**
*/
const SizeT 
RenderTextureGetNumLayers(const RenderTextureId id)
{
	return renderTextureAllocator.Get<0>(id.id24).layers;
}

//------------------------------------------------------------------------------
/**
*/
const CoreGraphics::PixelFormat::Code
RenderTextureGetPixelFormat(const RenderTextureId id)
{
	return renderTextureAllocator.Get<0>(id.id24).format;
}

//------------------------------------------------------------------------------
/**
*/
const bool
RenderTextureGetMSAA(const RenderTextureId id)
{
	return renderTextureAllocator.Get<0>(id.id24).msaa;
}

//------------------------------------------------------------------------------
/**
*/
const CoreGraphicsImageLayout
RenderTextureGetLayout(const RenderTextureId id)
{
	return renderTextureAllocator.Get<4>(id.id24);
}

//------------------------------------------------------------------------------
/**
*/
uint 
RenderTextureGetBindlessHandle(const RenderTextureId id)
{
	return renderTextureAllocator.Get<1>(id.id24).bind;
}

} // namespace CoreGraphics

namespace Vulkan
{

//------------------------------------------------------------------------------
/**
*/
void
RenderTextureGenerateMipChain(const CoreGraphics::RenderTextureId id)
{
	const VkRenderTextureLoadInfo& loadInfo = renderTextureAllocator.Get<0>(id.id24);
	uint32_t numMips = loadInfo.mips;
	uint32_t i;
	for (i = 1; i < numMips; i++)
	{
		RenderTextureGenerateMipHelper(id, 0, id, i);
	}
}

//------------------------------------------------------------------------------
/**
*/
void
RenderTextureGenerateMipChain(const CoreGraphics::RenderTextureId id, IndexT from)
{
	const VkRenderTextureLoadInfo& loadInfo = renderTextureAllocator.Get<0>(id.id24);
	n_assert(loadInfo.mips > (uint32_t)from);
	uint32_t numMips = loadInfo.mips - from;
	uint32_t i;
	for (i = from + 1; i < numMips; i++)
	{
		RenderTextureGenerateMipHelper(id, from, id, i);
	}
}

//------------------------------------------------------------------------------
/**
*/
void
RenderTextureGenerateMipChain(const CoreGraphics::RenderTextureId id, IndexT from, IndexT to)
{
	const VkRenderTextureLoadInfo& loadInfo = renderTextureAllocator.Get<0>(id.id24);
	n_assert(loadInfo.mips > (uint32_t)from && loadInfo.mips > (uint32_t)to);
	IndexT i;
	for (i = from+1; i < to; i++)
	{
		RenderTextureGenerateMipHelper(id, from, id, i);
	}	
}

//------------------------------------------------------------------------------
/**
*/
void
RenderTextureBlit(const CoreGraphics::RenderTextureId id, IndexT fromMip, IndexT toMip, const CoreGraphics::RenderTextureId target)
{
	const VkRenderTextureLoadInfo& loadInfo = renderTextureAllocator.Get<0>(id.id24);
	n_assert(loadInfo.mips > (uint32_t)fromMip && loadInfo.mips > (uint32_t)toMip);
	RenderTextureGenerateMipHelper(id, fromMip, target == CoreGraphics::RenderTextureId::Invalid() ? id : target, toMip);
}

//------------------------------------------------------------------------------
/**
*/
const VkImageView
RenderTextureGetVkAttachmentImageView(const CoreGraphics::RenderTextureId id)
{
	return renderTextureAllocator.Get<1>(id.id24).framebufferView;
}

//------------------------------------------------------------------------------
/**
*/
const VkImageView
RenderTextureGetVkSampleImageView(const CoreGraphics::RenderTextureId id)
{
	return renderTextureAllocator.Get<1>(id.id24).sampleView;
}

//------------------------------------------------------------------------------
/**
*/
const VkImage
RenderTextureGetVkImage(const CoreGraphics::RenderTextureId id)
{
	return renderTextureAllocator.Get<0>(id.id24).img;
}

//------------------------------------------------------------------------------
/**
	Internal helper function to generate mips, will assert that the texture is not within a pass, and witin Begin/End frame
*/
void
RenderTextureGenerateMipHelper(const CoreGraphics::RenderTextureId id, IndexT from, const CoreGraphics::RenderTextureId target, IndexT to)
{
	const VkRenderTextureLoadInfo& loadInfoFrom = renderTextureAllocator.Get<0>(id.id24);
	const VkRenderTextureLoadInfo& loadInfoTo = renderTextureAllocator.Get<0>(target.id24);
	const VkRenderTextureRuntimeInfo& rtInfo = renderTextureAllocator.Get<1>(id.id24);

	n_assert(loadInfoFrom.format == loadInfoTo.format);

	// setup from-region
	int32_t fromMipWidth = (int32_t)Math::n_max(1.0f, Math::n_floor(loadInfoFrom.dims.width / Math::n_pow(2, (float)from)));
	int32_t fromMipHeight = (int32_t)Math::n_max(1.0f, Math::n_floor(loadInfoFrom.dims.height / Math::n_pow(2, (float)from)));
	Math::rectangle<int> fromRegion;
	fromRegion.left = 0;
	fromRegion.top = 0;
	fromRegion.right = fromMipWidth;
	fromRegion.bottom = fromMipHeight;

	VkImageSubresourceRange fromSubres;
	fromSubres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	fromSubres.baseArrayLayer = 0;
	fromSubres.baseMipLevel = from;
	fromSubres.layerCount = 1;
	fromSubres.levelCount = 1;

	VkImageSubresourceRange toSubres;
	toSubres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	toSubres.baseArrayLayer = 0;
	toSubres.baseMipLevel = to;
	toSubres.layerCount = 1;
	toSubres.levelCount = 1;

	int32_t toMipWidth = (int32_t)Math::n_max(1.0f, Math::n_floor(loadInfoTo.dims.width / Math::n_pow(2, (float)to)));
	int32_t toMipHeight = (int32_t)Math::n_max(1.0f, Math::n_floor(loadInfoTo.dims.height / Math::n_pow(2, (float)to)));
	Math::rectangle<int> toRegion;
	toRegion.left = 0;
	toRegion.top = 0;
	toRegion.right = toMipWidth;
	toRegion.bottom = toMipHeight;

	// transition to transfer state
	VkUtilities::ImageBarrier(CoreGraphics::GetGfxCommandBuffer(),
		CoreGraphics::BarrierStage::AllGraphicsShaders,
		CoreGraphics::BarrierStage::Transfer,
		VkUtilities::ImageMemoryBarrier(loadInfoTo.img, toSubres, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));

	// transition the texture to destination, blit, and transition it back
	VkUtilities::Blit(CoreGraphics::GetGfxCommandBuffer(), loadInfoFrom.img, fromRegion, from, loadInfoTo.img, toRegion, to);

	// transition back to readable (outside of pass)
	VkUtilities::ImageBarrier(CoreGraphics::GetGfxCommandBuffer(),
		CoreGraphics::BarrierStage::Transfer,
		CoreGraphics::BarrierStage::AllGraphicsShaders,
		VkUtilities::ImageMemoryBarrier(loadInfoTo.img, toSubres, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
}

} // namespace Vulkan