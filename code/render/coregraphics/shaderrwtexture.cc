//------------------------------------------------------------------------------
//  shaderrwtexture.cc
//  (C) 2018 Individual contributors, see AUTHORS file
//------------------------------------------------------------------------------
#include "render/stdneb.h"
#include "shaderrwtexture.h"
#include "coregraphics/displaydevice.h"
namespace CoreGraphics
{

//------------------------------------------------------------------------------
/**
*/
ShaderRWTextureInfo
ShaderRWTextureInfoSetupHelper(const ShaderRWTextureCreateInfo& info)
{
	ShaderRWTextureInfo rt;
    rt.registerBindless = info.registerBindless;
	if (info.window)
	{
		rt.isWindow = true;
		rt.window = DisplayDevice::Instance()->GetCurrentWindow();
		const CoreGraphics::DisplayMode mode = CoreGraphics::WindowGetDisplayMode(rt.window);
		rt.width = mode.GetWidth();
		rt.height = mode.GetHeight();
		rt.depth = 1;
		rt.type = CoreGraphics::Texture2D;
		rt.format = mode.GetPixelFormat();
		rt.layers = 1;
		rt.mips = 1;
		rt.relativeSize = true;
		rt.widthScale = rt.heightScale = rt.depthScale = 1.0f;
		rt.name = "__WINDOW__";
	}
	else
	{
		n_assert(info.width > 0 && info.height > 0 && info.depth > 0);
		n_assert(info.type == CoreGraphics::Texture2D || info.type == CoreGraphics::TextureCube || info.type == CoreGraphics::Texture2DArray || info.type == CoreGraphics::TextureCubeArray);

		rt.isWindow = false;
		rt.window = Ids::InvalidId32;
		rt.name = info.name;
		rt.relativeSize = info.relativeSize;
		rt.mips = 1;
		rt.layers = info.type == CoreGraphics::TextureCubeArray ? 6 : 1;
		rt.width = (SizeT)info.width;
		rt.height = (SizeT)info.height;
		rt.depth = (SizeT)info.depth;
		rt.widthScale = 0;
		rt.heightScale = 0;
		rt.depthScale = 0;
		rt.type = info.type;
		rt.format = info.format;

		if (rt.relativeSize)
		{
			CoreGraphics::WindowId wnd = DisplayDevice::Instance()->GetCurrentWindow();
			const CoreGraphics::DisplayMode mode = CoreGraphics::WindowGetDisplayMode(wnd);
			rt.width = SizeT(mode.GetWidth() * info.width);
			rt.height = SizeT(mode.GetHeight() * info.height);
			rt.depth = 1;

			rt.widthScale = info.width;
			rt.heightScale = info.height;
			rt.depthScale = info.depth;
			rt.window = wnd;
		}
	}
	return rt;
}

//------------------------------------------------------------------------------
/**
*/
void
ShaderRWTextureInfoResizeHelper(ShaderRWTextureInfo& rwInfo, const ShaderRWTextureResizeInfo& info)
{
    if (!rwInfo.isWindow)
    {
        n_assert(info.width > 0 && info.height > 0 && info.depth > 0);
        n_assert(rwInfo.type == CoreGraphics::Texture2D || rwInfo.type == CoreGraphics::TextureCube || rwInfo.type == CoreGraphics::Texture2DArray || rwInfo.type == CoreGraphics::TextureCubeArray);

        rwInfo.width = (SizeT)info.width;
        rwInfo.height = (SizeT)info.height;
        rwInfo.depth = (SizeT)info.depth;
        rwInfo.widthScale = info.widthScale;
        rwInfo.heightScale = info.heightScale;
        rwInfo.depthScale = info.depthScale;

        if (rwInfo.relativeSize)
        {
            CoreGraphics::WindowId wnd = DisplayDevice::Instance()->GetCurrentWindow();
            const CoreGraphics::DisplayMode mode = CoreGraphics::WindowGetDisplayMode(wnd);
            rwInfo.width = SizeT(mode.GetWidth() * info.widthScale);
            rwInfo.height = SizeT(mode.GetHeight() * info.widthScale);
            rwInfo.depth = 1;

            rwInfo.widthScale = info.widthScale;
            rwInfo.heightScale = info.heightScale;
            rwInfo.depthScale = info.depthScale;
            rwInfo.window = wnd;
        }
    }
}


} // namespace CoreGraphics
