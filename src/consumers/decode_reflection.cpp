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
      {"reflection/shapeholders.bfbs", "shapeholders.bin"},
      {"reflection/telemetry.bfbs", "telemetry.bin"},
      {"reflection/people.bfbs", "people.bin"},
      {"reflection/devices.bfbs", "devices.bin"}
    };
  

  int exit_code = 0;
  for (auto &pr : pairs) {
       exit_code |= DecodeAndPrint(pr.first, pr.second);
    
  }
  return exit_code;
}
