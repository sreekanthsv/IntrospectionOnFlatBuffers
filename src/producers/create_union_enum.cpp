#include "flatbuffers/flatbuffers.h"
#include "union_enum_generated.h"
#include "shapeholders_generated.h"
#include <fstream>

int main() {
  flatbuffers::FlatBufferBuilder fbb;
  std::vector<flatbuffers::Offset<demo::ShapeHolder>> holders;
  const int count = 3;
  for (int i = 0; i < count; ++i) {
    auto circle = demo::CreateCircle(fbb, 3.14f + i);
    demo::ShapeHolderBuilder shb(fbb);
    shb.add_id(42 + i);
    shb.add_color(i % 2 == 0 ? demo::Color_Red : demo::Color_Green);
    shb.add_shape_type(demo::Shape_Circle);
    shb.add_shape(flatbuffers::Offset<void>(circle.o));
    holders.push_back(shb.Finish());
  }

  auto vec = fbb.CreateVector(holders);
  auto root = demo::CreateShapeHolders(fbb, vec);
  fbb.Finish(root);

  std::ofstream out("shapeholders.bin", std::ios::binary);
  out.write(reinterpret_cast<const char*>(fbb.GetBufferPointer()), fbb.GetSize());
  out.close();
  return 0;
}
