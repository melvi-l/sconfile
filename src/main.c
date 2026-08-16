#define BASE_IMPLEMENTATION
#include "base.h"
#include "sx.h"

// utils
static inline const char *sx_token_kind_string(SxTokenKind kind) {
  switch (kind) {
  case SX_TOKEN_EOF:
    return "EOF";
  case SX_TOKEN_LPAREN:
    return "LPAREN";
  case SX_TOKEN_RPAREN:
    return "RPAREN";
  case SX_TOKEN_SYMBOL:
    return "SYMBOL";
  case SX_TOKEN_STRING:
    return "STRING";
  case SX_TOKEN_NUMBER:
    return "NUMBER";
  case SX_TOKEN_ERROR:
    return "ERROR";
  }

  return "UNKNOWN";
}
static inline void sx_print_tokens(SxSlice source) {
  SxLexer lexer = {
      .data = source.data,
      .len = source.len,
      .pos = 0,
      .line = 1,
      .column = 1,
  };

  for (;;) {
    SxToken token = sx_next_token(&lexer);

    printf("%u:%u %-8s `%.*s`\n", token.line, token.column,
           sx_token_kind_string(token.kind), (int)token.text.len,
           token.text.data);

    if (token.kind == SX_TOKEN_EOF || token.kind == SX_TOKEN_ERROR) {
      break;
    }
  }
}

static inline const char *sx_node_kind_string(SxNodeKind kind) {
  switch (kind) {
  case SX_NODE_ROOT:
    return "ROOT";
  case SX_NODE_LIST:
    return "LIST";
  case SX_NODE_SYMBOL:
    return "SYMBOL";
  case SX_NODE_STRING:
    return "STRING";
  case SX_NODE_NUMBER:
    return "NUMBER";
  }

  return "UNKNOWN";
}

static inline void sx_print_node(SxNode *node, u32 depth) {
  for (u32 i = 0; i < depth; i++) {
    printf("  ");
  }

  printf("%u:%u %-8s ", node->line, node->column,
         sx_node_kind_string(node->kind));

  if (node->kind == SX_NODE_LIST || node->kind == SX_NODE_ROOT) {
    u32 count = 0;
    for (SxNode *c = node->first_child; c; c = c->next) {
      count++;
    }
    printf("(%u children)\n", count);
    for (SxNode *child = node->first_child; child; child = child->next) {
      sx_print_node(child, depth + 1);
    }
  } else {
    printf("`%.*s`\n", (int)node->text.len, node->text.data);
  }
}

static inline void sx_print_ast(SxNode *root) { sx_print_node(root, 0); }

static inline const char *sx_error_code_string(SxErrorCode code) {
  switch (code) {
  case SX_ERROR_NONE:
    return "NONE";
  case SX_ERROR_INVALID_TOKEN:
    return "INVALID_TOKEN";
  case SX_ERROR_EXPECTED_ATOM:
    return "EXPECTED_ATOM";
  case SX_ERROR_UNEXPECTED_RPAREN:
    return "UNEXPECTED_RPAREN";
  case SX_ERROR_UNEXPECTED_EOF:
    return "UNEXPECTED_EOF";
  case SX_ERROR_MISSING_RPAREN:
    return "MISSING_RPAREN";
  case SX_ERROR_OUT_OF_MEMORY:
    return "OUT_OF_MEMORY";
  }

  return "UNKNOWN";
}

static inline void sx_print_error_report(SxParser *parser) {
  printf("\n=== SxParser error report ===\n");

  if (parser->error.code == SX_ERROR_NONE) {
    printf("no errors\n");
    return;
  }

  u32 count = 0;
  for (SxError *e = &parser->error; e; e = e->next) {
    count++;
    printf("  [%u] %u:%u %s\n", count, e->line, e->column,
           sx_error_code_string(e->code));
  }

  printf("====== %u error(s) total =====\n", count);
}

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
  SxParser parser = {
      .lexer =
          {
              .data = source.data,
              .len = source.len,
              .pos = 0,
              .line = 1,
              .column = 1,
          },
      .arena = arena,
      .error = {SX_ERROR_NONE, 0, 0, NULL},
  };
  advance_parser(&parser);

  SxNode *root = sx_parse_document(&parser);
  sx_print_ast(root);
  sx_print_error_report(&parser);
  return parser.error.code == SX_ERROR_NONE ? 0 : 1;
}
