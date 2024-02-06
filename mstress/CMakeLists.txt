cmake_minimum_required(VERSION 3.15.0)

set(mstress_sources mstress.c)

if (DEFINED bm_logfile)
    add_compile_definitions(BM_LOGFILE=${bm_logfile})
    include_directories(AFTER ${CMAKE_SOURCE_DIR}/misc)
    list(APPEND mstress_sources ${CMAKE_SOURCE_DIR}/misc/harness.c)
endif()

set(target_shared_libs pthread)
if (DEFINED BENCHLIB)
    add_compile_definitions(BENCHLIB)
    add_library(mstress_shim SHARED ${mstress_sources})
    list(APPEND target_shared_libs mstress_shim)
    add_executable(mstress.elf ${CMAKE_SOURCE_DIR}/misc/shim.c)
else()
    add_executable(mstress.elf ${mstress_sources})
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

target_link_libraries(mstress.elf PRIVATE ${target_shared_libs})

if (DEFINED BENCHLIB)
    install(TARGETS mstress.elf mstress_shim
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib)
else()
    install(TARGETS mstress.elf
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib)
endif()
