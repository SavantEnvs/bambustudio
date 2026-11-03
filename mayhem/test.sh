#!/usr/bin/env bash
#
# mayhem/test.sh — RUNS the known-answer selftest that mayhem/build.sh compiled (normal flags, no
# sanitizers) and compares each "key=value" line it prints against mayhem/selftest.golden. The selftest
# drives the same BambuStudio code the fuzz targets cover (OBJ/MTL parsers, admesh STL load + repair,
# G-code reader, settings fuzzy matcher) with fixed inputs, so a no-op or exit(0) build prints nothing
# and every golden line fails.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=/mayhem}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

SELFTEST="$SRC/bbs_selftest"
if [ ! -x "$SELFTEST" ]; then
  echo "missing $SELFTEST — mayhem/build.sh should have built it" >&2
  emit_ctrf bbs-selftest 0 1 0
  exit $?
fi

actual="$(mktemp)"
trap 'rm -f "$actual"' EXIT
"$SELFTEST" > "$actual" 2> "$actual.err" || echo "selftest exited non-zero" >&2

passed=0 failed=0
while IFS= read -r line; do
  if grep -qxF -- "$line" "$actual"; then
    passed=$((passed + 1))
  else
    failed=$((failed + 1))
    echo "FAIL: expected '$line'" >&2
  fi
done < mayhem/selftest.golden
# Any unexpected extra output line is also a failure.
while IFS= read -r line; do
  grep -qxF -- "$line" mayhem/selftest.golden || { failed=$((failed + 1)); echo "FAIL: unexpected '$line'" >&2; }
done < "$actual"
rm -f "$actual.err"

emit_ctrf bbs-selftest "$passed" "$failed" 0
