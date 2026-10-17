//
// DDImage.h - shim for the Zuma port.
//
// The original SexyAppFramework backed Image with DirectDraw (DDImage, owned by
// SexyAppBase::mDDInterface). This port renders through SDL2, so a DDImage is
// simply a software-backed MemoryImage. The (unused) interface argument is kept
// so the decompiled game code compiles unchanged.
//
#ifndef __DDIMAGE_H__
#define __DDIMAGE_H__

#include "SexyAppFramework/graphics/MemoryImage.h"

namespace Sexy
{

class DDInterface;

class DDImage : public MemoryImage
{
public:
    explicit DDImage(DDInterface* theInterface) : MemoryImage() { (void)theInterface; }
    explicit DDImage(SexyAppBase* theApp) : MemoryImage(theApp) {}
    virtual ~DDImage() {}
};

} // namespace Sexy

#endif // __DDIMAGE_H__
