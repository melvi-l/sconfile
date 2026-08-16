#!/usr/bin/env bash
set -e

SRC_DIR="./src"
BUILD_DIR="./build"
METAGEN_EXEC="metagen"
METAGEN_TEST_PATH="test.sx"
MAIN_EXEC="main"
MAIN_TEST_PATH="test.sx"

TESTS_DIR="./tests"

COMMON_LIBS=""

SUBCMD="${1:-all}"
MODE="${2:-debug}"

cflags_for_mode() {
  case "$1" in
    debug)
      echo "-g3 -O0 -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -DDEBUG"
      ;;
    debug-sanitize)
      echo "-g3 -O0 -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -fanalyzer -fsanitize=address,undefined -fno-omit-frame-pointer -DDEBUG"
      ;;
    *)
      printf 'unknown mode: %s\n' "$1" >&2
      return 1
      ;;
  esac
}

build_metagen() {
  local cflags; cflags=$(cflags_for_mode "$MODE")
  local t0=$SECONDS
  gcc $cflags "$SRC_DIR/metagen.c" -o "$BUILD_DIR/$METAGEN_EXEC" \
    $COMMON_LIBS -I"$SRC_DIR" -Iexternal
  printf '[metagen]    built %s (%s, %ds)\n' "$METAGEN_EXEC" "$MODE" "$((SECONDS - t0))"
}

run_metagen() {
  printf '[run]     %s\n' "$METAGEN_EXEC"
  if [ "$MODE" = "debug-sanitize" ]; then
    ASAN_OPTIONS=detect_leaks=0 "$BUILD_DIR/$METAGEN_EXEC" $METAGEN_TEST_PATH
  else
    "$BUILD_DIR/$METAGEN_EXEC" $METAGEN_TEST_PATH
  fi
}

build_main() {
  local cflags; cflags=$(cflags_for_mode "$MODE")
  local t0=$SECONDS
  gcc $cflags "$SRC_DIR/main.c" -o "$BUILD_DIR/$MAIN_EXEC" \
    $COMMON_LIBS -I"$SRC_DIR" -Iexternal
  printf '[main]     built %s (%s, %ds)\n' "$MAIN_EXEC" "$MODE" "$((SECONDS - t0))"
}

run_main() {
  printf '[run]     %s\n' "$MAIN_EXEC"
  if [ "$MODE" = "debug-sanitize" ]; then
    ASAN_OPTIONS=detect_leaks=0 "$BUILD_DIR/$MAIN_EXEC" $MAIN_TEST_PATH
  else
    "$BUILD_DIR/$MAIN_EXEC" $MAIN_TEST_PATH
  fi
}

build_tests() {
  local cflags; cflags=$(cflags_for_mode "$MODE")
  local src base out t0
  for src in "$TESTS_DIR"/*.c; do
    [ -e "$src" ] || continue
    base=$(basename "$src" .c)
    out="$BUILD_DIR/test_$base"
    t0=$SECONDS
    gcc $cflags "$src" -o "$out" \
      $COMMON_LIBS -I"$SRC_DIR" -Iexternal
    printf '[tests]    built %s (%s, %ds)\n' "test_$base" "$MODE" "$((SECONDS - t0))"
  done
}

run_tests() {
  local base out
  for src in "$TESTS_DIR"/*.c; do
    [ -e "$src" ] || continue
    base=$(basename "$src" .c)
    out="$BUILD_DIR/test_$base"
    printf '[run]     test_%s\n' "$base"
    if [ "$MODE" = "debug-sanitize" ]; then
      ASAN_OPTIONS=detect_leaks=0 "$out"
    else
      "$out"
    fi
  done
}

mkdir -p "$BUILD_DIR"

case "$SUBCMD" in
  all)
    build_metagen
    run_metagen
    build_main
    run_main
    ;;
  build)
    build_metagen
    run_metagen
    build_main
    ;;
  main)
    build_main
    ;;
  metagen)
    build_metagen
    run_metagen
    ;;
  run)
    run_main
    ;;
  test|tests)
    build_tests
    run_tests
    ;;
  *)
    cat <<EOF >&2
Invalid command
EOF
    exit 1
    ;;
esac
