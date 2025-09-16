cmake_minimum_required(VERSION 3.10)
project(IntrospectionOnFlatBuffers LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(FlatBuffers QUIET)

if(NOT DEFINED FLATBUFFERS_FLATC_EXECUTABLE)
  find_program(FLATC_EXECUTABLE flatc)
  if(FLATC_EXECUTABLE)
    set(FLATBUFFERS_FLATC_EXECUTABLE ${FLATC_EXECUTABLE})
  endif()
endif()

if(NOT TARGET FlatBuffers::flatbuffers)
  # Try to find library manually; user should install libflatbuffers-dev in WSL
  find_library(FLATBUFFERS_LIB flatbuffers)
  if(FLATBUFFERS_LIB)
    add_library(FlatBuffers::flatbuffers UNKNOWN IMPORTED)
    set_target_properties(FlatBuffers::flatbuffers PROPERTIES IMPORTED_LOCATION ${FLATBUFFERS_LIB})
  endif()
endif()


set(SCHEMAS
  ${CMAKE_CURRENT_SOURCE_DIR}/schema/telemetry.fbs
  ${CMAKE_CURRENT_SOURCE_DIR}/schema/person.fbs
  ${CMAKE_CURRENT_SOURCE_DIR}/schema/people.fbs
  ${CMAKE_CURRENT_SOURCE_DIR}/schema/devices.fbs
  ${CMAKE_CURRENT_SOURCE_DIR}/schema/device.fbs
  ${CMAKE_CURRENT_SOURCE_DIR}/schema/shapeholders.fbs
  ${CMAKE_CURRENT_SOURCE_DIR}/schema/union_enum.fbs
  ${CMAKE_CURRENT_SOURCE_DIR}/schema/select_result.fbs
)

# Expected generated headers and bfbs (flatc will produce these into the build dir)
set(REFLECTION_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/reflection)

set(GENERATED_HEADERS
  ${REFLECTION_OUT_DIR}/telemetry_generated.h
  ${REFLECTION_OUT_DIR}/person_generated.h
  ${REFLECTION_OUT_DIR}/people_generated.h
  ${REFLECTION_OUT_DIR}/devices_generated.h
  ${REFLECTION_OUT_DIR}/device_generated.h
  ${REFLECTION_OUT_DIR}/shapeholders_generated.h
  ${REFLECTION_OUT_DIR}/union_enum_generated.h
  ${REFLECTION_OUT_DIR}/select_result_generated.h
)
set(GENERATED_BFBS
  ${REFLECTION_OUT_DIR}/telemetry.bfbs
  ${REFLECTION_OUT_DIR}/person.bfbs
  ${REFLECTION_OUT_DIR}/people.bfbs
  ${REFLECTION_OUT_DIR}/devices.bfbs
  ${REFLECTION_OUT_DIR}/device.bfbs
  ${REFLECTION_OUT_DIR}/shapeholders.bfbs
  ${REFLECTION_OUT_DIR}/union_enum.bfbs
  ${REFLECTION_OUT_DIR}/select_result.bfbs
)

add_custom_command(
  OUTPUT ${GENERATED_HEADERS} ${GENERATED_BFBS}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${REFLECTION_OUT_DIR}
  COMMAND ${FLATBUFFERS_FLATC_EXECUTABLE} -c --cpp --binary --schema -o ${REFLECTION_OUT_DIR} ${SCHEMAS}
  DEPENDS ${SCHEMAS}
  COMMENT "Running flatc to generate headers and binary schemas into ${REFLECTION_OUT_DIR}"
)

add_custom_target(generate_flatbuffers ALL DEPENDS ${GENERATED_HEADERS} ${GENERATED_BFBS})

include_directories(${REFLECTION_OUT_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(create_sample src/producers/create_sample.cpp)
add_executable(decode_reflection src/consumers/decode_reflection.cpp src/reflection/reflection_printer.cpp)
add_executable(create_person src/producers/create_person.cpp)
add_executable(create_device src/producers/create_device.cpp)
add_executable(create_union_enum src/producers/create_union_enum.cpp)
add_executable(select_example src/consumers/select_example.cpp src/reflection/reflection_printer.cpp)

add_dependencies(create_sample generate_flatbuffers)
add_dependencies(decode_reflection generate_flatbuffers)
add_dependencies(create_person generate_flatbuffers)
add_dependencies(create_device generate_flatbuffers)
add_dependencies(create_union_enum generate_flatbuffers)
add_dependencies(select_example generate_flatbuffers)

target_link_libraries(create_sample PRIVATE FlatBuffers::flatbuffers)
target_link_libraries(decode_reflection PRIVATE FlatBuffers::flatbuffers)
target_link_libraries(create_union_enum PRIVATE FlatBuffers::flatbuffers)
target_link_libraries(create_person PRIVATE FlatBuffers::flatbuffers)
target_link_libraries(create_device PRIVATE FlatBuffers::flatbuffers)
target_link_libraries(select_example PRIVATE FlatBuffers::flatbuffers)

# Simple unit test for reflection printer
enable_testing()
add_executable(test_reflection src/tests/test_reflection.cpp src/reflection/reflection_printer.cpp)
add_dependencies(test_reflection generate_flatbuffers)
target_link_libraries(test_reflection PRIVATE FlatBuffers::flatbuffers)
add_test(NAME ReflectionPrinterTest COMMAND test_reflection)
