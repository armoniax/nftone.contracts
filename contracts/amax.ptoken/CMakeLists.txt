add_contract(amax.ptoken amax.ptoken ${CMAKE_CURRENT_SOURCE_DIR}/src/amax.ptoken.cpp)

target_include_directories(amax.ptoken
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(amax.ptoken
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax.ptoken.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax.ptoken.contracts.md @ONLY )

target_compile_options( amax.ptoken PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
