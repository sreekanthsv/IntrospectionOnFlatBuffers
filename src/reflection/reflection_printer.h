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


// Select specific column names from a FlatBuffer binary using reflection.
// The caller may optionally provide the name of the top-level vector field to
// select from. If empty, the function falls back to the previous heuristic of
// using the first vector-of-objects field found in the root table.
//
// Column names may be nested paths using '.' as a separator, e.g.
// "address.city" will traverse into an object field `address` and then read
// its `city` string field. Missing intermediate fields or values produce an
// empty string for that cell.
//
// Returns true on success and fills `out_buffer` with a FlatBuffer (see
// `schema/select_result.fbs`) containing the rows. `out_bfbs_buffer` will be
// filled with the generated bfbs bytes for `select_result` so callers can
// introspect the result in-memory.
bool SelectColumnsForFlatbuffer(const std::string &bfbs_path,
                                const std::string &bin_path,
                                const std::string &top_level_vector_field,
                                const std::vector<std::string> &columns,
                                std::vector<uint8_t> &out_buffer,
                                std::vector<uint8_t> &out_bfbs_buffer);

// Decode and print directly from in-memory bfbs (schema bytes) and a flatbuffer binary buffer.
int DecodeAndPrintFromBuffers(const std::string &bfbs_data, const std::vector<uint8_t> &data_buf);
