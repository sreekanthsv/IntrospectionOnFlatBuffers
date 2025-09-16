#include "reflection_printer.h"
#include <iostream>
#include <unordered_map>
#include "flatbuffers/util.h"
#include "flatbuffers/idl.h"

// Per-field enum cache: map field pointer -> map(value -> name)
static std::unordered_map<const reflection::Field*, std::unordered_map<int64_t, const char*>> g_field_enum_cache;

// Build per-field enum mapping on first access. Returns nullptr if none.
static const char *FindEnumNameForField(const reflection::Schema *schema, const reflection::Field *field, int64_t value) {
  if (!schema || !field) return nullptr;
  auto fit = g_field_enum_cache.find(field);
  if (fit != g_field_enum_cache.end()) {
    auto &mapref = fit->second;
    auto mit = mapref.find(value);
    if (mit != mapref.end()) return mit->second;
    return nullptr;
  }

  // Build the enum mapping for this field by inspecting the type index if present.
  std::unordered_map<int64_t, const char*> mapref;
  // If the field's type index points into the schema enums, use that enum only.
  if (field->type()) {
    int idx = field->type()->index();
    if (idx >= 0 && schema->enums() && idx < schema->enums()->size()) {
      auto e = schema->enums()->Get(idx);
      if (e && e->values()) {
        for (auto vit = e->values()->begin(); vit != e->values()->end(); ++vit) {
          auto vv = *vit;
          if (!vv) continue;
          mapref[vv->value()] = vv->name()->c_str();
        }
      }
    }
  }

  // If the map remains empty, fall back to scanning all enums (compatibility).
  if (mapref.empty() && schema && schema->enums()) {
    for (auto eit = schema->enums()->begin(); eit != schema->enums()->end(); ++eit) {
      auto e = *eit;
      if (!e || !e->values()) continue;
      for (auto vit = e->values()->begin(); vit != e->values()->end(); ++vit) {
        auto vv = *vit;
        if (!vv) continue;
        mapref[vv->value()] = vv->name()->c_str();
      }
    }
  }

  // Install into global cache and look up the requested value.
  auto &entry = g_field_enum_cache[field] = std::move(mapref);
  auto mit = entry.find(value);
  if (mit != entry.end()) return mit->second;
  return nullptr;
}

void PrintTable(const reflection::Schema *schema,
                const reflection::Object *obj,
                const flatbuffers::Table *t,
                int indent) {
  if (!schema || !obj || !t) return;
  for (auto it = obj->fields()->begin(); it != obj->fields()->end(); ++it) {
    auto field = *it;
    for (int i = 0; i < indent; ++i) std::cout << "  ";
    std::cout << field->name()->c_str() << " : ";

    auto btype = field->type()->base_type();
    switch (btype) {
      case reflection::Union: {
        // Better union printing: try to find sibling discriminator (name_type or nameType),
        // resolve its enum name, then print concrete object type name before nested contents.
        std::string disc_name1 = std::string(field->name()->c_str()) + "_type";
        std::string disc_name2 = std::string(field->name()->c_str()) + "Type";
        int64_t disc_val = -1;
        const reflection::Field *disc_field = nullptr;
        for (auto fit = obj->fields()->begin(); fit != obj->fields()->end(); ++fit) {
          auto sibling = *fit;
          if (!sibling || !sibling->name()) continue;
          std::string sname = sibling->name()->str();
          if (sname == disc_name1 || sname == disc_name2) {
            disc_val = flatbuffers::GetAnyFieldI(*t, *sibling);
            disc_field = sibling;
            break;
          }
        }

        if (disc_val >= 0 && disc_field) {
          const char *ename = FindEnumNameForField(schema, disc_field, disc_val);
          if (ename) std::cout << "(" << disc_field->name()->c_str() << ": " << ename << ")\n";
          else std::cout << "(" << disc_field->name()->c_str() << ": " << disc_val << ")\n";
        }

        // Use the helper GetUnionType where possible to obtain the concrete object schema
        try {
          auto &member_obj = flatbuffers::GetUnionType(*schema, *obj, *field, *t);
          auto member_table = flatbuffers::GetFieldT(*t, *field);
          if (!member_table) { for (int i=0;i<indent;i++) std::cout<<"  "; std::cout << "null\n"; break; }
          // print concrete object type name if available
          if (member_obj.name() && member_obj.name()->c_str()) {
            for (int i=0;i<indent;i++) std::cout<<"  ";
            std::cout << "(concrete: " << member_obj.name()->c_str() << ")\n";
          }
          PrintTable(schema, &member_obj, member_table, indent+1);
        } catch (...) {
          for (int i=0;i<indent;i++) std::cout<<"  ";
          std::cout << "<union (unresolved)>\n";
        }
        break;
      }
      case reflection::Bool:
      case reflection::Byte:
      case reflection::UByte:
      case reflection::Short:
      case reflection::UShort:
      case reflection::Int:
      case reflection::UInt:
      case reflection::Long:
      case reflection::ULong: {
  int64_t v = flatbuffers::GetAnyFieldI(*t, *field);
  const char *ename = FindEnumNameForField(schema, field, v);
  if (ename) std::cout << v << " (" << ename << ")\n";
  else std::cout << v << "\n";
        break;
      }
      case reflection::Float:
      case reflection::Double: {
        double v = flatbuffers::GetAnyFieldF(*t, *field);
        std::cout << v << "\n";
        break;
      }
      case reflection::String: {
        auto s = flatbuffers::GetFieldS(*t, *field);
        if (s) std::cout << '"' << s->str() << '"' << "\n";
        else std::cout << "null\n";
        break;
      }
      case reflection::Vector: {
        auto vec_any = flatbuffers::GetFieldAnyV(*t, *field);
        if (!vec_any) { std::cout << "[]\n"; break; }
        auto elem_type = field->type()->element();
        std::cout << "[\n";
        size_t len = vec_any->size();
        for (size_t i = 0; i < len; ++i) {
          for (int j = 0; j < indent+1; ++j) std::cout << "  ";
          auto ebt = static_cast<reflection::BaseType>(elem_type);
          if (flatbuffers::IsScalar(ebt)) {
            if (flatbuffers::IsInteger(ebt)) {
              int64_t ev = flatbuffers::GetAnyVectorElemI(vec_any, ebt, i);
              // For vectors, we attempt to use the parent field to resolve enum values
              const char *ename = FindEnumNameForField(schema, field, ev);
              if (ename) std::cout << ev << " (" << ename << ")\n";
              else std::cout << ev << "\n";
            } else if (flatbuffers::IsFloat(ebt)) {
              std::cout << flatbuffers::GetAnyVectorElemF(vec_any, ebt, i) << "\n";
            } else {
              std::cout << flatbuffers::GetAnyVectorElemS(vec_any, ebt, i) << "\n";
            }
          } else if (ebt == reflection::String) {
            std::cout << '"' << flatbuffers::GetAnyVectorElemS(vec_any, ebt, i) << '"' << "\n";
          } else if (ebt == reflection::Obj) {
            auto elem_ptr = flatbuffers::GetAnyVectorElemPointer<const flatbuffers::Table>(vec_any, i);
            const flatbuffers::Table *elem_table = elem_ptr;
            if (elem_table) {
              int type_index = field->type()->index();
              if (type_index >= 0 && schema->objects() && type_index < schema->objects()->size()) {
                auto child_obj = schema->objects()->Get(type_index);
                PrintTable(schema, child_obj, elem_table, indent+2);
              } else {
                std::cout << "  <nested table>\n";
              }
            } else {
              std::cout << "null\n";
            }
          } else {
            std::cout << "<unsupported vector elem>\n";
          }
        }
        for (int j = 0; j < indent; ++j) std::cout << "  ";
        std::cout << "]\n";
        break;
      }
      case reflection::Obj: {
        // Support unions: many schemas present a sibling "<name>_type" discriminator.
        auto sub = flatbuffers::GetFieldT(*t, *field);
        std::cout << "\n";
        if (!sub) { std::cout << "null\n"; break; }

        // Try to find a discriminator field (heuristic): name + "_type" or name + "Type"
        const char *disc_name1 = (std::string(field->name()->c_str()) + "_type").c_str();
        const char *disc_name2 = (std::string(field->name()->c_str()) + "Type").c_str();
        int64_t disc_val = -1;
        for (auto fit = obj->fields()->begin(); fit != obj->fields()->end(); ++fit) {
          auto sibling = *fit;
          if (!sibling || !sibling->name()) continue;
          const char *sname = sibling->name()->c_str();
          if (strcmp(sname, disc_name1) == 0 || strcmp(sname, disc_name2) == 0) {
            // get integer discriminator
            disc_val = flatbuffers::GetAnyFieldI(*t, *sibling);
            break;
          }
        }

        if (disc_val >= 0) {
          // Use the sibling field (if found) to resolve enum names via per-field cache
          const char *ename = nullptr;
          // find the sibling field descriptor
          for (auto fit2 = obj->fields()->begin(); fit2 != obj->fields()->end(); ++fit2) {
            auto sibling = *fit2;
            if (!sibling || !sibling->name()) continue;
            if (strcmp(sibling->name()->c_str(), disc_name1) == 0 || strcmp(sibling->name()->c_str(), disc_name2) == 0) {
              ename = FindEnumNameForField(schema, sibling, disc_val);
              break;
            }
          }
          if (ename) std::cout << "(union type: " << ename << ")\n";
          else std::cout << "(union type: " << disc_val << ")\n";
        }

        int type_index = field->type()->index();
        if (type_index >= 0 && schema->objects() && type_index < schema->objects()->size()) {
          auto child_obj = schema->objects()->Get(type_index);
          PrintTable(schema, child_obj, sub, indent+1);
        } else {
          // If we cannot find the child object metadata, still attempt to print as nested table
          PrintTable(schema, /*obj=*/nullptr, sub, indent+1);
        }
        break;
      }
      default:
        std::cout << "<unsupported type>\n";
    }
  }
}

int DecodeAndPrint(const std::string &bfbs_path, const std::string &bin_path) {
  std::string bfbs_data;
  if (!flatbuffers::LoadFile(bfbs_path.c_str(), true, &bfbs_data)) {
    std::cerr << "Failed to load bfbs file: " << bfbs_path << "\n";
    return 1;
  }
  auto schema = reflection::GetSchema(bfbs_data.c_str());
  if (!schema) {
    std::cerr << "Failed to parse bfbs schema" << std::endl;
    return 1;
  }

  std::string data;
  if (!flatbuffers::LoadFile(bin_path.c_str(), true, &data)) {
    std::cerr << "Failed to load data file: " << bin_path << "\n";
    // Non-fatal: allow decoder to continue with other files
    return 0;
  }

  flatbuffers::Parser parser;
  if (!parser.Deserialize(schema)) {
    std::cerr << "Failed to deserialize bfbs into Parser" << std::endl;
    return 1;
  }

  const uint8_t *buf = reinterpret_cast<const uint8_t *>(data.c_str());
  auto table = flatbuffers::GetAnyRoot(buf);
  auto root_obj = schema->root_table();
  std::cout << "--- Decoding: " << bfbs_path << " + " << bin_path << " ---\n";
  PrintTable(schema, root_obj, table, 0);
  std::cout << std::endl;
  return 0;
}

// Helper: find a field by name in an object descriptor
static const reflection::Field *FindFieldByName(const reflection::Object *obj, const std::string &name) {
  if (!obj || !obj->fields()) return nullptr;
  for (auto it = obj->fields()->begin(); it != obj->fields()->end(); ++it) {
    auto f = *it;
    if (!f || !f->name()) continue;
    if (f->name()->str() == name) return f;
  }
  return nullptr;
}

#include "select_result_generated.h"

// Helper: split a path like "address.city" into components
static std::vector<std::string> SplitPath(const std::string &path) {
  std::vector<std::string> parts;
  size_t pos = 0, next;
  while ((next = path.find('.', pos)) != std::string::npos) {
    parts.push_back(path.substr(pos, next - pos));
    pos = next + 1;
  }
  if (pos < path.size()) parts.push_back(path.substr(pos));
  return parts;
}

// Resolve a field descriptor by name within an object (non-nested)
static const reflection::Field *ResolveField(const reflection::Object *obj, const std::string &name) {
  return FindFieldByName(obj, name);
}

// Walk a nested path on a table and return the final Table* and Field* for the last segment
// If the final target is scalar/string, return the Field* and the table containing it via out_table.
static const reflection::Field *ResolveNestedField(const reflection::Schema *schema,
                                                  const reflection::Object *root_obj,
                                                  const flatbuffers::Table *root_table,
                                                  const std::vector<std::string> &path,
                                                  const flatbuffers::Table *&out_table) {
  out_table = root_table;
  const reflection::Object *cur_obj = root_obj;
  const flatbuffers::Table *cur_table = root_table;
  const reflection::Field *last_field = nullptr;

  for (size_t i = 0; i < path.size(); ++i) {
    if (!cur_obj || !cur_obj->fields() || !cur_table) return nullptr;
    const std::string &seg = path[i];
    auto f = FindFieldByName(cur_obj, seg);
    if (!f) return nullptr;
    last_field = f;
    auto btype = f->type()->base_type();
    if (i + 1 == path.size()) {
      // last segment
      out_table = cur_table;
      return last_field;
    }
    // intermediate segment: must be Obj
    if (btype != reflection::Obj) return nullptr;
    // get nested table and object metadata
    auto nested_table = flatbuffers::GetFieldT(*cur_table, *f);
    if (!nested_table) return nullptr;
    int type_index = f->type()->index();
    if (type_index < 0 || !schema->objects() || type_index >= schema->objects()->size()) return nullptr;
    cur_obj = schema->objects()->Get(type_index);
    cur_table = nested_table;
  }
  return nullptr;
}

bool SelectColumnsForFlatbuffer(const std::string &bfbs_path,
                                const std::string &bin_path,
                                const std::string &top_level_vector_field,
                                const std::vector<std::string> &columns,
                                std::vector<uint8_t> &out_buffer,
                                std::vector<uint8_t> &out_bfbs_buffer) {
  out_buffer.clear();
  out_bfbs_buffer.clear();
  std::string bfbs_data;
  if (!flatbuffers::LoadFile(bfbs_path.c_str(), true, &bfbs_data)) {
    std::cerr << "SelectColumns: failed to load bfbs: " << bfbs_path << "\n";
    return false;
  }
  auto schema = reflection::GetSchema(bfbs_data.c_str());
  if (!schema) {
    std::cerr << "SelectColumns: failed to parse bfbs schema\n";
    return false;
  }
  std::string data;
  if (!flatbuffers::LoadFile(bin_path.c_str(), true, &data)) {
    std::cerr << "SelectColumns: failed to load bin: " << bin_path << "\n";
    return false;
  }

  const uint8_t *buf = reinterpret_cast<const uint8_t *>(data.c_str());
  auto root_table = flatbuffers::GetAnyRoot(buf);
  auto root_obj = schema->root_table();
  if (!root_obj || !root_obj->fields()) {
    std::cerr << "SelectColumns: root_obj or fields missing\n";
    return false;
  }

  // Locate the vector-of-objects field. If the caller supplied a name, use it;
  // otherwise fall back to the old heuristic of picking the first vector-of-objects.
  const reflection::Field *vec_field = nullptr;
  if (!top_level_vector_field.empty()) {
    vec_field = FindFieldByName(root_obj, top_level_vector_field);
    if (vec_field && !(vec_field->type() && vec_field->type()->base_type() == reflection::Vector && vec_field->type()->element() == reflection::Obj)) {
      std::cerr << "SelectColumns: specified top-level field '" << top_level_vector_field << "' is not a vector-of-objects\n";
      return false;
    }
  }
  if (!vec_field) {
    for (auto it = root_obj->fields()->begin(); it != root_obj->fields()->end(); ++it) {
      auto f = *it;
      if (!f || !f->type()) continue;
      if (f->type()->base_type() == reflection::Vector && f->type()->element() == reflection::Obj) {
        vec_field = f;
        break;
      }
    }
  }
  if (!vec_field) {
    std::cerr << "SelectColumns: no top-level vector-of-objects field found in root\n";
    return false;
  }

  // Obtain the vector<Table> pointer
  auto vec_any = flatbuffers::GetFieldAnyV(*root_table, *vec_field);
  if (!vec_any) {
    std::cerr << "SelectColumns: GetFieldAnyV returned null for vector field\n";
    return false;
  }
  size_t len = vec_any->size();

  // Pre-resolve requested fields in the child object descriptor (the vector's object type)
  int child_type_index = vec_field->type()->index();
  const reflection::Object *child_obj = nullptr;
  if (child_type_index >= 0 && schema->objects() && child_type_index < schema->objects()->size()) {
    child_obj = schema->objects()->Get(child_type_index);
  }
  if (!child_obj) {
    std::cerr << "SelectColumns: failed to resolve child object type for vector elements\n";
    return false;
  }

  // Pre-split column paths (may be nested like "address.city")
  std::vector<std::vector<std::string>> col_paths;
  col_paths.reserve(columns.size());
  for (const auto &c : columns) col_paths.push_back(SplitPath(c));

  // Build a FlatBuffer Result with rows and cols
  flatbuffers::FlatBufferBuilder fbb;
  std::vector<flatbuffers::Offset<selectresult::Row>> rows_off;
  rows_off.reserve(len);

  for (size_t i = 0; i < len; ++i) {
    auto elem_ptr = flatbuffers::GetAnyVectorElemPointer<const flatbuffers::Table>(vec_any, i);
    if (!elem_ptr) {
      // empty row: create an empty vector<Offset<String>>
      auto empty_vec = fbb.CreateVector<flatbuffers::Offset<flatbuffers::String>>({});
      rows_off.push_back(selectresult::CreateRow(fbb, empty_vec));
      continue;
    }
    std::vector<flatbuffers::Offset<flatbuffers::String>> col_strs;
    col_strs.reserve(columns.size());
    for (size_t ci = 0; ci < col_paths.size(); ++ci) {
      const auto &path = col_paths[ci];
      const flatbuffers::Table *value_table = nullptr;
      const reflection::Field *field = ResolveNestedField(schema, child_obj, elem_ptr, path, value_table);
      if (!field || !value_table) { col_strs.push_back(fbb.CreateString(std::string(""))); continue; }
      auto btype = field->type()->base_type();
      switch (btype) {
        case reflection::String: {
          auto s = flatbuffers::GetFieldS(*value_table, *field);
          col_strs.push_back(s ? fbb.CreateString(s->str()) : 0);
          break;
        }
        case reflection::Bool:
        case reflection::Byte:
        case reflection::UByte:
        case reflection::Short:
        case reflection::UShort:
        case reflection::Int:
        case reflection::UInt:
        case reflection::Long:
        case reflection::ULong: {
          int64_t v = flatbuffers::GetAnyFieldI(*value_table, *field);
          const char *ename = FindEnumNameForField(schema, field, v);
          if (ename) col_strs.push_back(fbb.CreateString(std::string(ename)));
          else col_strs.push_back(fbb.CreateString(std::to_string(v)));
          break;
        }
        case reflection::Float:
        case reflection::Double: {
          double dv = flatbuffers::GetAnyFieldF(*value_table, *field);
          col_strs.push_back(fbb.CreateString(std::to_string(dv)));
          break;
        }
        default:
          col_strs.push_back(fbb.CreateString(std::string("")));
      }
    }
    auto vec_off = fbb.CreateVector(col_strs);
    rows_off.push_back(selectresult::CreateRow(fbb, vec_off));
  }

  auto rows_vec = fbb.CreateVector(rows_off);
  auto root = selectresult::CreateResult(fbb, rows_vec);
  fbb.Finish(root);

  // return buffer bytes and the generated bfbs bytes so callers can introspect in-memory
  out_buffer.assign(fbb.GetBufferPointer(), fbb.GetBufferPointer() + fbb.GetSize());
  // load the generated bfbs for the select_result schema into the out buffer
  std::string gen_bfbs;
  if (!flatbuffers::LoadFile("reflection/select_result.bfbs", true, &gen_bfbs)) {
    std::cerr << "SelectColumns: failed to load generated select_result.bfbs\n";
    return false;
  }
  out_bfbs_buffer.assign(reinterpret_cast<const uint8_t*>(gen_bfbs.c_str()), reinterpret_cast<const uint8_t*>(gen_bfbs.c_str()) + gen_bfbs.size());
  return true;
}

int DecodeAndPrintFromBuffers(const std::string &bfbs_data, const std::vector<uint8_t> &data_buf) {
  auto schema = reflection::GetSchema(bfbs_data.c_str());
  if (!schema) {
    std::cerr << "DecodeAndPrintFromBuffers: failed to parse bfbs schema\n";
    return 1;
  }
  const uint8_t *buf = data_buf.empty() ? nullptr : data_buf.data();
  if (!buf) {
    std::cerr << "DecodeAndPrintFromBuffers: data buffer empty\n";
    return 1;
  }
  auto table = flatbuffers::GetAnyRoot(buf);
  auto root_obj = schema->root_table();
  PrintTable(schema, root_obj, table, 0);
  return 0;
}
