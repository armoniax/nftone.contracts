add_contract(pass.lock pass.lock ${CMAKE_CURRENT_SOURCE_DIR}/src/pass.lock.cpp)

target_include_directories(pass.lock
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include

)

set_target_properties(pass.lock
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

target_compile_options( pass.lock PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
