#!/bin/bash

# The committed libasn was generated with an asn1c that identifies as 0.9.29.
# Different asn1c builds/forks — even ones reporting the same version — emit
# cosmetically different code for EVERY file, drowning real changes in noise.
# The version pin below is necessary but not sufficient: after regenerating,
# ALWAYS review `git diff` and keep only the files whose change you intended,
# reverting pure-noise rewrites of untouched types.
EXPECTED_ASN1C_VERSION="0.9.29"
ACTUAL_ASN1C_VERSION=$(asn1c -version 2>&1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
if [ "$ACTUAL_ASN1C_VERSION" != "$EXPECTED_ASN1C_VERSION" ]; then
	echo "ERROR: asn1c $EXPECTED_ASN1C_VERSION required, found ${ACTUAL_ASN1C_VERSION:-none}." >&2
	echo "Regenerating with a different asn1c rewrites all of libasn; aborting." >&2
	exit 1
fi

cd ../src/ipa/libasn/

# Remove an old implementation
rm -f *.c
rm -f *.h

# Compile ASN.1 specification to actual C implementation
asn1c -fcompound-names -no-gen-example \
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
ls *.h *.c -1 >> CMakeLists.txt
echo ')' >> CMakeLists.txt
echo 'target_include_directories(libasn PUBLIC ${CMAKE_SOURCE_DIR}/include PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})' >> CMakeLists.txt
echo 'target_compile_options(libasn PRIVATE -Wall)' >> CMakeLists.txt
echo 'if (M32)' >> CMakeLists.txt
echo '  set_target_properties(libasn PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")' >> CMakeLists.txt
echo 'endif()' >> CMakeLists.txt


# Re-apply patches to generated sourcecode
cd ../../../
patch -p1 < ./asn1/0001-PKIX1Explicit88-remove-broken-constraint-check-in-Ce.patch
patch -p1 < ./asn1/0001-asn_internal-use-custom-memory-allocator-functions.patch
