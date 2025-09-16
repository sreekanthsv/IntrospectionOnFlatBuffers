#pragma once

#include <string>
#include "flatbuffers/reflection.h"
#include "flatbuffers/flatbuffers.h"

// Print any table using the provided reflection schema/object/table.
void PrintTable(const reflection::Schema *schema,
                const reflection::Object *obj,
                const flatbuffers::Table *t,
                int indent);

// Load a .bfbs and a flatbuffer binary, then print its contents using reflection.
int DecodeAndPrint(const std::string &bfbs_path, const std::string &bin_path);
