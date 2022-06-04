add_contract(nftone.mart nftone.mart ${CMAKE_CURRENT_SOURCE_DIR}/src/nftone.mart.cpp)

target_include_directories(nftone.mart
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(nftone.mart
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/nftone.mart.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/nftone.mart.contracts.md @ONLY )

target_compile_options( nftone.mart PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
