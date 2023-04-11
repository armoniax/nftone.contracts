add_contract(amax.locknft amax.locknft ${CMAKE_CURRENT_SOURCE_DIR}/src/amax.locknft.cpp)

target_include_directories(amax.locknft
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(amax.locknft
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax.locknft.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax.locknft.contracts.md @ONLY )

target_compile_options( amax.locknft PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
