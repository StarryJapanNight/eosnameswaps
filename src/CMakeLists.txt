cmake_minimum_required(VERSION 3.10)

project(eosnameswaps)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

add_contract( eosnameswaps eosnameswaps eosnameswaps.cpp )
target_include_directories( eosnameswaps PUBLIC ${CMAKE_SOURCE_DIR}/../include )
#target_ricardian_directory( eosnameswaps ${CMAKE_SOURCE_DIR}/../ricardian )