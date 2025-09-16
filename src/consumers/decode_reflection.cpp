#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include "reflection/reflection_printer.h"

namespace fs = std::filesystem;

int main() {
  // Discover all generated .bfbs in the reflection output directory and try to
  // find matching .bin files in the current working directory. This keeps the
  // demo flexible as new schemas are added.
    std::vector<std::pair<std::string, std::string>> pairs = {
      {"reflection/union_enum.bfbs", "union_enum.bin"},
      {"reflection/telemetry.bfbs", "telemetry.bin"},
      {"reflection/person.bfbs", "person.bin"},
      {"reflection/device.bfbs", "device.bin"}
    };
  

  int exit_code = 0;
  for (auto &pr : pairs) {
    // For each bfbs/bin pair, also look for multiple binaries that match the
    // stem prefix (e.g., person_0.bin, person_1.bin). If found, decode each.
    fs::path bfbs_path(pr.first);
    std::string stem = bfbs_path.stem().string();
    bool found_any = false;
    for (auto &entry : fs::directory_iterator(fs::current_path())) {
      if (!entry.is_regular_file()) continue;
      auto ename = entry.path().filename().string();
      if (ename.rfind(stem + "_", 0) == 0 && entry.path().extension() == ".bin") {
        found_any = true;
        exit_code |= DecodeAndPrint(pr.first, entry.path().string());
      }
    }
    if (!found_any) {
      // fallback to the single candidate
      exit_code |= DecodeAndPrint(pr.first, pr.second);
    }
  }
  return exit_code;
}
