#include <flatbuffers/flatbuffers.h>
#include "people_generated.h"
#include <fstream>

int main() {
  // Produce a single people.bin which contains a vector<Person> (table-like behavior).
  flatbuffers::FlatBufferBuilder builder;
  std::vector<flatbuffers::Offset<example::Person>> persons;
  for (int i = 0; i < 3; ++i) {
    std::string name_str = std::string("Person_") + std::to_string(i);
    std::string street_str = std::to_string(100 + i) + std::string(" Main St");
    std::string city_str = (i % 2 == 0) ? "Metropolis" : "Smallville";

    auto name = builder.CreateString(name_str);
    auto street = builder.CreateString(street_str);
    auto city = builder.CreateString(city_str);

    auto addr = example::CreateAddress(builder, street, city, static_cast<uint16_t>(12345 + i));

    example::PersonBuilder pb(builder);
    pb.add_id(1001ULL + i);
    pb.add_name(name);
    pb.add_age(static_cast<uint16_t>(20 + i));
    pb.add_address(addr);
    persons.push_back(pb.Finish());
  }

  auto persons_vec = builder.CreateVector(persons);
  auto people = example::CreatePeople(builder, persons_vec);
  builder.Finish(people);

  std::ofstream out("people.bin", std::ios::binary);
  out.write(reinterpret_cast<const char*>(builder.GetBufferPointer()), builder.GetSize());
  out.close();
  return 0;
}
