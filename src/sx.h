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

// ast
typedef enum SxNodeKind {
  SX_NODE_ROOT,
  SX_NODE_LIST,
  SX_NODE_SYMBOL,
  SX_NODE_STRING,
  SX_NODE_NUMBER
} SxNodeKind;

typedef struct SxNode SxNode;
struct SxNode {
  SxNodeKind kind;

  SxSlice text;

  SxNode *parent;
  SxNode *first_child;
  SxNode *last_child;
  SxNode *next;

  u32 line;
  u32 column;
};

typedef enum SxErrorCode {
  SX_ERROR_NONE = 0,

  SX_ERROR_INVALID_TOKEN,
  SX_ERROR_EXPECTED_ATOM,
  SX_ERROR_UNEXPECTED_RPAREN,
  SX_ERROR_UNEXPECTED_EOF,
  SX_ERROR_MISSING_RPAREN,
  SX_ERROR_OUT_OF_MEMORY
} SxErrorCode;

typedef struct SxError {
  SxErrorCode code;
  uint32_t line;
  uint32_t column;
} SxError;

typedef struct SxParser {
  SxLexer lexer;
  SxToken current;
  Arena *arena;
  SxError error;
} SxParser;

#define advance_parser(p) (p)->current = sx_next_token(&(p)->lexer)
static inline SxNode *sx_parse(SxParser *p) {
  SxNode *node = ARENA_PUSH_STRUCT(p->arena, SxNode);
  *node = (SxNode){0};
  switch (p->current.kind) {
  case (SX_TOKEN_LPAREN):
    // parse list
    node->kind = SX_NODE_LIST;
    node->line = p->current.line;
    node->column = p->current.column;

    advance_parser(p);
    while (p->current.kind != SX_TOKEN_RPAREN) {
      if (p->current.kind == SX_TOKEN_EOF) {
        // missing right paren SX_ERROR_MISSING_RPAREN
        assert(false);
      }
      SxNode *child = sx_parse(p);
      child->parent = node;
      if (node->last_child == NULL) {
        // first
        assert(node->first_child == NULL);
        node->first_child = child;
      } else {
        assert(node->first_child != NULL);
        node->last_child->next = child;
      }
      node->last_child = child;
    }

    // consume right paren
    advance_parser(p);
    break;
  case (SX_TOKEN_SYMBOL):
    node->kind = SX_NODE_SYMBOL;
    node->text = p->current.text;
    node->line = p->current.line;
    node->column = p->current.column;

    advance_parser(p);
    break;
  case (SX_TOKEN_STRING):
    node->kind = SX_NODE_STRING;
    node->text = p->current.text;
    node->line = p->current.line;
    node->column = p->current.column;

    advance_parser(p);
    break;
  case (SX_TOKEN_NUMBER):
    node->kind = SX_NODE_NUMBER;
    node->text = p->current.text;
    node->line = p->current.line;
    node->column = p->current.column;

    advance_parser(p);
    break;
  case SX_TOKEN_RPAREN: {
    /* SX_ERROR_UNEXPECTED_RPAREN */
    assert(false);
  } break;

  case SX_TOKEN_EOF: {
    /* SX_ERROR_UNEXPECTED_EOF */
    assert(false);
  } break;

  case SX_TOKEN_ERROR: {
    /* SX_ERROR_LEXER */
    assert(false);
  } break;
  }

  return node;
}

static inline SxNode *sx_parse_document(SxParser *p) {
  SxNode *root = ARENA_PUSH_STRUCT(p->arena, SxNode);
  *root = (SxNode){SX_NODE_ROOT};

  while (p->current.kind != SX_TOKEN_EOF) {
    SxNode *child = sx_parse(p);

    child->parent = root;

    if (root->last_child) {
      root->last_child->next = child;
    } else {
      root->first_child = child;
    }

    root->last_child = child;
  }

  return root;
}

#endif // SX_H
