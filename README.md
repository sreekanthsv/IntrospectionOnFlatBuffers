
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

## Results and how to reproduce

Run the producers and decoder in your build directory (WSL):

```bash
cd build
make -j
./create_sample         # writes telemetry.bin
./create_person         # writes people.bin (vector<Person>)
./create_device         # writes devices.bin (vector<Device>)
./create_union_enum     # writes shapeholders.bin (vector<ShapeHolder>)
./decode_reflection --reflect \
	build/reflection/shapeholders.bfbs \
	build/reflection/telemetry.bfbs \
	build/reflection/people.bfbs \
	build/reflection/devices.bfbs
```

Example decoder output (trimmed) observed while running the above commands:

--- Decoding: reflection/shapeholders.bfbs + shapeholders.bin ---
holders : [
	color : 0 (Red)
	id : 42
	shape :
		radius : 3.14
	color : 1 (Green)
	id : 43
	shape :
		radius : 4.14
	...
]

--- Decoding: reflection/telemetry.bfbs + telemetry.bin ---
device_id : "device-123"
sensors : [
	id : "temp"
	unit : "C"
	value : 23.5
	id : "pressure"
	unit : "kPa"
	value : 101.3
]
status : "OK"
timestamp : 1630000000

--- Decoding: reflection/people.bfbs + people.bin ---
persons : [
	name : "Person_0"
	id : 1001
	age : 20
	address : { street/city/zip }
	...
]

--- Decoding: reflection/devices.bfbs + devices.bin ---
devices : [
	device_id : "dev_0"
	online : 1
	readings : [ { sensor/value }, ... ]
	tags : [ "prod", "edge" ]
	...
]

Notes:
- The consumer prints nested tables and vectors by iterating schema metadata via reflection; enum values are resolved to names when possible.
- The union printing is best-effort: the nested object contents are printed but the discriminator formatting can be improved (left as a follow-up).
- If a referenced `.bin` file is missing the decoder will print a warning but continue; ensure you run the associated producer first to generate the example binary.

If you'd like, I can update the README to add a single `make run-all` CMake target that builds and runs all producers before the decoder.


## Additional files

- `.gitignore` is present and ignores the `build/` directory to avoid committing generated artifacts.

## Next improvements (suggested)

- Capture consumer output in unit tests and assert expected lines (convert `test_reflection` into assertive tests).
- Add CLI flags to `decode_reflection` to control search directories, patterns, and verbosity.
- Improve union printing to include both the discriminator name and the concrete object type more clearly.

If you'd like, I can implement any of the improvements above — say which one and I'll proceed.

## Using SelectColumnsForFlatbuffer (quick guide)

The project provides a helper `SelectColumnsForFlatbuffer` in `src/reflection/reflection_printer.h` that extracts specific columns from a vector-of-objects field and returns the results as a FlatBuffer (the schema is `schema/select_result.fbs`).

Key points:
- Provide the `.bfbs` for the source schema (generated by flatc into `build/reflection/`).
- Provide the binary `.bin` to read (the function will load it directly).
- Pass the name of the top-level vector field you want to select from (for example, `persons` in `people.bin` or `holders` in `shapeholders.bin`). If empty, the function will pick the first vector-of-objects it finds in the root.
- Columns may be nested paths (e.g. `address.city`). Only object-valued intermediate segments are supported; the final segment must be a scalar/string/enum.

Example (WSL):

1. Build and generate schemas:

```bash
mkdir -p build && cd build
cmake ..
make -j
```

2. Produce the example binaries (run producers):

```bash
./create_sample       # writes telemetry.bin
./create_person       # writes people.bin (root People { persons: [Person] })
./create_union_enum   # writes shapeholders.bin (root ShapeHolders { holders: [ShapeHolder] })
```

3. Use `select_example` to run the in-repo demonstrations, which call `SelectColumnsForFlatbuffer` and then `DecodeAndPrintFromBuffers` to introspect the result in-memory:

```bash
./select_example
```

This will run three demos and print select results for `people`, `shapeholders`, and `telemetry` (which uses the `sensors` vector inside the `Telemetry` root).

If you need an example of calling the API from your own code, see `src/consumers/select_example.cpp` for a minimal usage pattern.

### Example Select output (actual run)

Below are representative outputs produced by `./select_example` (the helper builds a `select_result` FlatBuffer and then introspects it in-memory):

People select (top-level `persons`, columns `name, age`):

```
--- People select ---
rows : [
			cols : [
			"Person_0"
			"20"
		]
			cols : [
			"Person_1"
			"21"
		]
			cols : [
			"Person_2"
			"22"
		]
]
```

ShapeHolders select (top-level `holders`, columns `id, color` — color resolved to enum name):

```
--- ShapeHolders select ---
rows : [
			cols : [
			"42"
			"Red"
		]
			cols : [
			"43"
			"Green"
		]
			cols : [
			"44"
			"Red"
		]
]
```

Telemetry sensors select (top-level `sensors` inside `Telemetry`, columns `id, value, unit`):

```
--- Telemetry sensors select ---
rows : [
			cols : [
			"temp"
			"23.500000"
			"C"
		]
			cols : [
			"pressure"
			"101.300000"
			"kPa"
		]
]
```

These examples show the result structure printed by the reflection-driven printer: a `rows` vector where each `Row` contains a `cols` vector of stringified column values (order preserved).
