#!/bin/bash

cd ../src/ipa/libasn/

# Remove an old implementation
rm -f *.c
rm -f *.h

# Compile ASN.1 specification to actual C implementation
# -no-gen-OER/-no-gen-PER: every RSP binding is DER (SGP.22 §2.4a, §5.7.2 for
# ES10x; SGP.32 §2.1.3, §6.1.1, §6.2 for ESipa); no RSP spec references
# X.691/X.696. The codecs hang off the per-type vtables and the tables off the
# descriptors, so --gc-sections cannot drop them: 21.4 kB of dead device flash.
# XER/print stay: host debug tooling (SHOW_ASN_OUTPUT) uses them.
asn1c -fcompound-names -no-gen-example -no-gen-OER -no-gen-PER \
      ../../../asn1/PKIX1Explicit88.asn \
      ../../../asn1/PKIX1Implicit88.asn \
      ../../../asn1/PEDefinitions.asn \
      ../../../asn1/RSPDefinitions.asn \
      ../../../asn1/SGP32Definitions.asn

# Remove file(s) we do not need
rm ./Makefile.am.libasncodec

# Generate CMakeLists.txt
echo "#CAUTION: autgenerated file, do not change, see "`basename $0` > CMakeLists.txt
echo 'add_library(libasn STATIC' >> CMakeLists.txt
printf '%s\n' *.h *.c | LC_ALL=C sort >> CMakeLists.txt
echo ')' >> CMakeLists.txt
echo 'target_include_directories(libasn PUBLIC ${CMAKE_SOURCE_DIR}/include PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})' >> CMakeLists.txt
echo '# Generated with -no-gen-OER -no-gen-PER: constr_TYPE.h gates the OER' >> CMakeLists.txt
echo '# declarations behind ASN_DISABLE_OER_SUPPORT; the PER define strips the' >> CMakeLists.txt
echo '# generated per-type constraint tables. PUBLIC: libipa includes libasn headers.' >> CMakeLists.txt
echo 'target_compile_definitions(libasn PUBLIC ASN_DISABLE_OER_SUPPORT ASN_DISABLE_PER_SUPPORT)' >> CMakeLists.txt
echo 'target_compile_options(libasn PRIVATE -Wall)' >> CMakeLists.txt
echo 'if (M32)' >> CMakeLists.txt
echo '  set_target_properties(libasn PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")' >> CMakeLists.txt
echo 'endif()' >> CMakeLists.txt


# Re-apply patches to generated sourcecode
cd ../../../
patch -p1 < ./asn1/0001-PKIX1Explicit88-remove-broken-constraint-check-in-Ce.patch
patch -p1 < ./asn1/0001-asn_internal-use-custom-memory-allocator-functions.patch
