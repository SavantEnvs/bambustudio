#!/usr/bin/env bash
#
# mayhem/build.sh — builds the BambuStudio fuzz harnesses, their standalone reproducers and the
# known-answer selftest. A full BambuStudio build (wxWidgets/OpenGL/OCCT/CGAL/...) is not needed for
# the parsers we fuzz, so the harnesses compile the relevant in-tree sources directly and link them
# against the distro's -dev packages (installed by mayhem/Dockerfile; nothing is fetched here).
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
OUT=/mayhem
B="$SRC/build_mayhem"          # matched by upstream's build_*/ .gitignore rule
mkdir -p "$B/gen" "$B/fuzz" "$B/plain" "$B/norm"

# Version header normally produced by CMake's configure_file(); regenerated on every run.
sed -e 's|@SLIC3R_APP_NAME@|BambuStudio|' -e 's|@SLIC3R_APP_KEY@|BambuStudio|' \
    -e 's|@SLIC3R_VERSION@|02.09.01.70|' -e 's|@SLIC3R_BUILD_ID@|mayhem|' \
    -e 's|@SLIC3R_BUILD_TIME@|mayhem|' -e 's|@SLIC3R_COMPILE_VERSION@|mayhem|' \
    -e 's|@BBL_RELEASE_TO_PUBLIC@|0|' -e 's|@BBL_INTERNAL_TESTING@|0|' \
    src/libslic3r/libslic3r_version.h.in > "$B/gen/libslic3r_version.h"

INC="-I $B/gen -I src -I src/libslic3r -I src -I src/fast_float -I src/Shiny \
-I /usr/include/eigen3 -I src/nlohmann -I src/libnest2d/include -I src/clipper2 -I mayhem"
CXXCOMMON="-std=c++17 -include memory -include cstring -w -ffunction-sections -fdata-sections -DBOOST_LOG_DYN_LINK $INC"
LIBS="-Wl,--as-needed -lboost_log -lboost_thread -lboost_filesystem -lboost_locale -lboost_nowide \
-lboost_regex -ltbb -ltbbmalloc -lcrypto -lpthread -lm"

ADMESH="connect normals shared stl_io stlinit util"
LIB_SRCS=""
for f in $ADMESH; do LIB_SRCS="$LIB_SRCS src/admesh/$f.cpp"; done
LIB_SRCS="$LIB_SRCS src/libslic3r/LocalesUtils.cpp src/libslic3r/Format/objparser.cpp \
src/slic3r/Utils/ASCIIFolding.cpp src/libslic3r/GCodeReader.cpp src/libslic3r/Config.cpp \
src/libslic3r/PrintConfig.cpp src/libslic3r/utils.cpp \
src/libslic3r/Semver.cpp src/semver/semver.c src/libslic3r/Log.cpp \
src/libslic3r/LogSink.cpp src/libslic3r/Desensitize.cpp"

# flavor -> compile flags for the project's own code
#   fuzz:  sanitizers + libFuzzer coverage instrumentation (linked into the libFuzzer binaries)
#   plain: sanitizers only (linked into the -standalone reproducers, which have no libFuzzer runtime)
#   norm:  no sanitizers, normal flags (the selftest that mayhem/test.sh runs)
flavor_flags() {
  case "$1" in
    fuzz)  echo "-O1 $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link" ;;
    plain) echo "-O1 $SANITIZER_FLAGS $DEBUG_FLAGS" ;;
    norm)  echo "-O1 $COVERAGE_FLAGS" ;;
  esac
}

JOBS="$B/jobs.txt"
: > "$JOBS"
for flavor in fuzz plain norm; do
  fl="$(flavor_flags "$flavor")"
  for s in $LIB_SRCS; do
    o="$B/$flavor/$(basename "$s").o"
    if [[ "$s" == *.c ]]; then
      echo "$CC $fl -ffunction-sections -fdata-sections -c $s -o $o" >> "$JOBS"
    else
      # PrintConfig.cpp's static initializer (line ~7537) static_casts a ConfigOptionBoolsNullable default
      # to ConfigOptionBools; UBSan's vptr check halts on it at startup, before any input is read. Only
      # that check, only on that one file (-fno-sanitize, not an ignorelist); ASan and the rest of UBSan stay on.
      extra=""; [[ "$s" == */PrintConfig.cpp ]] && extra="-fno-sanitize=vptr"
      echo "$CXX $CXXCOMMON $fl $extra -c $s -o $o" >> "$JOBS"
    fi
  done
done
for h in fts_fuzzy_match objparse mtlparse stl_load gcode_parse_buffer gcode_parse_file; do
  for flavor in fuzz plain; do
    echo "$CXX $CXXCOMMON $(flavor_flags "$flavor") -c mayhem/fuzz_$h.cpp -o $B/$flavor/fuzz_$h.o" >> "$JOBS"
  done
done
echo "$CXX $CXXCOMMON $(flavor_flags norm) -c mayhem/selftest.cpp -o $B/norm/selftest.o" >> "$JOBS"
echo "$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o $B/lsan_off.o" >> "$JOBS"
echo "$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c $STANDALONE_FUZZ_MAIN -o $B/standalone_main.o" >> "$JOBS"

xargs -P "$MAYHEM_JOBS" -I{} bash -c '{}' < "$JOBS"

# Per-harness project objects (everything else is dropped by --gc-sections).
LOGOBJS="Log.cpp LogSink.cpp Desensitize.cpp"
objs_for() {
  local flavor="$1" h="$2" list=""
  local d="$B/$flavor"
  case "$h" in
    fts_fuzzy_match)   list="ASCIIFolding.cpp" ;;
    objparse|mtlparse) list="objparser.cpp LocalesUtils.cpp $LOGOBJS" ;;
    stl_load)          list="LocalesUtils.cpp $LOGOBJS"; for f in $ADMESH; do list="$list $f.cpp"; done ;;
    gcode_parse_*)     list="GCodeReader.cpp Config.cpp PrintConfig.cpp utils.cpp LocalesUtils.cpp Semver.cpp semver.c $LOGOBJS" ;;
  esac
  for o in $list; do printf '%s ' "$d/$o.o"; done
}

# -z start-stop-gc: without it GNU ld keeps every function that sancov's __sancov_pcs table references,
# defeating --gc-sections and dragging in GUI/geometry code whose objects we deliberately do not build.
LINK_COMMON="-Wl,--gc-sections -Wl,-z,start-stop-gc $SANITIZER_FLAGS $DEBUG_FLAGS"
for h in fts_fuzzy_match objparse mtlparse stl_load gcode_parse_buffer gcode_parse_file; do
  # shellcheck disable=SC2046
  $CXX $LINK_COMMON $LIB_FUZZING_ENGINE "$B/fuzz/fuzz_$h.o" $(objs_for fuzz "$h") "$B/lsan_off.o" $LIBS \
      -o "$OUT/fuzz_$h"
  # shellcheck disable=SC2046
  $CXX $LINK_COMMON "$B/plain/fuzz_$h.o" $(objs_for plain "$h") "$B/lsan_off.o" "$B/standalone_main.o" $LIBS \
      -o "$OUT/fuzz_$h-standalone"
done

# Known-answer selftest (normal flags, no sanitizers) for mayhem/test.sh.
SELF=""
for s in $LIB_SRCS; do SELF="$SELF $B/norm/$(basename "$s").o"; done
# shellcheck disable=SC2086
$CXX -Wl,--gc-sections -Wl,-z,start-stop-gc $COVERAGE_FLAGS "$B/norm/selftest.o" $SELF $LIBS -o "$OUT/bbs_selftest"
