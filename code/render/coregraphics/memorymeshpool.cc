//------------------------------------------------------------------------------
//  memorymeshloader.cc
//  (C) 2007 Radon Labs GmbH
//  (C) 2013-2016 Individual contributors, see AUTHORS file
//------------------------------------------------------------------------------
#include "stdneb.h"
#include "coregraphics/memorymeshpool.h"
#include "coregraphics/mesh.h"
#include "coregraphics/legacy/nvx2streamreader.h"
#include "coregraphics/renderdevice.h"
#include "coregraphics/config.h"
namespace CoreGraphics
{
__ImplementClass(CoreGraphics::MemoryMeshPool, 'DMMP', Resources::ResourceMemoryPool);

using namespace Resources;

//------------------------------------------------------------------------------
/**
*/
MemoryMeshPool::MemoryMeshPool()
{
    // empty
}

//------------------------------------------------------------------------------
/**
*/
MemoryMeshPool::~MemoryMeshPool()
{
	// empty
}

//------------------------------------------------------------------------------
/**
*/
ResourcePool::LoadStatus
MemoryMeshPool::LoadFromMemory(const Ids::Id24 id, const void* info)
{
	MeshCreateInfo* data = (MeshCreateInfo*)info;
	MeshCreateInfo& mesh = this->Get<0>(id);
	mesh = *data;

	this->states[id] = Resource::Loaded;

	return ResourcePool::Success;
}

//------------------------------------------------------------------------------
/**
*/
void
MemoryMeshPool::Unload(const Ids::Id24 id)
{
}

//------------------------------------------------------------------------------
/**
*/
void
MemoryMeshPool::BindMesh(const MeshId id, const IndexT prim)
{
	RenderDevice* renderDevice = RenderDevice::Instance();
#if _DEBUG
	n_assert(id.id8 == MeshIdType);
#endif
	MeshCreateInfo& inf = this->allocator.Get<0>(id.id24);
	VertexBufferBind(inf.vertexBuffer, 0, inf.primitiveGroups[prim].GetBaseVertex());
	if (inf.indexBuffer != Ids::InvalidId64)
		IndexBufferBind(inf.indexBuffer, inf.primitiveGroups[prim].GetBaseIndex());
}

//------------------------------------------------------------------------------
/**
*/
const SizeT
MemoryMeshPool::GetPrimitiveGroups(const MeshId id)
{
	MeshCreateInfo& inf = this->allocator.Get<0>(id.id24);
	return inf.primitiveGroups.Size();
}

} // namespace CoreGraphics
