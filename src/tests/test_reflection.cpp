#include <cassert>
#include <cstdio>
#include <string>
#include "reflection/reflection_printer.h"

int main() {
  // Use the generated bfbs and sample binary produced by the producer.
  // The build system places generated .bfbs in build/reflection/ and the producers
  // write .bin files in the build directory when executed there. For this unit
  // test we will rely on that behavior.
  int rc = 0;
  rc |= DecodeAndPrint(std::string("reflection/union_enum.bfbs"), std::string("union_enum.bin"));
  // For robustness run the telemetry example too (ensures existing behavior)
  rc |= DecodeAndPrint(std::string("reflection/telemetry.bfbs"), std::string("telemetry.bin"));
  return rc;
}
