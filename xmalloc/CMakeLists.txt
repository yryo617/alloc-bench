cmake_minimum_required(VERSION 3.15.0)

set(xmalloc_sources xmalloc-test.c)

if (DEFINED bm_logfile)
    add_compile_definitions(BM_LOGFILE=${bm_logfile})
    include_directories(AFTER ${CMAKE_SOURCE_DIR}/misc)
    list(APPEND xmalloc_sources ${CMAKE_SOURCE_DIR}/misc/harness.c)
endif()

set(target_shared_libs pthread)
if (DEFINED BENCHLIB)
    add_compile_definitions(BENCHLIB)
    add_library(xmalloc_shim SHARED ${xmalloc_sources})
    list(APPEND target_shared_libs xmalloc_shim)
    add_executable(xmalloc.elf ${CMAKE_SOURCE_DIR}/misc/shim.c)
else()
    add_executable(xmalloc.elf ${xmalloc_sources})
endif()


if (${gclib} STREQUAL bdwgc)
    add_compile_definitions(BDWGC)
    include_directories(AFTER ${install_dir}/include)
    list(APPEND target_shared_libs gc)
elseif (${gclib} STREQUAL cheribumpalloc)
    add_compile_definitions(BUMPALLOC)
    include_directories(AFTER ${CMAKE_SOURCE_DIR}/cheri_bump_alloc)
    list(APPEND target_shared_libs cheribumpalloc)
endif()

target_link_libraries(xmalloc.elf PRIVATE ${target_shared_libs})

if (DEFINED BENCHLIB)
    install(TARGETS xmalloc.elf xmalloc_shim
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib)
else()
    install(TARGETS xmalloc.elf
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib)
endif()

