add_contract(pass.custody pass.custody ${CMAKE_CURRENT_SOURCE_DIR}/src/pass.custody.cpp)

target_include_directories(pass.custody
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include

)

set_target_properties(pass.custody
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

target_compile_options( pass.custody PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
