//------------------------------------------------------------------------------
//  vkgraphicsdevice.cc
//  (C) 2018 Individual contributors, see AUTHORS file
//------------------------------------------------------------------------------
#include "render/stdneb.h"
#include "coregraphics/config.h"
#include "vkgraphicsdevice.h"
#include "coregraphics/cmdbuffer.h"
#include "vkshaderprogram.h"
#include "vkscheduler.h"
#include "vkpipelinedatabase.h"
#include "vkcmdbuffer.h"
#include "vktransformdevice.h"
#include "vkresourcetable.h"
#include "vkshaderserver.h"
#include "vkpass.h"
#include "vkrendertexture.h"
#include "vkshaderrwtexture.h"
#include "vkshaderrwbuffer.h"
#include "vkbarrier.h"
#include "coregraphics/displaydevice.h"
#include "app/application.h"
#include "io/ioserver.h"
#include "vkevent.h"
#include "vkfence.h"
#include "vktypes.h"
#include "coregraphics/memoryindexbufferpool.h"
#include "coregraphics/memoryvertexbufferpool.h"
#include "coregraphics/vertexsignaturepool.h"
#include "coregraphics/glfw/glfwwindow.h"
#include "coregraphics/displaydevice.h"
namespace Vulkan
{

enum VkPipelineInfoBits
{
	NoInfoSet = 0,
	ShaderInfoSet = 1,
	VertexLayoutInfoSet = 2,
	FramebufferLayoutInfoSet = 4,
	InputLayoutInfoSet = 8,

	AllInfoSet = 15,

	PipelineBuilt = 16
};
__ImplementEnumBitOperators(VkPipelineInfoBits);

struct GraphicsDeviceState : CoreGraphics::GraphicsDeviceState
{
	uint32_t adapter;
	uint32_t frameId;
	VkPhysicalDeviceMemoryProperties memoryProps;

	VkInstance instance;
	VkDescriptorPool descPool;
	VkPipelineCache cache;
	VkAllocationCallbacks alloc;

	VkFence mainCmdDrawFence;
	VkFence mainCmdCmpFence;
	VkFence mainCmdTransFence;

	CoreGraphics::ShaderPipeline currentBindPoint;

	static const SizeT NumDrawThreads = 8;
	IndexT currentDrawThread;
	VkCommandPool dispatchableCmdDrawBufferPool[NumDrawThreads];
	VkCommandBuffer dispatchableDrawCmdBuffers[NumDrawThreads];
	Ptr<VkCmdBufferThread> drawThreads[NumDrawThreads];
	Threading::Event* drawCompletionEvents[NumDrawThreads];

	static const SizeT NumTransferThreads = 1;
	IndexT currentTransThread;
	VkCommandPool dispatchableCmdTransBufferPool[NumTransferThreads];
	VkCommandBuffer dispatchableTransCmdBuffers[NumTransferThreads];
	Ptr<VkCmdBufferThread> transThreads[NumTransferThreads];
	Threading::Event* transCompletionEvents[NumTransferThreads];

	static const SizeT NumComputeThreads = 1;
	IndexT currentComputeThread;
	VkCommandPool dispatchableCmdCompBufferPool[NumComputeThreads];
	VkCommandBuffer dispatchableCompCmdBuffers[NumComputeThreads];
	Ptr<VkCmdBufferThread> compThreads[NumComputeThreads];
	Threading::Event* compCompletionEvents[NumComputeThreads];

	Util::Array<VkCmdBufferThread::Command> propagateDescriptorSets;
	Util::Array<VkCmdBufferThread::Command> threadCmds[NumDrawThreads];
	SizeT numCallsLastFrame;
	SizeT numActiveThreads;
	SizeT numUsedThreads;

	VkCommandBufferInheritanceInfo passInfo;
	VkPipelineInputAssemblyStateCreateInfo inputInfo;
	VkPipelineColorBlendStateCreateInfo blendInfo;
	VkViewport* passViewports;
	uint32_t numVsInputs;

	CoreGraphics::CmdBufferId mainCmdDrawBuffer;
	CoreGraphics::CmdBufferId mainCmdComputeBuffer;
	CoreGraphics::CmdBufferId mainCmdTransferBuffer;
	CoreGraphics::CmdBufferId mainCmdSparseBuffer;

	VkExtensionProperties physicalExtensions[64];

	uint32_t usedPhysicalExtensions;
	const char* deviceExtensionStrings[64];

	uint32_t usedExtensions;
	const char* extensions[64];

	uint32_t numQueues;
	VkQueueFamilyProperties queuesProps[64];

	uint32_t drawQueueFamily;
	uint32_t computeQueueFamily;
	uint32_t transferQueueFamily;
	uint32_t sparseQueueFamily;
	uint32_t drawQueueIdx;
	uint32_t computeQueueIdx;
	uint32_t transferQueueIdx;
	uint32_t sparseQueueIdx;
	Util::Set<uint32_t> usedQueueFamilies;
	Util::FixedArray<uint32_t> queueFamilyMap;

	// setup management classes
	VkScheduler scheduler;
	VkSubContextHandler subcontextHandler;
	VkPipelineDatabase database;

	// device handling (multi GPU?!?!)
	Util::FixedArray<VkDevice> devices;
	Util::FixedArray<VkPhysicalDevice> physicalDevices;
	Util::FixedArray<VkPhysicalDeviceProperties> deviceProps;
	Util::FixedArray<VkPhysicalDeviceFeatures> deviceFeatures;
	Util::FixedArray<uint32_t> numCaps;
	Util::FixedArray<Util::FixedArray<VkExtensionProperties>> caps;
	Util::FixedArray<Util::FixedArray<const char*>> deviceFeatureStrings;
	IndexT currentDevice;

	CoreGraphics::ShaderProgramId currentShaderProgram;
	CoreGraphics::ShaderFeature::Mask currentShaderMask;

	VkGraphicsPipelineCreateInfo currentPipelineInfo;
	VkPipelineLayout currentPipelineLayout;
	VkPipeline currentPipeline;
	VkPipelineInfoBits currentPipelineBits;

	static const SizeT MaxClipSettings = 8;
	uint32_t numViewports;
	VkViewport viewports[MaxClipSettings];
	uint32_t numScissors;
	VkRect2D scissors[MaxClipSettings];
	bool viewportsDirty[NumDrawThreads];
	bool scissorsDirty[NumDrawThreads];

	uint32_t currentProgram;

	_declare_counter(NumImageBytesAllocated);
	_declare_counter(NumBufferBytesAllocated);
	_declare_counter(NumBytesAllocated);
	_declare_counter(NumPipelinesBuilt);
	_declare_counter(GraphicsDeviceNumComputes);
	_declare_counter(GraphicsDeviceNumPrimitives);
	_declare_counter(GraphicsDeviceNumDrawCalls);
	_declare_timer(DebugTimer);

} state;

VkDebugUtilsMessengerEXT VkDebugMessageHandle = nullptr;
PFN_vkCreateDebugUtilsMessengerEXT VkCreateDebugMessenger = nullptr;
PFN_vkDestroyDebugUtilsMessengerEXT VkDestroyDebugMessenger = nullptr;

PFN_vkSetDebugUtilsObjectNameEXT VkDebugObjectName = nullptr;
PFN_vkSetDebugUtilsObjectTagEXT VkDebugObjectTag = nullptr;
PFN_vkQueueBeginDebugUtilsLabelEXT VkQueueBeginLabel = nullptr;
PFN_vkQueueEndDebugUtilsLabelEXT VkQueueEndLabel = nullptr;
PFN_vkQueueInsertDebugUtilsLabelEXT VkQueueInsertLabel = nullptr;
PFN_vkCmdBeginDebugUtilsLabelEXT VkCmdDebugMarkerBegin = nullptr;
PFN_vkCmdEndDebugUtilsLabelEXT VkCmdDebugMarkerEnd = nullptr;
PFN_vkCmdInsertDebugUtilsLabelEXT VkCmdDebugMarkerInsert = nullptr;
//------------------------------------------------------------------------------
/**
*/
void
SetupAdapter()
{
	// retrieve available GPUs
	uint32_t gpuCount;
	VkResult res;
	res = vkEnumeratePhysicalDevices(state.instance, &gpuCount, NULL);
	n_assert(res == VK_SUCCESS);

	state.devices.Resize(gpuCount);
	state.physicalDevices.Resize(gpuCount);
	state.numCaps.Resize(gpuCount);
	state.caps.Resize(gpuCount);
	state.deviceFeatureStrings.Resize(gpuCount);
	state.deviceProps.Resize(gpuCount);
	state.deviceFeatures.Resize(gpuCount);

	if (gpuCount > 0)
	{
		res = vkEnumeratePhysicalDevices(state.instance, &gpuCount, state.physicalDevices.Begin());
		n_assert(res == VK_SUCCESS);

		if (gpuCount > 1)
			n_message("Found %d GPUs, which is more than 1! Perhaps the Render Device should be able to use it?\n", gpuCount);

		IndexT i;
		for (i = 0; i < (IndexT)gpuCount; i++)
		{
			res = vkEnumerateDeviceExtensionProperties(state.physicalDevices[i], nullptr, &state.numCaps[i], nullptr);
			n_assert(res == VK_SUCCESS);

			if (state.numCaps[i] > 0)
			{
				state.caps[i].Resize(state.numCaps[i]);
				state.deviceFeatureStrings[i].Resize(state.numCaps[i]);

				res = vkEnumerateDeviceExtensionProperties(state.physicalDevices[i], nullptr, &state.numCaps[i], state.caps[i].Begin());
				n_assert(res == VK_SUCCESS);

				uint32_t newNumCaps = 0;
				for (uint32_t j = 0; j < state.numCaps[i]; j++)
				{
					Util::String str(state.caps[i][j].extensionName);

					// only load khronos extensions
					if (str.BeginsWithString("VK_KHR_"))
						state.deviceFeatureStrings[i][newNumCaps++] = state.caps[i][j].extensionName;
				}
				state.numCaps[i] = newNumCaps;
				state.deviceFeatureStrings[i].Resize(newNumCaps);
			}

			// get device props and features
			vkGetPhysicalDeviceProperties(state.physicalDevices[0], &state.deviceProps[i]);
			vkGetPhysicalDeviceFeatures(state.physicalDevices[0], &state.deviceFeatures[i]);
		}
	}
	else
	{
		n_error("VkGraphicsDevice::SetupAdapter(): No GPU available.\n");
	}
}

//------------------------------------------------------------------------------
/**
*/
VkInstance
GetInstance()
{
	return state.instance;
}

//------------------------------------------------------------------------------
/**
*/
VkDevice
GetCurrentDevice()
{
	return state.devices[state.currentDevice];
}

//------------------------------------------------------------------------------
/**
*/
VkPhysicalDevice
GetCurrentPhysicalDevice()
{
	return state.physicalDevices[state.currentDevice];
}

//------------------------------------------------------------------------------
/**
*/
VkPhysicalDeviceProperties
GetCurrentProperties()
{
	return state.deviceProps[state.currentDevice];
}

//------------------------------------------------------------------------------
/**
*/
VkPhysicalDeviceFeatures 
GetCurrentFeatures()
{
	return state.deviceFeatures[state.currentDevice];;
}

//------------------------------------------------------------------------------
/**
*/
VkPipelineCache 
GetPipelineCache()
{
	return state.cache;
}

//------------------------------------------------------------------------------
/**
*/
VkPhysicalDeviceMemoryProperties
GetMemoryProperties()
{
	return state.memoryProps;
}

//------------------------------------------------------------------------------
/**
*/
VkCommandBuffer 
GetMainBuffer(const CoreGraphicsQueueType queue)
{
	switch (queue)
	{
	case GraphicsQueueType: return CommandBufferGetVk(state.mainCmdDrawBuffer);
	case TransferQueueType: return CommandBufferGetVk(state.mainCmdTransferBuffer);
	case ComputeQueueType: return CommandBufferGetVk(state.mainCmdComputeBuffer);
	case SparseQueueType: return CommandBufferGetVk(state.mainCmdSparseBuffer);
	}
	return VK_NULL_HANDLE;
}

//------------------------------------------------------------------------------
/**
*/
const Util::Set<uint32_t>& 
GetQueueFamilies()
{
	return state.usedQueueFamilies;
}

//------------------------------------------------------------------------------
/**
*/
const uint32_t 
GetQueueFamily(const CoreGraphicsQueueType type)
{
	return state.queueFamilyMap[type];
}

//------------------------------------------------------------------------------
/**
*/
const VkQueue 
GetQueue(const CoreGraphicsQueueType type, const IndexT index)
{
	switch (type)
	{
	case GraphicsQueueType:
		return state.subcontextHandler.drawQueues[index];
		break;
	case ComputeQueueType:
		return state.subcontextHandler.computeQueues[index];
		break;
	case TransferQueueType:
		return state.subcontextHandler.transferQueues[index];
		break;
	case SparseQueueType:
		return state.subcontextHandler.sparseQueues[index];
		break;
	}
	return VK_NULL_HANDLE;
}

//------------------------------------------------------------------------------
/**
*/
const VkQueue 
GetCurrentQueue(const CoreGraphicsQueueType type)
{
	return state.subcontextHandler.GetQueue(type);
}

//------------------------------------------------------------------------------
/**
*/
void 
Copy(const VkImage from, Math::rectangle<SizeT> fromRegion, const VkImage to, Math::rectangle<SizeT> toRegion)
{
	n_assert(!state.inBeginPass);
	VkImageCopy region;
	region.dstOffset = { fromRegion.left, fromRegion.top, 0 };
	region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	region.extent = { (uint32_t)toRegion.width(), (uint32_t)toRegion.height(), 1 };
	region.srcOffset = { toRegion.left, toRegion.top, 0 };
	region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	vkCmdCopyImage(CommandBufferGetVk(state.mainCmdDrawBuffer), from, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, to, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

//------------------------------------------------------------------------------
/**
*/
void 
Blit(const VkImage from, Math::rectangle<SizeT> fromRegion, IndexT fromMip, const VkImage to, Math::rectangle<SizeT> toRegion, IndexT toMip)
{
	n_assert(!state.inBeginPass);
	VkImageBlit blit;
	blit.srcOffsets[0] = { fromRegion.left, fromRegion.top, 0 };
	blit.srcOffsets[1] = { fromRegion.right, fromRegion.bottom, 1 };
	blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, (uint32_t)fromMip, 0, 1 };
	blit.dstOffsets[0] = { toRegion.left, toRegion.top, 0 };
	blit.dstOffsets[1] = { toRegion.right, toRegion.bottom, 1 };
	blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, (uint32_t)toMip, 0, 1 };
	vkCmdBlitImage(CommandBufferGetVk(state.mainCmdDrawBuffer), from, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, to, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
}

//------------------------------------------------------------------------------
/**
*/
void 
BindDescriptorsGraphics(const VkDescriptorSet* descriptors, uint32_t baseSet, uint32_t setCount, const uint32_t* offsets, uint32_t offsetCount, bool propagate)
{
	// if we are setting descriptors before we have a pipeline, add them for later submission
	if (state.currentProgram == -1)
	{
		VkCmdBufferThread::Command cmd;
		cmd.type = VkCmdBufferThread::BindDescriptors;
		cmd.descriptor.baseSet = baseSet;
		cmd.descriptor.numSets = setCount;
		cmd.descriptor.sets = descriptors;
		cmd.descriptor.numOffsets = offsetCount;
		cmd.descriptor.offsets = offsets;
		cmd.descriptor.type = VK_PIPELINE_BIND_POINT_GRAPHICS;
		state.propagateDescriptorSets.Append(cmd);
	}
	else
	{
		// if batching, draws goes to thread
		n_assert(state.currentProgram != -1);
		if (state.inBeginBatch)
		{
			VkCmdBufferThread::Command cmd;
			cmd.type = VkCmdBufferThread::BindDescriptors;
			cmd.descriptor.baseSet = baseSet;
			cmd.descriptor.numSets = setCount;
			cmd.descriptor.sets = descriptors;
			cmd.descriptor.numOffsets = offsetCount;
			cmd.descriptor.offsets = offsets;
			cmd.descriptor.type = VK_PIPELINE_BIND_POINT_GRAPHICS;
			PushToThread(cmd, state.currentDrawThread);
		}
		else
		{
			// otherwise they go on the main draw
			vkCmdBindDescriptorSets(CommandBufferGetVk(state.mainCmdDrawBuffer), VK_PIPELINE_BIND_POINT_GRAPHICS, state.currentPipelineLayout, baseSet, setCount, descriptors, offsetCount, offsets);
		}
	}
}

//------------------------------------------------------------------------------
/**
*/
void 
BindDescriptorsCompute(const VkDescriptorSet* descriptors, uint32_t baseSet, uint32_t setCount, const uint32_t* offsets, uint32_t offsetCount)
{
	if (state.inBeginFrame)
	{
		vkCmdBindDescriptorSets(CommandBufferGetVk(state.mainCmdDrawBuffer), VK_PIPELINE_BIND_POINT_COMPUTE, state.currentPipelineLayout, baseSet, setCount, descriptors, offsetCount, offsets);
	}
	else
	{
		VkDeferredCommand cmd;
		cmd.del.type = VkDeferredCommand::BindDescriptorSets;
		cmd.del.descSetBind.baseSet = baseSet;
		cmd.del.descSetBind.numOffsets = offsetCount;
		cmd.del.descSetBind.offsets = offsets;
		cmd.del.descSetBind.numSets = setCount;
		cmd.del.descSetBind.sets = descriptors;
		cmd.del.descSetBind.type = VK_PIPELINE_BIND_POINT_COMPUTE;
		cmd.dev = state.devices[state.currentDevice];
		state.scheduler.PushCommand(cmd, VkScheduler::OnBindComputePipeline);
	}
}

//------------------------------------------------------------------------------
/**
*/
void 
UpdatePushRanges(const VkShaderStageFlags& stages, const VkPipelineLayout& layout, uint32_t offset, uint32_t size, void* data)
{
	if (state.inBeginBatch)
	{
		VkCmdBufferThread::Command cmd;
		cmd.type = VkCmdBufferThread::PushRange;
		cmd.pushranges.layout = layout;
		cmd.pushranges.offset = offset;
		cmd.pushranges.size = size;
		cmd.pushranges.stages = stages;

		// copy data here, will be deleted in the thread
		cmd.pushranges.data = Memory::Alloc(Memory::ScratchHeap, size);
		memcpy(cmd.pushranges.data, data, size);
		PushToThread(cmd, state.currentDrawThread);
	}
	else
	{
		vkCmdPushConstants(CommandBufferGetVk(state.mainCmdDrawBuffer), layout, stages, offset, size, data);
	}
	
}

//------------------------------------------------------------------------------
/**
*/
void 
BindGraphicsPipelineInfo(const VkGraphicsPipelineCreateInfo& shader, const uint32_t programId)
{
	if (state.currentProgram != programId || !(state.currentPipelineBits & ShaderInfoSet))
	{
		state.database.SetShader(programId, shader);
		state.currentPipelineBits |= ShaderInfoSet;

		state.blendInfo.pAttachments = shader.pColorBlendState->pAttachments;
		memcpy(state.blendInfo.blendConstants, shader.pColorBlendState->blendConstants, sizeof(float) * 4);
		state.blendInfo.logicOp = shader.pColorBlendState->logicOp;
		state.blendInfo.logicOpEnable = shader.pColorBlendState->logicOpEnable;

		state.currentPipelineInfo.pDepthStencilState = shader.pDepthStencilState;
		state.currentPipelineInfo.pRasterizationState = shader.pRasterizationState;
		state.currentPipelineInfo.pMultisampleState = shader.pMultisampleState;
		state.currentPipelineInfo.pDynamicState = shader.pDynamicState;
		state.currentPipelineInfo.pTessellationState = shader.pTessellationState;
		state.currentPipelineInfo.stageCount = shader.stageCount;
		state.currentPipelineInfo.pStages = shader.pStages;
		state.currentPipelineInfo.layout = shader.layout;
		state.currentPipelineBits &= ~PipelineBuilt;
		state.currentProgram = programId;
	}
}

//------------------------------------------------------------------------------
/**
*/
void 
SetVertexLayoutPipelineInfo(VkPipelineVertexInputStateCreateInfo* vertexLayout)
{
	if (state.currentPipelineInfo.pVertexInputState != vertexLayout || !(state.currentPipelineBits & VertexLayoutInfoSet))
	{
		state.database.SetVertexLayout(vertexLayout);
		state.currentPipelineBits |= VertexLayoutInfoSet;
		state.currentPipelineInfo.pVertexInputState = vertexLayout;

		state.currentPipelineBits &= ~PipelineBuilt;
	}
}

//------------------------------------------------------------------------------
/**
*/
void 
SetFramebufferLayoutInfo(const VkGraphicsPipelineCreateInfo& framebufferLayout)
{
	state.currentPipelineBits |= FramebufferLayoutInfoSet;
	state.currentPipelineInfo.renderPass = framebufferLayout.renderPass;
	state.currentPipelineInfo.subpass = framebufferLayout.subpass;
	state.currentPipelineInfo.pViewportState = framebufferLayout.pViewportState;
	state.currentPipelineBits &= ~PipelineBuilt;
}

//------------------------------------------------------------------------------
/**
*/
void 
SetInputLayoutInfo(VkPipelineInputAssemblyStateCreateInfo* inputLayout)
{
	if (state.currentPipelineInfo.pInputAssemblyState != inputLayout || !(state.currentPipelineBits & InputLayoutInfoSet))
	{
		state.database.SetInputLayout(inputLayout);
		state.currentPipelineBits |= InputLayoutInfoSet;
		state.currentPipelineInfo.pInputAssemblyState = inputLayout;
		state.currentPipelineBits &= ~PipelineBuilt;
	}
}

//------------------------------------------------------------------------------
/**
*/
void 
CreateAndBindGraphicsPipeline()
{
	VkPipeline pipeline = state.database.GetCompiledPipeline();
	_incr_counter(state.NumPipelinesBuilt, 1);

	if (state.inBeginBatch)
	{ 
		VkCmdBufferThread::Command cmd;

		// send pipeline bind command, this is the first step in our procedure, so we use this as a trigger to switch threads
		cmd.type = VkCmdBufferThread::GraphicsPipeline;
		cmd.pipe.pipeline = pipeline;
		cmd.pipe.layout = state.currentPipelineLayout;
		PushToThread(cmd, state.currentDrawThread);
		
		// bind textures and camera descriptors
		VkShaderServer::Instance()->BindTextureDescriptorSetsGraphics();
		VkTransformDevice::Instance()->BindCameraDescriptorSetsGraphics();

		// push propagation descriptors
		for (IndexT i = 0; i < state.propagateDescriptorSets.Size(); i++)
			PushToThread(state.propagateDescriptorSets[i], state.currentDrawThread);

		uint32_t i;
		for (i = 0; i < state.numScissors; i++)
		{
			cmd.type = VkCmdBufferThread::ScissorRect;
			cmd.scissorRect.sc = state.scissors[i];
			cmd.scissorRect.index = i;
			PushToThread(cmd, state.currentDrawThread);
		}

		for (i = 0; i < state.numViewports; i++)
		{
			cmd.type = VkCmdBufferThread::Viewport;
			cmd.viewport.vp = state.viewports[i];
			cmd.viewport.index = i;
			PushToThread(cmd, state.currentDrawThread);
		}
		state.viewportsDirty[state.currentDrawThread] = false;
		state.scheduler.ExecuteCommandPass(VkScheduler::OnBindGraphicsPipeline);
	}
	else
	{
		// bind pipeline
		vkCmdBindPipeline(CommandBufferGetVk(state.mainCmdDrawBuffer), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		state.scheduler.ExecuteCommandPass(VkScheduler::OnBindGraphicsPipeline);
	}
	
}

//------------------------------------------------------------------------------
/**
*/
void 
BindComputePipeline(const VkPipeline& pipeline, const VkPipelineLayout& layout)
{
	// bind compute pipeline
	state.currentBindPoint = CoreGraphics::ComputePipeline;

	// bind pipeline
	vkCmdBindPipeline(CommandBufferGetVk(state.mainCmdDrawBuffer), VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

	// bind shared descriptors
	VkShaderServer::Instance()->BindTextureDescriptorSetsCompute();
	VkTransformDevice::Instance()->BindCameraDescriptorSetsCompute();

	// run command pass
	state.scheduler.ExecuteCommandPass(VkScheduler::OnBindComputePipeline);
}

//------------------------------------------------------------------------------
/**
*/
void 
UnbindPipeline()
{
	state.currentBindPoint = CoreGraphics::InvalidPipeline;
	state.currentPipelineBits &= ~ShaderInfoSet;
}

//------------------------------------------------------------------------------
/**
*/
void
SetViewports(VkViewport* viewports, SizeT num)
{
	n_assert(num < state.MaxClipSettings);
	memcpy(state.viewports, viewports, sizeof(VkViewport) * num);
	state.numViewports = num;
	if (state.currentProgram != -1)
	{
		if (state.inBeginBatch)
		{
			VkCmdBufferThread::Command cmd;
			cmd.type = VkCmdBufferThread::ViewportArray;
			cmd.viewportArray.first = 0;
			cmd.viewportArray.num = num;
			cmd.viewportArray.vps = viewports;
			PushToThread(cmd, state.currentDrawThread);
		}
		else
		{
			// activate this code when we have main thread secondary buffers
			//vkCmdSetViewport(CommandBufferGetVk(state.mainCmdDrawBuffer), 0, num, viewports);
		}
	}
}

//------------------------------------------------------------------------------
/**
*/
void
SetScissorRects(VkRect2D* scissors, SizeT num)
{
	n_assert(num < state.MaxClipSettings);
	memcpy(state.scissors, scissors, sizeof(VkRect2D) * num);
	state.numScissors = num;
	if (state.currentProgram != -1)
	{
		if (state.inBeginBatch)
		{
			VkCmdBufferThread::Command cmd;
			cmd.type = VkCmdBufferThread::ScissorRectArray;
			cmd.scissorRectArray.first = 0;
			cmd.scissorRectArray.num = num;
			cmd.scissorRectArray.scs = scissors;
			PushToThread(cmd, state.currentDrawThread);
		}
		else
		{
			// activate this code when we have main thread secondary buffers
			//vkCmdSetScissor(CommandBufferGetVk(state.mainCmdDrawBuffer), 0, num, scissors);
		}
	}
}

//------------------------------------------------------------------------------
/**
*/
void 
SubmitToQueue(VkQueue queue, VkPipelineStageFlags flags, uint32_t numBuffers, VkCommandBuffer* buffers)
{
	uint32_t i;
	for (i = 0; i < numBuffers; i++)
	{
		VkResult res = vkEndCommandBuffer(buffers[i]);
		n_assert(res == VK_SUCCESS);
	}

	// submit to queue
	const VkSubmitInfo submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,
		NULL,
		0,
		NULL,
		&flags,
		numBuffers,
		buffers,
		0,
		NULL
	};

	// submit to queue
	VkResult res = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
	n_assert(res == VK_SUCCESS);
}

//------------------------------------------------------------------------------
/**
*/
void 
SubmitToQueue(VkQueue queue, VkFence fence)
{
	// submit to queue
	VkResult res = vkQueueSubmit(queue, 0, VK_NULL_HANDLE, fence);
	n_assert(res == VK_SUCCESS);
}

//------------------------------------------------------------------------------
/**
*/
void 
WaitForFences(VkFence* fences, uint32_t numFences, bool waitForAll)
{
	VkResult res = vkWaitForFences(state.devices[state.currentDevice], numFences, fences, waitForAll, UINT_MAX);
	n_assert(res == VK_SUCCESS);
	res = vkResetFences(state.devices[state.currentDevice], numFences, fences);
	n_assert(res == VK_SUCCESS);
}

//------------------------------------------------------------------------------
/**
*/
void 
BeginDrawThread()
{
	n_assert(state.numActiveThreads < state.NumDrawThreads);

	// allocate command buffer
	VkCommandBufferAllocateInfo info =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		NULL,
		state.dispatchableCmdDrawBufferPool[state.currentDrawThread],
		VK_COMMAND_BUFFER_LEVEL_SECONDARY,
		1
	};
	vkAllocateCommandBuffers(state.devices[state.currentDevice], &info, &state.dispatchableDrawCmdBuffers[state.currentDrawThread]);

	// tell thread to begin command buffer recording
	VkCommandBufferBeginInfo begin =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		NULL,
		VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
		&state.passInfo
	};

	VkCmdBufferThread::Command cmd;
	cmd.type = VkCmdBufferThread::BeginCommand;
	cmd.bgCmd.buf = state.dispatchableDrawCmdBuffers[state.currentDrawThread];
	cmd.bgCmd.info = begin;
	PushToThread(cmd, state.currentDrawThread);

	// run begin command buffer pass
	state.scheduler.ExecuteCommandPass(VkScheduler::OnBeginDrawThread);
	state.numActiveThreads++;
}

//------------------------------------------------------------------------------
/**
*/
void 
EndDrawThreads()
{
	if (state.numActiveThreads > 0)
	{
		IndexT i;
		for (i = 0; i < state.numActiveThreads; i++)
		{
			// push remaining cmds to thread
			FlushToThread(i);

			// end thread
			VkCmdBufferThread::Command cmd;
			cmd.type = VkCmdBufferThread::EndCommand;
			PushToThread(cmd, i, false);

			cmd.type = VkCmdBufferThread::Sync;
			cmd.syncEvent = state.drawCompletionEvents[i];
			PushToThread(cmd, i, false);
			state.drawCompletionEvents[i]->Wait();
			state.drawCompletionEvents[i]->Reset();
		}

		// run end-of-threads pass
		state.scheduler.ExecuteCommandPass(VkScheduler::OnDrawThreadsSubmitted);

		// execute commands
		vkCmdExecuteCommands(CommandBufferGetVk(state.mainCmdDrawBuffer), state.numActiveThreads, state.dispatchableDrawCmdBuffers);

		// destroy command buffers
		for (i = 0; i < state.numActiveThreads; i++)
		{
			VkDeferredCommand cmd;
			cmd.del.type = VkDeferredCommand::FreeCmdBuffers;
			cmd.del.cmdbufferfree.buffers[0] = state.dispatchableDrawCmdBuffers[i];
			cmd.del.cmdbufferfree.numBuffers = 1;
			cmd.del.cmdbufferfree.pool = state.dispatchableCmdDrawBufferPool[i];
			cmd.dev = state.devices[state.currentDevice];
			state.scheduler.PushCommand(cmd, VkScheduler::OnHandleDrawFences);
		}
		state.currentDrawThread = state.NumDrawThreads - 1;
		state.numActiveThreads = 0;
	}
}

//------------------------------------------------------------------------------
/**
*/
void 
PushToThread(const VkCmdBufferThread::Command& cmd, const IndexT& index, bool allowStaging)
{
	//this->threadCmds[index].Append(cmd);
	if (allowStaging)
	{
		state.threadCmds[index].Append(cmd);
		if (state.threadCmds[index].Size() == 1500)
		{
			state.drawThreads[index]->PushCommands(state.threadCmds[index]);
			state.threadCmds[index].Reset();
		}
	}
	else
	{
		state.drawThreads[index]->PushCommand(cmd);
	}
}

//------------------------------------------------------------------------------
/**
*/
void 
FlushToThread(const IndexT& index)
{
	if (!state.threadCmds[index].IsEmpty())
	{
		state.drawThreads[index]->PushCommands(state.threadCmds[index]);
		state.threadCmds[index].Clear();
	}
}

//------------------------------------------------------------------------------
/**
*/
void 
BindSharedDescriptorSets()
{

}

#if NEBULA_GRAPHICS_DEBUG
//------------------------------------------------------------------------------
/**
*/
void 
CmdBufBeginMarker(VkCommandBuffer buf, const Math::float4& color, const char* name)
{
	alignas(16) float col[4];
	color.store(col);
	VkDebugUtilsLabelEXT info =
	{
		VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
		nullptr,
		name,
		{ col[0], col[1], col[2], col[3] }
	};
	VkCmdDebugMarkerBegin(buf, &info);
}

//------------------------------------------------------------------------------
/**
*/
void 
CmdBufEndMarker(VkCommandBuffer buf)
{
	VkCmdDebugMarkerEnd(buf);
}
#endif
} // namespace Vulkan

//------------------------------------------------------------------------------
/**
*/
VKAPI_ATTR VkBool32 VKAPI_CALL
NebulaVulkanDebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT type,
	const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
	void* userData)
{
	const int32_t ignore[] =
	{
		61
	};

	for (IndexT i = 0; i < sizeof(ignore) / sizeof(int32_t); i++)
	{
		if (callbackData->messageIdNumber == ignore[i]) return VK_FALSE;
	}

	if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
	{
		n_warning("VULKAN ERROR: %s\n", callbackData->pMessage);
	}
	else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
	{
		n_warning("VULKAN WARNING: %s\n", callbackData->pMessage);
	}
	return VK_FALSE;
}


namespace CoreGraphics
{
using namespace Vulkan;

#if NEBULA_GRAPHICS_DEBUG
template<> void ObjectSetName(const CoreGraphics::CmdBufferId id, const Util::String& name);
#endif

//------------------------------------------------------------------------------
/**
*/
bool 
CreateGraphicsDevice(const GraphicsDeviceCreateInfo& info)
{
	DisplayDevice* displayDevice = DisplayDevice::Instance();
	n_assert(displayDevice->IsOpen());

	state.enableValidation = info.enableValidation;

	// create result
	VkResult res;

	// setup application
	VkApplicationInfo appInfo =
	{
		VK_STRUCTURE_TYPE_APPLICATION_INFO,
		NULL,
		App::Application::Instance()->GetAppTitle().AsCharPtr(),
		2,																// application version
		"Nebula Trifid",												// engine name
		2,																// engine version
		VK_MAKE_VERSION(1, 0, VK_HEADER_VERSION)						// API version
	};

	state.usedExtensions = 0;
	uint32_t requiredExtensionsNum;
	const char** requiredExtensions = glfwGetRequiredInstanceExtensions(&requiredExtensionsNum);
	uint32_t i;
	for (i = 0; i < (uint32_t)requiredExtensionsNum; i++)
	{
		state.extensions[state.usedExtensions++] = requiredExtensions[i];
	}

	const char* layers[] = { "VK_LAYER_LUNARG_standard_validation", "VK_LAYER_LUNARG_object_tracker" };
	int numLayers = 0;
	const char* usedLayers = nullptr;

#if NEBULA_GRAPHICS_DEBUG
	if (info.enableValidation)
	{
		usedLayers = layers[0];
		numLayers = 1;
	}
	else
	{
		usedLayers = layers[1];
		numLayers = 1;
	}
	state.extensions[state.usedExtensions++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
#else
	if (info.enableValidation)
	{
		usedLayers = layers[0];
		numLayers = 1;
		state.extensions[state.usedExtensions++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
	}
#endif

	// setup instance
	VkInstanceCreateInfo instanceInfo =
	{
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,		// type of struct
		NULL,										// pointer to next
		0,											// flags
		&appInfo,									// application
		numLayers,
		layers,
		state.usedExtensions,
		state.extensions
	};

	// create instance
	res = vkCreateInstance(&instanceInfo, NULL, &state.instance);
	if (res == VK_ERROR_INCOMPATIBLE_DRIVER)
	{
		n_error("VkGraphicsDevice::OpenVulkanContext(): Your GPU driver is not compatible with Vulkan.\n");
	}
	else if (res == VK_ERROR_EXTENSION_NOT_PRESENT)
	{
		n_error("VkGraphicsDevice::OpenVulkanContext(): Vulkan extension failed to load.\n");
	}
	else if (res == VK_ERROR_LAYER_NOT_PRESENT)
	{
		n_error("VkGraphicsDevice::OpenVulkanContext(): Vulkan layer failed to load.\n");
	}
	n_assert(res == VK_SUCCESS);

	// setup adapter
	SetupAdapter();
	state.currentDevice = 0;

#if NEBULA_GRAPHICS_DEBUG
#else
	if (info.enableValidation)
#endif
	{
		VkCreateDebugMessenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(state.instance, "vkCreateDebugUtilsMessengerEXT");
		VkDestroyDebugMessenger = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(state.instance, "vkDestroyDebugUtilsMessengerEXT");
		VkDebugUtilsMessengerCreateInfoEXT dbgInfo;
		dbgInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		dbgInfo.flags = 0;
		dbgInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
		dbgInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		dbgInfo.pNext = nullptr;
		dbgInfo.pfnUserCallback = NebulaVulkanDebugCallback;
		dbgInfo.pUserData = nullptr;
		res = VkCreateDebugMessenger(state.instance, &dbgInfo, NULL, &VkDebugMessageHandle);
		n_assert(res == VK_SUCCESS);
	}

#if NEBULA_GRAPHICS_DEBUG
	VkDebugObjectName = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(state.instance, "vkSetDebugUtilsObjectNameEXT");
	VkDebugObjectTag = (PFN_vkSetDebugUtilsObjectTagEXT)vkGetInstanceProcAddr(state.instance, "vkSetDebugUtilsObjectTagEXT");
	VkQueueBeginLabel = (PFN_vkQueueBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(state.instance, "vkQueueBeginDebugUtilsLabelEXT");
	VkQueueEndLabel = (PFN_vkQueueEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(state.instance, "vkQueueEndDebugUtilsLabelEXT");
	VkQueueInsertLabel = (PFN_vkQueueInsertDebugUtilsLabelEXT)vkGetInstanceProcAddr(state.instance, "vkQueueInsertDebugUtilsLabelEXT");
	VkCmdDebugMarkerBegin = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(state.instance, "vkCmdBeginDebugUtilsLabelEXT");
	VkCmdDebugMarkerEnd = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(state.instance, "vkCmdEndDebugUtilsLabelEXT");
	VkCmdDebugMarkerInsert = (PFN_vkCmdInsertDebugUtilsLabelEXT)vkGetInstanceProcAddr(state.instance, "vkCmdInsertDebugUtilsLabelEXT");
#endif

	vkGetPhysicalDeviceQueueFamilyProperties(state.physicalDevices[state.currentDevice], &state.numQueues, NULL);
	n_assert(state.numQueues > 0);

	// now get queues from device
	vkGetPhysicalDeviceQueueFamilyProperties(state.physicalDevices[state.currentDevice], &state.numQueues, state.queuesProps);
	vkGetPhysicalDeviceMemoryProperties(state.physicalDevices[state.currentDevice], &state.memoryProps);

	state.drawQueueIdx = UINT32_MAX;
	state.computeQueueIdx = UINT32_MAX;
	state.transferQueueIdx = UINT32_MAX;
	state.sparseQueueIdx = UINT32_MAX;

	// create three queues for each family
	Util::FixedArray<uint> indexMap;
	indexMap.Resize(state.numQueues);
	indexMap.Fill(0);
	for (i = 0; i < state.numQueues; i++)
	{

		if (state.queuesProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && state.drawQueueIdx == UINT32_MAX)
		{
			if (state.queuesProps[i].queueCount == indexMap[i]) continue;
			state.drawQueueFamily = i;
			state.drawQueueIdx = indexMap[i]++;
		}

		// compute queues may not support graphics
		if (state.queuesProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT && state.computeQueueIdx == UINT32_MAX)
		{
			if (state.queuesProps[i].queueCount == indexMap[i]) continue;
			state.computeQueueFamily = i;
			state.computeQueueIdx = indexMap[i]++;
		}

		// transfer queues may not support compute or graphics
		if (state.queuesProps[i].queueFlags & VK_QUEUE_TRANSFER_BIT &&
			!(state.queuesProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
			!(state.queuesProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
			state.transferQueueIdx == UINT32_MAX)
		{
			if (state.queuesProps[i].queueCount == indexMap[i]) continue;
			state.transferQueueFamily = i;
			state.transferQueueIdx = indexMap[i]++;
		}

		// sparse queues may not support compute or graphics
		if (state.queuesProps[i].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT &&
			!(state.queuesProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
			!(state.queuesProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
			state.sparseQueueIdx == UINT32_MAX)
		{
			if (state.queuesProps[i].queueCount == indexMap[i]) continue;
			state.sparseQueueFamily = i;
			state.sparseQueueIdx = indexMap[i]++;
		}
	}

	if (state.transferQueueIdx == UINT32_MAX)
	{
		// assert the draw queue can do both transfers and computes
		n_assert(state.queuesProps[state.drawQueueFamily].queueFlags & VK_QUEUE_TRANSFER_BIT);
		n_assert(state.queuesProps[state.drawQueueFamily].queueFlags & VK_QUEUE_COMPUTE_BIT);

		// this is actually sub-optimal, but on my AMD card, using the compute queue transfer or the sparse queue doesn't work
		state.transferQueueFamily = state.drawQueueFamily;
		state.transferQueueIdx = state.drawQueueIdx;
		//state.transferQueueFamily = 2;
		//state.transferQueueIdx = indexMap[2]++;
	}

	if (state.drawQueueFamily == UINT32_MAX)		n_error("VkDisplayDevice: Could not find a queue for graphics and present.\n");
	if (state.computeQueueFamily == UINT32_MAX)		n_error("VkDisplayDevice: Could not find a queue for compute.\n");
	if (state.transferQueueFamily == UINT32_MAX)	n_error("VkDisplayDevice: Could not find a queue for transfers.\n");
	if (state.sparseQueueFamily == UINT32_MAX)		n_warning("VkDisplayDevice: Could not find a queue for sparse binding.\n");

	// create device
	Util::FixedArray<Util::FixedArray<float>> prios;
	Util::Array<VkDeviceQueueCreateInfo> queueInfos;
	prios.Resize(state.numQueues);

	for (i = 0; i < state.numQueues; i++)
	{
		if (indexMap[i] == 0) continue;
		prios[i].Resize(indexMap[i]);
		prios[i].Fill(1.0f);
		queueInfos.Append(
		{
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			NULL,
			0,
			i,
			indexMap[i],
			&prios[i][0]
		});
	}

	// get physical device features
	VkPhysicalDeviceFeatures features;
	vkGetPhysicalDeviceFeatures(state.physicalDevices[state.currentDevice], &features);

	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(state.physicalDevices[state.currentDevice], &props);

	VkDeviceCreateInfo deviceInfo =
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		NULL,
		0,
		(uint32_t)queueInfos.Size(),
		&queueInfos[0],
		numLayers,
		layers,
		state.numCaps[state.currentDevice],
		state.deviceFeatureStrings[state.currentDevice].Begin(),
		&features
	};

	// create device
	res = vkCreateDevice(state.physicalDevices[state.currentDevice], &deviceInfo, NULL, &state.devices[state.currentDevice]);
	n_assert(res == VK_SUCCESS);

	// setup queue handler
	Util::FixedArray<uint> families(4);
	families[GraphicsQueueType] = state.drawQueueFamily;
	families[ComputeQueueType] = state.computeQueueFamily;
	families[TransferQueueType] = state.transferQueueFamily;
	families[SparseQueueType] = state.sparseQueueFamily;
	state.subcontextHandler.Setup(state.devices[state.currentDevice], indexMap, families);

	state.usedQueueFamilies.Add(state.drawQueueFamily);
	state.usedQueueFamilies.Add(state.computeQueueFamily);
	state.usedQueueFamilies.Add(state.transferQueueFamily);
	state.usedQueueFamilies.Add(state.sparseQueueFamily);
	state.queueFamilyMap.Resize(NumQueueTypes);
	state.queueFamilyMap[GraphicsQueueType] = state.drawQueueFamily;
	state.queueFamilyMap[ComputeQueueType] = state.computeQueueFamily;
	state.queueFamilyMap[TransferQueueType] = state.transferQueueFamily;
	state.queueFamilyMap[SparseQueueType] = state.sparseQueueFamily;

	// hmm, a bit inflexible, if we implement some 'cycle device' feature, or switch device feature, we should make sure we have schedulers for all devices...
	state.scheduler.SetDevice(state.devices[state.currentDevice]);

	VkPipelineCacheCreateInfo cacheInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
		NULL,
		0,
		0,
		NULL
	};

	// create cache
	res = vkCreatePipelineCache(state.devices[state.currentDevice], &cacheInfo, NULL, &state.cache);
	n_assert(res == VK_SUCCESS);

	// setup our own pipeline database
	state.database.Setup(state.devices[state.currentDevice], state.cache);

	// setup the empty descriptor set
	SetupEmptyDescriptorSetLayout();

	// setup pools (from VkCmdBuffer.h)
	SetupVkPools(state.devices[state.currentDevice], state.drawQueueFamily, state.computeQueueFamily, state.transferQueueFamily, state.sparseQueueFamily);

	CmdBufferCreateInfo cmdCreateInfo =
	{
		false,
		true,
		false,
		InvalidCmdUsage
	};
	cmdCreateInfo.usage = CmdDraw;
	state.mainCmdDrawBuffer = CreateCmdBuffer(cmdCreateInfo);

	cmdCreateInfo.usage = CmdCompute;
	state.mainCmdComputeBuffer = CreateCmdBuffer(cmdCreateInfo);

	cmdCreateInfo.usage = CmdTransfer;
	state.mainCmdTransferBuffer = CreateCmdBuffer(cmdCreateInfo);

	cmdCreateInfo.usage = CmdSparse;
	state.mainCmdSparseBuffer = CreateCmdBuffer(cmdCreateInfo);

#if NEBULA_GRAPHICS_DEBUG
	ObjectSetName(state.mainCmdDrawBuffer, "Main Draw Command Buffer");
	ObjectSetName(state.mainCmdComputeBuffer, "Main Compute Command Buffer");
	ObjectSetName(state.mainCmdTransferBuffer, "Main Transfer Command Buffer");
	ObjectSetName(state.mainCmdSparseBuffer, "Main Sparse Bind Command Buffer");
#endif

	// setup draw threads
	Util::String threadName;
	for (i = 0; i < state.NumDrawThreads; i++)
	{
		threadName.Format("DrawCmdBufferThread%d", i);
		state.drawThreads[i] = VkCmdBufferThread::Create();
		state.drawThreads[i]->SetPriority(Threading::Thread::High);
		state.drawThreads[i]->SetThreadAffinity(System::Cpu::Core5);
		state.drawThreads[i]->SetName(threadName);
		state.drawThreads[i]->Start();

		state.drawCompletionEvents[i] = n_new(Threading::Event(true));

		VkCommandPoolCreateInfo info =
		{
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			nullptr,
			VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
			state.drawQueueFamily
		};
		VkResult res = vkCreateCommandPool(state.devices[state.currentDevice], &info, nullptr, &state.dispatchableCmdDrawBufferPool[i]);
		n_assert(res == VK_SUCCESS);
	}

	// setup compute threads
	for (i = 0; i < state.NumComputeThreads; i++)
	{
		threadName.Format("ComputeCmdBufferThread%d", i);
		state.compThreads[i] = VkCmdBufferThread::Create();
		state.compThreads[i]->SetPriority(Threading::Thread::Low);
		state.compThreads[i]->SetThreadAffinity(System::Cpu::Core5);
		state.compThreads[i]->SetName(threadName);
		state.compThreads[i]->Start();

		state.compCompletionEvents[i] = n_new(Threading::Event(true));

		VkCommandPoolCreateInfo info =
		{
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			nullptr,
			VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
			state.computeQueueFamily
		};
		VkResult res = vkCreateCommandPool(state.devices[state.currentDevice], &info, nullptr, &state.dispatchableCmdCompBufferPool[i]);
		n_assert(res == VK_SUCCESS);
	}

	// setup transfer threads
	for (i = 0; i < state.NumTransferThreads; i++)
	{
		threadName.Format("TransferCmdBufferThread%d", i);
		state.transThreads[i] = VkCmdBufferThread::Create();
		state.transThreads[i]->SetPriority(Threading::Thread::Low);
		state.transThreads[i]->SetThreadAffinity(System::Cpu::Core5);
		state.transThreads[i]->SetName(threadName);
		state.transThreads[i]->Start();

		state.transCompletionEvents[i] = n_new(Threading::Event(true));

		VkCommandPoolCreateInfo info =
		{
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			nullptr,
			VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
			state.transferQueueFamily
		};
		VkResult res = vkCreateCommandPool(state.devices[state.currentDevice], &info, nullptr, &state.dispatchableCmdTransBufferPool[i]);
		n_assert(res == VK_SUCCESS);
	}

	VkFenceCreateInfo fenceInfo =
	{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		nullptr,
		0
	};

	res = vkCreateFence(state.devices[state.currentDevice], &fenceInfo, nullptr, &state.mainCmdDrawFence);
	n_assert(res == VK_SUCCESS);
	res = vkCreateFence(state.devices[state.currentDevice], &fenceInfo, nullptr, &state.mainCmdCmpFence);
	n_assert(res == VK_SUCCESS);
	res = vkCreateFence(state.devices[state.currentDevice], &fenceInfo, nullptr, &state.mainCmdTransFence);
	n_assert(res == VK_SUCCESS);

	state.passInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		nullptr,
		0,
	};

	// setup pipeline construction struct
	state.currentPipelineInfo.pNext = nullptr;
	state.currentPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	state.currentPipelineInfo.flags = 0;
	state.currentPipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	state.currentPipelineInfo.basePipelineIndex = -1;
	state.currentPipelineInfo.pColorBlendState = &state.blendInfo;

	state.inputInfo.pNext = nullptr;
	state.inputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	state.inputInfo.flags = 0;

	state.blendInfo.pNext = nullptr;
	state.blendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	state.blendInfo.flags = 0;

	// reset state
	state.inputInfo.topology = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
	state.currentProgram = -1;
	state.currentPipelineInfo.pVertexInputState = nullptr;
	state.currentPipelineInfo.pInputAssemblyState = nullptr;

	state.inBeginAsyncCompute = false;
	state.inBeginBatch = false;
	state.inBeginCompute = false;
	state.inBeginFrame = false;
	state.inBeginPass = false;

	state.currentDrawThread = 0;
	state.numActiveThreads = 0;

	_setup_timer(state.DebugTimer);
	_setup_counter(state.NumImageBytesAllocated);
	_begin_counter(state.NumImageBytesAllocated);
	_setup_counter(state.NumBufferBytesAllocated);
	_begin_counter(state.NumBufferBytesAllocated);
	_setup_counter(state.NumBytesAllocated);
	_begin_counter(state.NumBytesAllocated);
	_setup_counter(state.NumPipelinesBuilt);
	_begin_counter(state.NumPipelinesBuilt);
    _setup_counter(state.GraphicsDeviceNumComputes);
    _begin_counter(state.GraphicsDeviceNumComputes);
    _setup_counter(state.GraphicsDeviceNumPrimitives);
    _begin_counter(state.GraphicsDeviceNumPrimitives);
    _setup_counter(state.GraphicsDeviceNumDrawCalls);
    _begin_counter(state.GraphicsDeviceNumDrawCalls);

	// yay, Vulkan!
	return true;
}

//------------------------------------------------------------------------------
/**
*/
void 
DestroyGraphicsDevice()
{
	_discard_timer(state.DebugTimer);
    _end_counter(state.GraphicsDeviceNumDrawCalls);
    _discard_counter(state.GraphicsDeviceNumDrawCalls);
    _end_counter(state.GraphicsDeviceNumPrimitives);
    _discard_counter(state.GraphicsDeviceNumPrimitives);
    _end_counter(state.GraphicsDeviceNumComputes);
    _discard_counter(state.GraphicsDeviceNumComputes);
	_end_counter(state.NumImageBytesAllocated);
	_discard_counter(state.NumImageBytesAllocated);
	_end_counter(state.NumBufferBytesAllocated);
	_discard_counter(state.NumBufferBytesAllocated);
	_end_counter(state.NumBytesAllocated);
	_discard_counter(state.NumBytesAllocated);
	_end_counter(state.NumPipelinesBuilt);
	_discard_counter(state.NumPipelinesBuilt);

	// save pipeline cache
	size_t size;
	vkGetPipelineCacheData(state.devices[0], state.cache, &size, nullptr);
	uint8_t* data = (uint8_t*)Memory::Alloc(Memory::ScratchHeap, size);
	vkGetPipelineCacheData(state.devices[0], state.cache, &size, data);
	Util::String path = Util::String::Sprintf("bin:%s_vkpipelinecache", App::Application::Instance()->GetAppTitle().AsCharPtr());
	Ptr<IO::Stream> cachedData = IO::IoServer::Instance()->CreateStream(path);
	cachedData->SetAccessMode(IO::Stream::WriteAccess);
	if (cachedData->Open())
	{
		cachedData->Write(data, (IO::Stream::Size)size);
		cachedData->Close();
	}

	IndexT i;
	for (i = 0; i < state.NumDrawThreads; i++)
	{
		state.drawThreads[i]->Stop();
		state.drawThreads[i] = nullptr;
		n_delete(state.drawCompletionEvents[i]);
	}

	for (i = 0; i < state.NumTransferThreads; i++)
	{
		state.transThreads[i]->Stop();
		state.transThreads[i] = nullptr;
		n_delete(state.transCompletionEvents[i]);
	}

	for (i = 0; i < state.NumComputeThreads; i++)
	{
		state.compThreads[i]->Stop();
		state.compThreads[i] = nullptr;
		n_delete(state.compCompletionEvents[i]);
	}

	// wait for all commands to be done first
	state.subcontextHandler.WaitIdle(GraphicsQueueType);
	state.subcontextHandler.WaitIdle(ComputeQueueType);
	state.subcontextHandler.WaitIdle(TransferQueueType);
	state.subcontextHandler.WaitIdle(SparseQueueType);

	// wait for queues and run all pending commands
	state.subcontextHandler.Discard();
	state.scheduler.Discard();

	// destroy command pools
	for (i = 0; i < state.NumDrawThreads; i++)
		vkDestroyCommandPool(state.devices[state.currentDevice], state.dispatchableCmdDrawBufferPool[i], nullptr);
	for (i = 0; i < state.NumTransferThreads; i++)
		vkDestroyCommandPool(state.devices[state.currentDevice], state.dispatchableCmdTransBufferPool[i], nullptr);
	for (i = 0; i < state.NumComputeThreads; i++)
		vkDestroyCommandPool(state.devices[state.currentDevice], state.dispatchableCmdCompBufferPool[i], nullptr);

	state.database.Discard();

	// destroy pipeline
	vkDestroyPipelineCache(state.devices[state.currentDevice], state.cache, nullptr);

	// free our main buffers, our secondary buffers should be fine so the pools should be free to destroy
	DestroyCmdBuffer(state.mainCmdDrawBuffer);
	DestroyCmdBuffer(state.mainCmdComputeBuffer);
	DestroyCmdBuffer(state.mainCmdTransferBuffer);
	DestroyCmdBuffer(state.mainCmdSparseBuffer);
	DestroyVkPools(state.devices[0]);

	vkDestroyFence(state.devices[0], state.mainCmdDrawFence, nullptr);
	vkDestroyFence(state.devices[0], state.mainCmdCmpFence, nullptr);
	vkDestroyFence(state.devices[0], state.mainCmdTransFence, nullptr);

#if NEBULA_VULKAN_DEBUG
	VkDestroyDebugMessenger(state.instance, VkDebugMessageHandle, nullptr);
#endif

	vkDestroyDevice(state.devices[0], nullptr);
	vkDestroyInstance(state.instance, nullptr);
}

//------------------------------------------------------------------------------
/**
*/
void 
AttachEventHandler(const Ptr<CoreGraphics::RenderEventHandler>& h)
{
	n_assert(h.isvalid());
	n_assert(InvalidIndex == state.eventHandlers.FindIndex(h));
	n_assert(!state.inNotifyEventHandlers);
	state.eventHandlers.Append(h);
	h->OnAttach();
}

//------------------------------------------------------------------------------
/**
*/
void 
RemoveEventHandler(const Ptr<CoreGraphics::RenderEventHandler>& h)
{
	n_assert(h.isvalid());
	n_assert(!state.inNotifyEventHandlers);
	IndexT index = state.eventHandlers.FindIndex(h);
	n_assert(InvalidIndex != index);
	state.eventHandlers.EraseIndex(index);
	h->OnRemove();
}

//------------------------------------------------------------------------------
/**
*/
bool 
NotifyEventHandlers(const CoreGraphics::RenderEvent& e)
{
	n_assert(!state.inNotifyEventHandlers);
	bool handled = false;
	state.inNotifyEventHandlers = true;
	IndexT i;
	for (i = 0; i < state.eventHandlers.Size(); i++)
	{
		handled |= state.eventHandlers[i]->PutEvent(e);
	}
	state.inNotifyEventHandlers = false;
	return handled;
}

//------------------------------------------------------------------------------
/**
*/
bool
BeginFrame(IndexT frameIndex)
{
	n_assert(!state.inBeginFrame);
	n_assert(!state.inBeginPass);
	n_assert(!state.inBeginBatch);

	if (frameIndex != state.currentFrameIndex)
	{
		_begin_counter(state.GraphicsDeviceNumComputes);
		_begin_counter(state.GraphicsDeviceNumPrimitives);
		_begin_counter(state.GraphicsDeviceNumDrawCalls);
	}
	state.inBeginFrame = true;

	VkShaderServer::Instance()->SubmitTextureDescriptorChanges();

	const CmdBufferBeginInfo cmdInfo =
	{
		true, false, false
	};
	CmdBufferBeginRecord(state.mainCmdDrawBuffer, cmdInfo);
	CmdBufferBeginRecord(state.mainCmdComputeBuffer, cmdInfo);
	CmdBufferBeginRecord(state.mainCmdTransferBuffer, cmdInfo);
	CmdBufferBeginRecord(state.mainCmdSparseBuffer, cmdInfo);

	// handle fences from previous frame(s)
	state.scheduler.ExecuteCommandPass(VkScheduler::OnHandleTransferFences);
	state.scheduler.ExecuteCommandPass(VkScheduler::OnHandleDrawFences);
	state.scheduler.ExecuteCommandPass(VkScheduler::OnHandleComputeFences);

	state.scheduler.Begin();
	state.scheduler.ExecuteCommandPass(VkScheduler::OnBeginFrame);

	// reset current thread
	state.currentDrawThread = state.NumDrawThreads - 1;
	state.currentPipelineBits = NoInfoSet;

	return true;
}

//------------------------------------------------------------------------------
/**
*/
void 
BeginPass(const CoreGraphics::PassId pass)
{
	n_assert(state.inBeginFrame);
	n_assert(!state.inBeginPass);
	n_assert(!state.inBeginBatch);
	n_assert(state.pass == PassId::Invalid());
	state.inBeginPass = true;

	state.pass = pass;

	// set info
	SetFramebufferLayoutInfo(PassGetVkFramebufferInfo(pass));
	state.database.SetPass(pass);
	state.database.SetSubpass(0);

	const VkRenderPassBeginInfo& info = PassGetVkRenderPassBeginInfo(pass);
	vkCmdBeginRenderPass(CommandBufferGetVk(state.mainCmdDrawBuffer), &info, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

	// run this phase for scheduler
	state.scheduler.ExecuteCommandPass(VkScheduler::OnBeginPass);

	state.passInfo.framebuffer = info.framebuffer;
	state.passInfo.renderPass = info.renderPass;
	state.passInfo.subpass = 0;
	state.passInfo.pipelineStatistics = 0;
	state.passInfo.queryFlags = 0;
	state.passInfo.occlusionQueryEnable = VK_FALSE;
}

//------------------------------------------------------------------------------
/**
*/
void 
SetToNextSubpass()
{
	n_assert(state.inBeginFrame);
	n_assert(state.inBeginPass);
	n_assert(!state.inBeginBatch);
	n_assert(state.pass != PassId::Invalid());
	SetFramebufferLayoutInfo(PassGetVkFramebufferInfo(state.pass));
	state.database.SetSubpass(state.currentPipelineInfo.subpass);
	state.passInfo.subpass = state.currentPipelineInfo.subpass;
	vkCmdNextSubpass(CommandBufferGetVk(state.mainCmdDrawBuffer), VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
}

//------------------------------------------------------------------------------
/**
*/
void 
BeginBatch(Frame::FrameBatchType::Code batchType)
{
	n_assert(state.inBeginPass);
	n_assert(!state.inBeginBatch);
	n_assert(state.pass != PassId::Invalid());

	state.inBeginBatch = true;
	PassBeginBatch(state.pass, batchType);

	// set to first sub-batch which starts a new draw thread
	SetToNextSubBatch();
}

//------------------------------------------------------------------------------
/**
*/
void 
SetToNextSubBatch()
{
	n_assert(state.inBeginBatch);

	// begin new draw thread
	state.currentDrawThread = (state.currentDrawThread + 1) % state.NumDrawThreads;
	if (state.numActiveThreads < state.NumDrawThreads)
	{
		// if we want a new thread, make one, then bind shared descriptor sets
		BeginDrawThread();
	}
}

//------------------------------------------------------------------------------
/**
*/
void 
ResetClipSettings()
{
	PassApplyClipSettings(state.pass);
}

//------------------------------------------------------------------------------
/**
*/
void 
SetStreamVertexBuffer(IndexT streamIndex, const CoreGraphics::VertexBufferId& vb, IndexT offsetVertexIndex)
{
	if (state.inBeginBatch)
	{
		VkCmdBufferThread::Command cmd;
		cmd.type = VkCmdBufferThread::InputAssemblyVertex;
		cmd.vbo.buffer = CoreGraphics::vboPool->allocator.Get<1>(vb.allocId).buf;
		cmd.vbo.index = streamIndex;
		cmd.vbo.offset = offsetVertexIndex;
		PushToThread(cmd, state.currentDrawThread);
	}
	else
	{
		vkCmdBindVertexBuffers(CommandBufferGetVk(state.mainCmdDrawBuffer), streamIndex, 1, &CoreGraphics::vboPool->allocator.Get<1>(vb.allocId).buf, (VkDeviceSize*)&offsetVertexIndex);
	}
}

//------------------------------------------------------------------------------
/**
*/
void 
SetVertexLayout(const CoreGraphics::VertexLayoutId& vl)
{
	n_assert(state.currentShaderProgram != CoreGraphics::ShaderProgramId::Invalid());
	VkPipelineVertexInputStateCreateInfo* info = CoreGraphics::layoutPool->GetDerivativeLayout(vl, state.currentShaderProgram);
	SetVertexLayoutPipelineInfo(info);
}

//------------------------------------------------------------------------------
/**
*/
void 
SetIndexBuffer(const CoreGraphics::IndexBufferId& ib, IndexT offsetIndex)
{
	if (state.inBeginBatch)
	{
		VkCmdBufferThread::Command cmd;
		cmd.type = VkCmdBufferThread::InputAssemblyIndex;
		cmd.ibo.buffer = CoreGraphics::iboPool->allocator.Get<1>(ib.allocId).buf;
		cmd.ibo.indexType = CoreGraphics::iboPool->allocator.Get<1>(ib.allocId).type == IndexType::Index16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
		cmd.ibo.offset = offsetIndex;
		PushToThread(cmd, state.currentDrawThread);
	}
	else
	{
		vkCmdBindIndexBuffer(CommandBufferGetVk(state.mainCmdDrawBuffer), CoreGraphics::iboPool->allocator.Get<1>(ib.allocId).buf, offsetIndex, CoreGraphics::iboPool->allocator.Get<1>(ib.allocId).type == IndexType::Index16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
	}
}

//------------------------------------------------------------------------------
/**
*/
void 
SetPrimitiveTopology(const CoreGraphics::PrimitiveTopology::Code topo)
{
	state.primitiveTopology = topo;
	VkPrimitiveTopology comp = VkTypes::AsVkPrimitiveType(topo);
	if (state.inputInfo.topology != comp)
	{
		state.inputInfo.topology = comp;
		state.inputInfo.primitiveRestartEnable = VK_FALSE;
		SetInputLayoutInfo(&state.inputInfo);
	}
}

//------------------------------------------------------------------------------
/**
*/
const CoreGraphics::PrimitiveTopology::Code& 
GetPrimitiveTopology()
{
	return state.primitiveTopology;
}

//------------------------------------------------------------------------------
/**
*/
void 
SetPrimitiveGroup(const CoreGraphics::PrimitiveGroup& pg)
{
	state.primitiveGroup = pg;
}

//------------------------------------------------------------------------------
/**
*/
const CoreGraphics::PrimitiveGroup& 
GetPrimitiveGroup()
{
	return state.primitiveGroup;
}

//------------------------------------------------------------------------------
/**
*/
void
SetShaderProgram(const CoreGraphics::ShaderProgramId& pro)
{
	const VkShaderProgramRuntimeInfo& info = CoreGraphics::shaderPool->shaderAlloc.Get<3>(pro.shaderId).Get<2>(pro.programId);
	state.currentShaderProgram = pro;
	state.currentPipelineLayout = info.layout;

	// if we are compute, we can set the pipeline straight away, otherwise we have to accumulate the infos
	if (info.type == ComputePipeline)		Vulkan::BindComputePipeline(info.pipeline, info.layout);
	else if (info.type == GraphicsPipeline)
	{
		// setup pipeline information regarding the shader state
		VkGraphicsPipelineCreateInfo ginfo =
		{
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			NULL,
			VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT,
			info.stageCount,
			info.shaderInfos,
			&info.vertexInfo,			// we only save how many vs inputs we allow here
			NULL,						// this is input type related (triangles, patches etc)
			&info.tessInfo,
			NULL,						// this is our viewport and is setup by the framebuffer
			&info.rasterizerInfo,
			&info.multisampleInfo,
			&info.depthStencilInfo,
			&info.colorBlendInfo,
			&info.dynamicInfo,
			info.layout,
			NULL,							// pass specific stuff, keep as NULL, handled by the framebuffer
			0,
			VK_NULL_HANDLE, 0				// base pipeline is kept as NULL too, because this is the base for all derivatives
		};
		Vulkan::BindGraphicsPipelineInfo(ginfo, info.uniqueId);
	}
	else
		Vulkan::UnbindPipeline();
}

//------------------------------------------------------------------------------
/**
*/
void 
SetShaderProgram(const CoreGraphics::ShaderId shaderId, const CoreGraphics::ShaderFeature::Mask mask)
{
	VkShaderPool::VkShaderRuntimeInfo& runtime = CoreGraphics::shaderPool->shaderAlloc.Get<2>(shaderId.allocId);
	ShaderProgramId& programId = runtime.activeShaderProgram;
	VkShaderProgramAllocator& programs = CoreGraphics::shaderPool->shaderAlloc.Get<3>(shaderId.allocId);

	// change variation if it's actually changed
	if (state.currentShaderProgram != programId && runtime.activeMask != mask)
	{
		programId = runtime.programMap[mask];
		runtime.activeMask = mask;
	}
	SetShaderProgram(programId);
}

//------------------------------------------------------------------------------
/**
*/
void
SetResourceTable(const CoreGraphics::ResourceTableId table, const IndexT slot, ShaderPipeline pipeline, const Util::FixedArray<uint>& offsets)
{
	switch (pipeline)
	{
	case GraphicsPipeline:
		Vulkan::BindDescriptorsGraphics(&ResourceTableGetVkDescriptorSet(table),
			slot,
			1,
			offsets.IsEmpty() ? nullptr : offsets.Begin(),
			offsets.Size());
		break;
	case ComputePipeline:
		Vulkan::BindDescriptorsCompute(&ResourceTableGetVkDescriptorSet(table),
			slot,
			1,
			offsets.IsEmpty() ? nullptr : offsets.Begin(),
			offsets.Size());
		break;
	}
}

//------------------------------------------------------------------------------
/**
*/
void 
SetResourceTablePipeline(const CoreGraphics::ResourcePipelineId layout)
{
	n_assert(layout != CoreGraphics::ResourcePipelineId::Invalid());
	state.currentPipelineLayout = ResourcePipelineGetVk(layout);
}

//------------------------------------------------------------------------------
/**
*/
void
PushConstants(ShaderPipeline pipeline, uint offset, uint size, byte* data)
{
	switch (pipeline)
	{
	case GraphicsPipeline:
		Vulkan::UpdatePushRanges(VK_SHADER_STAGE_ALL_GRAPHICS, state.currentPipelineLayout, offset, size, data);
		break;
	case ComputePipeline:
		Vulkan::UpdatePushRanges(VK_SHADER_STAGE_COMPUTE_BIT, state.currentPipelineLayout, offset, size, data);
		break;
	}
}


//------------------------------------------------------------------------------
/**
*/
void 
SetGraphicsPipeline()
{
	n_assert((state.currentPipelineBits & AllInfoSet) != 0);
	state.currentBindPoint = CoreGraphics::GraphicsPipeline;
	if ((state.currentPipelineBits & PipelineBuilt) == 0)
	{
		CreateAndBindGraphicsPipeline();
		state.currentPipelineBits |= PipelineBuilt;
	}
}

//------------------------------------------------------------------------------
/**
*/
void 
ReloadShaderProgram(const CoreGraphics::ShaderProgramId& pro)
{
	state.database.Reload(pro);
}

//------------------------------------------------------------------------------
/**
*/
void 
InsertBarrier(const CoreGraphics::BarrierId barrier, const CoreGraphicsQueueType queue)
{
	VkBarrierInfo& info = barrierAllocator.Get<0>(barrier.id24);
	if (queue == GraphicsQueueType && state.inBeginPass)
	{
		if (state.inBeginBatch)
		{
			VkCmdBufferThread::Command cmd;
			cmd.type = VkCmdBufferThread::Barrier;
			cmd.barrier.dep = info.dep;
			cmd.barrier.srcMask = info.srcFlags;
			cmd.barrier.dstMask = info.dstFlags;
			cmd.barrier.memoryBarrierCount = info.numMemoryBarriers;
			cmd.barrier.memoryBarriers = info.memoryBarriers;
			cmd.barrier.bufferBarrierCount = info.numBufferBarriers;
			cmd.barrier.bufferBarriers = info.bufferBarriers;
			cmd.barrier.imageBarrierCount = info.numImageBarriers;
			cmd.barrier.imageBarriers = info.imageBarriers;
			PushToThread(cmd, state.currentDrawThread);
		}
		else
		{
			vkCmdPipelineBarrier(CommandBufferGetVk(state.mainCmdDrawBuffer), 
				info.srcFlags, info.dstFlags, 
				info.dep, 
				info.numMemoryBarriers, info.memoryBarriers, 
				info.numBufferBarriers, info.bufferBarriers, 
				info.numImageBarriers, info.imageBarriers);
		}
	}
	else
	{
		VkCommandBuffer buf;
		switch (queue)
		{
		case GraphicsQueueType: buf = CommandBufferGetVk(state.mainCmdDrawBuffer); break;
		case TransferQueueType: buf = CommandBufferGetVk(state.mainCmdTransferBuffer); break;
		case ComputeQueueType: buf = CommandBufferGetVk(state.mainCmdComputeBuffer); break;
		case SparseQueueType: buf = CommandBufferGetVk(state.mainCmdSparseBuffer); break;
		}

		vkCmdPipelineBarrier(buf,
			info.srcFlags,
			info.dstFlags,
			info.dep,
			info.numMemoryBarriers, info.memoryBarriers,
			info.numBufferBarriers, info.bufferBarriers,
			info.numImageBarriers, info.imageBarriers);
	}
}

//------------------------------------------------------------------------------
/**
*/
void 
SignalEvent(const CoreGraphics::EventId ev, const CoreGraphicsQueueType queue)
{
	VkEventInfo& info = eventAllocator.Get<1>(ev.id24);
	if (queue == GraphicsQueueType && state.inBeginPass)
	{
		if (state.inBeginBatch)
		{
			VkCmdBufferThread::Command cmd;
			cmd.type = VkCmdBufferThread::SetEvent;
			cmd.setEvent.event = info.event;
			cmd.setEvent.stages = info.leftDependency;
			PushToThread(cmd, state.currentDrawThread);
		}
		else
		{
			vkCmdSetEvent(CommandBufferGetVk(state.mainCmdDrawBuffer), info.event, info.leftDependency);
		}
	}
	else
	{
		VkCommandBuffer buf;
		switch (queue)
		{
		case GraphicsQueueType: buf = CommandBufferGetVk(state.mainCmdDrawBuffer); break;
		case TransferQueueType: buf = CommandBufferGetVk(state.mainCmdTransferBuffer); break;
		case ComputeQueueType: buf = CommandBufferGetVk(state.mainCmdComputeBuffer); break;
		case SparseQueueType: buf = CommandBufferGetVk(state.mainCmdSparseBuffer); break;
		}

		vkCmdSetEvent(buf, info.event, info.leftDependency);
	}
}

//------------------------------------------------------------------------------
/**
*/
void 
WaitEvent(const CoreGraphics::EventId ev, const CoreGraphicsQueueType queue)
{
	VkEventInfo& info = eventAllocator.Get<1>(ev.id24);
	if (queue == GraphicsQueueType && state.inBeginPass)
	{
		if (state.inBeginBatch)
		{
			VkCmdBufferThread::Command cmd;
			cmd.type = VkCmdBufferThread::WaitForEvent;
			cmd.waitEvent.events = &info.event;
			cmd.waitEvent.numEvents = 1;
			cmd.waitEvent.memoryBarrierCount = info.numMemoryBarriers;
			cmd.waitEvent.memoryBarriers = info.memoryBarriers;
			cmd.waitEvent.bufferBarrierCount = info.numBufferBarriers;
			cmd.waitEvent.bufferBarriers = info.bufferBarriers;
			cmd.waitEvent.imageBarrierCount = info.numImageBarriers;
			cmd.waitEvent.imageBarriers = info.imageBarriers;
			cmd.waitEvent.waitingStage = info.leftDependency;
			cmd.waitEvent.signalingStage = info.rightDependency;
			PushToThread(cmd, state.currentDrawThread);
		}
		else
		{
			vkCmdWaitEvents(CommandBufferGetVk(state.mainCmdDrawBuffer), 1, &info.event,
				info.leftDependency,
				info.rightDependency,
				info.numMemoryBarriers,
				info.memoryBarriers,
				info.numBufferBarriers,
				info.bufferBarriers,
				info.numImageBarriers,
				info.imageBarriers);
		}
	}
	else
	{
		VkCommandBuffer buf;
		switch (queue)
		{
		case GraphicsQueueType: buf = CommandBufferGetVk(state.mainCmdDrawBuffer); break;
		case TransferQueueType: buf = CommandBufferGetVk(state.mainCmdTransferBuffer); break;
		case ComputeQueueType: buf = CommandBufferGetVk(state.mainCmdComputeBuffer); break;
		case SparseQueueType: buf = CommandBufferGetVk(state.mainCmdSparseBuffer); break;
		}

		vkCmdWaitEvents(buf, 1, &info.event,
			info.leftDependency,
			info.rightDependency,
			info.numMemoryBarriers,
			info.memoryBarriers,
			info.numBufferBarriers,
			info.bufferBarriers,
			info.numImageBarriers,
			info.imageBarriers);
	}
}

//------------------------------------------------------------------------------
/**
*/
void 
ResetEvent(const CoreGraphics::EventId ev, const CoreGraphicsQueueType queue)
{
	VkEventInfo& info = eventAllocator.Get<1>(ev.id24);
	if (queue == GraphicsQueueType && state.inBeginPass)	
	{
		if (state.inBeginBatch)
		{
			VkCmdBufferThread::Command cmd;
			cmd.type = VkCmdBufferThread::ResetEvent;
			cmd.setEvent.event = info.event;
			cmd.setEvent.stages = info.rightDependency;
			PushToThread(cmd, state.currentDrawThread);
		}
		else
		{
			vkCmdResetEvent(CommandBufferGetVk(state.mainCmdDrawBuffer), info.event, info.rightDependency);
		}
	}
	else
	{
		VkCommandBuffer buf;
		switch (queue)
		{
		case GraphicsQueueType: buf = CommandBufferGetVk(state.mainCmdDrawBuffer); break;
		case TransferQueueType: buf = CommandBufferGetVk(state.mainCmdTransferBuffer); break;
		case ComputeQueueType: buf = CommandBufferGetVk(state.mainCmdComputeBuffer); break;
		case SparseQueueType: buf = CommandBufferGetVk(state.mainCmdSparseBuffer); break;
		}

		vkCmdResetEvent(buf, info.event, info.rightDependency);
	}
}

//------------------------------------------------------------------------------
/**
*/
void 
SignalFence(const CoreGraphics::FenceId fe, const CoreGraphicsQueueType queue)
{
	n_assert2(!fenceAllocator.Get<1>(fe.id24).pending, "Fence is already waiting to be signaled!");
	VkQueue q = state.subcontextHandler.GetQueue(queue);
	VkResult res = vkQueueSubmit(q, 0, nullptr, fenceAllocator.Get<1>(fe.id24).fence);
	n_assert(res == VK_SUCCESS);
	fenceAllocator.Get<1>(fe.id24).pending = true;
}

//------------------------------------------------------------------------------
/**
*/
bool 
PeekFence(const CoreGraphics::FenceId fe)
{
	VkResult res = vkGetFenceStatus(fenceAllocator.Get<0>(fe.id24), fenceAllocator.Get<1>(fe.id24).fence);
	return res == VK_SUCCESS;
}

//------------------------------------------------------------------------------
/**
*/
void 
ResetFence(const CoreGraphics::FenceId fe)
{
	VkResult res = vkResetFences(fenceAllocator.Get<0>(fe.id24), 1, &fenceAllocator.Get<1>(fe.id24).fence);
	n_assert(res == VK_SUCCESS);
}

//------------------------------------------------------------------------------
/**
*/
bool 
WaitFence(const CoreGraphics::FenceId fe, uint64 wait)
{
	n_assert(fenceAllocator.Get<1>(fe.id24).pending);
	VkResult res = vkWaitForFences(fenceAllocator.Get<0>(fe.id24), 1, &fenceAllocator.Get<1>(fe.id24).fence, false, UINT_MAX);
	if (res == VK_SUCCESS)
	{
		res = vkResetFences(fenceAllocator.Get<0>(fe.id24), 1, &fenceAllocator.Get<1>(fe.id24).fence);
		n_assert(res == VK_SUCCESS);

		// reset pending status
		fenceAllocator.Get<1>(fe.id24).pending = false;
	}
	return res;
}

//------------------------------------------------------------------------------
/**
*/
void 
Draw()
{
	n_assert(state.inBeginPass);

	if (state.inBeginBatch)
	{
		VkCmdBufferThread::Command cmd;
		cmd.type = VkCmdBufferThread::Draw;
		cmd.draw.baseIndex = state.primitiveGroup.GetBaseIndex();
		cmd.draw.baseVertex = state.primitiveGroup.GetBaseVertex();
		cmd.draw.baseInstance = 0;
		cmd.draw.numIndices = state.primitiveGroup.GetNumIndices();
		cmd.draw.numVerts = state.primitiveGroup.GetNumVertices();
		cmd.draw.numInstances = 1;
		PushToThread(cmd, state.currentDrawThread);
	}
	else
	{
		vkCmdDraw(CommandBufferGetVk(state.mainCmdDrawBuffer), state.primitiveGroup.GetNumVertices(), 1, state.primitiveGroup.GetBaseVertex(), 0);
	}

	// go to next thread
	_incr_counter(state.GraphicsDeviceNumDrawCalls, 1);
	_incr_counter(state.GraphicsDeviceNumPrimitives, state.primitiveGroup.GetNumVertices() / 3);
}

//------------------------------------------------------------------------------
/**
*/
void 
DrawIndexedInstanced(SizeT numInstances, IndexT baseInstance)
{
	n_assert(state.inBeginPass);

	if (state.inBeginBatch)
	{
		VkCmdBufferThread::Command cmd;
		cmd.type = VkCmdBufferThread::Draw;
		cmd.draw.baseIndex = state.primitiveGroup.GetBaseIndex();
		cmd.draw.baseVertex = state.primitiveGroup.GetBaseVertex();
		cmd.draw.baseInstance = baseInstance;
		cmd.draw.numIndices = state.primitiveGroup.GetNumIndices();
		cmd.draw.numVerts = state.primitiveGroup.GetNumVertices();
		cmd.draw.numInstances = numInstances;
		PushToThread(cmd, state.currentDrawThread);
	}
	else
	{
		vkCmdDrawIndexed(CommandBufferGetVk(state.mainCmdDrawBuffer), state.primitiveGroup.GetNumIndices(), numInstances, state.primitiveGroup.GetBaseIndex(), state.primitiveGroup.GetBaseVertex(), baseInstance);
	}

	// go to next thread
	_incr_counter(state.GraphicsDeviceNumDrawCalls, 1);
	_incr_counter(state.GraphicsDeviceNumPrimitives, state.primitiveGroup.GetNumIndices() * numInstances / 3);
}

//------------------------------------------------------------------------------
/**
*/
void 
Compute(int dimX, int dimY, int dimZ)
{
	n_assert(!state.inBeginPass);
	vkCmdDispatch(CommandBufferGetVk(state.mainCmdDrawBuffer), dimX, dimY, dimZ);
}

//------------------------------------------------------------------------------
/**
*/
void
EndBatch()
{
	n_assert(state.inBeginBatch);
	n_assert(state.pass != PassId::Invalid());

	state.currentProgram = -1;
	state.inBeginBatch = false;
	PassEndBatch(state.pass);

	// end draw threads, if any are remaining
	EndDrawThreads();
}

//------------------------------------------------------------------------------
/**
*/
void
EndPass()
{
	n_assert(state.inBeginPass);
	n_assert(state.pass != PassId::Invalid());

	state.pass = PassId::Invalid();
	state.inBeginPass = false;

	//this->currentPipelineBits = 0;
	state.propagateDescriptorSets.Clear();
	state.currentProgram = -1;

	// tell scheduler pass is ending
	state.scheduler.ExecuteCommandPass(VkScheduler::OnEndPass);

	// end render pass
	vkCmdEndRenderPass(CommandBufferGetVk(state.mainCmdDrawBuffer));
}

//------------------------------------------------------------------------------
/**
*/
void
EndFrame(IndexT frameIndex)
{
	n_assert(state.inBeginFrame);

	if (state.currentFrameIndex != frameIndex)
	{
		_end_counter(state.GraphicsDeviceNumComputes);
		_end_counter(state.GraphicsDeviceNumPrimitives);
		_end_counter(state.GraphicsDeviceNumDrawCalls);
		state.currentFrameIndex = frameIndex;
	}

	state.inBeginFrame = false;

	CmdBufferEndRecord(state.mainCmdTransferBuffer);
	CmdBufferEndRecord(state.mainCmdComputeBuffer);
	CmdBufferEndRecord(state.mainCmdDrawBuffer);
	CmdBufferEndRecord(state.mainCmdSparseBuffer);

#if NEBULA_GRAPHICS_DEBUG
	CoreGraphics::QueueBeginMarker(TransferQueueType, NEBULA_MARKER_BLUE, "End of frame transfer queue submission");
#endif

	// kick transfer commands
	state.subcontextHandler.InsertCommandBuffer(TransferQueueType, CommandBufferGetVk(state.mainCmdTransferBuffer));
	state.subcontextHandler.InsertDependency({ GraphicsQueueType, TransferQueueType }, TransferQueueType, VK_PIPELINE_STAGE_TRANSFER_BIT);
	state.subcontextHandler.Submit(TransferQueueType, state.mainCmdTransFence, true);

	// put in frame fences if needed
	state.scheduler.ExecuteCommandPass(VkScheduler::OnMainTransferSubmitted);
	state.scheduler.EndTransfers();

#if NEBULA_GRAPHICS_DEBUG
	CoreGraphics::QueueEndMarker(TransferQueueType);
	CoreGraphics::QueueBeginMarker(ComputeQueueType, NEBULA_MARKER_BLUE, "End of frame compute queue submission");
#endif

	// submit compute stuff
	state.subcontextHandler.InsertCommandBuffer(ComputeQueueType, CommandBufferGetVk(state.mainCmdComputeBuffer));
	state.subcontextHandler.Submit(ComputeQueueType, state.mainCmdCmpFence, true);

	state.scheduler.ExecuteCommandPass(VkScheduler::OnMainComputeSubmitted);
	state.scheduler.EndComputes();

#if NEBULA_GRAPHICS_DEBUG
	CoreGraphics::QueueEndMarker(ComputeQueueType);
	CoreGraphics::QueueBeginMarker(GraphicsQueueType, NEBULA_MARKER_BLUE, "End of frame graphics queue submission");
#endif

	// submit draw stuff
	state.subcontextHandler.InsertCommandBuffer(GraphicsQueueType, CommandBufferGetVk(state.mainCmdDrawBuffer));
	state.subcontextHandler.Submit(GraphicsQueueType, state.mainCmdDrawFence, true);

	state.scheduler.ExecuteCommandPass(VkScheduler::OnMainDrawSubmitted);
	state.scheduler.EndDraws();

#if NEBULA_GRAPHICS_DEBUG
	CoreGraphics::QueueEndMarker(GraphicsQueueType);
#endif

	static VkFence fences[] = { state.mainCmdTransFence, state.mainCmdCmpFence, state.mainCmdDrawFence };
	WaitForFences(fences, 3, true);

	CmdBufferClearInfo clearInfo =
	{
		true
	};
	CmdBufferClear(state.mainCmdTransferBuffer, clearInfo);
	CmdBufferClear(state.mainCmdComputeBuffer, clearInfo);
	CmdBufferClear(state.mainCmdDrawBuffer, clearInfo);
	CmdBufferClear(state.mainCmdSparseBuffer, clearInfo);

	// run end-of-frame commands
	state.scheduler.ExecuteCommandPass(VkScheduler::OnEndFrame);

	// reset state
	state.inputInfo.topology = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
	state.currentProgram = -1;
	state.currentPipelineInfo.pVertexInputState = nullptr;
	state.currentPipelineInfo.pInputAssemblyState = nullptr;
}

//------------------------------------------------------------------------------
/**
*/
bool
IsInBeginFrame()
{
	return state.inBeginFrame;
}

//------------------------------------------------------------------------------
/**
*/
void 
Present()
{
	n_assert(!state.inBeginFrame);
	state.frameId++;
}

//------------------------------------------------------------------------------
/**
*/
CoreGraphics::ImageFileFormat::Code
SaveScreenshot(CoreGraphics::ImageFileFormat::Code fmt, const Ptr<IO::Stream>& outStream)
{
	return CoreGraphics::ImageFileFormat::InvalidImageFileFormat;
}

//------------------------------------------------------------------------------
/**
*/
CoreGraphics::ImageFileFormat::Code 
SaveScreenshot(CoreGraphics::ImageFileFormat::Code fmt, const Ptr<IO::Stream>& outStream, const Math::rectangle<int>& rect, int x, int y)
{
	return CoreGraphics::ImageFileFormat::InvalidImageFileFormat;
}

//------------------------------------------------------------------------------
/**
*/
bool 
GetVisualizeMipMaps()
{
	return state.visualizeMipMaps;
}

//------------------------------------------------------------------------------
/**
*/
void 
SetVisualizeMipMaps(bool val)
{
	state.visualizeMipMaps = val;
}

//------------------------------------------------------------------------------
/**
*/
bool
GetRenderWireframe()
{
	return state.renderWireframe;
}

//------------------------------------------------------------------------------
/**
*/
void 
SetRenderWireframe(bool b)
{
	state.renderWireframe = b;
}

//------------------------------------------------------------------------------
/**
*/
void 
Copy(const CoreGraphics::TextureId from, Math::rectangle<SizeT> fromRegion, const CoreGraphics::TextureId to, Math::rectangle<SizeT> toRegion)
{
	n_assert(from != CoreGraphics::TextureId::Invalid() && to != CoreGraphics::TextureId::Invalid());
	n_assert(!state.inBeginPass);
	Vulkan::Copy(TextureGetVkImage(from), fromRegion, TextureGetVkImage(to), toRegion);
}

//------------------------------------------------------------------------------
/**
*/
void 
Copy(const CoreGraphics::RenderTextureId from, Math::rectangle<SizeT> fromRegion, const CoreGraphics::RenderTextureId to, Math::rectangle<SizeT> toRegion)
{
	n_assert(from != CoreGraphics::RenderTextureId::Invalid() && to != CoreGraphics::RenderTextureId::Invalid());
	n_assert(!state.inBeginPass);
	Vulkan::Copy(RenderTextureGetVkImage(from), fromRegion, RenderTextureGetVkImage(to), toRegion);
}

//------------------------------------------------------------------------------
/**
*/
void 
Blit(const CoreGraphics::TextureId from, Math::rectangle<SizeT> fromRegion, IndexT fromMip, const CoreGraphics::TextureId to, Math::rectangle<SizeT> toRegion, IndexT toMip)
{
	n_assert(from != CoreGraphics::TextureId::Invalid() && to != CoreGraphics::TextureId::Invalid());
	Vulkan::Blit(TextureGetVkImage(from), fromRegion, fromMip, TextureGetVkImage(to), toRegion, toMip);
}

//------------------------------------------------------------------------------
/**
*/
void
Blit(const CoreGraphics::RenderTextureId from, Math::rectangle<SizeT> fromRegion, IndexT fromMip, const CoreGraphics::RenderTextureId to, Math::rectangle<SizeT> toRegion, IndexT toMip)
{
	n_assert(from != CoreGraphics::RenderTextureId::Invalid() && to != CoreGraphics::RenderTextureId::Invalid());
	Vulkan::Blit(RenderTextureGetVkImage(from), fromRegion, fromMip, RenderTextureGetVkImage(to), toRegion, toMip);
}

//------------------------------------------------------------------------------
/**
*/
void 
SetUsePatches(bool b)
{
	state.usePatches = b;
}

//------------------------------------------------------------------------------
/**
*/
bool 
GetUsePatches()
{
	return state.usePatches;
}

//------------------------------------------------------------------------------
/**
*/
void 
SetViewport(const Math::rectangle<int>& rect, int index)
{
	// copy here is on purpose, because we don't want to modify the state viewports (they are pointers to the pass)
	VkViewport& vp = state.viewports[index];
	vp.width = (float)rect.width();
	vp.height = (float)rect.height();
	vp.x = (float)rect.left;
	vp.y = (float)rect.top;

	// only apply to batch or command buffer if we have a program bound
	if (state.currentProgram != -1)
	{
		if (state.inBeginBatch)
		{
			VkCmdBufferThread::Command cmd;
			cmd.type = VkCmdBufferThread::Viewport;
			cmd.viewport.index = index;
			cmd.viewport.vp = vp;
			PushToThread(cmd, state.currentDrawThread);
		}
		else
		{
			vkCmdSetViewport(CommandBufferGetVk(state.mainCmdDrawBuffer), index, 1, &state.viewports[index]);
		}
	}
}

//------------------------------------------------------------------------------
/**
*/
void 
SetScissorRect(const Math::rectangle<int>& rect, int index)
{
	// copy here is on purpose, because we don't want to modify the state scissors (they are pointers to the pass)
	VkRect2D& sc = state.scissors[index];
	sc.extent.width = rect.width();
	sc.extent.height = rect.height();
	sc.offset.x = rect.left;
	sc.offset.y = rect.top;

	if (state.currentProgram != -1)
	{
		if (state.inBeginBatch)
		{
			VkCmdBufferThread::Command cmd;
			cmd.type = VkCmdBufferThread::ScissorRect;
			cmd.scissorRect.index = index;
			cmd.scissorRect.sc = sc;
			PushToThread(cmd, state.currentDrawThread);
		}
		else
		{
			vkCmdSetScissor(CommandBufferGetVk(state.mainCmdDrawBuffer), index, 1, &state.scissors[index]);
		}
	}
}


//------------------------------------------------------------------------------
/**
*/
void 
RegisterRenderTexture(const Util::StringAtom& name, const CoreGraphics::RenderTextureId id)
{
	state.renderTextures.Add(name, id);
}

//------------------------------------------------------------------------------
/**
*/
const CoreGraphics::RenderTextureId 
GetRenderTexture(const Util::StringAtom & name)
{
	IndexT i = state.renderTextures.FindIndex(name);
	if (i == InvalidIndex)		return CoreGraphics::RenderTextureId::Invalid();
	else						return state.renderTextures[name];
}

//------------------------------------------------------------------------------
/**
*/
void 
RegisterShaderRWTexture(const Util::StringAtom& name, const CoreGraphics::ShaderRWTextureId id)
{
	state.shaderRWTextures.Add(name, id);
}


//------------------------------------------------------------------------------
/**
*/
const CoreGraphics::ShaderRWTextureId 
GetShaderRWTexture(const Util::StringAtom & name)
{
	IndexT i = state.shaderRWTextures.FindIndex(name);
	if (i == InvalidIndex)		return CoreGraphics::ShaderRWTextureId::Invalid();
	else						return state.shaderRWTextures[name];
}

//------------------------------------------------------------------------------
/**
*/
void 
RegisterShaderRWBuffer(const Util::StringAtom& name, const CoreGraphics::ShaderRWBufferId id)
{
	state.shaderRWBuffers.Add(name, id);
}

//------------------------------------------------------------------------------
/**
*/
const CoreGraphics::ShaderRWBufferId 
GetShaderRWBuffer(const Util::StringAtom& name)
{
	IndexT i = state.shaderRWBuffers.FindIndex(name);
	if (i == InvalidIndex)		return CoreGraphics::ShaderRWBufferId::Invalid();
	else						return state.shaderRWBuffers[name];
}

#if NEBULA_GRAPHICS_DEBUG

//------------------------------------------------------------------------------
/**
*/
template<>
void
ObjectSetName(const CoreGraphics::TextureId id, const Util::String& name)
{
	VkDebugUtilsObjectNameInfoEXT info =
	{
		VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		nullptr,
		VK_OBJECT_TYPE_IMAGE,
		(uint64_t)Vulkan::TextureGetVkImage(id),
		name.AsCharPtr()
	};
	VkDevice dev = GetCurrentDevice();
	n_assert(VkDebugObjectName(dev, &info) == VK_SUCCESS);

	info.objectHandle = (uint64_t)Vulkan::TextureGetVkImageView(id);
	info.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
	Util::String str = Util::String::Sprintf("%s - View", name.AsCharPtr());
	info.pObjectName = str.AsCharPtr();
	n_assert(VkDebugObjectName(dev, &info) == VK_SUCCESS);
}

//------------------------------------------------------------------------------
/**
*/
template<>
void
ObjectSetName(const CoreGraphics::RenderTextureId id, const Util::String& name)
{
	VkDebugUtilsObjectNameInfoEXT info =
	{
		VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		nullptr,
		VK_OBJECT_TYPE_IMAGE,
		(uint64_t)Vulkan::RenderTextureGetVkImage(id),
		name.AsCharPtr()
	};
	VkDevice dev = GetCurrentDevice();
	n_assert(VkDebugObjectName(dev, &info) == VK_SUCCESS);

	info.objectHandle = (uint64_t)Vulkan::RenderTextureGetVkImageView(id);
	info.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
	Util::String str = Util::String::Sprintf("%s - View", name.AsCharPtr());
	info.pObjectName = str.AsCharPtr();
	n_assert(VkDebugObjectName(dev, &info) == VK_SUCCESS);
}

//------------------------------------------------------------------------------
/**
*/
template<>
void
ObjectSetName(const CoreGraphics::ShaderRWTextureId id, const Util::String& name)
{
	VkDebugUtilsObjectNameInfoEXT info =
	{
		VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		nullptr,
		VK_OBJECT_TYPE_IMAGE,
		(uint64_t)Vulkan::ShaderRWTextureGetVkImage(id),
		name.AsCharPtr()
	};
	VkDevice dev = GetCurrentDevice();
	n_assert(VkDebugObjectName(dev, &info) == VK_SUCCESS);

	info.objectHandle = (uint64_t)Vulkan::ShaderRWTextureGetVkImageView(id);
	info.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
	Util::String str = Util::String::Sprintf("%s - View", name.AsCharPtr());
	info.pObjectName = str.AsCharPtr();
	n_assert(VkDebugObjectName(dev, &info) == VK_SUCCESS);
}

//------------------------------------------------------------------------------
/**
*/
template<>
void
ObjectSetName(const CoreGraphics::ShaderRWBufferId id, const Util::String& name)
{
	VkDebugUtilsObjectNameInfoEXT info =
	{
		VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		nullptr,
		VK_OBJECT_TYPE_BUFFER,
		(uint64_t)Vulkan::ShaderRWBufferGetVkBuffer(id),
		name.AsCharPtr()
	};
	VkDevice dev = GetCurrentDevice();
	n_assert(VkDebugObjectName(dev, &info) == VK_SUCCESS);
}

//------------------------------------------------------------------------------
/**
*/
template<>
void
ObjectSetName(const CoreGraphics::ResourceTableLayoutId id, const Util::String& name)
{
	VkDebugUtilsObjectNameInfoEXT info =
	{
		VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		nullptr,
		VK_OBJECT_TYPE_IMAGE,
		(uint64_t)Vulkan::ResourceTableLayoutGetVk(id),
		name.AsCharPtr()
	};
	VkDevice dev = GetCurrentDevice();
	n_assert(VkDebugObjectName(dev, &info) == VK_SUCCESS);
}

//------------------------------------------------------------------------------
/**
*/
template<>
void
ObjectSetName(const CoreGraphics::ResourcePipelineId id, const Util::String& name)
{
	VkDebugUtilsObjectNameInfoEXT info =
	{
		VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		nullptr,
		VK_OBJECT_TYPE_IMAGE,
		(uint64_t)Vulkan::ResourcePipelineGetVk(id),
		name.AsCharPtr()
	};
	VkDevice dev = GetCurrentDevice();
	n_assert(VkDebugObjectName(dev, &info) == VK_SUCCESS);
}

//------------------------------------------------------------------------------
/**
*/
template<>
void
ObjectSetName(const CoreGraphics::CmdBufferId id, const Util::String& name)
{
	VkDebugUtilsObjectNameInfoEXT info =
	{
		VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		nullptr,
		VK_OBJECT_TYPE_COMMAND_BUFFER,
		(uint64_t)Vulkan::CommandBufferGetVk(id),
		name.AsCharPtr()
	};
	VkDevice dev = GetCurrentDevice();
	n_assert(VkDebugObjectName(dev, &info) == VK_SUCCESS);
}

//------------------------------------------------------------------------------
/**
*/
template<>
void
ObjectSetName(const VkShaderModule id, const Util::String& name)
{
	VkDebugUtilsObjectNameInfoEXT info =
	{
		VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		nullptr,
		VK_OBJECT_TYPE_SHADER_MODULE,
		(uint64_t)id,
		name.AsCharPtr()
	};
	VkDevice dev = GetCurrentDevice();
	n_assert(VkDebugObjectName(dev, &info) == VK_SUCCESS);
}

//------------------------------------------------------------------------------
/**
*/
void 
QueueBeginMarker(const CoreGraphicsQueueType queue, const Math::float4& color, const Util::String& name)
{
	VkQueue vkqueue = state.subcontextHandler.GetQueue(queue);
	alignas(16) float col[4];
	color.store(col);
	VkDebugUtilsLabelEXT info =
	{
		VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
		nullptr,
		name.AsCharPtr(),
		{ col[0], col[1], col[2], col[3] }
	};
	VkQueueBeginLabel(vkqueue, &info);
}

//------------------------------------------------------------------------------
/**
*/
void 
QueueEndMarker(const CoreGraphicsQueueType queue)
{
	VkQueue vkqueue = state.subcontextHandler.GetQueue(queue);
	VkQueueEndLabel(vkqueue);
}

//------------------------------------------------------------------------------
/**
*/
void 
QueueInsertMarker(const CoreGraphicsQueueType queue, const Math::float4& color, const Util::String& name)
{
	VkQueue vkqueue = state.subcontextHandler.GetQueue(queue);
	alignas(16) float col[4];
	color.store(col);
	VkDebugUtilsLabelEXT info =
	{
		VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
		nullptr,
		name.AsCharPtr(),
		{ col[0], col[1], col[2], col[3] }
	};
	VkQueueInsertLabel(vkqueue, &info);
}

//------------------------------------------------------------------------------
/**
*/
void 
CmdBufBeginMarker(const CoreGraphicsQueueType queue, const Math::float4& color, const Util::String& name)
{
	VkCommandBuffer buf;
	switch (queue)
	{
	case GraphicsQueueType: buf = CommandBufferGetVk(state.mainCmdDrawBuffer); break;
	case ComputeQueueType: buf = CommandBufferGetVk(state.mainCmdComputeBuffer); break;
	case TransferQueueType: buf = CommandBufferGetVk(state.mainCmdTransferBuffer); break;
	case SparseQueueType: buf = CommandBufferGetVk(state.mainCmdSparseBuffer); break;
	}
	alignas(16) float col[4];
	color.store(col);
	VkDebugUtilsLabelEXT info =
	{
		VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
		nullptr,
		name.AsCharPtr(),
		{ col[0], col[1], col[2], col[3] }
	};
	VkCmdDebugMarkerBegin(buf, &info);
}

//------------------------------------------------------------------------------
/**
*/
void 
CmdBufEndMarker(const CoreGraphicsQueueType queue)
{
	VkCommandBuffer buf;
	switch (queue)
	{
	case GraphicsQueueType: buf = CommandBufferGetVk(state.mainCmdDrawBuffer); break;
	case ComputeQueueType: buf = CommandBufferGetVk(state.mainCmdComputeBuffer); break;
	case TransferQueueType: buf = CommandBufferGetVk(state.mainCmdTransferBuffer); break;
	case SparseQueueType: buf = CommandBufferGetVk(state.mainCmdSparseBuffer); break;
	}
	VkCmdDebugMarkerEnd(buf);
}

//------------------------------------------------------------------------------
/**
*/
void 
CmdBufInsertMarker(const CoreGraphicsQueueType queue, const Math::float4& color, const Util::String& name)
{
	VkCommandBuffer buf;
	switch (queue)
	{
	case GraphicsQueueType: buf = CommandBufferGetVk(state.mainCmdDrawBuffer); break;
	case ComputeQueueType: buf = CommandBufferGetVk(state.mainCmdComputeBuffer); break;
	case TransferQueueType: buf = CommandBufferGetVk(state.mainCmdTransferBuffer); break;
	case SparseQueueType: buf = CommandBufferGetVk(state.mainCmdSparseBuffer); break;
	}
	alignas(16) float col[4];
	color.store(col);
	VkDebugUtilsLabelEXT info =
	{
		VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
		nullptr,
		name.AsCharPtr(),
		{ col[0], col[1], col[2], col[3] }
	};
	VkCmdDebugMarkerInsert(buf, &info);
}
#endif

} // namespace CoreGraphics
