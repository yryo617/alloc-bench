cmake_minimum_required(VERSION 3.15.0)

set(glibc_simple_sources bench-malloc-simple.c)
set(glibc_thread_sources bench-malloc-thread.c)

if (DEFINED bm_logfile)
    add_compile_definitions(BM_LOGFILE=${bm_logfile})
    include_directories(AFTER ${CMAKE_SOURCE_DIR}/misc)
    list(APPEND glibc_simple_sources ${CMAKE_SOURCE_DIR}/misc/harness.c)
    list(APPEND glibc_thread_sources ${CMAKE_SOURCE_DIR}/misc/harness.c)
endif()

set(target_simple_shared_libs pthread)
set(target_thread_shared_libs pthread m)
if (DEFINED BENCHLIB)
    add_compile_definitions(BENCHLIB)

    add_library(glibc_bench_simple_shim SHARED ${glibc_simple_sources})
    list(APPEND target_simple_shared_libs glibc_bench_simple_shim)
    add_executable(glibc_bench_simple.elf ${CMAKE_SOURCE_DIR}/misc/shim.c)

    add_library(glibc_bench_thread_shim SHARED ${glibc_thread_sources})
    list(APPEND target_thread_shared_libs glibc_bench_thread_shim)
    add_executable(glibc_bench_thread.elf ${CMAKE_SOURCE_DIR}/misc/shim.c)
else()
    add_executable(glibc_bench_simple.elf ${glibc_simple_sources})
    add_executable(glibc_bench_thread.elf ${glibc_thread_sources})
endif()



if (${gclib} STREQUAL bdwgc)
    add_compile_definitions(BDWGC IGNOREFREE)
    include_directories(AFTER ${install_dir}/include)
    list(APPEND target_simple_shared_libs gc)
    list(APPEND target_thread_shared_libs gc)
elseif (${gclib} STREQUAL cheribumpalloc)
    add_compile_definitions(BUMPALLOC)
    include_directories(AFTER ${CMAKE_SOURCE_DIR}/cheri_bump_alloc)
    list(APPEND target_simple_shared_libs cheribumpalloc)
    list(APPEND target_thread_shared_libs cheribumpalloc)
endif()


target_link_libraries(glibc_bench_simple.elf PRIVATE ${target_simple_shared_libs})
target_link_libraries(glibc_bench_thread.elf PRIVATE ${target_thread_shared_libs})


if (DEFINED BENCHLIB)
    install(TARGETS
	    glibc_bench_simple.elf glibc_bench_simple_shim
	    glibc_bench_thread.elf glibc_bench_thread_shim
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib)
else()
    install(TARGETS
	    glibc_bench_simple.elf
	    glibc_bench_thread.elf
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib)
endif()
