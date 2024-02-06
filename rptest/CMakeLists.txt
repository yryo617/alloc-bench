cmake_minimum_required(VERSION 3.15.0)

set(rptest_sources 
    rptest.c thread.c timer.c)

if (DEFINED bm_logfile)
    add_compile_definitions(BM_LOGFILE=${bm_logfile})
    include_directories(AFTER ${CMAKE_SOURCE_DIR}/misc)
    list(APPEND rptest_sources ${CMAKE_SOURCE_DIR}/misc/harness.c)
endif()

set(target_shared_libs pthread)
if (DEFINED BENCHLIB)
    add_compile_definitions(BENCHLIB)
    add_library(rptest_shim SHARED ${rptest_sources})
    list(APPEND target_shared_libs rptest_shim)
    add_executable(rptest.elf ${CMAKE_SOURCE_DIR}/misc/shim.c)
else()
    add_executable(rptest.elf ${rptest_sources})
endif()


if (${gclib} STREQUAL bdwgc)
    message(FATAL_ERROR "rptest not yet changed for GC")
    add_compile_definitions(BDWGC IGNOREFREE)
    include_directories(AFTER ${install_dir}/include)
    list(APPEND target_shared_libs gc)
elseif (${gclib} STREQUAL cheribumpalloc)
    add_compile_definitions(BUMPALLOC)
    include_directories(AFTER ${CMAKE_SOURCE_DIR}/cheri_bump_alloc)
    list(APPEND target_shared_libs cheribumpalloc)
endif()

target_link_libraries(rptest.elf PRIVATE ${target_shared_libs})


if (DEFINED BENCHLIB)
    install(TARGETS rptest.elf rptest_shim
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib)
else()
    install(TARGETS rptest.elf
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib)
endif()

