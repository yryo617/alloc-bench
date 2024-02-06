cmake_minimum_required(VERSION 3.15.0)

set(espresso_sources
    cofactor.c cols.c compl.c contain.c cubestr.c cvrin.c cvrm.c cvrmisc.c cvrout.c
    dominate.c equiv.c espresso.c essen.c exact.c expand.c gasp.c getopt.c gimpel.c
    globals.c hack.c indep.c irred.c main.c map.c matrix.c mincov.c opo.c pair.c part.c
    primes.c reduce.c rows.c set.c setc.c sharp.c sminterf.c solution.c sparse.c unate.c
    utility.c verify.c)

if (DEFINED bm_logfile)
    add_compile_definitions(BM_LOGFILE=${bm_logfile})
    include_directories(AFTER ${CMAKE_SOURCE_DIR}/misc)
    list(APPEND espresso_sources ${CMAKE_SOURCE_DIR}/misc/harness.c)
endif()

set(target_shared_libs m)
if (DEFINED BENCHLIB)
    add_compile_definitions(BENCHLIB)
    add_library(espresso_shim SHARED ${espresso_sources})
    list(APPEND target_shared_libs espresso_shim)
    add_executable(espresso.elf ${CMAKE_SOURCE_DIR}/misc/shim.c)
else()
    add_executable(espresso.elf ${espresso_sources})
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

target_link_libraries(espresso.elf PRIVATE ${target_shared_libs})


if (DEFINED BENCHLIB)
    install(TARGETS espresso.elf espresso_shim
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib)
else()
    install(TARGETS espresso.elf
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib)
endif()

install(FILES largest.espresso 
	DESTINATION data/espresso
	PERMISSIONS OWNER_READ OWNER_WRITE)
