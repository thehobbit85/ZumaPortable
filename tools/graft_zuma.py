#!/usr/bin/env python3
"""
Graft the Zuma Deluxe game code (CircleShootApp 1.0.0 decompilation) onto the
modern SDL2/OpenGL-ES cross-platform SexyAppFramework from PvZ-Portable.

Run:  python tools/graft_zuma.py [--force]
Idempotent: re-running re-copies the game sources and re-applies include fixes.
"""
import os
import re
import shutil
import sys
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent      # .../zuma-portable
SRC = ROOT / "src"
FW = SRC / "SexyAppFramework"
ZUMA_DST = SRC / "Zuma"
HERE = pathlib.Path(__file__).resolve().parent

# Zuma decompilation tree (upstream abduznik/CircleShootApp checkout)
ZUMA_SRC_CANDIDATES = [
    pathlib.Path(r"C:\Users\admin\zuma-port\csa\source\CircleShoot"),
    ROOT.parent / "csa" / "source" / "CircleShoot",
]

# ---------------------------------------------------------------- shims ----
DDIMAGE_H = """//
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
"""

DDINTERFACE_H = """//
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
"""

D3DINTERFACE_H = """//
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
"""

MAIN_CPP = """//
// Zuma Deluxe - Android/desktop entry point.
//
// Upstream (Windows) entry was WinMain in Main.cpp; SDL2main provides WinMain on
// Windows and SDLActivity drives this on Android, so a plain main() is correct
// for every target.
//
#include "Zuma_Prefix.pch"
#include "CircleShootApp.h"

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    Sexy::CircleShootApp* anApp = new Sexy::CircleShootApp();
    anApp->Init();
    anApp->Start();
    anApp->Shutdown();
    delete anApp;

    return 0;
}
"""

# The DirectDraw/D3D backend is gone; the game code's only live DDImage use is
# SpriteMgr.cpp's background buffer, which needs no interface pointer.
SPRITEMGR_PATCH = (
    "    mBackgroundImage = new DDImage(gSexyAppBase->mDDInterface);",
    "    mBackgroundImage = new DDImage((DDInterface*)nullptr);",
)

SHIMS = [
    (FW / "graphics" / "DDImage.h", DDIMAGE_H),
    (FW / "misc" / "DDInterface.h", DDINTERFACE_H),
    (FW / "misc" / "D3DInterface.h", D3DINTERFACE_H),
]

# PvZ game code replaced wholesale by Zuma's.
PVZ_DROPS = [
    "Lawn", "PvzpLib", "LawnApp.cpp", "LawnApp.h",
    "Resources.cpp", "Resources.h", "ConstEnums.h", "GameConstants.h",
]


def find_zuma_src() -> pathlib.Path:
    for c in ZUMA_SRC_CANDIDATES:
        if c.is_dir():
            return c
    sys.exit("ERROR: Zuma decompilation source tree not found")


def framework_header_map() -> dict:
    """basename -> path relative to src/SexyAppFramework (e.g. 'widget/Widget.h')"""
    m = {}
    for r, _, files in os.walk(FW):
        for f in files:
            if f.endswith((".h", ".hpp")):
                rel = os.path.relpath(os.path.join(r, f), FW).replace("\\", "/")
                m.setdefault(f, rel)
    return m


def copy_game_code(src: pathlib.Path) -> int:
    ZUMA_DST.mkdir(parents=True, exist_ok=True)
    n = 0
    for f in sorted(os.listdir(src)):
        if f.endswith((".cpp", ".h", ".pch")):
            shutil.copy2(src / f, ZUMA_DST / f)
            n += 1
    return n


def rewrite_includes(hmap: dict) -> tuple:
    """<SexyAppFramework/Foo.h> -> <SexyAppFramework/<real subdir>/Foo.h>"""
    pat = re.compile(r'#include\s*[<"]SexyAppFramework/([A-Za-z0-9_]+\.h)[>"]')
    unresolved, rewritten = set(), 0
    for f in sorted(ZUMA_DST.iterdir()):
        if f.suffix not in (".cpp", ".h", ".pch"):
            continue
        text = f.read_text(encoding="utf-8", errors="surrogateescape")
        def sub(m):
            nonlocal rewritten
            base = m.group(1)
            loc = hmap.get(base)
            if loc is None:
                unresolved.add(base)
                return m.group(0)
            rewritten += 1
            return f"#include <SexyAppFramework/{loc}>"
        new = pat.sub(sub, text)
        if new != text:
            f.write_text(new, encoding="utf-8", errors="surrogateescape")
    return unresolved, rewritten


def patch_ddimage_use() -> bool:
    p = ZUMA_DST / "SpriteMgr.cpp"
    if not p.exists():
        return False
    t = p.read_text(encoding="utf-8", errors="surrogateescape")
    if SPRITEMGR_PATCH[0] in t:
        p.write_text(t.replace(*SPRITEMGR_PATCH), encoding="utf-8", errors="surrogateescape")
        return True
    return False


def drop_pvz_game_code() -> list:
    dropped = []
    for name in PVZ_DROPS:
        p = SRC / name
        if p.is_dir():
            shutil.rmtree(p); dropped.append(name + "/")
        elif p.exists():
            p.unlink(); dropped.append(name)
    return dropped


def patch_cmake() -> bool:
    cm = ROOT / "CMakeLists.txt"
    t = cm.read_text(encoding="utf-8", errors="surrogateescape")
    if "src/Zuma/*.cpp" in t:
        return False
    for line in ("\tsrc/PvzpLib/*.cpp\n", "\tsrc/Lawn/*.cpp\n",
                 "\tsrc/Lawn/Widget/*.cpp\n", "\tsrc/Lawn/System/*.cpp\n"):
        t = t.replace(line, "")
    t = t.replace("\tsrc/SexyAppFramework/fcaseopen/*.c\n",
                  "\tsrc/SexyAppFramework/fcaseopen/*.c\n\tsrc/Zuma/*.cpp\n")
    cm.write_text(t, encoding="utf-8", errors="surrogateescape")
    return True


def main():
    zsrc = find_zuma_src()
    hmap = framework_header_map()

    for path, content in SHIMS:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
    # shims must be resolvable by the include rewriter
    hmap["DDImage.h"] = "graphics/DDImage.h"
    hmap["DDInterface.h"] = "misc/DDInterface.h"
    hmap["D3DInterface.h"] = "misc/D3DInterface.h"

    dropped = drop_pvz_game_code()
    n = copy_game_code(zsrc)
    unresolved, rewritten = rewrite_includes(hmap)
    (SRC / "main.cpp").write_text(MAIN_CPP, encoding="utf-8")
    dd = patch_ddimage_use()
    cm = patch_cmake()

    print(f"source            : {zsrc}")
    print(f"game files copied : {n} -> src/Zuma/")
    print(f"includes rewritten: {rewritten}")
    print(f"unresolved headers: {sorted(unresolved) or 'none'}")
    print(f"Dropped PvZ code  : {dropped}")
    print(f"DDImage call fixed: {dd}")
    print(f"CMakeLists patched: {cm}")


if __name__ == "__main__":
    main()
