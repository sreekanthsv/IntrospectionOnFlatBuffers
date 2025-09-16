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
        // Use the helper GetUnionType(schema, parent, unionfield, table) which
        // returns the concrete object type for the current union value.
        try {
          auto &member_obj = flatbuffers::GetUnionType(*schema, *obj, *field, *t);
          auto member_table = flatbuffers::GetFieldT(*t, *field);
          std::cout << "\n";
          if (!member_table) { std::cout << "null\n"; break; }
          PrintTable(schema, &member_obj, member_table, indent+1);
        } catch (...) {
          // GetUnionType asserts on error; fall back to heuristic behavior
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
