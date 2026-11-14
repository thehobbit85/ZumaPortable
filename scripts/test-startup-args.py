#!/usr/bin/env python3
"""Check the real desktop entry point and argument parser without SDL or game data."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

# Replace only the graphical lifecycle. The entry point and parser below are compiled
# from their source files, so forgetting either startup call breaks these checks.
LIFECYCLE = r'''
#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
using uint = unsigned int;
namespace Sexy {
class SexyAppBase {
public:
    int mArgc = 0;
    char** mArgv = nullptr;
    bool mCmdLineParsed = false, mPlayingDemoBuffer = false;
    bool mRecordingDemoBuffer = false, mHasCustomDemoFile = false, mIsScreenSaver = false;
    size_t mDemoPlayIndex = 0;
    uint mDemoRecordFileLimit = 0;
    std::string mDemoPrefix, mDemoFileName, mCustomSaveDir;
    std::string mResourceDir = std::getenv("ZUMA_TEST_DEFAULT_RESOURCES");
    void SetArgs(int, char**);
    void DoParseCmdLine();
    void HandleCmdLineParam(std::string_view, std::string_view);
    void Popup(const std::string& message) { std::cerr << message; }
    void DoExit(int code) { std::exit(code); }
    std::string GetString(const char*, const char* fallback) { return fallback; }
    void Init() {
        std::ifstream resource(std::filesystem::path(mResourceDir) / "resource.txt");
        std::string contents;
        std::getline(resource, contents);
        std::cout << contents << '\n' << mCustomSaveDir << '\n';
        std::cout << std::filesystem::current_path().string() << '\n';
    }
    void Start() {}
    void Shutdown() {}
};
class CircleShootApp : public SexyAppBase {};
}
using namespace Sexy;
std::vector<std::string> FindDemoFiles(const std::string&) { return {}; }
std::string GetTimestampedDemoFileName(const std::string&) { return {}; }
'''


class StartupArgs(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="zuma-startup-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.root = Path(cls.temp.name).resolve()
        cls.default = cls.root / "executable resources"
        cls.selected = cls.root / "selected game resources"
        cls.cwd = cls.root / "working directory"
        for folder in (cls.default, cls.selected, cls.cwd):
            folder.mkdir()
        (cls.default / "resource.txt").write_text("default resources")
        (cls.selected / "resource.txt").write_text("selected resources")
        base = (ROOT / "src/SexyAppFramework/SexyAppBase.cpp").read_text()
        parser = base[base.index("static bool ParamTakesValue("):base.index("void SexyAppBase::PreDisplayHook()")]
        main = (ROOT / "src/main.cpp").read_text()
        entry = main[main.index("int main(int argc, char** argv)"):]
        source = cls.root / "startup.cpp"
        source.write_text(LIFECYCLE + parser + entry)
        cls.exe = cls.root / ("startup.exe" if os.name == "nt" else "startup")
        compiler = shlex.split(os.environ.get("CXX", "c++"))
        subprocess.run([*compiler, "-std=c++20", str(source), "-o", str(cls.exe)], check=True)

    def run_startup(self, *args):
        env = dict(os.environ, ZUMA_TEST_DEFAULT_RESOURCES=str(self.default))
        result = subprocess.run([str(self.exe), *args], cwd=self.cwd, env=env,
                                text=True, capture_output=True, check=True)
        contents, save_dir, cwd = result.stdout.splitlines()
        self.assertEqual(cwd, str(self.cwd))
        return contents, save_dir

    def test_resource_directory_with_spaces(self):
        contents, _ = self.run_startup("-resdir", str(self.selected))
        self.assertEqual(contents, "selected resources")

    def test_equals_resource_directory(self):
        contents, _ = self.run_startup(f"-resdir={self.selected}")
        self.assertEqual(contents, "selected resources")

    def test_default_resources_without_arguments(self):
        contents, save_dir = self.run_startup()
        self.assertEqual(contents, "default resources")
        self.assertEqual(save_dir, "")

    def test_save_directory_with_spaces(self):
        folder = str(self.root / "selected saves")
        _, save_dir = self.run_startup("-savedir", folder)
        self.assertEqual(save_dir, folder)


if __name__ == "__main__":
    unittest.main()
