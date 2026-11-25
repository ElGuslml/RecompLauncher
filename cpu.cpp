// RecompLauncher - see cpu.h.

#include "cpu.h"

#include <intrin.h>

#include "game.h"
#include "launcher_log.h"
#include "settings.h"

namespace launcher {

bool CpuHasX86_64V3() {
  int r[4];
  __cpuid(r, 0);
  if (r[0] < 7) return false;
  __cpuid(r, 1);
  // FMA, MOVBE, OSXSAVE, AVX, F16C.
  const unsigned need1 = (1u << 12) | (1u << 22) | (1u << 27) | (1u << 28) | (1u << 29);
  if ((unsigned(r[2]) & need1) != need1) return false;
  // The system saves the YMM registers.
  if ((_xgetbv(0) & 6) != 6) return false;
  __cpuidex(r, 7, 0);
  // BMI1, AVX2, BMI2.
  const unsigned need7 = (1u << 3) | (1u << 5) | (1u << 8);
  if ((unsigned(r[1]) & need7) != need7) return false;
  __cpuid(r, int(0x80000001));
  // LZCNT.
  return (unsigned(r[2]) & (1u << 5)) != 0;
}

std::filesystem::path GameExecutableToRun(const std::filesystem::path& exe_dir, const Options& options) {
  const std::filesystem::path compatible = exe_dir / game::kRecompExecutable;
  if (!game::kRecompExecutableAvx2) return compatible;
  const std::filesystem::path avx2 = exe_dir / game::kRecompExecutableAvx2;
  std::error_code error;
  const char* why = nullptr;
  if (!options.avx2) {
    why = "turned off (launcher_avx2 = false)";
  } else if (!CpuHasX86_64V3()) {
    why = "this processor does not run it";
  } else if (!std::filesystem::exists(avx2, error)) {
    why = "not beside the launcher";
  }
  static const char* logged = "";
  const char* choice = why ? why : "this processor runs it";
  if (choice != logged) {
    logged = choice;
    Log("the AVX2 build (%s): %s", game::kRecompExecutableAvx2, why ? why : "chosen, this processor runs it");
  }
  return why ? compatible : avx2;
}

}  // namespace launcher
