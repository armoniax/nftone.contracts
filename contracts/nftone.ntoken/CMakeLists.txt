add_contract(nftone.ntoken nftone.ntoken ${CMAKE_CURRENT_SOURCE_DIR}/src/nftone.ntoken.cpp)

target_include_directories(nftone.ntoken
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(nftone.ntoken
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/nftone.ntoken.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/nftone.ntoken.contracts.md @ONLY )

target_compile_options( nftone.ntoken PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
