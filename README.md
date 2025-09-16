
# IntrospectionOnFlatBuffers

This repository demonstrates using FlatBuffers reflection in C++ to decode FlatBuffer binaries and print their contents without requiring generated readers for the decoding side. The project contains example schemas, producers that write sample binaries, and a reflection-based consumer that discovers schemas and binaries and prints them.

## Highlights (what's included)

- Example schemas in `schema/` (telemetry, person, device, union+enum demo).
- Producers in `src/producers/` that write example binaries. Many producers now emit multiple instances (e.g., `person_0.bin`, `person_1.bin`).
- A reflection-based consumer in `src/consumers/decode_reflection.cpp` that discovers generated `.bfbs` (in `reflection/`) and decodes matching `.bin` files (supports matching `stem_*.bin`).
- Reflection traversal helpers in `src/reflection/` (`reflection_printer.h/.cpp`) that recursively print tables, vectors, structs, and try to resolve enum names and union members.
- A basic unit test `src/tests/test_reflection.cpp` that runs the reflection decoder on the example binaries.

## Layout

- `schema/` — FlatBuffers `.fbs` schema files used for examples.
- `src/producers/` — small utilities that build example FlatBuffer binaries (multiple instance output supported).
- `src/consumers/` — consumer programs that decode FlatBuffer binaries using reflection.
- `src/reflection/` — shared reflection helpers for printing.

## Build artifacts

During the build CMake runs `flatc` to generate C++ headers and binary schema (`.bfbs`) into the build tree at `build/reflection/`. The project adds that directory to the include path so generated headers are available to producers and consumers.

## Prerequisites (WSL)

- Debian/Ubuntu-based WSL is recommended.
- Install developer packages and FlatBuffers:

```powershell
# in WSL
sudo apt update
sudo apt install -y build-essential cmake libflatbuffers-dev flatbuffers
```

## Build and run (WSL)

From the repository root in a WSL shell:

```bash
mkdir -p build
cd build
cmake ..
make -j

# Run producers to generate example binaries (producers now emit multiple numbered files)
./create_sample
./create_person    # produces person_0.bin, person_1.bin, ...
./create_device    # produces device_0.bin, device_1.bin, ...
./create_union_enum # produces union_enum_0.bin, union_enum_1.bin, ...

# Run the reflection-based decoder; it will discover .bfbs in reflection/ and decode matching stem_*.bin files
./decode_reflection
```

If you prefer to run a single producer and decode only its output, run that producer then call `./decode_reflection`.

## Runtime behavior

- The consumer loads `.bfbs` files from `reflection/` and deserializes them into a `flatbuffers::Parser` so the reflection helpers can inspect schema metadata.
- For each found schema it searches the current working directory for binaries matching `stem_*.bin` (for example, `person_0.bin`) and decodes each.
- Enum name resolution is implemented with a per-field cache for performance: when the printer encounters an integer field that maps to an enum, it resolves values to names using the schema metadata and caches the mapping per `reflection::Field`.
- Unions are detected and the reflection helpers attempt to resolve and print the selected variant (best-effort; see notes below).

## Sample output (trimmed)

--- Decoding: reflection/person.bfbs + person_0.bin ---
address :
	city : "Metropolis"
	street : "100 Main St"
	zip : 12345
age : 20
id : 1001
name : "Person_0"

--- Decoding: reflection/union_enum.bfbs + union_enum_1.bin ---
color : 1 (Green)
id : 43
shape :
	radius : 4.14


## Additional files

- `.gitignore` is present and ignores the `build/` directory to avoid committing generated artifacts.

## Next improvements (suggested)

- Capture consumer output in unit tests and assert expected lines (convert `test_reflection` into assertive tests).
- Add CLI flags to `decode_reflection` to control search directories, patterns, and verbosity.
- Improve union printing to include both the discriminator name and the concrete object type more clearly.

If you'd like, I can implement any of the improvements above — say which one and I'll proceed.
