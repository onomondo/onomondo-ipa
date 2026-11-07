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

# Stop at the first failing step: a half-applied rewrite or patch must not leave
# a tree that still builds.
set -e

cd ../src/ipa/libasn/

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

# Type and member names feed only print, XER and ASN_DEBUG. Each descriptor starts
# with its name and its XML tag, and each member entry ends with its name.
# ASN_TYPE_NAME() and ASN_MEMBER_NAME() (asn1/0014) let a build drop the name fields
# with ASN_DISABLE_NAMES; XER is never built, so the XML tag line goes outright.
# Generated type files only (asn1/0014 converts the skeleton descriptors); both
# rewrites must hit every descriptor and member entry.
GENERATED=$(grep -l 'From ASN.1 module' *.c)
perl -0pi -e 's/^(asn_TYPE_descriptor_t asn_DEF_\w+ = \{\n)\t("[^"]*"),\n\t"[^"]*",\n/$1\tASN_TYPE_NAME($2)\n/mg' $GENERATED
perl -pi -e 's/^(\t\t)"([^"]*)"$/$1ASN_MEMBER_NAME("$2")/' $GENERATED
DESCRIPTORS=$(cat $GENERATED | grep -c '^asn_TYPE_descriptor_t asn_DEF_' || true)
MEMBERS=$(cat $GENERATED | grep -c $'^\t{ ATF_' || true)
TYPE_NAMES=$(cat $GENERATED | grep -c $'^\tASN_TYPE_NAME(' || true)
MEMBER_NAMES=$(cat $GENERATED | grep -c $'^\t\tASN_MEMBER_NAME(' || true)
if [ "$TYPE_NAMES" -ne "$DESCRIPTORS" ] || [ "$MEMBER_NAMES" -ne "$MEMBERS" ]; then
	echo "ERROR: name rewrite hit $TYPE_NAMES of $DESCRIPTORS descriptors and $MEMBER_NAMES of $MEMBERS members." >&2
	exit 1
fi

# With -no-gen-OER, -no-gen-PER and -fno-constraints every generated
# encoding_constraints initialiser is { 0, 0, 0 }. ASN_ENC_CONSTRAINTS() (asn1/0012)
# lets constr_TYPE.h drop the field. Each descriptor and member entry has exactly
# one, and a missed one would shift the fields after it, so count them.
perl -pi -e 's/^(\t{1,2})\{ 0, 0, 0 \},$/$1ASN_ENC_CONSTRAINTS(0, 0, 0)/' $GENERATED
ENC=$(cat $GENERATED | grep -c 'ASN_ENC_CONSTRAINTS(0, 0, 0)' || true)
if [ "$ENC" -ne $((DESCRIPTORS + MEMBERS)) ]; then
	echo "ERROR: ASN_ENC_CONSTRAINTS() rewrote $ENC lines, expected $((DESCRIPTORS + MEMBERS))." >&2
	exit 1
fi

# The PER-only fields of the SEQUENCE and CHOICE specifics tables are zero in every
# generated table. ASN_SEQ_OMS() and ASN_CHOICE_ORDER() (asn1/0013) drop them along
# with PER. The bare CHOICE line "0, 0," also occurs elsewhere, so it is matched
# only right before the table's "Extensions start" line. Both rewrites must hit
# every table.
perl -pi -e 's/^\t0, 0, 0,\t(\/\* Optional elements \(not needed\) \*\/)$/\tASN_SEQ_OMS(0, 0, 0)\t$1/' $GENERATED
perl -0pi -e 's/^\t0, 0,\n(\t-?[0-9]+\t\/\* Extensions start \*\/)$/\tASN_CHOICE_ORDER(0, 0)\n$1/mg' $GENERATED
SEQ_TABLES=$(cat $GENERATED | grep -c 'asn_SEQUENCE_specifics_t asn_SPC_' || true)
CHOICE_TABLES=$(cat $GENERATED | grep -c 'asn_CHOICE_specifics_t asn_SPC_' || true)
SEQ_OMS=$(cat $GENERATED | grep -c 'ASN_SEQ_OMS(0, 0, 0)' || true)
CHOICE_ORDER=$(cat $GENERATED | grep -c 'ASN_CHOICE_ORDER(0, 0)' || true)
if [ "$SEQ_OMS" -ne "$SEQ_TABLES" ] || [ "$CHOICE_ORDER" -ne "$CHOICE_TABLES" ]; then
	echo "ERROR: PER-only field rewrite hit $SEQ_OMS of $SEQ_TABLES SEQUENCE and $CHOICE_ORDER of $CHOICE_TABLES CHOICE tables." >&2
	exit 1
fi

# Remove file(s) we do not need
rm ./Makefile.am.libasncodec

# Generate CMakeLists.txt
echo "#CAUTION: autgenerated file, do not change, see "`basename $0` > CMakeLists.txt
echo 'add_library(libasn STATIC' >> CMakeLists.txt
ls -1 *.h *.c >> CMakeLists.txt
echo ')' >> CMakeLists.txt
echo 'target_include_directories(libasn PUBLIC ${CMAKE_SOURCE_DIR}/include PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})' >> CMakeLists.txt
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
cd ../../../
patch -p1 < ./asn1/0001-asn_internal-use-custom-memory-allocator-functions.patch
patch -p1 < ./asn1/0004-OCTET_STRING-exact-fit-first-allocation.patch
patch -p1 < ./asn1/0005-skeletons-vtable-guards.patch
patch -p1 < ./asn1/0006-constraints-optional-messages.patch
patch -p1 < ./asn1/0010-SET_OF-fail-DER-encoding-on-allocation-failure.patch
patch -p1 < ./asn1/0011-constr_TYPE-optional-names.patch
patch -p1 < ./asn1/0012-skeletons-drop-encoding-constraints.patch
patch -p1 < ./asn1/0013-skeletons-drop-PER-only-specifics-fields.patch
patch -p1 < ./asn1/0014-skeletons-drop-name-fields.patch
