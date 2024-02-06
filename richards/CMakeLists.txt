cmake_minimum_required(VERSION 3.15.0)

set(richards_sources richards.c)

if (DEFINED bm_logfile)
    add_compile_definitions(BM_LOGFILE=${bm_logfile})
    include_directories(AFTER ${CMAKE_SOURCE_DIR}/misc)
    list(APPEND richards_sources ${CMAKE_SOURCE_DIR}/misc/harness.c)
endif()

set(target_shared_libs "")
if (DEFINED BENCHLIB)
    add_compile_definitions(BENCHLIB)
    add_library(richards_shim SHARED ${richards_sources})
    list(APPEND target_shared_libs richards_shim)
    add_executable(richards.elf ${CMAKE_SOURCE_DIR}/misc/shim.c)
else()
    add_executable(richards.elf ${richards_sources})
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

target_link_libraries(richards.elf PRIVATE ${target_shared_libs})

if (DEFINED BENCHLIB)
    install(TARGETS richards.elf richards_shim
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib)
else()
    install(TARGETS richards.elf
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib)
endif()
