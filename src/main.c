#define BASE_IMPLEMENTATION
#include "base.h"
#include "sx.h"

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Please input a config file\n");
    return 1;
  }
  printf("%s\n", argv[1]);
  Arena *arena = arena_create(ARENA_DEFAULT_BLOCK_SIZE);
  Str out;
  read_file(arena, S(argv[1]), &out);
  SxSlice source = {.data = (const char *)out.data, .len = out.length};
  sx_print_tokens(source);
}
