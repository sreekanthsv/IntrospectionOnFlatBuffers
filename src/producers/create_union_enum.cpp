#include "flatbuffers/flatbuffers.h"
#include "union_enum_generated.h"

int main() {
  const int count = 3;
  for (int i = 0; i < count; ++i) {
    flatbuffers::FlatBufferBuilder fbb;
    auto circle = demo::CreateCircle(fbb, 3.14f + i);
    // Use builder API to construct the ShapeHolder
    demo::ShapeHolderBuilder shb(fbb);
    shb.add_id(42 + i);
    shb.add_color(i % 2 == 0 ? demo::Color_Red : demo::Color_Green);
    shb.add_shape_type(demo::Shape_Circle);
    shb.add_shape(flatbuffers::Offset<void>(circle.o));
    auto sh = shb.Finish();
    fbb.Finish(sh);

    std::string fname = std::string("union_enum_") + std::to_string(i) + ".bin";
    FILE *f = fopen(fname.c_str(), "wb");
    if (f) {
      fwrite(fbb.GetBufferPointer(), 1, fbb.GetSize(), f);
      fclose(f);
    }
  }
  return 0;
}
