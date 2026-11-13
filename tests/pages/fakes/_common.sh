# Sourced by every fake command of the M3 gate (tests/run-pages.sh copies them first on PATH). Each fake appends its
# argv, shell-quoted (bash printf %q), as one line to $RILEY_FAKE_DIR/calls.log, then answers from the canned files in
# $RILEY_FAKE_DIR/canned (tests/pages/fixtures) and its state in $RILEY_FAKE_DIR/state. An argv the gate did not plan
# for is logged as "UNEXPECTED ..." and answered with exit 97, which fails the gate.
D=${RILEY_FAKE_DIR:?RILEY_FAKE_DIR is not set}
NAME=$(basename "$0")
{ printf '%s' "$NAME"; [ $# -gt 0 ] && printf ' %q' "$@"; printf '\n'; } >>"$D/calls.log"
unexpected() {
    { printf 'UNEXPECTED %s' "$NAME"; [ $# -gt 0 ] && printf ' %q' "$@"; printf '\n'; } >>"$D/calls.log"
    echo "$NAME: the M3 gate's fake does not know this call" >&2
    exit 97
}
