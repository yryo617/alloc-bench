cmake_minimum_required(VERSION 3.15.0)

set(small_fixed_sources small_fixed_alloc.c)
set(random_alloc_sources random_mixed_alloc.c)

if (DEFINED bm_logfile)
    add_compile_definitions(BM_LOGFILE=${bm_logfile})
    include_directories(AFTER ${CMAKE_SOURCE_DIR}/misc)
    list(APPEND small_fixed_sources ${CMAKE_SOURCE_DIR}/misc/harness.c)
    list(APPEND random_alloc_sources ${CMAKE_SOURCE_DIR}/misc/harness.c)
endif()

set(target_small_shared_libs "")
set(target_random_shared_libs "")
if (DEFINED BENCHLIB)
    add_compile_definitions(BENCHLIB)

    add_library(small_fixed_alloc_shim SHARED ${small_fixed_sources})
    list(APPEND target_small_shared_libs small_fixed_alloc_shim)
    add_executable(small_fixed_alloc.elf ${CMAKE_SOURCE_DIR}/misc/shim.c)

    add_library(random_mixed_alloc_shim SHARED ${random_alloc_sources})
    list(APPEND target_random_shared_libs random_mixed_alloc_shim)
    add_executable(random_mixed_alloc.elf ${CMAKE_SOURCE_DIR}/misc/shim.c)
else()
    add_executable(small_fixed_alloc.elf ${small_fixed_sources})
    add_executable(random_mixed_alloc.elf ${random_alloc_sources})
endif()


if (${gclib} STREQUAL bdwgc)
    add_compile_definitions(BDWGC IGNOREFREE)
    include_directories(AFTER ${install_dir}/include)
    list(APPEND target_small_shared_libs gc)
    list(APPEND target_random_shared_libs gc)
elseif (${gclib} STREQUAL cheribumpalloc)
    add_compile_definitions(BUMPALLOC)
    include_directories(AFTER ${CMAKE_SOURCE_DIR}/cheri_bump_alloc)
    list(APPEND target_small_shared_libs cheribumpalloc)
    list(APPEND target_random_shared_libs cheribumpalloc)
endif()

target_link_libraries(small_fixed_alloc.elf PRIVATE ${target_small_shared_libs})
target_link_libraries(random_mixed_alloc.elf PRIVATE ${target_random_shared_libs})

if (DEFINED BENCHLIB)
    install(TARGETS
            small_fixed_alloc.elf small_fixed_alloc_shim
            random_mixed_alloc.elf random_mixed_alloc_shim
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib)
else()
    install(TARGETS
            small_fixed_alloc.elf
            random_mixed_alloc.elf
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib)
endif()
