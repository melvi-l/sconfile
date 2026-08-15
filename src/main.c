#define BASE_IMPLEMENTATION
#include "base.h"

// @lexer
typedef enum SxTokenKind {
  SX_TOKEN_EOF,
  SX_TOKEN_LPAREN,
  SX_TOKEN_RPAREN,
  SX_TOKEN_SYMBOL,
  SX_TOKEN_STRING,
  SX_TOKEN_NUMBER,
  SX_TOKEN_ERROR
} SxTokenKind;

#define char_slice(lexer)                                                      \
  (SxSlice){.data = (lexer)->data + (lexer)->pos, .len = 1}
typedef struct SxSlice {
  const char *data;
  u64 len;
} SxSlice;

typedef struct SxToken {
  SxTokenKind kind;
  SxSlice text;

  u32 line;
  u32 column;
} SxToken;

typedef struct SxLexer {
  const char *data;
  u64 len;
  u64 pos;

  u32 line;
  u32 column;
} SxLexer;

static inline void sx_lexer_advance(SxLexer *lexer) {
  const char curr = lexer->data[lexer->pos++];
  if (curr == '\n') {
    lexer->line++;
    lexer->column = 0;
  } else {
    lexer->column++;
  }
}
static inline SxToken sx_next_token(SxLexer *lexer) {
  // sanitize
  for (;;) {
    if (lexer->pos >= lexer->len) {
      return (SxToken){SX_TOKEN_EOF,
                       {lexer->data + lexer->pos, 0},
                       lexer->line,
                       lexer->column};
    }

    const char c = lexer->data[lexer->pos];

    // skip whitespace
    bool is_whitespace = c == ' ' || c == '\t' || c == '\n' || c == '\r';
    if (is_whitespace) {
      sx_lexer_advance(lexer);
      continue;
    }

    // skip comment
    bool is_comment_start = c == ';';
    if (is_comment_start) {
      while (lexer->pos < lexer->len && lexer->data[lexer->pos] != '\n') {
        sx_lexer_advance(lexer);
      }
      continue;
    }

    break;
  }

  u64 start = lexer->pos;
  u32 line = lexer->line;
  u32 column = lexer->column;

  const char c = lexer->data[lexer->pos];
  switch (c) {
  case '(':
    sx_lexer_advance(lexer);
    return (SxToken){SX_TOKEN_LPAREN, {lexer->data + start, 1}, line, column};
  case ')':
    sx_lexer_advance(lexer);
    return (SxToken){SX_TOKEN_RPAREN, {lexer->data + start, 1}, line, column};
  case '"':
    sx_lexer_advance(lexer);
    while (lexer->pos < lexer->len) {
      if (lexer->data[lexer->pos] != '"') {
        sx_lexer_advance(lexer);
        continue;
      }
      sx_lexer_advance(lexer);
      return (SxToken){SX_TOKEN_STRING,
                       {lexer->data + start, lexer->pos - start},
                       line,
                       column};
    }
    return (SxToken){SX_TOKEN_ERROR,
                     {lexer->data + start, lexer->pos - start},
                     line,
                     column};

  case '+':
  case '-':
    if (lexer->pos + 1 >= lexer->len || lexer->data[lexer->pos + 1] < '0' ||
        lexer->data[lexer->pos + 1] > '9') {
      break;
    }
    sx_lexer_advance(lexer);
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    while (lexer->pos < lexer->len && lexer->data[lexer->pos] >= '0' &&
           lexer->data[lexer->pos] <= '9') {
      sx_lexer_advance(lexer);
    }
    if (lexer->data[lexer->pos] == '.') {
      sx_lexer_advance(lexer);
      while (lexer->pos < lexer->len && lexer->data[lexer->pos] >= '0' &&
             lexer->data[lexer->pos] <= '9') {
        sx_lexer_advance(lexer);
      }
    }
    return (SxToken){SX_TOKEN_NUMBER,
                     {lexer->data + start, lexer->pos - start},
                     line,
                     column};

  default:
    break;
  }
  while (lexer->pos < lexer->len) {
    const char c = lexer->data[lexer->pos];

    bool is_delimiter = c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
                        c == '(' || c == ')' || c == '"' || c == ';';
    if (is_delimiter) {
      break;
    }

    sx_lexer_advance(lexer);
  }
  if (lexer->pos == start) {
    sx_lexer_advance(lexer);
    return (SxToken){
        SX_TOKEN_ERROR,
        {
            lexer->data + start,
            lexer->pos - start,
        },
        line,
        column,
    };
  }

  return (SxToken){
      SX_TOKEN_SYMBOL,
      {
          lexer->data + start,
          lexer->pos - start,
      },
      line,
      column,
  };
}

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

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Please input a config file\n");
  }
  printf("%s\n", argv[1]);
  Arena *arena = arena_create(ARENA_DEFAULT_BLOCK_SIZE);
  Str out;
  read_file(arena, S(argv[1]), &out);
  SxSlice source = {.data = (const char *)out.data, .len = out.length};
  sx_print_tokens(source);
}
