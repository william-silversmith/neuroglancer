#!/bin/bash -xve

LIBJPEGTURBO=/usr/src/libjpeg-turbo

compile_options=(
    -O2 -Wno-shift-count-overflow
    -I$LIBJPEGTURBO -I./config
    $LIBJPEGTURBO/jcapimin.c $LIBJPEGTURBO/jcapistd.c 
    $LIBJPEGTURBO/jccoefct.c $LIBJPEGTURBO/jccolor.c 
    $LIBJPEGTURBO/jcdctmgr.c $LIBJPEGTURBO/jchuff.c 
    $LIBJPEGTURBO/jcicc.c $LIBJPEGTURBO/jcinit.c 
    $LIBJPEGTURBO/jcmainct.c $LIBJPEGTURBO/jcmarker.c 
    $LIBJPEGTURBO/jcmaster.c $LIBJPEGTURBO/jcomapi.c 
    $LIBJPEGTURBO/jcparam.c $LIBJPEGTURBO/jcphuff.c 
    $LIBJPEGTURBO/jcprepct.c $LIBJPEGTURBO/jcsample.c 
    $LIBJPEGTURBO/jctrans.c 

    $LIBJPEGTURBO/jdapimin.c $LIBJPEGTURBO/jdarith.c 
    $LIBJPEGTURBO/jdapistd.c $LIBJPEGTURBO/jdatadst.c 
    $LIBJPEGTURBO/jdatasrc.c $LIBJPEGTURBO/jdcoefct.c 
    $LIBJPEGTURBO/jdcolor.c $LIBJPEGTURBO/jddctmgr.c 
    $LIBJPEGTURBO/jdhuff.c $LIBJPEGTURBO/jdicc.c 
    $LIBJPEGTURBO/jdinput.c $LIBJPEGTURBO/jdmainct.c 
    $LIBJPEGTURBO/jdmarker.c $LIBJPEGTURBO/jdmaster.c 
    $LIBJPEGTURBO/jdmerge.c $LIBJPEGTURBO/jdphuff.c 
    $LIBJPEGTURBO/jdpostct.c $LIBJPEGTURBO/jdsample.c 
    $LIBJPEGTURBO/jdtrans.c $LIBJPEGTURBO/jerror.c 
    $LIBJPEGTURBO/jfdctflt.c $LIBJPEGTURBO/jfdctfst.c 
    $LIBJPEGTURBO/jfdctint.c $LIBJPEGTURBO/jidctflt.c 
    $LIBJPEGTURBO/jidctfst.c $LIBJPEGTURBO/jidctint.c 
    $LIBJPEGTURBO/jidctred.c $LIBJPEGTURBO/jquant1.c 
    $LIBJPEGTURBO/jquant2.c $LIBJPEGTURBO/jutils.c 
    $LIBJPEGTURBO/jmemmgr.c $LIBJPEGTURBO/jmemnobs.c 
    $LIBJPEGTURBO/jsimd_none.c $LIBJPEGTURBO/turbojpeg.c 
    $LIBJPEGTURBO/transupp.c $LIBJPEGTURBO/jdatadst-tj.c 
    $LIBJPEGTURBO/jdatasrc-tj.c $LIBJPEGTURBO/rdbmp.c 
    $LIBJPEGTURBO/rdppm.c $LIBJPEGTURBO/wrbmp.c 
    $LIBJPEGTURBO/wrppm.c

    libjpegturbo_wasm.c
     -DNDEBUG
     --no-entry
     -s FILESYSTEM=0
     -s ALLOW_MEMORY_GROWTH=1
     -s TOTAL_STACK=32768
     -s TOTAL_MEMORY=64kb
     -s EXPORTED_FUNCTIONS='["_jpeg_decompress","_jpeg_image_bytes","_malloc","_free"]'
     -s MALLOC=emmalloc
     -s ENVIRONMENT=worker
     -s STANDALONE_WASM=1
     # -s ASSERTIONS=1 
     # -s SAFE_HEAP=1
     -o libjpegturbo.wasm
)
emcc ${compile_options[@]}
