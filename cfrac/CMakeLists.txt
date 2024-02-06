cmake_minimum_required(VERSION 3.15.0)

set(cfrac_sources
    cfrac.c
    pops.c pconst.c pio.c
    pabs.c pneg.c pcmp.c podd.c phalf.c
    padd.c psub.c pmul.c pdivmod.c psqrt.c ppowmod.c
    atop.c ptoa.c itop.c utop.c ptou.c errorp.c
    pfloat.c pidiv.c pimod.c picmp.c
    primes.c pcfrac.c pgcd.c)

if (DEFINED bm_logfile)
    add_compile_definitions(BM_LOGFILE=${bm_logfile})
    include_directories(AFTER ${CMAKE_SOURCE_DIR}/misc)
    list(APPEND cfrac_sources ${CMAKE_SOURCE_DIR}/misc/harness.c)
endif()

set(target_shared_libs m)
if (DEFINED BENCHLIB)
    add_compile_definitions(BENCHLIB)
    add_library(cfrac_shim SHARED ${cfrac_sources})
    list(APPEND target_shared_libs cfrac_shim)
    add_executable(cfrac.elf ${CMAKE_SOURCE_DIR}/misc/shim.c)
else()
    add_executable(cfrac.elf ${cfrac_sources})
endif()


if (${gclib} STREQUAL bdwgc)
    add_compile_definitions(BWGC BDWGC IGNOREFREE)
    include_directories(AFTER ${install_dir}/include)
    list(APPEND target_shared_libs gc)
elseif (${gclib} STREQUAL cheribumpalloc)
    add_compile_definitions(BUMPALLOC)
    include_directories(AFTER ${CMAKE_SOURCE_DIR}/cheri_bump_alloc)
    list(APPEND target_shared_libs cheribumpalloc)
endif()


target_link_libraries(cfrac.elf PRIVATE ${target_shared_libs})

install(TARGETS cfrac.elf 
	RUNTIME DESTINATION bin)

if (DEFINED BENCHLIB)
    install(TARGETS cfrac.elf cfrac_shim
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib)
else()
    install(TARGETS cfrac.elf
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib)
endif()
