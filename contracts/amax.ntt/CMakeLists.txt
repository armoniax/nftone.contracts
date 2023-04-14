add_contract(amax.ntt amax.ntt ${CMAKE_CURRENT_SOURCE_DIR}/src/amax.ntt.cpp)

target_include_directories(amax.ntt
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(amax.ntt
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax.ntt.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax.ntt.contracts.md @ONLY )

target_compile_options( amax.ntt PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
