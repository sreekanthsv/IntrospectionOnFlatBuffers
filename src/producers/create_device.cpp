#include <flatbuffers/flatbuffers.h>
#include "device_generated.h"
#include <fstream>

int main() {
  // Produce multiple device instances
  for (int i = 0; i < 2; ++i) {
    flatbuffers::FlatBufferBuilder builder;
    std::string id_str = std::string("dev_") + std::to_string(i);
    auto dev_id = builder.CreateString(id_str);
    auto s1 = builder.CreateString("temp");
    auto s2 = builder.CreateString("hum");
    auto r1 = example::CreateReading(builder, s1, 20.0 + i);
    auto r2 = example::CreateReading(builder, s2, 40.0 + i);
    std::vector<flatbuffers::Offset<example::Reading>> readings = {r1, r2};
    auto readings_vec = builder.CreateVector(readings);

    auto tag1 = builder.CreateString("prod");
    auto tag2 = builder.CreateString("edge");
    auto tags = builder.CreateVector(std::vector<flatbuffers::Offset<flatbuffers::String>>{tag1, tag2});

    example::DeviceBuilder db(builder);
    db.add_device_id(dev_id);
    db.add_online(i % 2 == 0);
    db.add_readings(readings_vec);
    db.add_tags(tags);
    auto root = db.Finish();
    builder.Finish(root);

    std::string fname = std::string("device_") + std::to_string(i) + ".bin";
    std::ofstream out(fname, std::ios::binary);
    out.write(reinterpret_cast<const char*>(builder.GetBufferPointer()), builder.GetSize());
    out.close();
  }
  return 0;
}
