add_contract(nftpass.lock nftpass.lock ${CMAKE_CURRENT_SOURCE_DIR}/src/nftpass.lock.cpp)

target_include_directories(nftpass.lock
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include

)

set_target_properties(nftpass.lock
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

target_compile_options( nftpass.lock PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
