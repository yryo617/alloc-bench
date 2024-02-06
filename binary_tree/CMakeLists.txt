cmake_minimum_required(VERSION 3.15.0)

set(binary_tree_sources binary_tree.c)

if (DEFINED bm_logfile)
    add_compile_definitions(BM_LOGFILE=${bm_logfile})
    include_directories(AFTER ${CMAKE_SOURCE_DIR}/misc)
    list(APPEND binary_tree_sources ${CMAKE_SOURCE_DIR}/misc/harness.c)
endif()

set(target_shared_libs m)
if (DEFINED BENCHLIB)
    add_compile_definitions(BENCHLIB)
    add_library(binary_tree_shim SHARED ${binary_tree_sources})
    list(APPEND target_shared_libs binary_tree_shim)
    add_executable(binary_tree.elf ${CMAKE_SOURCE_DIR}/misc/shim.c)
else()
    add_executable(binary_tree.elf ${binary_tree_sources})
endif()


if (${gclib} STREQUAL bdwgc)
    add_compile_definitions(BDWGC IGNOREFREE)
    include_directories(AFTER ${install_dir}/include)
    list(APPEND target_shared_libs gc)
elseif (${gclib} STREQUAL cheribumpalloc)
    add_compile_definitions(BUMPALLOC)
    include_directories(AFTER ${CMAKE_SOURCE_DIR}/cheri_bump_alloc)
    list(APPEND target_shared_libs cheribumpalloc)
endif()

target_link_libraries(binary_tree.elf PRIVATE ${target_shared_libs})

if (DEFINED BENCHLIB)
    install(TARGETS binary_tree.elf binary_tree_shim
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib)
else()
    install(TARGETS binary_tree.elf
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib)
endif()

