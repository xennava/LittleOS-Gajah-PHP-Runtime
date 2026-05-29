/*
 * LittleOS Gajah PHP - php_runtime.hpp
 * PHP 8 Runtime Engine untuk bare-metal kernel
 * Ini adalah inti dari kernel — semua logika kernel ditulis dalam PHP
 */

#pragma once

/* ============================================================
 * TIPE NILAI PHP (PHP Value Types)
 * ============================================================ */
#include <stdint.h>

enum class PhpType : uint8_t { Null, Bool, Int, Float, String, Array };

/* Entry dalam PHP array (linked list) */
struct PhpArrayEntry {
  char key[128];
  bool is_int_key;
  int64_t int_key;
  struct PhpValue *value;
  PhpArrayEntry *next;
};

/* Nilai PHP — tipe dinamis seperti PHP asli */
struct PhpValue {
  PhpType type;
  int64_t int_val;
  bool bool_val;
  char *str_val;
  int str_len;
  PhpArrayEntry *array_head;
  int array_count;
  int ref_count;
  bool in_pool; /* true jika dari pool, false jika dari kmalloc */
};

/* ============================================================
 * TOKEN PHP (PHP Tokens)
 * ============================================================ */
enum class TokenType : uint8_t {
  /* Literal */
  Integer,
  String,
  Identifier,
  Variable,
  /* Keyword */
  Echo,
  If,
  Else,
  ElseIf,
  While,
  For,
  ForEach,
  As,
  Function,
  Return,
  True,
  False,
  NullLiteral,
  Break,
  Continue,
  /* Operator */
  Plus,
  Minus,
  Star,
  Slash,
  Percent,
  Dot,
  Assign,
  PlusAssign,
  MinusAssign,
  StarAssign,
  SlashAssign,
  DotAssign,
  Equal,
  NotEqual,
  Less,
  Greater,
  LessEqual,
  GreaterEqual,
  And,
  Or,
  Not,
  Increment,
  Decrement,
  /* Delimiter */
  Semicolon,
  Comma,
  LeftParen,
  RightParen,
  LeftBrace,
  RightBrace,
  LeftBracket,
  RightBracket,
  Arrow,
  DoubleArrow,
  DoubleColon,
  /* PHP tags */
  OpenTag,
  CloseTag,
  /* Spesial */
  Eof,
  Error
};

struct Token {
  TokenType type;
  char text[256];
  int line;
};

/* ============================================================
 * LEXER PHP (PHP Tokenizer)
 * ============================================================ */
class PhpLexer {
public:
  void init(const char *source);
  Token next_token();
  Token peek_token();
  int get_line() const { return line; }

private:
  const char *src;
  int pos;
  int len;
  int line;
  bool has_peeked;
  Token peeked;

  char current() const;
  char peek_char() const;
  char advance();
  void skip_whitespace_and_comments();
  Token make_token(TokenType type, const char *text);
  Token read_string(char quote);
  Token read_number();
  Token read_identifier_or_keyword();
  Token read_variable();
};

/* ============================================================
 * AST NODE (Abstract Syntax Tree)
 * ============================================================ */
enum class NodeKind : uint8_t {
  Program,
  EchoStmt,
  ExprStmt,
  Assignment,
  CompoundAssign,
  IfStmt,
  WhileStmt,
  ForStmt,
  FunctionDecl,
  ReturnStmt,
  BreakStmt,
  ContinueStmt,
  Block,
  BinaryOp,
  UnaryOp,
  IntLiteral,
  StringLiteral,
  BoolLiteral,
  NullLiteral,
  Variable,
  FunctionCall,
  ArrayAccess,
  ArrayLiteral,
  ArrayPush,
  Concat
};

/* Node AST — representasi kode PHP dalam pohon */
struct AstNode {
  NodeKind kind;
  int64_t int_val;
  char str_val[256];
  bool bool_val;
  TokenType op;
  char name[128];

  AstNode *left;
  AstNode *right;
  AstNode *condition;
  AstNode *body;
  AstNode *else_body;
  AstNode *init_node;
  AstNode *update_node;

  AstNode *children[64];
  int child_count;
  AstNode *params[16];
  int param_count;
};

/* ============================================================
 * PARSER PHP (PHP Parser)
 * ============================================================ */
class PhpParser {
public:
  void init(PhpLexer *lexer);
  AstNode *parse_program();

private:
  PhpLexer *lex;
  Token current;

  void advance();
  bool check(TokenType type);
  bool match(TokenType type);
  bool expect(TokenType type);

  AstNode *alloc_node(NodeKind kind);
  AstNode *parse_statement();
  AstNode *parse_echo_stmt();
  AstNode *parse_if_stmt();
  AstNode *parse_while_stmt();
  AstNode *parse_for_stmt();
  AstNode *parse_function_decl();
  AstNode *parse_return_stmt();
  AstNode *parse_block();
  AstNode *parse_expression();
  AstNode *parse_assignment_expr();
  AstNode *parse_or_expr();
  AstNode *parse_and_expr();
  AstNode *parse_equality_expr();
  AstNode *parse_comparison_expr();
  AstNode *parse_concat_expr();
  AstNode *parse_additive_expr();
  AstNode *parse_multiplicative_expr();
  AstNode *parse_unary_expr();
  AstNode *parse_postfix_expr();
  AstNode *parse_primary_expr();
  AstNode *parse_function_call(const char *name);
  AstNode *parse_array_literal();
};

/* ============================================================
 * VARIABEL DAN SCOPE PHP
 * ============================================================ */
struct PhpVariable {
  char name[128];
  PhpValue *value;
  PhpVariable *next;
};

struct PhpFunction {
  char name[128];
  AstNode *node;
  PhpFunction *next;
};

struct PhpScope {
  PhpVariable *variables;
  PhpScope *parent;
  PhpValue *return_value;
  bool has_return;
  bool has_break;
  bool has_continue;
};

/* ============================================================
 * INTERPRETER PHP
 * ============================================================ */
class PhpInterpreter {
public:
  /* Tipe fungsi built-in C++ */
  typedef PhpValue *(*BuiltinFunc)(PhpInterpreter *interp, PhpValue **args,
                                   int argc);

  void init();
  void execute(AstNode *program);
  PhpValue *eval(AstNode *node);
  void register_builtin(const char *name, BuiltinFunc func);

  /* Pembuat nilai PHP */
  PhpValue *create_null();
  PhpValue *create_bool(bool val);
  PhpValue *create_int(int64_t val);
  PhpValue *create_string(const char *str);
  PhpValue *create_string_n(const char *str, int len);
  PhpValue *create_array();

  /* Bebaskan nilai PHP */
  void free_value(PhpValue *val);

  /* Konversi nilai */
  const char *value_to_cstr(PhpValue *val);
  int64_t value_to_int(PhpValue *val);
  bool value_to_bool(PhpValue *val);

  /* Akses scope */
  PhpValue *get_variable(const char *name);
  void set_variable(const char *name, PhpValue *value);

private:
  PhpScope *current_scope;
  PhpFunction *functions;

  struct BuiltinEntry {
    char name[128];
    BuiltinFunc func;
    BuiltinEntry *next;
  };
  BuiltinEntry *builtins;

  /* Value pool — menghindari leak memori */
  static const int VALUE_POOL_SIZE = 4096;
  PhpValue value_pool[VALUE_POOL_SIZE];
  PhpValue *free_list;
  int pool_initialized;

  PhpValue *alloc_value();

  /* String pool — menghindari string leak */
  static const int STR_POOL_SIZE = 256;
  static const int STR_SLOT_SIZE = 256;
  char str_pool[STR_POOL_SIZE][STR_SLOT_SIZE];
  bool str_pool_used[STR_POOL_SIZE];
  int str_pool_initialized;

  char *alloc_str(const char *s, int len);
  void free_str(char *s);

  /* Scope management */
  void push_scope();
  void pop_scope();

  /* Pencarian */
  PhpFunction *find_function(const char *name);
  BuiltinEntry *find_builtin(const char *name);

  /* Eksekusi statement */
  void exec_statement(AstNode *node);
  void exec_echo(AstNode *node);
  void exec_if(AstNode *node);
  void exec_while(AstNode *node);
  void exec_for(AstNode *node);
  void exec_function_decl(AstNode *node);
  void exec_block(AstNode *node);

  /* Evaluasi expression */
  PhpValue *eval_binary(AstNode *node);
  PhpValue *eval_unary(AstNode *node);
  PhpValue *eval_call(AstNode *node);
  PhpValue *eval_assignment(AstNode *node);
  PhpValue *eval_compound_assign(AstNode *node);
  PhpValue *eval_array_access(AstNode *node);
  PhpValue *eval_array_literal(AstNode *node);

  /* Operasi nilai */
  PhpValue *value_add(PhpValue *a, PhpValue *b);
  PhpValue *value_sub(PhpValue *a, PhpValue *b);
  PhpValue *value_mul(PhpValue *a, PhpValue *b);
  PhpValue *value_div(PhpValue *a, PhpValue *b);
  PhpValue *value_mod(PhpValue *a, PhpValue *b);
  PhpValue *value_concat(PhpValue *a, PhpValue *b);
  bool values_equal(PhpValue *a, PhpValue *b);
  bool values_less(PhpValue *a, PhpValue *b);

  /* Buffer untuk konversi string */
  char conversion_buf[512];
};

/* ============================================================
 * PHP RUNTIME — top-level engine
 * ============================================================ */
class PhpRuntime {
public:
  void init();
  void execute_script(const char *source);
  PhpInterpreter *get_interpreter() { return &interpreter; }

private:
  PhpLexer lexer;
  PhpParser parser;
  PhpInterpreter interpreter;

  void register_kernel_builtins();
};
