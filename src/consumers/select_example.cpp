#include <iostream>
#include <vector>
#include <string>
#include "reflection/reflection_printer.h"

int main() {
  // Demo 1: people.bin (root table People { persons: [Person] })
  {
    std::vector<uint8_t> out_buf;
    std::vector<uint8_t> out_bfbs;
    bool ok = SelectColumnsForFlatbuffer("reflection/people.bfbs", "people.bin",
                                         std::string("persons"),
                                         std::vector<std::string>{"name", "age"}, out_buf, out_bfbs);
    if (!ok) { std::cerr << "Select failed for people\n"; }
    else {
      std::cout << "--- People select ---\n";
      DecodeAndPrintFromBuffers(std::string(reinterpret_cast<const char*>(out_bfbs.data()), out_bfbs.size()), out_buf);
    }
  }

  // Demo 2: shapeholders.bin (root table ShapeHolders { holders: [ShapeHolder] })
  {
    std::vector<uint8_t> out_buf;
    std::vector<uint8_t> out_bfbs;
    bool ok = SelectColumnsForFlatbuffer("reflection/shapeholders.bfbs", "shapeholders.bin",
                                         std::string("holders"),
                                         std::vector<std::string>{"id", "color"}, out_buf, out_bfbs);
    if (!ok) { std::cerr << "Select failed for shapeholders\n"; }
    else {
      std::cout << "--- ShapeHolders select ---\n";
      DecodeAndPrintFromBuffers(std::string(reinterpret_cast<const char*>(out_bfbs.data()), out_bfbs.size()), out_buf);
    }
  }

  // Demo 3: telemetry.bin (root Telemetry has a field sensors: [Sensor])
  {
    std::vector<uint8_t> out_buf;
    std::vector<uint8_t> out_bfbs;
    bool ok = SelectColumnsForFlatbuffer("reflection/telemetry.bfbs", "telemetry.bin",
                                         std::string("sensors"),
                                         std::vector<std::string>{"id", "value", "unit"}, out_buf, out_bfbs);
    if (!ok) { std::cerr << "Select failed for telemetry\n"; }
    else {
      std::cout << "--- Telemetry sensors select ---\n";
      DecodeAndPrintFromBuffers(std::string(reinterpret_cast<const char*>(out_bfbs.data()), out_bfbs.size()), out_buf);
    }
  }

  return 0;
}
