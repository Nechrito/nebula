#pragma once
//------------------------------------------------------------------------------
/**
    @class GLFW::GLFWGraphicsDisplayEventHandler
  
    Handles DisplayEvents that are relevant for the graphics system
        
    (C) 2019 Individual contributors, see AUTHORS file
*/    
#include "coregraphics/displayeventhandler.h"

//------------------------------------------------------------------------------
namespace GLFW
{
class GLFWGraphicsDisplayEventHandler : public CoreGraphics::DisplayEventHandler
{
    __DeclareClass(GLFWGraphicsDisplayEventHandler);
public:
    /// called when an event happens
    virtual bool HandleEvent(const CoreGraphics::DisplayEvent& event);
};

} // namespace GLFW
//------------------------------------------------------------------------------