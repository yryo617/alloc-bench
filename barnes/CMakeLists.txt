cmake_minimum_required(VERSION 3.15.0)

set(barnes_sources
    code.c code_io.c load.c grav.c getparam.c util.c)

if (DEFINED bm_logfile)
    add_compile_definitions(BM_LOGFILE=${bm_logfile})
    include_directories(AFTER ${CMAKE_SOURCE_DIR}/misc)
    list(APPEND barnes_sources ${CMAKE_SOURCE_DIR}/misc/harness.c)
endif()

set(target_shared_libs m)
if (DEFINED BENCHLIB)
    add_compile_definitions(BENCHLIB)
    add_library(barnes_shim SHARED ${barnes_sources})
    list(APPEND target_shared_libs barnes_shim)
    add_executable(barnes.elf ${CMAKE_SOURCE_DIR}/misc/shim.c)
else()
    add_executable(barnes.elf ${barnes_sources})
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

target_link_libraries(barnes.elf PRIVATE ${target_shared_libs})

if (DEFINED BENCHLIB)
    install(TARGETS barnes.elf barnes_shim
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib)
else()
    install(TARGETS barnes.elf
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib)
endif()

install(FILES input 
    DESTINATION conf/barnes
    PERMISSIONS OWNER_READ OWNER_WRITE)
