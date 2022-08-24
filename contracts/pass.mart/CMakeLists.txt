add_contract(pass.mart pass.mart ${CMAKE_CURRENT_SOURCE_DIR}/src/pass.mart.cpp)

target_include_directories(pass.mart
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include

)

set_target_properties(pass.mart
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

target_compile_options( pass.mart PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
