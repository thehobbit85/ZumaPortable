//
// DDInterface.h - shim for the Zuma port.
//
// DirectDraw's display interface has no counterpart in the SDL2 port; the
// window, the framebuffer and the blitters all live in the platform layer
// (SexyAppFramework/platform/default + graphics/). Nothing constructs a
// DDInterface any more, so this is an opaque forward-declaration-only type.
//
#ifndef __DDINTERFACE_H__
#define __DDINTERFACE_H__

namespace Sexy
{

class DDInterface
{
public:
    virtual ~DDInterface() {}
};

} // namespace Sexy

#endif // __DDINTERFACE_H__
