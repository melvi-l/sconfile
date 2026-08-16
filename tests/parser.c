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

#define CHECK_NODE_KIND(node, expected_kind)                                   \
  do {                                                                         \
    if ((node) == NULL) {                                                      \
      printf("FAIL\n    %s:%d: node is NULL (expected %s)\n", __FILE__,        \
             __LINE__, #expected_kind);                                        \
      tests_failed++;                                                          \
      return;                                                                  \
    }                                                                          \
    if ((node)->kind != (expected_kind)) {                                     \
      printf("FAIL\n    %s:%d: node kind %d != %s\n", __FILE__, __LINE__,      \
             (int)(node)->kind, #expected_kind);                               \
      tests_failed++;                                                          \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define CHECK_NODE_TEXT(node, expected)                                        \
  do {                                                                         \
    if ((node) == NULL) {                                                      \
      printf("FAIL\n    %s:%d: node is NULL (expected \"%s\")\n", __FILE__,    \
             __LINE__, expected);                                              \
      tests_failed++;                                                          \
      return;                                                                  \
    }                                                                          \
    u64 _elen = (u64)strlen(expected);                                         \
    if ((node)->text.len != _elen ||                                           \
        memcmp((node)->text.data, (expected), _elen) != 0) {                   \
      printf("FAIL\n    %s:%d: node text \"%.*s\" != \"%s\"\n", __FILE__,      \
             __LINE__, (int)(node)->text.len, (node)->text.data, expected);    \
      tests_failed++;                                                          \
      return;                                                                  \
    }                                                                          \
  } while (0)

static SxParser make_parser(const char *src) {
  Arena *arena = arena_create(ARENA_DEFAULT_BLOCK_SIZE);
  SxParser p = {0};
  p.arena = arena;
  p.lexer = (SxLexer){
      .data = src,
      .len = (u64)strlen(src),
      .pos = 0,
      .line = 1,
      .column = 1,
  };
  p.current = sx_next_token(&p.lexer);
  return p;
}

static bool sx_node_text_eq(SxNode *node, const char *expected) {
  if (node == NULL) {
    return false;
  }
  u64 elen = (u64)strlen(expected);
  if (node->text.len != elen) {
    return false;
  }
  if (elen == 0) {
    return true;
  }
  return memcmp(node->text.data, expected, elen) == 0;
}

static u64 count_children(SxNode *parent) {
  u64 n = 0;
  for (SxNode *c = parent->first_child; c != NULL; c = c->next) {
    n++;
  }
  return n;
}

static SxNode *nth_child(SxNode *parent, u64 n) {
  SxNode *c = parent->first_child;
  for (u64 i = 0; i < n && c != NULL; i++) {
    c = c->next;
  }
  return c;
}

static u64 count_errors(SxParser *p) {
  if (p->error.code == SX_ERROR_NONE) {
    return 0;
  }
  u64 n = 1;
  for (SxError *e = p->error.next; e != NULL; e = e->next) {
    n++;
  }
  return n;
}

static SxError *nth_error(SxParser *p, u64 n) {
  SxError *e = &p->error;
  for (u64 i = 0; i < n && e != NULL; i++) {
    e = e->next;
  }
  return e;
}

TEST(root_kind) {
  SxParser p = make_parser("foo");
  SxNode *root = sx_parse_document(&p);
  CHECK(root != NULL);
  CHECK_NODE_KIND(root, SX_NODE_ROOT);
  CHECK(root->text.len == 0);
  CHECK(root->parent == NULL);
}

TEST(empty_input) {
  SxParser p = make_parser("");
  SxNode *root = sx_parse_document(&p);
  CHECK_NODE_KIND(root, SX_NODE_ROOT);
  CHECK(root->first_child == NULL);
  CHECK(root->last_child == NULL);
  CHECK(count_children(root) == 0);
}

TEST(symbol_simple) {
  SxParser p = make_parser("hello");
  SxNode *root = sx_parse_document(&p);
  CHECK(count_children(root) == 1);
  SxNode *sym = root->first_child;
  CHECK_NODE_KIND(sym, SX_NODE_SYMBOL);
  CHECK_NODE_TEXT(sym, "hello");
  CHECK(sym->parent == root);
  CHECK(sym->first_child == NULL);
  CHECK(sym->next == NULL);
}

TEST(symbol_variants) {
  const char *syms[] = {"snake_case", "kebab-case", "camelCase",
                        "PascalCase", "symbol123",  "foo.bar"};
  for (u64 i = 0; i < sizeof(syms) / sizeof(syms[0]); i++) {
    SxParser p = make_parser(syms[i]);
    SxNode *root = sx_parse_document(&p);
    CHECK(count_children(root) == 1);
    SxNode *sym = root->first_child;
    CHECK_NODE_KIND(sym, SX_NODE_SYMBOL);
    CHECK_NODE_TEXT(sym, syms[i]);
  }
}

TEST(number_integer) {
  SxParser p = make_parser("42");
  SxNode *root = sx_parse_document(&p);
  CHECK(count_children(root) == 1);
  SxNode *num = root->first_child;
  CHECK_NODE_KIND(num, SX_NODE_NUMBER);
  CHECK_NODE_TEXT(num, "42");
}

TEST(number_zero) {
  SxParser p = make_parser("0");
  SxNode *root = sx_parse_document(&p);
  SxNode *num = root->first_child;
  CHECK_NODE_KIND(num, SX_NODE_NUMBER);
  CHECK_NODE_TEXT(num, "0");
}

TEST(number_signed_positive) {
  SxParser p = make_parser("+42");
  SxNode *root = sx_parse_document(&p);
  SxNode *num = root->first_child;
  CHECK_NODE_KIND(num, SX_NODE_NUMBER);
  CHECK_NODE_TEXT(num, "+42");
}

TEST(number_signed_negative) {
  SxParser p = make_parser("-42");
  SxNode *root = sx_parse_document(&p);
  SxNode *num = root->first_child;
  CHECK_NODE_KIND(num, SX_NODE_NUMBER);
  CHECK_NODE_TEXT(num, "-42");
}

TEST(number_decimal) {
  SxParser p = make_parser("3.14");
  SxNode *root = sx_parse_document(&p);
  SxNode *num = root->first_child;
  CHECK_NODE_KIND(num, SX_NODE_NUMBER);
  CHECK_NODE_TEXT(num, "3.14");
}

TEST(number_signed_decimal) {
  SxParser p = make_parser("-0.25");
  SxNode *root = sx_parse_document(&p);
  SxNode *num = root->first_child;
  CHECK_NODE_KIND(num, SX_NODE_NUMBER);
  CHECK_NODE_TEXT(num, "-0.25");
}

TEST(string) {
  SxParser p = make_parser("\"hello\"");
  SxNode *root = sx_parse_document(&p);
  SxNode *str = root->first_child;
  CHECK_NODE_KIND(str, SX_NODE_STRING);
  CHECK_NODE_TEXT(str, "\"hello\"");
}

TEST(string_empty) {
  SxParser p = make_parser("\"\"");
  SxNode *root = sx_parse_document(&p);
  SxNode *str = root->first_child;
  CHECK_NODE_KIND(str, SX_NODE_STRING);
  CHECK_NODE_TEXT(str, "\"\"");
}

TEST(string_with_spaces) {
  SxParser p = make_parser("\"hello world\"");
  SxNode *root = sx_parse_document(&p);
  SxNode *str = root->first_child;
  CHECK_NODE_KIND(str, SX_NODE_STRING);
  CHECK_NODE_TEXT(str, "\"hello world\"");
}

TEST(string_keeps_quotes) {
  SxParser p = make_parser("\"hello\"");
  SxNode *root = sx_parse_document(&p);
  SxNode *str = root->first_child;
  CHECK(str->text.len == 7);
  CHECK(str->text.data[0] == '"');
  CHECK(str->text.data[str->text.len - 1] == '"');
}

TEST(list_empty) {
  SxParser p = make_parser("()");
  SxNode *root = sx_parse_document(&p);
  CHECK(count_children(root) == 1);
  SxNode *list = root->first_child;
  CHECK_NODE_KIND(list, SX_NODE_LIST);
  CHECK(list->first_child == NULL);
  CHECK(list->last_child == NULL);
  CHECK(count_children(list) == 0);
}

TEST(list_one_symbol) {
  SxParser p = make_parser("(a)");
  SxNode *root = sx_parse_document(&p);
  SxNode *list = root->first_child;
  CHECK_NODE_KIND(list, SX_NODE_LIST);
  CHECK(count_children(list) == 1);
  SxNode *a = list->first_child;
  CHECK_NODE_KIND(a, SX_NODE_SYMBOL);
  CHECK_NODE_TEXT(a, "a");
  CHECK(a->parent == list);
  CHECK(list->last_child == a);
}

TEST(list_two_symbols) {
  SxParser p = make_parser("(a b)");
  SxNode *root = sx_parse_document(&p);
  SxNode *list = root->first_child;
  CHECK_NODE_KIND(list, SX_NODE_LIST);
  CHECK(count_children(list) == 2);
  SxNode *a = nth_child(list, 0);
  SxNode *b = nth_child(list, 1);
  CHECK_NODE_TEXT(a, "a");
  CHECK_NODE_TEXT(b, "b");
  CHECK(list->first_child == a);
  CHECK(list->last_child == b);
  CHECK(a->next == b);
  CHECK(b->next == NULL);
  CHECK(a->parent == list);
  CHECK(b->parent == list);
}

TEST(list_mixed) {
  SxParser p = make_parser("(foo \"bar\" 42 baz)");
  SxNode *root = sx_parse_document(&p);
  SxNode *list = root->first_child;
  CHECK_NODE_KIND(list, SX_NODE_LIST);
  CHECK(count_children(list) == 4);

  SxNode *foo = nth_child(list, 0);
  CHECK_NODE_KIND(foo, SX_NODE_SYMBOL);
  CHECK_NODE_TEXT(foo, "foo");

  SxNode *bar = nth_child(list, 1);
  CHECK_NODE_KIND(bar, SX_NODE_STRING);
  CHECK_NODE_TEXT(bar, "\"bar\"");

  SxNode *num = nth_child(list, 2);
  CHECK_NODE_KIND(num, SX_NODE_NUMBER);
  CHECK_NODE_TEXT(num, "42");

  SxNode *baz = nth_child(list, 3);
  CHECK_NODE_KIND(baz, SX_NODE_SYMBOL);
  CHECK_NODE_TEXT(baz, "baz");
}

TEST(nested_list) {
  SxParser p = make_parser("(a (b c) d)");
  SxNode *root = sx_parse_document(&p);
  SxNode *outer = root->first_child;
  CHECK_NODE_KIND(outer, SX_NODE_LIST);
  CHECK(count_children(outer) == 3);

  SxNode *a = nth_child(outer, 0);
  CHECK_NODE_KIND(a, SX_NODE_SYMBOL);
  CHECK_NODE_TEXT(a, "a");

  SxNode *inner = nth_child(outer, 1);
  CHECK_NODE_KIND(inner, SX_NODE_LIST);
  CHECK(count_children(inner) == 2);
  CHECK_NODE_TEXT(nth_child(inner, 0), "b");
  CHECK_NODE_TEXT(nth_child(inner, 1), "c");
  CHECK(inner->parent == outer);

  SxNode *d = nth_child(outer, 2);
  CHECK_NODE_KIND(d, SX_NODE_SYMBOL);
  CHECK_NODE_TEXT(d, "d");

  CHECK(outer->first_child == a);
  CHECK(outer->last_child == d);
}

TEST(deeply_nested) {
  SxParser p = make_parser("((a))");
  SxNode *root = sx_parse_document(&p);
  SxNode *l1 = root->first_child;
  CHECK_NODE_KIND(l1, SX_NODE_LIST);
  CHECK(count_children(l1) == 1);

  SxNode *l2 = l1->first_child;
  CHECK_NODE_KIND(l2, SX_NODE_LIST);
  CHECK(count_children(l2) == 1);
  CHECK(l2->parent == l1);

  SxNode *a = l2->first_child;
  CHECK_NODE_KIND(a, SX_NODE_SYMBOL);
  CHECK_NODE_TEXT(a, "a");
  CHECK(a->parent == l2);
}

TEST(adjacent_parens_mixed) {
  SxParser p = make_parser("(a(b)c)");
  SxNode *root = sx_parse_document(&p);
  SxNode *outer = root->first_child;
  CHECK_NODE_KIND(outer, SX_NODE_LIST);
  CHECK(count_children(outer) == 3);

  SxNode *a = nth_child(outer, 0);
  CHECK_NODE_TEXT(a, "a");

  SxNode *inner = nth_child(outer, 1);
  CHECK_NODE_KIND(inner, SX_NODE_LIST);
  CHECK(count_children(inner) == 1);
  CHECK_NODE_TEXT(inner->first_child, "b");

  SxNode *c = nth_child(outer, 2);
  CHECK_NODE_TEXT(c, "c");
}

TEST(multiple_top_level) {
  SxParser p = make_parser("a b c");
  SxNode *root = sx_parse_document(&p);
  CHECK(count_children(root) == 3);
  CHECK_NODE_TEXT(nth_child(root, 0), "a");
  CHECK_NODE_TEXT(nth_child(root, 1), "b");
  CHECK_NODE_TEXT(nth_child(root, 2), "c");
  CHECK(root->first_child->parent == root);
  CHECK(root->last_child->parent == root);
  CHECK(root->first_child->next == nth_child(root, 1));
  CHECK(root->last_child->next == NULL);
}

TEST(multiple_top_level_lists) {
  SxParser p = make_parser("(a) (b) (c)");
  SxNode *root = sx_parse_document(&p);
  CHECK(count_children(root) == 3);
  CHECK_NODE_KIND(nth_child(root, 0), SX_NODE_LIST);
  CHECK_NODE_KIND(nth_child(root, 1), SX_NODE_LIST);
  CHECK_NODE_KIND(nth_child(root, 2), SX_NODE_LIST);
  CHECK_NODE_TEXT(nth_child(root, 0)->first_child, "a");
  CHECK_NODE_TEXT(nth_child(root, 1)->first_child, "b");
  CHECK_NODE_TEXT(nth_child(root, 2)->first_child, "c");
}

TEST(list_of_numbers) {
  SxParser p = make_parser("(1 2 3)");
  SxNode *root = sx_parse_document(&p);
  SxNode *list = root->first_child;
  CHECK_NODE_KIND(list, SX_NODE_LIST);
  CHECK(count_children(list) == 3);
  for (u64 i = 0; i < 3; i++) {
    SxNode *n = nth_child(list, i);
    CHECK_NODE_KIND(n, SX_NODE_NUMBER);
  }
}

TEST(node_text_eq_helper) {
  SxParser p = make_parser("hello");
  SxNode *root = sx_parse_document(&p);
  SxNode *sym = root->first_child;
  CHECK(sx_node_text_eq(sym, "hello"));
  CHECK(!sx_node_text_eq(sym, "hell"));
  CHECK(!sx_node_text_eq(sym, "helloo"));
  CHECK(!sx_node_text_eq(NULL, "hello"));
  // root has empty text
  CHECK(sx_node_text_eq(root, ""));
  CHECK(!sx_node_text_eq(root, "x"));
}

TEST(node_positions_simple) {
  SxParser p = make_parser("(foo)");
  SxNode *root = sx_parse_document(&p);
  SxNode *list = root->first_child;
  CHECK(list->line == 1);
  CHECK(list->column == 1);
  SxNode *foo = list->first_child;
  CHECK(foo->line == 1);
  CHECK(foo->column == 2);
}

TEST(node_positions_indented) {
  // (foo
  //   bar 42)
  SxParser p = make_parser("(foo\n  bar 42)");
  SxNode *root = sx_parse_document(&p);
  SxNode *list = root->first_child;
  CHECK(list->line == 1);
  CHECK(list->column == 1);

  SxNode *foo = nth_child(list, 0);
  CHECK(foo->line == 1);
  CHECK(foo->column == 2);

  SxNode *bar = nth_child(list, 1);
  CHECK(bar->line == 2);
  CHECK(bar->column == 3);

  SxNode *num = nth_child(list, 2);
  CHECK(num->line == 2);
  CHECK(num->column == 7);
}

TEST(node_positions_nested) {
  // (a (b c) d)
  SxParser p = make_parser("(a (b c) d)");
  SxNode *root = sx_parse_document(&p);
  SxNode *outer = root->first_child;
  CHECK(outer->line == 1);
  CHECK(outer->column == 1);

  SxNode *a = nth_child(outer, 0);
  CHECK(a->column == 2);

  SxNode *inner = nth_child(outer, 1);
  CHECK(inner->kind == SX_NODE_LIST);
  CHECK(inner->column == 4);
  CHECK(nth_child(inner, 0)->column == 5);
  CHECK(nth_child(inner, 1)->column == 7);

  SxNode *d = nth_child(outer, 2);
  // ( a _ ( b _ c ) _ d ) -> d at column 10
  CHECK(d->column == 10);
}

TEST(comments_skipped) {
  SxParser p = make_parser("; comment\n(foo)");
  SxNode *root = sx_parse_document(&p);
  CHECK(count_children(root) == 1);
  SxNode *list = root->first_child;
  CHECK_NODE_KIND(list, SX_NODE_LIST);
  CHECK_NODE_TEXT(list->first_child, "foo");
  CHECK(list->line == 2);
}

TEST(comment_between_forms) {
  SxParser p = make_parser("a ; trailing\nb");
  SxNode *root = sx_parse_document(&p);
  CHECK(count_children(root) == 2);
  CHECK_NODE_TEXT(nth_child(root, 0), "a");
  CHECK_NODE_TEXT(nth_child(root, 1), "b");
}

TEST(whitespace_skipped) {
  SxParser p = make_parser("   \t\n  hello");
  SxNode *root = sx_parse_document(&p);
  SxNode *sym = root->first_child;
  CHECK_NODE_KIND(sym, SX_NODE_SYMBOL);
  CHECK_NODE_TEXT(sym, "hello");
  CHECK(sym->line == 2);
  CHECK(sym->column == 3);
}

TEST(node_text_slices_into_source) {
  // node text points into the original source buffer (no copy)
  SxParser p = make_parser("(foo)");
  SxNode *root = sx_parse_document(&p);
  SxNode *foo = root->first_child->first_child;
  CHECK(foo->text.data == p.lexer.data + 1);
  CHECK(foo->text.len == 3);
}

TEST(parent_links_recursive) {
  SxParser p = make_parser("(a (b c) d)");
  SxNode *root = sx_parse_document(&p);
  SxNode *outer = root->first_child;
  CHECK(outer->parent == root);
  SxNode *inner = nth_child(outer, 1);
  CHECK(inner->parent == outer);
  SxNode *b = nth_child(inner, 0);
  CHECK(b->parent == inner);
  SxNode *c = nth_child(inner, 1);
  CHECK(c->parent == inner);
}

TEST(next_links_chain) {
  SxParser p = make_parser("(a b c d)");
  SxNode *root = sx_parse_document(&p);
  SxNode *list = root->first_child;
  SxNode *cur = list->first_child;
  for (u64 i = 0; i < 4; i++) {
    CHECK(cur != NULL);
    cur = cur->next;
  }
  CHECK(cur == NULL);
}

TEST(first_last_child_consistency) {
  SxParser p = make_parser("(a b c)");
  SxNode *root = sx_parse_document(&p);
  SxNode *list = root->first_child;
  CHECK(list->first_child == nth_child(list, 0));
  CHECK(list->last_child == nth_child(list, 2));
  CHECK(list->last_child->next == NULL);
}

TEST(realistic_editor_config) {
  SxParser p = make_parser("  (font\n"
                           "    (path \"assets/fonts/mono.bdf\")\n"
                           "    (size 16))\n"
                           "  (wrap true)\n"
                           "  (background \"#181818\")\n");
  SxNode *root = sx_parse_document(&p);
  CHECK_NODE_KIND(root, SX_NODE_ROOT);
  CHECK(count_children(root) == 3);

  // (font (path "...") (size 16))
  SxNode *font = nth_child(root, 0);
  CHECK_NODE_KIND(font, SX_NODE_LIST);
  CHECK_NODE_TEXT(nth_child(font, 0), "font");
  CHECK(count_children(font) == 3);

  SxNode *path = nth_child(font, 1);
  CHECK_NODE_KIND(path, SX_NODE_LIST);
  CHECK_NODE_TEXT(nth_child(path, 0), "path");
  CHECK_NODE_TEXT(nth_child(path, 1), "\"assets/fonts/mono.bdf\"");

  SxNode *size = nth_child(font, 2);
  CHECK_NODE_KIND(size, SX_NODE_LIST);
  CHECK_NODE_TEXT(nth_child(size, 0), "size");
  CHECK_NODE_TEXT(nth_child(size, 1), "16");

  // (wrap true)
  SxNode *wrap = nth_child(root, 1);
  CHECK_NODE_KIND(wrap, SX_NODE_LIST);
  CHECK_NODE_TEXT(nth_child(wrap, 0), "wrap");
  CHECK_NODE_TEXT(nth_child(wrap, 1), "true");

  // (background "#181818")
  SxNode *bg = nth_child(root, 2);
  CHECK_NODE_KIND(bg, SX_NODE_LIST);
  CHECK_NODE_TEXT(nth_child(bg, 0), "background");
  CHECK_NODE_TEXT(nth_child(bg, 1), "\"#181818\"");
}

// --- error paths -------------------------------------------------------------

#define CHECK_PARSER_ERROR(p, expected_code, expected_line, expected_col)       \
  do {                                                                         \
    if ((p)->error.code != (expected_code)) {                                  \
      printf("FAIL\n    %s:%d: error code %d != %s\n", __FILE__, __LINE__,     \
             (int)(p)->error.code, #expected_code);                            \
      tests_failed++;                                                          \
      return;                                                                  \
    }                                                                          \
    if ((p)->error.line != (expected_line) ||                                  \
        (p)->error.column != (expected_col)) {                                 \
      printf("FAIL\n    %s:%d: error pos %u:%u != %u:%u\n", __FILE__,          \
             __LINE__, (p)->error.line, (p)->error.column, (expected_line),    \
             (expected_col));                                                  \
      tests_failed++;                                                          \
      return;                                                                  \
    }                                                                          \
  } while (0)

TEST(unexpected_rparen) {
  SxParser p = make_parser(")");
  SxNode *node = sx_parse(&p);
  CHECK(node == NULL);
  CHECK_PARSER_ERROR(&p, SX_ERROR_UNEXPECTED_RPAREN, 1, 1);
}

TEST(extra_rparen_after_valid_form) {
  SxParser p = make_parser("(a))");
  SxNode *root = sx_parse_document(&p);
  CHECK(root != NULL);
  CHECK(count_children(root) == 1);
  CHECK_NODE_KIND(root->first_child, SX_NODE_LIST);
  CHECK_PARSER_ERROR(&p, SX_ERROR_UNEXPECTED_RPAREN, 1, 4);
}

TEST(missing_rparen) {
  SxParser p = make_parser("(a");
  SxNode *root = sx_parse_document(&p);
  CHECK(root != NULL);
  CHECK(count_children(root) == 0);
  // position reports the opening '('
  CHECK_PARSER_ERROR(&p, SX_ERROR_MISSING_RPAREN, 1, 1);
}

TEST(missing_nested_rparen) {
  SxParser p = make_parser("(a (b");
  SxNode *root = sx_parse_document(&p);
  CHECK(root != NULL);
  CHECK(count_children(root) == 0);
  // position reports the inner opening '('
  CHECK_PARSER_ERROR(&p, SX_ERROR_MISSING_RPAREN, 1, 4);
}

TEST(unterminated_string) {
  SxParser p = make_parser("\"foo");
  SxNode *node = sx_parse(&p);
  CHECK(node == NULL);
  CHECK_PARSER_ERROR(&p, SX_ERROR_INVALID_TOKEN, 1, 1);
}

TEST(lexer_error_inside_list) {
  SxParser p = make_parser("(\"foo)");
  SxNode *node = sx_parse(&p);
  CHECK(node == NULL);
  CHECK_PARSER_ERROR(&p, SX_ERROR_INVALID_TOKEN, 1, 2);
}

TEST(multiple_errors_collected_as_linked_list) {
  // two stray ')' then a valid symbol: recovery collects both rparen errors
  // and still parses 'foo'
  SxParser p = make_parser(") ) foo");
  SxNode *root = sx_parse_document(&p);
  CHECK(root != NULL);
  CHECK(count_children(root) == 1);
  CHECK_NODE_TEXT(root->first_child, "foo");
  CHECK(count_errors(&p) == 2);
  SxError *e0 = nth_error(&p, 0);
  SxError *e1 = nth_error(&p, 1);
  CHECK(e0->code == SX_ERROR_UNEXPECTED_RPAREN);
  CHECK(e0->line == 1);
  CHECK(e0->column == 1);
  CHECK(e1->code == SX_ERROR_UNEXPECTED_RPAREN);
  CHECK(e1->line == 1);
  CHECK(e1->column == 3);
  CHECK(e0->next == e1);
  CHECK(e1->next == NULL);
}

TEST(error_chain_after_invalid_token) {
  // unterminated string inside unclosed list: INVALID_TOKEN then MISSING_RPAREN
  SxParser p = make_parser("(\"foo)");
  SxNode *root = sx_parse_document(&p);
  CHECK(root != NULL);
  CHECK(count_errors(&p) == 2);
  SxError *e0 = nth_error(&p, 0);
  SxError *e1 = nth_error(&p, 1);
  CHECK(e0->code == SX_ERROR_INVALID_TOKEN);
  CHECK(e0->line == 1);
  CHECK(e0->column == 2);
  CHECK(e1->code == SX_ERROR_MISSING_RPAREN);
  CHECK(e1->line == 1);
  CHECK(e1->column == 1);
  CHECK(e0->next == e1);
  CHECK(e1->next == NULL);
}

TEST(recovery_keeps_valid_forms_after_error) {
  // stray ')' between two valid forms: both forms survive, one error recorded
  SxParser p = make_parser("(a) ) (b)");
  SxNode *root = sx_parse_document(&p);
  CHECK(root != NULL);
  CHECK(count_children(root) == 2);
  CHECK_NODE_KIND(nth_child(root, 0), SX_NODE_LIST);
  CHECK_NODE_KIND(nth_child(root, 1), SX_NODE_LIST);
  CHECK_NODE_TEXT(nth_child(root, 0)->first_child, "a");
  CHECK_NODE_TEXT(nth_child(root, 1)->first_child, "b");
  CHECK(count_errors(&p) == 1);
  CHECK_PARSER_ERROR(&p, SX_ERROR_UNEXPECTED_RPAREN, 1, 5);
}

// --- whitespace / comment edge cases ----------------------------------------

TEST(only_whitespace) {
  SxParser p = make_parser("   \t\n  ");
  SxNode *root = sx_parse_document(&p);
  CHECK_NODE_KIND(root, SX_NODE_ROOT);
  CHECK(count_children(root) == 0);
  CHECK(p.current.kind == SX_TOKEN_EOF);
}

TEST(only_comment) {
  SxParser p = make_parser("; just a comment");
  SxNode *root = sx_parse_document(&p);
  CHECK_NODE_KIND(root, SX_NODE_ROOT);
  CHECK(count_children(root) == 0);
  CHECK(p.current.kind == SX_TOKEN_EOF);
}

TEST(comment_at_eof) {
  SxParser p = make_parser("foo ; trailing comment");
  SxNode *root = sx_parse_document(&p);
  CHECK(count_children(root) == 1);
  CHECK_NODE_TEXT(root->first_child, "foo");
  CHECK(p.current.kind == SX_TOKEN_EOF);
}

TEST(trailing_comment_at_eof) {
  SxParser p = make_parser("(a) ; trailing");
  SxNode *root = sx_parse_document(&p);
  CHECK(count_children(root) == 1);
  SxNode *list = root->first_child;
  CHECK_NODE_KIND(list, SX_NODE_LIST);
  CHECK_NODE_TEXT(list->first_child, "a");
  CHECK(p.current.kind == SX_TOKEN_EOF);
}

// --- string content variants (strings keep literal content) -----------------

TEST(string_with_semicolon) {
  SxParser p = make_parser("\"foo;bar\"");
  SxNode *s = sx_parse_document(&p)->first_child;
  CHECK_NODE_KIND(s, SX_NODE_STRING);
  CHECK_NODE_TEXT(s, "\"foo;bar\"");
}

TEST(string_with_parentheses) {
  SxParser p = make_parser("\"(foo)\"");
  SxNode *s = sx_parse_document(&p)->first_child;
  CHECK_NODE_KIND(s, SX_NODE_STRING);
  CHECK_NODE_TEXT(s, "\"(foo)\"");
}

TEST(string_with_number) {
  SxParser p = make_parser("\"42abc\"");
  SxNode *s = sx_parse_document(&p)->first_child;
  CHECK_NODE_KIND(s, SX_NODE_STRING);
  CHECK_NODE_TEXT(s, "\"42abc\"");
}

TEST(string_with_escape_quote) {
  SxParser p = make_parser("\"foo \\\"bar\\\"\"");
  SxNode *s = sx_parse_document(&p)->first_child;
  CHECK_NODE_KIND(s, SX_NODE_STRING);
  CHECK_NODE_TEXT(s, "\"foo \\\"bar\\\"\"");
}

TEST(string_with_backslash) {
  SxParser p = make_parser("\"foo\\\\bar\"");
  SxNode *s = sx_parse_document(&p)->first_child;
  CHECK_NODE_KIND(s, SX_NODE_STRING);
  CHECK_NODE_TEXT(s, "\"foo\\\\bar\"");
}

// --- number vs symbol classification ----------------------------------------

TEST(lone_plus_is_symbol) {
  SxParser p = make_parser("+");
  SxNode *n = sx_parse_document(&p)->first_child;
  CHECK_NODE_KIND(n, SX_NODE_SYMBOL);
  CHECK_NODE_TEXT(n, "+");
}

TEST(lone_minus_is_symbol) {
  SxParser p = make_parser("-");
  SxNode *n = sx_parse_document(&p)->first_child;
  CHECK_NODE_KIND(n, SX_NODE_SYMBOL);
  CHECK_NODE_TEXT(n, "-");
}

TEST(plus_symbol) {
  SxParser p = make_parser("+abc");
  SxNode *n = sx_parse_document(&p)->first_child;
  CHECK_NODE_KIND(n, SX_NODE_SYMBOL);
  CHECK_NODE_TEXT(n, "+abc");
}

TEST(minus_symbol) {
  SxParser p = make_parser("-abc");
  SxNode *n = sx_parse_document(&p)->first_child;
  CHECK_NODE_KIND(n, SX_NODE_SYMBOL);
  CHECK_NODE_TEXT(n, "-abc");
}

TEST(number_trailing_dot) {
  SxParser p = make_parser("1.");
  SxNode *n = sx_parse_document(&p)->first_child;
  CHECK_NODE_KIND(n, SX_NODE_SYMBOL);
  CHECK_NODE_TEXT(n, "1.");
}

TEST(number_leading_dot) {
  SxParser p = make_parser(".5");
  SxNode *n = sx_parse_document(&p)->first_child;
  CHECK_NODE_KIND(n, SX_NODE_SYMBOL);
  CHECK_NODE_TEXT(n, ".5");
}

TEST(number_signed_leading_dot) {
  SxParser p = make_parser("-.5");
  SxNode *n = sx_parse_document(&p)->first_child;
  CHECK_NODE_KIND(n, SX_NODE_SYMBOL);
  CHECK_NODE_TEXT(n, "-.5");
}

TEST(number_followed_by_symbol) {
  SxParser p = make_parser("12foo");
  SxNode *n = sx_parse_document(&p)->first_child;
  CHECK_NODE_KIND(n, SX_NODE_SYMBOL);
  CHECK_NODE_TEXT(n, "12foo");
}

// --- adjacency (no whitespace between forms) --------------------------------

TEST(adjacent_empty_lists) {
  SxParser p = make_parser("()()");
  SxNode *root = sx_parse_document(&p);
  CHECK(count_children(root) == 2);
  CHECK_NODE_KIND(nth_child(root, 0), SX_NODE_LIST);
  CHECK_NODE_KIND(nth_child(root, 1), SX_NODE_LIST);
  CHECK(count_children(nth_child(root, 0)) == 0);
  CHECK(count_children(nth_child(root, 1)) == 0);
}

TEST(adjacent_top_level_lists) {
  SxParser p = make_parser("(a)(b)");
  SxNode *root = sx_parse_document(&p);
  CHECK(count_children(root) == 2);
  CHECK_NODE_TEXT(nth_child(root, 0)->first_child, "a");
  CHECK_NODE_TEXT(nth_child(root, 1)->first_child, "b");
}

TEST(symbol_adjacent_to_list) {
  SxParser p = make_parser("a(b)");
  SxNode *root = sx_parse_document(&p);
  CHECK(count_children(root) == 2);
  CHECK_NODE_KIND(nth_child(root, 0), SX_NODE_SYMBOL);
  CHECK_NODE_TEXT(nth_child(root, 0), "a");
  CHECK_NODE_KIND(nth_child(root, 1), SX_NODE_LIST);
  CHECK_NODE_TEXT(nth_child(root, 1)->first_child, "b");
}

TEST(number_adjacent_to_list) {
  SxParser p = make_parser("1(b)");
  SxNode *root = sx_parse_document(&p);
  CHECK(count_children(root) == 2);
  CHECK_NODE_KIND(nth_child(root, 0), SX_NODE_NUMBER);
  CHECK_NODE_TEXT(nth_child(root, 0), "1");
  CHECK_NODE_KIND(nth_child(root, 1), SX_NODE_LIST);
  CHECK_NODE_TEXT(nth_child(root, 1)->first_child, "b");
}

// --- CRLF line endings ------------------------------------------------------

TEST(crlf) {
  SxParser p = make_parser("foo\r\nbar");
  SxNode *root = sx_parse_document(&p);
  CHECK(count_children(root) == 2);
  SxNode *a = nth_child(root, 0);
  SxNode *b = nth_child(root, 1);
  CHECK_NODE_TEXT(a, "foo");
  CHECK_NODE_TEXT(b, "bar");
  CHECK(a->line == 1);
  CHECK(a->column == 1);
  CHECK(b->line == 2);
  CHECK(b->column == 1);
}

TEST(crlf_positions) {
  // (foo\r\n  bar)
  SxParser p = make_parser("(foo\r\n  bar)");
  SxNode *list = sx_parse_document(&p)->first_child;
  CHECK(list->line == 1);
  CHECK(list->column == 1);
  SxNode *foo = nth_child(list, 0);
  CHECK(foo->line == 1);
  CHECK(foo->column == 2);
  SxNode *bar = nth_child(list, 1);
  CHECK(bar->line == 2);
  CHECK(bar->column == 3);
}

// --- structural invariants --------------------------------------------------

TEST(atom_has_no_children) {
  SxParser p = make_parser("foo");
  SxNode *sym = sx_parse_document(&p)->first_child;
  CHECK(sym->first_child == NULL);
  CHECK(sym->last_child == NULL);
}

TEST(root_links) {
  SxParser p = make_parser("a b");
  SxNode *root = sx_parse_document(&p);
  CHECK(root->parent == NULL);
  CHECK(root->first_child->parent == root);
  CHECK(root->last_child->parent == root);
  CHECK(root->first_child->next == root->last_child);
  CHECK(root->last_child->next == NULL);
}

TEST(child_links) {
  SxParser p = make_parser("(a b c)");
  SxNode *list = sx_parse_document(&p)->first_child;
  SxNode *a = nth_child(list, 0);
  SxNode *b = nth_child(list, 1);
  SxNode *c = nth_child(list, 2);
  CHECK(list->first_child == a);
  CHECK(list->last_child == c);
  CHECK(a->next == b);
  CHECK(b->next == c);
  CHECK(c->next == NULL);
  CHECK(a->parent == list);
  CHECK(b->parent == list);
  CHECK(c->parent == list);
}

TEST(deep_nesting) {
  // ((((a)))) -> 4 lists deep, then symbol
  SxParser p = make_parser("((((a))))");
  SxNode *cur = sx_parse_document(&p)->first_child;
  for (int i = 0; i < 4; i++) {
    CHECK(cur != NULL);
    CHECK_NODE_KIND(cur, SX_NODE_LIST);
    CHECK(count_children(cur) == 1);
    cur = cur->first_child;
  }
  CHECK_NODE_KIND(cur, SX_NODE_SYMBOL);
  CHECK_NODE_TEXT(cur, "a");
}

TEST(large_sibling_count) {
  char buf[600];
  int off = 0;
  buf[off++] = '(';
  for (int i = 0; i < 100; i++) {
    int w = snprintf(buf + off, sizeof(buf) - (size_t)off, "%d ", i);
    CHECK(w > 0);
    off += w;
  }
  if (off > 1 && buf[off - 1] == ' ') {
    off--;
  }
  buf[off++] = ')';
  buf[off] = '\0';
  SxParser p = make_parser(buf);
  SxNode *list = sx_parse_document(&p)->first_child;
  CHECK_NODE_KIND(list, SX_NODE_LIST);
  CHECK(count_children(list) == 100);
  CHECK_NODE_TEXT(list->first_child, "0");
  CHECK_NODE_TEXT(list->last_child, "99");
}

TEST(empty_list_inside_list) {
  SxParser p = make_parser("(())");
  SxNode *outer = sx_parse_document(&p)->first_child;
  CHECK_NODE_KIND(outer, SX_NODE_LIST);
  CHECK(count_children(outer) == 1);
  SxNode *inner = outer->first_child;
  CHECK_NODE_KIND(inner, SX_NODE_LIST);
  CHECK(count_children(inner) == 0);
  CHECK(inner->parent == outer);
  CHECK(outer->last_child == inner);
}

TEST(empty_lists_nested) {
  SxParser p = make_parser("((()))");
  SxNode *l1 = sx_parse_document(&p)->first_child;
  CHECK_NODE_KIND(l1, SX_NODE_LIST);
  CHECK(count_children(l1) == 1);
  SxNode *l2 = l1->first_child;
  CHECK_NODE_KIND(l2, SX_NODE_LIST);
  CHECK(count_children(l2) == 1);
  SxNode *l3 = l2->first_child;
  CHECK_NODE_KIND(l3, SX_NODE_LIST);
  CHECK(count_children(l3) == 0);
  CHECK(l3->parent == l2);
  CHECK(l2->parent == l1);
}

TEST(source_slice_boundaries) {
  const char *src = "(abc)";
  SxParser p = make_parser(src);
  SxNode *root = sx_parse_document(&p);
  SxNode *list = root->first_child;
  // list is a container: no own text
  CHECK(list->text.data == NULL);
  CHECK(list->text.len == 0);
  // atom text slices directly into the source buffer (no copy)
  SxNode *abc = list->first_child;
  CHECK(abc->text.data == src + 1);
  CHECK(abc->text.len == 3);
}

TEST(eof_position) {
  SxParser p = make_parser("foo");
  sx_parse_document(&p);
  CHECK(p.current.kind == SX_TOKEN_EOF);
  CHECK(p.current.line == 1);
  CHECK(p.current.column == 4);
}

TEST(parser_consumes_exactly_one_expression) {
  SxParser p = make_parser("foo bar");
  SxNode *node = sx_parse(&p);
  CHECK_NODE_KIND(node, SX_NODE_SYMBOL);
  CHECK_NODE_TEXT(node, "foo");
  // parser must be positioned at "bar", not consume it
  CHECK(p.current.kind == SX_TOKEN_SYMBOL);
  CHECK(p.current.text.len == 3);
  CHECK(memcmp(p.current.text.data, "bar", 3) == 0);
}

TEST(document_consumes_until_eof) {
  SxParser p = make_parser("a b c");
  SxNode *root = sx_parse_document(&p);
  CHECK(count_children(root) == 3);
  CHECK(p.current.kind == SX_TOKEN_EOF);
}

// --- branch-coverage drivers ------------------------------------------------
// These exercise the branch space of each function so gcov reports full
// coverage when built with --coverage. Build for coverage:
//   gcc --coverage -g -O0 tests/parser.c -o build/parser_cov -Isrc
//   ./build/parser_cov && gcov tests/parser.c

TEST(branch_coverage_sx_next_token) {
  // hits: EOF, LPAREN, RPAREN, SYMBOL, NUMBER, STRING, ERROR, comment, ws,
  // signed +/-, decimal, trailing/leading dot, alpha-after-digit, escape
  const char *src = "( ) hello 42 +42 -42 3.14 +3.0 -0.5 00 01 "
                    "1. .5 -.5 1.2.3 12foo . + +. - \"s\" \"\" \"x;y\" "
                    "\"(p)\" \"a\\\"b\" \"a\\\\b\" ;c\n x \"unterm";
  SxLexer lx = (SxLexer){src, (u64)strlen(src), 0, 1, 1};
  SxTokenKind kinds[64];
  u64 n = 0;
  bool saw_error = false;
  for (;;) {
    SxToken t = sx_next_token(&lx);
    if (n < 64) {
      kinds[n++] = t.kind;
    }
    if (t.kind == SX_TOKEN_ERROR) {
      saw_error = true;
    }
    if (t.kind == SX_TOKEN_EOF) {
      break;
    }
  }
  CHECK(n > 0);
  CHECK(kinds[n - 1] == SX_TOKEN_EOF);
  CHECK(saw_error);
}

TEST(branch_coverage_sx_parse) {
  // list / symbol / string / number branches
  SxParser p = make_parser("(a (b) \"s\" 1) sym \"t\" 2");
  SxNode *root = sx_parse_document(&p);
  CHECK(root != NULL);
  CHECK(count_children(root) == 4);
  CHECK(nth_child(root, 0)->kind == SX_NODE_LIST);
  CHECK(nth_child(root, 1)->kind == SX_NODE_SYMBOL);
  CHECK(nth_child(root, 2)->kind == SX_NODE_STRING);
  CHECK(nth_child(root, 3)->kind == SX_NODE_NUMBER);
}

TEST(branch_coverage_sx_parse_document) {
  // empty / single / multi child branches
  {
    SxParser p = make_parser("");
    SxNode *root = sx_parse_document(&p);
    CHECK(count_children(root) == 0);
  }
  {
    SxParser p = make_parser("a");
    SxNode *root = sx_parse_document(&p);
    CHECK(count_children(root) == 1);
  }
  {
    SxParser p = make_parser("a b c d e");
    SxNode *root = sx_parse_document(&p);
    CHECK(count_children(root) == 5);
  }
}

int main(void) {
  printf("Running sx parser tests...\n");
  // structural / happy-path
  RUN(root_kind);
  RUN(empty_input);
  RUN(symbol_simple);
  RUN(symbol_variants);
  RUN(number_integer);
  RUN(number_zero);
  RUN(number_signed_positive);
  RUN(number_signed_negative);
  RUN(number_decimal);
  RUN(number_signed_decimal);
  RUN(string);
  RUN(string_empty);
  RUN(string_with_spaces);
  RUN(string_keeps_quotes);
  RUN(list_empty);
  RUN(list_one_symbol);
  RUN(list_two_symbols);
  RUN(list_mixed);
  RUN(nested_list);
  RUN(deeply_nested);
  RUN(adjacent_parens_mixed);
  RUN(multiple_top_level);
  RUN(multiple_top_level_lists);
  RUN(list_of_numbers);
  RUN(node_text_eq_helper);
  RUN(node_positions_simple);
  RUN(node_positions_indented);
  RUN(node_positions_nested);
  RUN(comments_skipped);
  RUN(comment_between_forms);
  RUN(whitespace_skipped);
  RUN(node_text_slices_into_source);
  RUN(parent_links_recursive);
  RUN(next_links_chain);
  RUN(first_last_child_consistency);
  RUN(realistic_editor_config);
  // whitespace / comment edges
  RUN(only_whitespace);
  RUN(only_comment);
  RUN(comment_at_eof);
  RUN(trailing_comment_at_eof);
  // string content variants
  RUN(string_with_semicolon);
  RUN(string_with_parentheses);
  RUN(string_with_number);
  RUN(string_with_escape_quote);
  RUN(string_with_backslash);
  // number vs symbol classification
  RUN(lone_plus_is_symbol);
  RUN(lone_minus_is_symbol);
  RUN(plus_symbol);
  RUN(minus_symbol);
  RUN(number_trailing_dot);
  RUN(number_leading_dot);
  RUN(number_signed_leading_dot);
  RUN(number_followed_by_symbol);
  // adjacency
  RUN(adjacent_empty_lists);
  RUN(adjacent_top_level_lists);
  RUN(symbol_adjacent_to_list);
  RUN(number_adjacent_to_list);
  // CRLF
  RUN(crlf);
  RUN(crlf_positions);
  // structural invariants
  RUN(atom_has_no_children);
  RUN(root_links);
  RUN(child_links);
  RUN(deep_nesting);
  RUN(large_sibling_count);
  RUN(empty_list_inside_list);
  RUN(empty_lists_nested);
  RUN(source_slice_boundaries);
  RUN(eof_position);
  RUN(parser_consumes_exactly_one_expression);
  RUN(document_consumes_until_eof);
  // error paths
  RUN(unexpected_rparen);
  RUN(extra_rparen_after_valid_form);
  RUN(missing_rparen);
  RUN(missing_nested_rparen);
  RUN(unterminated_string);
  RUN(lexer_error_inside_list);
  RUN(multiple_errors_collected_as_linked_list);
  RUN(error_chain_after_invalid_token);
  RUN(recovery_keeps_valid_forms_after_error);
  // branch coverage drivers
  RUN(branch_coverage_sx_next_token);
  RUN(branch_coverage_sx_parse);
  RUN(branch_coverage_sx_parse_document);
  printf("\n%d/%d passed\n", tests_run - tests_failed, tests_run);
  return tests_failed ? 1 : 0;
}
