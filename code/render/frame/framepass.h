#pragma once
//------------------------------------------------------------------------------
/**
	A frame pass prepares a rendering sequence, draws and subpasses must reside
	within one of these objects.
	
	(C) 2016 Individual contributors, see AUTHORS file
*/
//------------------------------------------------------------------------------
#include "frameop.h"
#include "framesubpass.h"
#include "coregraphics/pass.h"
namespace Frame
{
class FramePass : public FrameOp
{
public:
	/// constructor
	FramePass();
	/// destructor
	virtual ~FramePass();

	/// add subpass
	void AddSubpass(FrameSubpass* subpass);

	/// get list of subpasses
	const Util::Array<FrameSubpass*>& GetSubpasses() const;

	/// discard operation
	void Discard();
	/// handle display resizing
	void OnWindowResized();

	struct CompiledImpl : public FrameOp::Compiled
	{
		void Run(const IndexT frameIndex);
		void Discard();

		void CrossQueuePreSync() override;
		void CrossQueuePostSync() override;

		Util::Array<FrameOp::Compiled*> subpasses;
		CoreGraphics::PassId pass;
	};

	FrameOp::Compiled* AllocCompiled(Memory::ChunkAllocator<BIG_CHUNK>& allocator);

	CoreGraphics::PassId pass;

private:

	void Build(
		Memory::ChunkAllocator<BIG_CHUNK>& allocator,
		Util::Array<FrameOp::Compiled*>& compiledOps,
		Util::Array<CoreGraphics::EventId>& events,
		Util::Array<CoreGraphics::BarrierId>& barriers,
		Util::Array<CoreGraphics::SemaphoreId>& semaphores,
		Util::Dictionary<CoreGraphics::ShaderRWTextureId, Util::Array<std::tuple<CoreGraphics::ImageSubresourceInfo, TextureDependency>>>& rwTextures,
		Util::Dictionary<CoreGraphics::ShaderRWBufferId, BufferDependency>& rwBuffers,
		Util::Dictionary<CoreGraphics::RenderTextureId, Util::Array<std::tuple<CoreGraphics::ImageSubresourceInfo, TextureDependency>>>& renderTextures) override;

	Util::Array<FrameSubpass*> subpasses;
};

//------------------------------------------------------------------------------
/**
*/
inline const Util::Array<Frame::FrameSubpass*>&
FramePass::GetSubpasses() const
{
	return this->subpasses;
}

} // namespace Frame2