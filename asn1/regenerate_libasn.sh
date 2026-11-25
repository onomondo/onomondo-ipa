#!/bin/bash
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)

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

cd "$ROOT/src/ipa/libasn"

# Remove an old implementation
rm -f *.c
rm -f *.h

# Compile ASN.1 specification to actual C implementation
# -no-gen-OER/-no-gen-PER: every RSP binding is DER (SGP.22 §2.4a, §5.7.2 for
# ES10x; SGP.32 §2.1.3, §6.1.1, §6.2 for ESipa); no RSP spec references
# X.691/X.696. The codecs hang off the per-type vtables and the tables off the
# descriptors, so --gc-sections cannot drop them: 21.4 kB of dead device flash.
# XER/print are still generated (asn1c has no flag for them); the CMake
# definitions emitted below compile them out.
# -fno-constraints: nothing calls asn_check_constraints(); BER decode and DER
# encode never run the constraint functions, only the PER/OER codecs (no longer
# generated) did. The generated bodies were dead code the linker kept through
# the descriptors' general_constraints slots, which are now 0.
asn1c -fcompound-names -no-gen-example -no-gen-OER -no-gen-PER -fno-constraints \
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
echo '# Wire codecs (BER/DER) are all the library consumes; XER, compare, random-fill' >> CMakeLists.txt
echo '# and the constraint-failure message strings (never consumed - no ctfailcb) are' >> CMakeLists.txt
echo '# compiled out. print_struct is the SHOW_ASN_OUTPUT debug facility.' >> CMakeLists.txt
echo 'target_compile_definitions(libasn PUBLIC ASN_DISABLE_XER_SUPPORT ASN_DISABLE_COMPARE_SUPPORT ASN_DISABLE_RFILL_SUPPORT ASN_DISABLE_CONSTRAINT_MSG)' >> CMakeLists.txt
echo 'if (NOT SHOW_ASN_OUTPUT)' >> CMakeLists.txt
echo '  target_compile_definitions(libasn PUBLIC ASN_DISABLE_PRINT_SUPPORT)' >> CMakeLists.txt
echo 'endif()' >> CMakeLists.txt
echo 'target_compile_options(libasn PRIVATE -Wall)' >> CMakeLists.txt
echo 'if (M32)' >> CMakeLists.txt
echo '  set_target_properties(libasn PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")' >> CMakeLists.txt
echo 'endif()' >> CMakeLists.txt


# Re-apply patches to generated sourcecode
cd "$ROOT"
for p in ./asn1/0*.patch; do
	patch -p1 --no-backup-if-mismatch < "$p"
done
# BSD/macOS patch backs up every patched file under --no-backup-if-mismatch.
rm -f src/ipa/libasn/*.orig
