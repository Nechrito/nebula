//------------------------------------------------------------------------------
// frameplugin.cc
// (C) 2016-2018 Individual contributors, see AUTHORS file
//------------------------------------------------------------------------------
#include "render/stdneb.h"
#include "frameplugin.h"

namespace Frame
{
Util::Dictionary<Util::StringAtom, std::function<void(IndexT)>> FramePlugin::nameToFunction;

//------------------------------------------------------------------------------
/**
*/
FramePlugin::FramePlugin()
{
	// empty
}

//------------------------------------------------------------------------------
/**
*/
FramePlugin::~FramePlugin()
{
	// empty
}

//------------------------------------------------------------------------------
/**
*/
void
FramePlugin::Setup()
{
	// empty
}

//------------------------------------------------------------------------------
/**
*/
void
FramePlugin::Discard()
{
	// override in subclass for any special discard behavior
}

//------------------------------------------------------------------------------
/**
*/
const std::function<void(IndexT)>&
FramePlugin::GetCallback(const Util::StringAtom& str)
{
	return nameToFunction[str];
}

//------------------------------------------------------------------------------
/**
*/
void 
FramePlugin::AddCallback(const Util::StringAtom name, std::function<void(IndexT)> func)
{
	nameToFunction.Add(name, func);
}

//------------------------------------------------------------------------------
/**
*/
void
FramePlugin::Resize()
{
	// override in subclass
}

} // namespace Frame