//
// D3DInterface.h - shim for the Zuma port.
//
// Direct3D 8's hardware renderer is likewise replaced by the SDL2/OpenGL ES
// path in this port. Opaque type, kept only so ported call sites compile.
//
#ifndef __D3DINTERFACE_H__
#define __D3DINTERFACE_H__

namespace Sexy
{

class D3DInterface
{
public:
    virtual ~D3DInterface() {}
};

} // namespace Sexy

#endif // __D3DINTERFACE_H__
