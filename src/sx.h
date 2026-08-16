#ifndef SX_H
#define SX_H

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
    lexer->column = 1;
  } else {
    lexer->column++;
  }
}
static inline bool sx_is_delimiter(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '(' ||
         c == ')' || c == '"' || c == ';';
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
  case '"': {
    sx_lexer_advance(lexer);
    for (;;) {
      if (lexer->pos >= lexer->len) {
        return (SxToken){SX_TOKEN_ERROR,
                         {lexer->data + start, lexer->pos - start},
                         line,
                         column};
      }
      char ch = lexer->data[lexer->pos];
      if (ch == '\n' || ch == '\r') {
        return (SxToken){SX_TOKEN_ERROR,
                         {lexer->data + start, lexer->pos - start},
                         line,
                         column};
      }
      if (ch == '\\') {
        sx_lexer_advance(lexer);
        if (lexer->pos >= lexer->len) {
          return (SxToken){SX_TOKEN_ERROR,
                           {lexer->data + start, lexer->pos - start},
                           line,
                           column};
        }
        char esc = lexer->data[lexer->pos];
        if (esc == '\n' || esc == '\r') {
          return (SxToken){SX_TOKEN_ERROR,
                           {lexer->data + start, lexer->pos - start},
                           line,
                           column};
        }
        sx_lexer_advance(lexer);
        continue;
      }
      if (ch == '"') {
        sx_lexer_advance(lexer); // closing quote
        return (SxToken){SX_TOKEN_STRING,
                         {lexer->data + start, lexer->pos - start},
                         line,
                         column};
      }
      sx_lexer_advance(lexer);
    }
  }

  default:
    break;
  }

  // consume until delimiter
  while (lexer->pos < lexer->len && !sx_is_delimiter(lexer->data[lexer->pos])) {
    sx_lexer_advance(lexer);
  }

  SxSlice text = {lexer->data + start, lexer->pos - start};

  // classify NUMBER vs SYMBOL
  u64 i = 0;

  if (i < text.len && (text.data[i] == '+' || text.data[i] == '-')) {
    i++;
  }

  u64 digits_start = i;
  while (i < text.len && text.data[i] >= '0' && text.data[i] <= '9') {
    i++;
  }

  bool is_number = i > digits_start;

  if (is_number && i < text.len && text.data[i] == '.') {
    i++;

    u64 frac_start = i;
    while (i < text.len && text.data[i] >= '0' && text.data[i] <= '9') {
      i++;
    }

    is_number = i > frac_start;
  }

  is_number = is_number && i == text.len;
  if (is_number) {
    return (SxToken){SX_TOKEN_NUMBER, text, line, column};
  }

  return (SxToken){SX_TOKEN_SYMBOL, text, line, column};
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
static inline bool sx_token_text_eq(SxToken tok, const char *expected) {
  u64 elen = (u64)strlen(expected);
  return tok.text.len == elen && memcmp(tok.text.data, expected, elen) == 0;
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

#endif // SX_H
