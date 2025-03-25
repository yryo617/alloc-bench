## build instructions 
All instructions are t obe executed 

1. Instruction for purecap 
rm -rf build install && cmake -S src -B build --install-prefix $(pwd)/install --toolchain  morello-purecap.cmake -DINDEX_VEC_DEFINED=1 -DCMAKE_BUILD_TYPE=RelWithDebInfo  -Dgclib=bdwgc   -Dbm_logfile=dj.log

cmake --build build && cmake --install build 


2. Instruction for hybrid
rm -rf build-hybrid install-hybrid && cmake -S src -B build-hybrid --install-prefix $(pwd)/install-hybrid --toolchain  morello-hybrid.cmake -DINDEX_VEC_DEFINED=1 -DCMAKE_BUILD_TYPE=RelWithDebInfo  -Dgclib=bdwgc   -Dbm_logfile=dj.log

cmake --build build-hybrid && cmake --install build-hybrid
