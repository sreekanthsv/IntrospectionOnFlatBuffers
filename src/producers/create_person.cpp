#include <flatbuffers/flatbuffers.h>
#include "person_generated.h"
#include <fstream>

int main() {
  // Produce several person instances into separate binaries to demonstrate multiple objects.
  for (int i = 0; i < 3; ++i) {
    flatbuffers::FlatBufferBuilder builder;
    std::string name_str = std::string("Person_") + std::to_string(i);
    std::string street_str = std::string("") + std::to_string(100 + i) + std::string(" Main St");
    std::string city_str = i % 2 == 0 ? "Metropolis" : "Smallville";

    auto name = builder.CreateString(name_str);
    auto street = builder.CreateString(street_str);
    auto city = builder.CreateString(city_str);

    auto addr = example::CreateAddress(builder, street, city, static_cast<uint16_t>(12345 + i));

    example::PersonBuilder pb(builder);
    pb.add_id(1001ULL + i);
    pb.add_name(name);
    pb.add_age(static_cast<uint16_t>(20 + i));
    pb.add_address(addr);
    auto root = pb.Finish();
    builder.Finish(root);

    std::string fname = std::string("person_") + std::to_string(i) + ".bin";
    std::ofstream out(fname, std::ios::binary);
    out.write(reinterpret_cast<const char*>(builder.GetBufferPointer()), builder.GetSize());
    out.close();
  }
  return 0;
}
