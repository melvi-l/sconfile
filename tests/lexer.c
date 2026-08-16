#include "../src/sx.h"

#include <assert.h>

static int tests_run = 0;
static int tests_failed = 0;

#define TEST(name) static void name(void)
#define RUN(name)                                                              \
  do {                                                                         \
    int failed_before = tests_failed;                                          \
    tests_run++;                                                               \
    printf("  [test] %-64s ... ", #name);                                      \
    name();                                                                    \
    if (tests_failed == failed_before) {                                       \
      printf("OK\n");                                                          \
    }                                                                          \
  } while (0)

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("FAIL\n    %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
      tests_failed++;                                                          \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define CHECK_TOKEN_TEXT(tok, expected)                                        \
  do {                                                                         \
    u64 _elen = (u64)strlen(expected);                                         \
    if ((tok).text.len != _elen ||                                             \
        memcmp((tok).text.data, (expected), _elen) != 0) {                     \
      printf("FAIL\n    %s:%d: token text \"%.*s\" != \"%s\"\n", __FILE__,     \
             __LINE__, (int)(tok).text.len, (tok).text.data, (expected));      \
      tests_failed++;                                                          \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define EXPECT_TOKEN(lx_ptr, expected_kind, expected_text)                     \
  do {                                                                         \
    SxToken _t = sx_next_token(lx_ptr);                                        \
    CHECK(_t.kind == (expected_kind));                                         \
    CHECK_TOKEN_TEXT(_t, expected_text);                                       \
  } while (0)

static SxLexer make_lexer(const char *src) {
  return (SxLexer){
      .data = src,
      .len = (u64)strlen(src),
      .pos = 0,
      .line = 1,
      .column = 1,
  };
}

static bool sx_token_text_eq(SxToken tok, const char *expected) {
  u64 elen = (u64)strlen(expected);
  return tok.text.len == elen && memcmp(tok.text.data, expected, elen) == 0;
}

TEST(test_lparen) {
  SxLexer lx = make_lexer("(");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_LPAREN);
  CHECK_TOKEN_TEXT(t, "(");
}

TEST(test_rparen) {
  SxLexer lx = make_lexer(")");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_RPAREN);
  CHECK_TOKEN_TEXT(t, ")");
}

TEST(test_symbol_simple) {
  SxLexer lx = make_lexer("hello");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_SYMBOL);
  CHECK_TOKEN_TEXT(t, "hello");
}

TEST(test_symbol_variants) {
  const char *syms[] = {"snake_case", "kebab-case", "camelCase",
                        "PascalCase", "symbol123",  "<="};
  for (u64 i = 0; i < sizeof(syms) / sizeof(syms[0]); i++) {
    SxLexer lx = make_lexer(syms[i]);
    SxToken t = sx_next_token(&lx);
    CHECK(t.kind == SX_TOKEN_SYMBOL);
    CHECK_TOKEN_TEXT(t, syms[i]);
  }
}

TEST(test_symbol_representative) {
  const char *syms[] = {"font-size", "max-length", "foo.bar",      "?",    "!",
                        "*",         "/",          "EditorConfig", "Color"};
  for (u64 i = 0; i < sizeof(syms) / sizeof(syms[0]); i++) {
    SxLexer lx = make_lexer(syms[i]);
    SxToken t = sx_next_token(&lx);
    CHECK(t.kind == SX_TOKEN_SYMBOL);
    CHECK_TOKEN_TEXT(t, syms[i]);
  }
}

TEST(test_number_integer) {
  SxLexer lx = make_lexer("42");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_NUMBER);
  CHECK_TOKEN_TEXT(t, "42");
}

TEST(test_number_zero) {
  SxLexer lx = make_lexer("0");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_NUMBER);
  CHECK_TOKEN_TEXT(t, "0");
}

TEST(test_number_signed_positive) {
  SxLexer lx = make_lexer("+42");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_NUMBER);
  CHECK_TOKEN_TEXT(t, "+42");
}

TEST(test_number_signed_negative) {
  SxLexer lx = make_lexer("-42");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_NUMBER);
  CHECK_TOKEN_TEXT(t, "-42");
}

TEST(test_number_decimal) {
  SxLexer lx = make_lexer("3.14");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_NUMBER);
  CHECK_TOKEN_TEXT(t, "3.14");
}

TEST(test_number_signed_decimal) {
  SxLexer lx = make_lexer("-0.25");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_NUMBER);
  CHECK_TOKEN_TEXT(t, "-0.25");
}

TEST(test_number_plus_decimal) {
  SxLexer lx = make_lexer("+3.14");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_NUMBER);
  CHECK_TOKEN_TEXT(t, "+3.14");
}

TEST(test_number_double_zero) {
  SxLexer lx = make_lexer("00");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_NUMBER);
  CHECK_TOKEN_TEXT(t, "00");
}

TEST(test_number_leading_zero) {
  SxLexer lx = make_lexer("01");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_NUMBER);
  CHECK_TOKEN_TEXT(t, "01");
}

TEST(test_number_trailing_dot_is_symbol) {
  // digits required after '.'
  SxLexer lx = make_lexer("1.");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_SYMBOL);
  CHECK_TOKEN_TEXT(t, "1.");
}

TEST(test_number_leading_dot_is_symbol) {
  SxLexer lx = make_lexer(".5");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_SYMBOL);
  CHECK_TOKEN_TEXT(t, ".5");
}

TEST(test_number_signed_leading_dot_is_symbol) {
  SxLexer lx = make_lexer("-.5");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_SYMBOL);
  CHECK_TOKEN_TEXT(t, "-.5");
}

TEST(test_number_double_dot_is_symbol) {
  SxLexer lx = make_lexer("1.2.3");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_SYMBOL);
  CHECK_TOKEN_TEXT(t, "1.2.3");
}

TEST(test_number_trailing_alpha_is_symbol) {
  SxLexer lx = make_lexer("12foo");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_SYMBOL);
  CHECK_TOKEN_TEXT(t, "12foo");
}

TEST(test_dot_alone_is_symbol) {
  SxLexer lx = make_lexer(".");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_SYMBOL);
  CHECK_TOKEN_TEXT(t, ".");
}

TEST(test_plus_dot_is_symbol) {
  SxLexer lx = make_lexer("+.");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_SYMBOL);
  CHECK_TOKEN_TEXT(t, "+.");
}

TEST(test_plus_alone_is_symbol) {
  SxLexer lx = make_lexer("+");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_SYMBOL);
  CHECK_TOKEN_TEXT(t, "+");
}

TEST(test_minus_alone_is_symbol) {
  SxLexer lx = make_lexer("-");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_SYMBOL);
  CHECK_TOKEN_TEXT(t, "-");
}

TEST(test_string) {
  SxLexer lx = make_lexer("\"hello\"");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_STRING);
  CHECK_TOKEN_TEXT(t, "\"hello\"");
}

TEST(test_string_empty) {
  SxLexer lx = make_lexer("\"\"");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_STRING);
  CHECK_TOKEN_TEXT(t, "\"\"");
}

TEST(test_string_with_spaces) {
  SxLexer lx = make_lexer("\"hello world\"");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_STRING);
  CHECK_TOKEN_TEXT(t, "\"hello world\"");
}

TEST(test_string_keeps_quotes) {
  // retains the surrounding quotes
  SxLexer lx = make_lexer("\"hello\"");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_STRING);
  CHECK_TOKEN_TEXT(t, "\"hello\"");
  CHECK(t.text.len == 7);
  CHECK(t.text.data[0] == '"');
  CHECK(t.text.data[t.text.len - 1] == '"');
}

TEST(test_string_with_semicolon) {
  // ';' inside a string is not a comment
  SxLexer lx = make_lexer("\"foo;bar\"");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_STRING);
  CHECK_TOKEN_TEXT(t, "\"foo;bar\"");
}

TEST(test_string_with_parens) {
  // parens inside a string are literal
  SxLexer lx = make_lexer("\"(foo)\"");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_STRING);
  CHECK_TOKEN_TEXT(t, "\"(foo)\"");
}

TEST(test_string_escaped_quotes) {
  // escaped quotes do not terminate the string; text keeps backslashes
  SxLexer lx = make_lexer("\"foo \\\"bar\\\"\""); // "foo \"bar\""
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_STRING);
  CHECK_TOKEN_TEXT(t, "\"foo \\\"bar\\\"\"");
}

TEST(test_string_escaped_backslash) {
  // escaped backslash: "\\" is not a string terminator context
  SxLexer lx = make_lexer("\"foo\\\\bar\"");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_STRING);
  CHECK_TOKEN_TEXT(t, "\"foo\\\\bar\"");
}

TEST(test_string_newline_is_error) {
  // raw newline inside a string is forbidden
  SxLexer lx = make_lexer("\"foo\nbar\"");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_ERROR);
  CHECK_TOKEN_TEXT(t, "\"foo");
}

TEST(test_unterminated_string_is_error) {
  SxLexer lx = make_lexer("\"unterminated");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_ERROR);
  CHECK_TOKEN_TEXT(t, "\"unterminated");
}

TEST(test_eof_empty_input) {
  SxLexer lx = make_lexer("");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_EOF);
  CHECK_TOKEN_TEXT(t, "");
}

TEST(test_whitespace_skipped) {
  SxLexer lx = make_lexer("   \t\n  hello");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_SYMBOL);
  CHECK_TOKEN_TEXT(t, "hello");
}

TEST(test_comment_full_line_skipped) {
  SxLexer lx = make_lexer("; this is a comment\nhello");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_SYMBOL);
  CHECK_TOKEN_TEXT(t, "hello");
}

TEST(test_comment_inline_skipped) {
  SxLexer lx = make_lexer("(a ; trailing comment\n)");
  EXPECT_TOKEN(&lx, SX_TOKEN_LPAREN, "(");
  EXPECT_TOKEN(&lx, SX_TOKEN_SYMBOL, "a");
  EXPECT_TOKEN(&lx, SX_TOKEN_RPAREN, ")");
  EXPECT_TOKEN(&lx, SX_TOKEN_EOF, "");
}

TEST(test_comment_to_eof_alone) {
  // comment with no trailing newline runs to EOF
  SxLexer lx = make_lexer("; comment");
  EXPECT_TOKEN(&lx, SX_TOKEN_EOF, "");
}

TEST(test_comment_to_eof_after_atom) {
  SxLexer lx = make_lexer("foo; comment");
  EXPECT_TOKEN(&lx, SX_TOKEN_SYMBOL, "foo");
  EXPECT_TOKEN(&lx, SX_TOKEN_EOF, "");
}

TEST(test_list_tokens) {
  SxLexer lx = make_lexer("(a 1)");
  EXPECT_TOKEN(&lx, SX_TOKEN_LPAREN, "(");
  EXPECT_TOKEN(&lx, SX_TOKEN_SYMBOL, "a");
  EXPECT_TOKEN(&lx, SX_TOKEN_NUMBER, "1");
  EXPECT_TOKEN(&lx, SX_TOKEN_RPAREN, ")");
  EXPECT_TOKEN(&lx, SX_TOKEN_EOF, "");
}

TEST(test_nested_list) {
  SxLexer lx = make_lexer("(a (b c) d)");
  EXPECT_TOKEN(&lx, SX_TOKEN_LPAREN, "(");
  EXPECT_TOKEN(&lx, SX_TOKEN_SYMBOL, "a");
  EXPECT_TOKEN(&lx, SX_TOKEN_LPAREN, "(");
  EXPECT_TOKEN(&lx, SX_TOKEN_SYMBOL, "b");
  EXPECT_TOKEN(&lx, SX_TOKEN_SYMBOL, "c");
  EXPECT_TOKEN(&lx, SX_TOKEN_RPAREN, ")");
  EXPECT_TOKEN(&lx, SX_TOKEN_SYMBOL, "d");
  EXPECT_TOKEN(&lx, SX_TOKEN_RPAREN, ")");
  EXPECT_TOKEN(&lx, SX_TOKEN_EOF, "");
}

TEST(test_adjacent_parens_foo) {
  // tokens adjacent without whitespace
  SxLexer lx = make_lexer("(foo)");
  EXPECT_TOKEN(&lx, SX_TOKEN_LPAREN, "(");
  EXPECT_TOKEN(&lx, SX_TOKEN_SYMBOL, "foo");
  EXPECT_TOKEN(&lx, SX_TOKEN_RPAREN, ")");
  EXPECT_TOKEN(&lx, SX_TOKEN_EOF, "");
}

TEST(test_adjacent_parens_nested) {
  SxLexer lx = make_lexer("((a))");
  EXPECT_TOKEN(&lx, SX_TOKEN_LPAREN, "(");
  EXPECT_TOKEN(&lx, SX_TOKEN_LPAREN, "(");
  EXPECT_TOKEN(&lx, SX_TOKEN_SYMBOL, "a");
  EXPECT_TOKEN(&lx, SX_TOKEN_RPAREN, ")");
  EXPECT_TOKEN(&lx, SX_TOKEN_RPAREN, ")");
  EXPECT_TOKEN(&lx, SX_TOKEN_EOF, "");
}

TEST(test_adjacent_parens_mixed) {
  SxLexer lx = make_lexer("(a(b)c)");
  EXPECT_TOKEN(&lx, SX_TOKEN_LPAREN, "(");
  EXPECT_TOKEN(&lx, SX_TOKEN_SYMBOL, "a");
  EXPECT_TOKEN(&lx, SX_TOKEN_LPAREN, "(");
  EXPECT_TOKEN(&lx, SX_TOKEN_SYMBOL, "b");
  EXPECT_TOKEN(&lx, SX_TOKEN_RPAREN, ")");
  EXPECT_TOKEN(&lx, SX_TOKEN_SYMBOL, "c");
  EXPECT_TOKEN(&lx, SX_TOKEN_RPAREN, ")");
  EXPECT_TOKEN(&lx, SX_TOKEN_EOF, "");
}

TEST(test_unbalanced_lparens) {
  // lexer does not balance parens; no error at lexer level
  SxLexer lx = make_lexer("(((");
  EXPECT_TOKEN(&lx, SX_TOKEN_LPAREN, "(");
  EXPECT_TOKEN(&lx, SX_TOKEN_LPAREN, "(");
  EXPECT_TOKEN(&lx, SX_TOKEN_LPAREN, "(");
  EXPECT_TOKEN(&lx, SX_TOKEN_EOF, "");
}

TEST(test_unbalanced_rparens) {
  SxLexer lx = make_lexer(")))");
  EXPECT_TOKEN(&lx, SX_TOKEN_RPAREN, ")");
  EXPECT_TOKEN(&lx, SX_TOKEN_RPAREN, ")");
  EXPECT_TOKEN(&lx, SX_TOKEN_RPAREN, ")");
  EXPECT_TOKEN(&lx, SX_TOKEN_EOF, "");
}

TEST(test_multiple_tokens_sequential) {
  SxLexer lx = make_lexer("(foo \"bar\" 42 baz)");
  EXPECT_TOKEN(&lx, SX_TOKEN_LPAREN, "(");
  EXPECT_TOKEN(&lx, SX_TOKEN_SYMBOL, "foo");
  EXPECT_TOKEN(&lx, SX_TOKEN_STRING, "\"bar\"");
  EXPECT_TOKEN(&lx, SX_TOKEN_NUMBER, "42");
  EXPECT_TOKEN(&lx, SX_TOKEN_SYMBOL, "baz");
  EXPECT_TOKEN(&lx, SX_TOKEN_RPAREN, ")");
  EXPECT_TOKEN(&lx, SX_TOKEN_EOF, "");
}

TEST(test_token_text_eq_helper) {
  SxLexer lx = make_lexer("hello");
  SxToken t = sx_next_token(&lx);
  CHECK(sx_token_text_eq(t, "hello"));
  CHECK(!sx_token_text_eq(t, "hell"));
  CHECK(!sx_token_text_eq(t, "helloo"));
  SxToken e =
      (SxToken){SX_TOKEN_EOF, {lx.data + lx.pos, 0}, lx.line, lx.column};
  CHECK(sx_token_text_eq(e, ""));
  CHECK(!sx_token_text_eq(e, "x"));
}

TEST(test_line_column_tracking) {
  SxLexer lx = make_lexer("a\nb");
  SxToken t = sx_next_token(&lx);
  CHECK(t.line == 1);
  CHECK(t.column == 1);
  t = sx_next_token(&lx);
  CHECK(t.line == 2);
  CHECK(t.column == 1);
}

TEST(test_column_after_newline_starts_at_one) {
  // line/column are strictly 1-based; after '\n' the next char is column 1
  SxLexer lx = make_lexer("\nx");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_SYMBOL);
  CHECK_TOKEN_TEXT(t, "x");
  CHECK(t.line == 2);
  CHECK(t.column == 1);
}

TEST(test_crlf_line_column) {
  // CRLF: after "\r\n" the next token is at line=2, column=1
  SxLexer lx = make_lexer("foo\r\nbar");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_SYMBOL);
  CHECK_TOKEN_TEXT(t, "foo");
  CHECK(t.line == 1);
  CHECK(t.column == 1);
  t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_SYMBOL);
  CHECK_TOKEN_TEXT(t, "bar");
  CHECK(t.line == 2);
  CHECK(t.column == 1);
}

TEST(test_indented_positions) {
  // (foo
  //   bar 42)
  SxLexer lx = make_lexer("(foo\n  bar 42)");

  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_LPAREN);
  CHECK_TOKEN_TEXT(t, "(");
  CHECK(t.line == 1);
  CHECK(t.column == 1);

  t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_SYMBOL);
  CHECK_TOKEN_TEXT(t, "foo");
  CHECK(t.line == 1);
  CHECK(t.column == 2);

  t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_SYMBOL);
  CHECK_TOKEN_TEXT(t, "bar");
  CHECK(t.line == 2);
  CHECK(t.column == 3);

  t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_NUMBER);
  CHECK_TOKEN_TEXT(t, "42");
  CHECK(t.line == 2);
  CHECK(t.column == 7);

  t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_RPAREN);
  CHECK_TOKEN_TEXT(t, ")");
  CHECK(t.line == 2);
  CHECK(t.column == 9);

  t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_EOF);
  CHECK(t.line == 2);
  CHECK(t.column == 10);
}

TEST(test_position_after_comment_indent) {
  // ; comment\n  bar -> bar at line=2, column=3
  SxLexer lx = make_lexer("; comment\n  bar");
  SxToken t = sx_next_token(&lx);
  CHECK(t.kind == SX_TOKEN_SYMBOL);
  CHECK_TOKEN_TEXT(t, "bar");
  CHECK(t.line == 2);
  CHECK(t.column == 3);
}

TEST(test_eof_stable) {
  // repeated calls after EOF keep returning EOF
  SxLexer lx = make_lexer("foo");
  EXPECT_TOKEN(&lx, SX_TOKEN_SYMBOL, "foo");
  EXPECT_TOKEN(&lx, SX_TOKEN_EOF, "");
  EXPECT_TOKEN(&lx, SX_TOKEN_EOF, "");
  EXPECT_TOKEN(&lx, SX_TOKEN_EOF, "");
}

TEST(test_realistic_editor_config) {
  SxLexer lx = make_lexer("  (font\n"
                          "    (path \"assets/fonts/mono.bdf\")\n"
                          "    (size 16))\n"
                          "  (wrap true)\n"
                          "  (background \"#181818\"))");

  EXPECT_TOKEN(&lx, SX_TOKEN_LPAREN, "(");
  EXPECT_TOKEN(&lx, SX_TOKEN_SYMBOL, "font");
  EXPECT_TOKEN(&lx, SX_TOKEN_LPAREN, "(");
  EXPECT_TOKEN(&lx, SX_TOKEN_SYMBOL, "path");
  EXPECT_TOKEN(&lx, SX_TOKEN_STRING, "\"assets/fonts/mono.bdf\"");
  EXPECT_TOKEN(&lx, SX_TOKEN_RPAREN, ")");
  EXPECT_TOKEN(&lx, SX_TOKEN_LPAREN, "(");
  EXPECT_TOKEN(&lx, SX_TOKEN_SYMBOL, "size");
  EXPECT_TOKEN(&lx, SX_TOKEN_NUMBER, "16");
  EXPECT_TOKEN(&lx, SX_TOKEN_RPAREN, ")");
  EXPECT_TOKEN(&lx, SX_TOKEN_RPAREN, ")");
  EXPECT_TOKEN(&lx, SX_TOKEN_LPAREN, "(");
  EXPECT_TOKEN(&lx, SX_TOKEN_SYMBOL, "wrap");
  EXPECT_TOKEN(&lx, SX_TOKEN_SYMBOL, "true");
  EXPECT_TOKEN(&lx, SX_TOKEN_RPAREN, ")");
  EXPECT_TOKEN(&lx, SX_TOKEN_LPAREN, "(");
  EXPECT_TOKEN(&lx, SX_TOKEN_SYMBOL, "background");
  EXPECT_TOKEN(&lx, SX_TOKEN_STRING, "\"#181818\"");
  EXPECT_TOKEN(&lx, SX_TOKEN_RPAREN, ")");
  EXPECT_TOKEN(&lx, SX_TOKEN_RPAREN, ")");
  EXPECT_TOKEN(&lx, SX_TOKEN_EOF, "");
}

int main(void) {
  printf("Running sx lexer tests...\n");
  RUN(test_lparen);
  RUN(test_rparen);
  RUN(test_symbol_simple);
  RUN(test_symbol_variants);
  RUN(test_symbol_representative);
  RUN(test_number_integer);
  RUN(test_number_zero);
  RUN(test_number_signed_positive);
  RUN(test_number_signed_negative);
  RUN(test_number_decimal);
  RUN(test_number_signed_decimal);
  RUN(test_number_plus_decimal);
  RUN(test_number_double_zero);
  RUN(test_number_leading_zero);
  RUN(test_number_trailing_dot_is_symbol);
  RUN(test_number_leading_dot_is_symbol);
  RUN(test_number_signed_leading_dot_is_symbol);
  RUN(test_number_double_dot_is_symbol);
  RUN(test_number_trailing_alpha_is_symbol);
  RUN(test_dot_alone_is_symbol);
  RUN(test_plus_dot_is_symbol);
  RUN(test_plus_alone_is_symbol);
  RUN(test_minus_alone_is_symbol);
  RUN(test_string);
  RUN(test_string_empty);
  RUN(test_string_with_spaces);
  RUN(test_string_keeps_quotes);
  RUN(test_string_with_semicolon);
  RUN(test_string_with_parens);
  RUN(test_string_escaped_quotes);
  RUN(test_string_escaped_backslash);
  RUN(test_string_newline_is_error);
  RUN(test_unterminated_string_is_error);
  RUN(test_eof_empty_input);
  RUN(test_whitespace_skipped);
  RUN(test_comment_full_line_skipped);
  RUN(test_comment_inline_skipped);
  RUN(test_comment_to_eof_alone);
  RUN(test_comment_to_eof_after_atom);
  RUN(test_list_tokens);
  RUN(test_nested_list);
  RUN(test_adjacent_parens_foo);
  RUN(test_adjacent_parens_nested);
  RUN(test_adjacent_parens_mixed);
  RUN(test_unbalanced_lparens);
  RUN(test_unbalanced_rparens);
  RUN(test_multiple_tokens_sequential);
  RUN(test_token_kind_string);
  RUN(test_token_text_eq_helper);
  RUN(test_line_column_tracking);
  RUN(test_column_after_newline_starts_at_one);
  RUN(test_crlf_line_column);
  RUN(test_indented_positions);
  RUN(test_position_after_comment_indent);
  RUN(test_eof_stable);
  RUN(test_realistic_editor_config);
  printf("\n%d/%d passed\n", tests_run - tests_failed, tests_run);
  return tests_failed ? 1 : 0;
}
