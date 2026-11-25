// RecompLauncher - which of the game's executables Play starts (launcher 3.36). A game may ship its executable twice:
// the baseline's (x86-64-v2, any processor of the last fifteen years) and one built for x86-64-v3 (AVX2, FMA, BMI1/2,
// MOVBE, LZCNT, F16C: Intel Haswell and AMD Zen 1 or later), faster on the processors that have it
// (game::kRecompExecutableAvx2). The launcher starts the second where the processor runs it, the player has not
// turned it off (launcher_avx2 = false in the options file) and the file is there.

#pragma once

#include <filesystem>

namespace launcher {

struct Options;

// Whether this processor and the system run x86-64-v3 code (the system saving the AVX registers included).
bool CpuHasX86_64V3();

// The executable Play starts, logged once per choice.
std::filesystem::path GameExecutableToRun(const std::filesystem::path& exe_dir, const Options& options);

}  // namespace launcher
