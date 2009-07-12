////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2009 Laurent Gomila (laurent.gom@gmail.com)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Window/Linux/ContextGLX.hpp>
#include <SFML/Window/WindowImpl.hpp>
#include <SFML/OpenGL.hpp>
#include <SFML/Window/glext/glxext.h>
#include <iostream>


namespace sf
{
namespace priv
{
////////////////////////////////////////////////////////////
/// Create a new context, not associated to a window
////////////////////////////////////////////////////////////
ContextGLX::ContextGLX(ContextGLX* shared) :
myWindow    (0),
myContext   (NULL),
myOwnsWindow(true)
{
    // Create a dummy window (disabled and hidden)
    int screen = DefaultScreen(myDisplay.GetDisplay());
    myWindow = XCreateWindow(myDisplay.GetDisplay(),
                             RootWindow(myDisplay.GetDisplay(), screen),
                             0, 0,
                             1, 1,
                             0,
                             DefaultDepth(myDisplay.GetDisplay(), screen),
                             InputOutput,
                             DefaultVisual(myDisplay.GetDisplay(), screen),
                             0, NULL);

    // Create the context
    CreateContext(shared, VideoMode::GetDesktopMode().BitsPerPixel, ContextSettings(0, 0, 0));

    // Activate the context
    if (shared)
        SetActive(true);
}


////////////////////////////////////////////////////////////
/// Create a new context attached to a window
////////////////////////////////////////////////////////////
ContextGLX::ContextGLX(ContextGLX* shared, const WindowImpl* owner, unsigned int bitsPerPixel, const ContextSettings& settings) :
myWindow    (0),
myContext   (NULL),
myOwnsWindow(false)
{
    // Get the owner window and its device context
    myWindow = static_cast<Window>(owner->GetHandle());

    // Create the context
    if (myWindow)
        CreateContext(shared, bitsPerPixel, settings);

    // Activate the context
    if (shared)
        SetActive(true);
}


////////////////////////////////////////////////////////////
/// Destructor
////////////////////////////////////////////////////////////
ContextGLX::~ContextGLX()
{
    // Destroy the context
    if (myContext)
    {
        if (glXGetCurrentContext() == myContext)
            glXMakeCurrent(myDisplay.GetDisplay(), None, NULL);
        glXDestroyContext(myDisplay.GetDisplay(), myContext);
    }
    
    // Destroy the window if we own it
    if (myWindow && myOwnsWindow)
    {
        XDestroyWindow(myDisplay.GetDisplay(), myWindow);
        XFlush(myDisplay.GetDisplay());
    }
}


////////////////////////////////////////////////////////////
/// \see Context::MakeCurrent
////////////////////////////////////////////////////////////
bool ContextGLX::MakeCurrent(bool active)
{
    if (active)
    {
        if (myContext)
        {
            if (glXGetCurrentContext() != myContext)
                return glXMakeCurrent(myDisplay.GetDisplay(), myWindow, myContext) != 0;
            else
                return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        if (glXGetCurrentContext() == myContext)
            return glXMakeCurrent(myDisplay.GetDisplay(), None, NULL) != 0;
        else
            return true;
    }
}


////////////////////////////////////////////////////////////
/// \see Context::Display
////////////////////////////////////////////////////////////
void ContextGLX::Display()
{
    if (myWindow)
        glXSwapBuffers(myDisplay.GetDisplay(), myWindow);
}


////////////////////////////////////////////////////////////
/// \see Context::UseVerticalSync
////////////////////////////////////////////////////////////
void ContextGLX::UseVerticalSync(bool enabled)
{
    const GLubyte* name = reinterpret_cast<const GLubyte*>("glXSwapIntervalSGI");
    PFNGLXSWAPINTERVALSGIPROC glXSwapIntervalSGI = reinterpret_cast<PFNGLXSWAPINTERVALSGIPROC>(glXGetProcAddress(name));
    if (glXSwapIntervalSGI)
        glXSwapIntervalSGI(enabled ? 1 : 0);
}


////////////////////////////////////////////////////////////
/// Check if a context is active on the current thread
////////////////////////////////////////////////////////////
bool ContextGLX::IsContextActive()
{
    return glXGetCurrentContext() != NULL;
}


////////////////////////////////////////////////////////////
/// Create the context
////////////////////////////////////////////////////////////
void ContextGLX::CreateContext(ContextGLX* shared, unsigned int bitsPerPixel, const ContextSettings& settings)
{
    // Save the creation settings
    mySettings = settings;

    // Get the attributes of the target window
    XWindowAttributes windowAttributes;
    if (XGetWindowAttributes(myDisplay.GetDisplay(), myWindow, &windowAttributes) == 0)
    {
        std::cerr << "Failed to get the window attributes" << std::endl;
        return;
    }

    // Setup the visual infos to match
    XVisualInfo tpl;
    tpl.depth    = windowAttributes.depth;
    tpl.visualid = XVisualIDFromVisual(windowAttributes.visual);
    tpl.screen   = DefaultScreen(myDisplay.GetDisplay());

    // Get all the visuals matching the template
    int nbVisuals = 0;
    XVisualInfo* visuals = XGetVisualInfo(myDisplay.GetDisplay(), VisualDepthMask | VisualIDMask | VisualScreenMask, &tpl, &nbVisuals);
    if (!visuals || (nbVisuals == 0))
    {
        if (visuals)
            XFree(visuals);
        std::cerr << "There is no valid visual for the selected screen" << std::endl;
        return;
    }

    // Find the best visual
    int          bestScore  = 0xFFFF;
    XVisualInfo* bestVisual = NULL;
    while (!bestVisual)
    {
        for (int i = 0; i < nbVisuals; ++i)
        {
            // Get the current visual attributes
            int RGBA, doubleBuffer, red, green, blue, alpha, depth, stencil, multiSampling, samples;
            glXGetConfig(myDisplay.GetDisplay(), &visuals[i], GLX_RGBA,               &RGBA);
            glXGetConfig(myDisplay.GetDisplay(), &visuals[i], GLX_DOUBLEBUFFER,       &doubleBuffer); 
            glXGetConfig(myDisplay.GetDisplay(), &visuals[i], GLX_RED_SIZE,           &red);
            glXGetConfig(myDisplay.GetDisplay(), &visuals[i], GLX_GREEN_SIZE,         &green); 
            glXGetConfig(myDisplay.GetDisplay(), &visuals[i], GLX_BLUE_SIZE,          &blue); 
            glXGetConfig(myDisplay.GetDisplay(), &visuals[i], GLX_ALPHA_SIZE,         &alpha); 
            glXGetConfig(myDisplay.GetDisplay(), &visuals[i], GLX_DEPTH_SIZE,         &depth);        
            glXGetConfig(myDisplay.GetDisplay(), &visuals[i], GLX_STENCIL_SIZE,       &stencil);
            glXGetConfig(myDisplay.GetDisplay(), &visuals[i], GLX_SAMPLE_BUFFERS_ARB, &multiSampling);        
            glXGetConfig(myDisplay.GetDisplay(), &visuals[i], GLX_SAMPLES_ARB,        &samples);

            // First check the mandatory parameters
            if ((RGBA == 0) || (doubleBuffer == 0))
                continue;

            // Evaluate the current configuration
            int color = red + green + blue + alpha;
            int score = EvaluateFormat(bitsPerPixel, mySettings, color, depth, stencil, multiSampling ? samples : 0);

            // Keep it if it's better than the current best
            if (score < bestScore)
            {
                bestScore  = score;
                bestVisual = &visuals[i];
            }
        }

        // If no visual has been found, try a lower level of antialiasing
        if (!bestVisual)
        {
            if (mySettings.AntialiasingLevel > 2)
            {
                std::cerr << "Failed to find a pixel format supporting "
                          << mySettings.AntialiasingLevel << " antialiasing levels ; trying with 2 levels" << std::endl;
                mySettings.AntialiasingLevel = 2;
            }
            else if (mySettings.AntialiasingLevel > 0)
            {
                std::cerr << "Failed to find a pixel format supporting antialiasing ; antialiasing will be disabled" << std::endl;
                mySettings.AntialiasingLevel = 0;
            }
            else
            {
                std::cerr << "Failed to find a suitable pixel format for the window -- cannot create OpenGL context" << std::endl;
                return;
            }
        }
    }

    // Get the context to share display lists with
    GLXContext toShare = shared ? shared->myContext : NULL;

    // Create the context
    myContext = glXCreateContext(myDisplay.GetDisplay(), bestVisual, toShare, true);
    if (!myContext)
    {
        std::cerr << "Failed to create an OpenGL context for this window" << std::endl;
        return;
    }

    // Update the creation settings from the chosen format
    int depth, stencil;
    glXGetConfig(myDisplay.GetDisplay(), bestVisual, GLX_DEPTH_SIZE,   &depth);
    glXGetConfig(myDisplay.GetDisplay(), bestVisual, GLX_STENCIL_SIZE, &stencil);
    mySettings.DepthBits   = static_cast<unsigned int>(depth);
    mySettings.StencilBits = static_cast<unsigned int>(stencil);

    // Change the target window's colormap so that it matches the context's one
    ::Window root = RootWindow(myDisplay.GetDisplay(), DefaultScreen(myDisplay.GetDisplay()));
    Colormap colorMap = XCreateColormap(myDisplay.GetDisplay(), root, bestVisual->visual, AllocNone);
    XSetWindowColormap(myDisplay.GetDisplay(), myWindow, colorMap);

    // Free the temporary visuals array
    XFree(visuals);
}

} // namespace priv

} // namespace sf