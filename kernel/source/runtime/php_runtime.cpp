/*
 * LittleOS Gajah PHP - php_runtime.cpp
 * PHP 8 Runtime Engine — Lexer, Parser, Interpreter
 * Ini adalah INTI KERNEL — semua logika OS berjalan dalam PHP
 *
 * Fitur PHP yang didukung:
 *   - echo, print
 *   - Variabel ($var)
 *   - String ("...", '...'), Integer, Boolean, Null
 *   - Operator: + - * / % . (concat)
 *   - Perbandingan: == != < > <= >=
 *   - Logika: && || !
 *   - Assignment: = += -= .=
 *   - if / elseif / else
 *   - while, for
 *   - function declaration & call
 *   - return, break, continue
 *   - Array: [1,2,3], ["key" => "val"]
 *   - Built-in functions (echo, strlen, substr, dll)
 *   - Komentar: // * #
 */

#include "php_runtime.hpp"
#include "console.hpp"
#include "desktop.hpp"
#include "hal.hpp"
#include "system.hpp"
#include "themes.hpp"
#include "ui.hpp"
#include <string.h>

/* ============================================================
 * UTILITY — helper string untuk runtime
 * ============================================================ */
// static char *str_dup(const char *s) {
//   if (!s)
//     return nullptr;
//   size_t len = hal::string::strlen(s);
//   char *r = (char *)hal::memory::kmalloc(len + 1);
//   if (r)
//     hal::string::strcpy(r, s);
//   return r;
// }
//
// static char *str_dup_n(const char *s, int len) {
//   if (!s || len <= 0)
//     return nullptr;
//   char *r = (char *)hal::memory::kmalloc(len + 1);
//   if (r) {
//     memcpy(r, s, len);
//     r[len] = '\0';
//   }
//   return r;
// }

static char *str_concat(const char *a, const char *b) {
  size_t la = a ? hal::string::strlen(a) : 0;
  size_t lb = b ? hal::string::strlen(b) : 0;
  char *r = (char *)hal::memory::kmalloc(la + lb + 1);
  if (r) {
    if (a)
      memcpy(r, a, la);
    if (b)
      memcpy(r + la, b, lb);
    r[la + lb] = '\0';
  }
  return r;
}

/* ============================================================
 * PHP LEXER — Tokenizer PHP 8
 * ============================================================ */
void PhpLexer::init(const char *source) {
  src = source;
  pos = 0;
  len = 0;
  line = 1;
  has_peeked = false;
  while (src[len])
    len++;
}

char PhpLexer::current() const { return (pos < len) ? src[pos] : '\0'; }

char PhpLexer::peek_char() const {
  return (pos + 1 < len) ? src[pos + 1] : '\0';
}

char PhpLexer::advance() {
  char c = current();
  if (c == '\n')
    line++;
  pos++;
  return c;
}

void PhpLexer::skip_whitespace_and_comments() {
  while (pos < len) {
    char c = current();
    /* Whitespace */
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      advance();
      continue;
    }
    /* Komentar // atau # */
    if ((c == '/' && peek_char() == '/') || c == '#') {
      while (pos < len && current() != '\n')
        advance();
      continue;
    }
    /* Komentar * ... */
    if (c == '/' && peek_char() == '*') {
      advance();
      advance(); /* skip /\* */
      while (pos < len) {
        if (current() == '*' && peek_char() == '/') {
          advance();
          advance();
          break;
        }
        advance();
      }
      continue;
    }
    break;
  }
}

Token PhpLexer::make_token(TokenType type, const char *text) {
  Token t;
  t.type = type;
  t.line = line;
  size_t tlen = hal::string::strlen(text);
  if (tlen >= 255)
    tlen = 255;
  memcpy(t.text, text, tlen);
  t.text[tlen] = '\0';
  return t;
}

Token PhpLexer::read_string(char quote) {
  advance(); /* skip pembukaan quote */
  char buf[256];
  int i = 0;

  while (pos < len && current() != quote && i < 254) {
    if (current() == '\\') {
      advance();
      char esc = current();
      switch (esc) {
      case 'n':
        buf[i++] = '\n';
        break;
      case 't':
        buf[i++] = '\t';
        break;
      case 'r':
        buf[i++] = '\r';
        break;
      case '\\':
        buf[i++] = '\\';
        break;
      case '\'':
        buf[i++] = '\'';
        break;
      case '"':
        buf[i++] = '"';
        break;
      case '$':
        buf[i++] = '$';
        break;
      default:
        buf[i++] = '\\';
        buf[i++] = esc;
        break;
      }
      advance();
    } else {
      buf[i++] = current();
      advance();
    }
  }
  if (current() == quote)
    advance(); /* skip penutup quote */
  buf[i] = '\0';
  return make_token(TokenType::String, buf);
}

Token PhpLexer::read_number() {
  char buf[64];
  int i = 0;
  while (pos < len && hal::string::is_digit(current()) && i < 62) {
    buf[i++] = advance();
  }
  buf[i] = '\0';
  return make_token(TokenType::Integer, buf);
}

Token PhpLexer::read_identifier_or_keyword() {
  char buf[128];
  int i = 0;
  while (pos < len && (hal::string::is_alnum(current()) || current() == '_') &&
         i < 126) {
    buf[i++] = advance();
  }
  buf[i] = '\0';

  /* Cek keyword */
  if (hal::string::strcmp(buf, "echo") == 0)
    return make_token(TokenType::Echo, buf);
  if (hal::string::strcmp(buf, "if") == 0)
    return make_token(TokenType::If, buf);
  if (hal::string::strcmp(buf, "else") == 0)
    return make_token(TokenType::Else, buf);
  if (hal::string::strcmp(buf, "elseif") == 0)
    return make_token(TokenType::ElseIf, buf);
  if (hal::string::strcmp(buf, "while") == 0)
    return make_token(TokenType::While, buf);
  if (hal::string::strcmp(buf, "for") == 0)
    return make_token(TokenType::For, buf);
  if (hal::string::strcmp(buf, "foreach") == 0)
    return make_token(TokenType::ForEach, buf);
  if (hal::string::strcmp(buf, "as") == 0)
    return make_token(TokenType::As, buf);
  if (hal::string::strcmp(buf, "function") == 0)
    return make_token(TokenType::Function, buf);
  if (hal::string::strcmp(buf, "return") == 0)
    return make_token(TokenType::Return, buf);
  if (hal::string::strcmp(buf, "true") == 0)
    return make_token(TokenType::True, buf);
  if (hal::string::strcmp(buf, "false") == 0)
    return make_token(TokenType::False, buf);
  if (hal::string::strcmp(buf, "null") == 0)
    return make_token(TokenType::NullLiteral, buf);
  if (hal::string::strcmp(buf, "break") == 0)
    return make_token(TokenType::Break, buf);
  if (hal::string::strcmp(buf, "continue") == 0)
    return make_token(TokenType::Continue, buf);

  return make_token(TokenType::Identifier, buf);
}

Token PhpLexer::read_variable() {
  advance(); /* skip $ */
  char buf[128];
  int i = 0;
  while (pos < len && (hal::string::is_alnum(current()) || current() == '_') &&
         i < 126) {
    buf[i++] = advance();
  }
  buf[i] = '\0';
  return make_token(TokenType::Variable, buf);
}

Token PhpLexer::next_token() {
  if (has_peeked) {
    has_peeked = false;
    return peeked;
  }

  skip_whitespace_and_comments();

  if (pos >= len)
    return make_token(TokenType::Eof, "");

  char c = current();

  /* PHP open tag <?php */
  if (c == '<' && peek_char() == '?') {
    advance();
    advance(); /* skip <? */
    /* Skip "php" */
    if (current() == 'p')
      advance();
    if (current() == 'h')
      advance();
    if (current() == 'p')
      advance();
    return make_token(TokenType::OpenTag, "<?php");
  }

  /* PHP close tag ?> */
  if (c == '?' && peek_char() == '>') {
    advance();
    advance();
    return make_token(TokenType::CloseTag, "?>");
  }

  /* String */
  if (c == '"' || c == '\'')
    return read_string(c);

  /* Number */
  if (hal::string::is_digit(c))
    return read_number();

  /* Variable */
  if (c == '$')
    return read_variable();

  /* Identifier / Keyword */
  if (hal::string::is_alpha(c) || c == '_')
    return read_identifier_or_keyword();

  /* Operator dan delimiter */
  switch (c) {
  case '+':
    advance();
    if (current() == '+') {
      advance();
      return make_token(TokenType::Increment, "++");
    }
    if (current() == '=') {
      advance();
      return make_token(TokenType::PlusAssign, "+=");
    }
    return make_token(TokenType::Plus, "+");
  case '-':
    advance();
    if (current() == '-') {
      advance();
      return make_token(TokenType::Decrement, "--");
    }
    if (current() == '>') {
      advance();
      return make_token(TokenType::Arrow, "->");
    }
    if (current() == '=') {
      advance();
      return make_token(TokenType::MinusAssign, "-=");
    }
    return make_token(TokenType::Minus, "-");
  case '*':
    advance();
    if (current() == '=') {
      advance();
      return make_token(TokenType::StarAssign, "*=");
    }
    return make_token(TokenType::Star, "*");
  case '/':
    advance();
    if (current() == '=') {
      advance();
      return make_token(TokenType::SlashAssign, "/=");
    }
    return make_token(TokenType::Slash, "/");
  case '%':
    advance();
    return make_token(TokenType::Percent, "%");
  case '.':
    advance();
    if (current() == '=') {
      advance();
      return make_token(TokenType::DotAssign, ".=");
    }
    return make_token(TokenType::Dot, ".");
  case '=':
    advance();
    if (current() == '=') {
      advance();
      return make_token(TokenType::Equal, "==");
    }
    if (current() == '>') {
      advance();
      return make_token(TokenType::DoubleArrow, "=>");
    }
    return make_token(TokenType::Assign, "=");
  case '!':
    advance();
    if (current() == '=') {
      advance();
      return make_token(TokenType::NotEqual, "!=");
    }
    return make_token(TokenType::Not, "!");
  case '<':
    advance();
    if (current() == '=') {
      advance();
      return make_token(TokenType::LessEqual, "<=");
    }
    return make_token(TokenType::Less, "<");
  case '>':
    advance();
    if (current() == '=') {
      advance();
      return make_token(TokenType::GreaterEqual, ">=");
    }
    return make_token(TokenType::Greater, ">");
  case '&':
    advance();
    if (current() == '&') {
      advance();
      return make_token(TokenType::And, "&&");
    }
    return make_token(TokenType::Error, "&");
  case '|':
    advance();
    if (current() == '|') {
      advance();
      return make_token(TokenType::Or, "||");
    }
    return make_token(TokenType::Error, "|");
  case ';':
    advance();
    return make_token(TokenType::Semicolon, ";");
  case ',':
    advance();
    return make_token(TokenType::Comma, ",");
  case '(':
    advance();
    return make_token(TokenType::LeftParen, "(");
  case ')':
    advance();
    return make_token(TokenType::RightParen, ")");
  case '{':
    advance();
    return make_token(TokenType::LeftBrace, "{");
  case '}':
    advance();
    return make_token(TokenType::RightBrace, "}");
  case '[':
    advance();
    return make_token(TokenType::LeftBracket, "[");
  case ']':
    advance();
    return make_token(TokenType::RightBracket, "]");
  case ':':
    advance();
    if (current() == ':') {
      advance();
      return make_token(TokenType::DoubleColon, "::");
    }
    return make_token(TokenType::Error, ":");
  default:
    advance();
    char err[2] = {c, '\0'};
    return make_token(TokenType::Error, err);
  }
}

Token PhpLexer::peek_token() {
  if (!has_peeked) {
    peeked = next_token();
    has_peeked = true;
  }
  return peeked;
}

/* ============================================================
 * PHP PARSER — Recursive Descent Parser
 * Menghasilkan Abstract Syntax Tree (AST)
 * ============================================================ */
void PhpParser::init(PhpLexer *lexer) {
  lex = lexer;
  advance();
}

void PhpParser::advance() { current = lex->next_token(); }

bool PhpParser::check(TokenType type) { return current.type == type; }

bool PhpParser::match(TokenType type) {
  if (current.type == type) {
    advance();
    return true;
  }
  return false;
}

bool PhpParser::expect(TokenType type) {
  if (current.type == type) {
    advance();
    return true;
  }
  /* Error: token tidak sesuai */
  Console::printf("[PHP Error] Baris %d: diharapkan token %d, dapat '%s'",
                  lex->get_line(), (int)type, current.text);
  return false;
}

AstNode *PhpParser::alloc_node(NodeKind kind) {
  AstNode *node = (AstNode *)hal::memory::kcalloc(1, sizeof(AstNode));
  if (node)
    node->kind = kind;
  return node;
}

AstNode *PhpParser::parse_program() {
  AstNode *program = alloc_node(NodeKind::Program);
  if (!program)
    return nullptr;

  /* Skip <?php tag */
  if (check(TokenType::OpenTag))
    advance();

  while (!check(TokenType::Eof) && !check(TokenType::CloseTag)) {
    AstNode *stmt = parse_statement();
    if (stmt && program->child_count < 64) {
      program->children[program->child_count++] = stmt;
    }
    if (!stmt) {
      /* Skip token yang bermasalah agar tidak loop forever */
      if (!check(TokenType::Eof) && !check(TokenType::CloseTag)) {
        advance();
      }
    }
  }

  return program;
}

AstNode *PhpParser::parse_statement() {
  if (check(TokenType::Echo))
    return parse_echo_stmt();
  if (check(TokenType::If))
    return parse_if_stmt();
  if (check(TokenType::While))
    return parse_while_stmt();
  if (check(TokenType::For))
    return parse_for_stmt();
  if (check(TokenType::Function))
    return parse_function_decl();
  if (check(TokenType::Return))
    return parse_return_stmt();
  if (check(TokenType::LeftBrace))
    return parse_block();

  if (check(TokenType::Break)) {
    advance();
    AstNode *node = alloc_node(NodeKind::BreakStmt);
    match(TokenType::Semicolon);
    return node;
  }
  if (check(TokenType::Continue)) {
    advance();
    AstNode *node = alloc_node(NodeKind::ContinueStmt);
    match(TokenType::Semicolon);
    return node;
  }

  /* Expression statement */
  AstNode *expr = parse_expression();
  if (expr) {
    AstNode *stmt = alloc_node(NodeKind::ExprStmt);
    stmt->left = expr;
    match(TokenType::Semicolon);
    return stmt;
  }

  return nullptr;
}

AstNode *PhpParser::parse_echo_stmt() {
  advance(); /* skip 'echo' */
  AstNode *node = alloc_node(NodeKind::EchoStmt);

  /* echo bisa punya beberapa ekspresi dipisah koma */
  node->left = parse_expression();

  /* Tambahan ekspresi setelah koma */
  while (match(TokenType::Comma)) {
    AstNode *extra = parse_expression();
    if (extra && node->child_count < 64) {
      node->children[node->child_count++] = extra;
    }
  }

  match(TokenType::Semicolon);
  return node;
}

AstNode *PhpParser::parse_if_stmt() {
  advance(); /* skip 'if' */
  AstNode *node = alloc_node(NodeKind::IfStmt);

  expect(TokenType::LeftParen);
  node->condition = parse_expression();
  expect(TokenType::RightParen);

  if (check(TokenType::LeftBrace)) {
    node->body = parse_block();
  } else {
    node->body = parse_statement();
  }

  /* elseif / else */
  if (check(TokenType::ElseIf)) {
    node->else_body = parse_if_stmt(); /* Rekursif: elseif -> if baru */
  } else if (match(TokenType::Else)) {
    if (check(TokenType::If)) {
      node->else_body = parse_if_stmt();
    } else if (check(TokenType::LeftBrace)) {
      node->else_body = parse_block();
    } else {
      node->else_body = parse_statement();
    }
  }

  return node;
}

AstNode *PhpParser::parse_while_stmt() {
  advance(); /* skip 'while' */
  AstNode *node = alloc_node(NodeKind::WhileStmt);

  expect(TokenType::LeftParen);
  node->condition = parse_expression();
  expect(TokenType::RightParen);

  if (check(TokenType::LeftBrace)) {
    node->body = parse_block();
  } else {
    node->body = parse_statement();
  }

  return node;
}

AstNode *PhpParser::parse_for_stmt() {
  advance(); /* skip 'for' */
  AstNode *node = alloc_node(NodeKind::ForStmt);

  expect(TokenType::LeftParen);

  /* Init */
  if (!check(TokenType::Semicolon)) {
    node->init_node = parse_expression();
  }
  expect(TokenType::Semicolon);

  /* Condition */
  if (!check(TokenType::Semicolon)) {
    node->condition = parse_expression();
  }
  expect(TokenType::Semicolon);

  /* Update */
  if (!check(TokenType::RightParen)) {
    node->update_node = parse_expression();
  }
  expect(TokenType::RightParen);

  if (check(TokenType::LeftBrace)) {
    node->body = parse_block();
  } else {
    node->body = parse_statement();
  }

  return node;
}

AstNode *PhpParser::parse_function_decl() {
  advance(); /* skip 'function' */
  AstNode *node = alloc_node(NodeKind::FunctionDecl);

  /* Nama fungsi */
  hal::string::strcpy(node->name, current.text);
  expect(TokenType::Identifier);

  /* Parameter */
  expect(TokenType::LeftParen);
  while (!check(TokenType::RightParen) && !check(TokenType::Eof)) {
    if (check(TokenType::Variable) && node->param_count < 16) {
      AstNode *param = alloc_node(NodeKind::Variable);
      hal::string::strcpy(param->name, current.text);
      node->params[node->param_count++] = param;
      advance();
    }
    if (!match(TokenType::Comma))
      break;
  }
  expect(TokenType::RightParen);

  /* Body */
  node->body = parse_block();

  return node;
}

AstNode *PhpParser::parse_return_stmt() {
  advance(); /* skip 'return' */
  AstNode *node = alloc_node(NodeKind::ReturnStmt);

  if (!check(TokenType::Semicolon)) {
    node->left = parse_expression();
  }

  match(TokenType::Semicolon);
  return node;
}

AstNode *PhpParser::parse_block() {
  expect(TokenType::LeftBrace);
  AstNode *block = alloc_node(NodeKind::Block);

  while (!check(TokenType::RightBrace) && !check(TokenType::Eof)) {
    AstNode *stmt = parse_statement();
    if (stmt && block->child_count < 64) {
      block->children[block->child_count++] = stmt;
    }
    if (!stmt && !check(TokenType::RightBrace) && !check(TokenType::Eof)) {
      advance();
    }
  }

  expect(TokenType::RightBrace);
  return block;
}

/* ============================================================
 * EXPRESSION PARSING — dengan precedence
 * ============================================================ */
AstNode *PhpParser::parse_expression() { return parse_assignment_expr(); }

AstNode *PhpParser::parse_assignment_expr() {
  AstNode *left = parse_or_expr();

  if (check(TokenType::Assign)) {
    advance();
    AstNode *node = alloc_node(NodeKind::Assignment);
    node->left = left;
    node->right = parse_assignment_expr(); /* right-associative */
    return node;
  }

  if (check(TokenType::PlusAssign) || check(TokenType::MinusAssign) ||
      check(TokenType::StarAssign) || check(TokenType::SlashAssign) ||
      check(TokenType::DotAssign)) {
    TokenType op = current.type;
    advance();
    AstNode *node = alloc_node(NodeKind::CompoundAssign);
    node->op = op;
    node->left = left;
    node->right = parse_assignment_expr();
    return node;
  }

  return left;
}

AstNode *PhpParser::parse_or_expr() {
  AstNode *left = parse_and_expr();
  while (check(TokenType::Or)) {
    advance();
    AstNode *node = alloc_node(NodeKind::BinaryOp);
    node->op = TokenType::Or;
    node->left = left;
    node->right = parse_and_expr();
    left = node;
  }
  return left;
}

AstNode *PhpParser::parse_and_expr() {
  AstNode *left = parse_equality_expr();
  while (check(TokenType::And)) {
    advance();
    AstNode *node = alloc_node(NodeKind::BinaryOp);
    node->op = TokenType::And;
    node->left = left;
    node->right = parse_equality_expr();
    left = node;
  }
  return left;
}

AstNode *PhpParser::parse_equality_expr() {
  AstNode *left = parse_comparison_expr();
  while (check(TokenType::Equal) || check(TokenType::NotEqual)) {
    TokenType op = current.type;
    advance();
    AstNode *node = alloc_node(NodeKind::BinaryOp);
    node->op = op;
    node->left = left;
    node->right = parse_comparison_expr();
    left = node;
  }
  return left;
}

AstNode *PhpParser::parse_comparison_expr() {
  AstNode *left = parse_concat_expr();
  while (check(TokenType::Less) || check(TokenType::Greater) ||
         check(TokenType::LessEqual) || check(TokenType::GreaterEqual)) {
    TokenType op = current.type;
    advance();
    AstNode *node = alloc_node(NodeKind::BinaryOp);
    node->op = op;
    node->left = left;
    node->right = parse_concat_expr();
    left = node;
  }
  return left;
}

AstNode *PhpParser::parse_concat_expr() {
  AstNode *left = parse_additive_expr();
  while (check(TokenType::Dot)) {
    advance();
    AstNode *node = alloc_node(NodeKind::Concat);
    node->left = left;
    node->right = parse_additive_expr();
    left = node;
  }
  return left;
}

AstNode *PhpParser::parse_additive_expr() {
  AstNode *left = parse_multiplicative_expr();
  while (check(TokenType::Plus) || check(TokenType::Minus)) {
    TokenType op = current.type;
    advance();
    AstNode *node = alloc_node(NodeKind::BinaryOp);
    node->op = op;
    node->left = left;
    node->right = parse_multiplicative_expr();
    left = node;
  }
  return left;
}

AstNode *PhpParser::parse_multiplicative_expr() {
  AstNode *left = parse_unary_expr();
  while (check(TokenType::Star) || check(TokenType::Slash) ||
         check(TokenType::Percent)) {
    TokenType op = current.type;
    advance();
    AstNode *node = alloc_node(NodeKind::BinaryOp);
    node->op = op;
    node->left = left;
    node->right = parse_unary_expr();
    left = node;
  }
  return left;
}

AstNode *PhpParser::parse_unary_expr() {
  if (check(TokenType::Not) || check(TokenType::Minus)) {
    TokenType op = current.type;
    advance();
    AstNode *node = alloc_node(NodeKind::UnaryOp);
    node->op = op;
    node->left = parse_unary_expr();
    return node;
  }
  return parse_postfix_expr();
}

AstNode *PhpParser::parse_postfix_expr() {
  AstNode *left = parse_primary_expr();

  /* Array access: $var[index] */
  while (check(TokenType::LeftBracket)) {
    advance();
    if (check(TokenType::RightBracket)) {
      /* $arr[] = value — array push */
      advance();
      AstNode *node = alloc_node(NodeKind::ArrayPush);
      node->left = left;
      return node;
    }
    AstNode *node = alloc_node(NodeKind::ArrayAccess);
    node->left = left;
    node->right = parse_expression();
    expect(TokenType::RightBracket);
    left = node;
  }

  /* Increment / Decrement postfix */
  if (check(TokenType::Increment)) {
    advance();
    AstNode *node = alloc_node(NodeKind::UnaryOp);
    node->op = TokenType::Increment;
    node->left = left;
    return node;
  }
  if (check(TokenType::Decrement)) {
    advance();
    AstNode *node = alloc_node(NodeKind::UnaryOp);
    node->op = TokenType::Decrement;
    node->left = left;
    return node;
  }

  return left;
}

AstNode *PhpParser::parse_primary_expr() {
  /* Integer literal */
  if (check(TokenType::Integer)) {
    AstNode *node = alloc_node(NodeKind::IntLiteral);
    node->int_val = hal::string::atoi(current.text);
    advance();
    return node;
  }

  /* String literal */
  if (check(TokenType::String)) {
    AstNode *node = alloc_node(NodeKind::StringLiteral);
    hal::string::strncpy(node->str_val, current.text, 255);
    advance();
    return node;
  }

  /* Boolean */
  if (check(TokenType::True)) {
    advance();
    AstNode *node = alloc_node(NodeKind::BoolLiteral);
    node->bool_val = true;
    return node;
  }
  if (check(TokenType::False)) {
    advance();
    AstNode *node = alloc_node(NodeKind::BoolLiteral);
    node->bool_val = false;
    return node;
  }

  /* Null */
  if (check(TokenType::NullLiteral)) {
    advance();
    return alloc_node(NodeKind::NullLiteral);
  }

  /* Variable */
  if (check(TokenType::Variable)) {
    AstNode *node = alloc_node(NodeKind::Variable);
    hal::string::strcpy(node->name, current.text);
    advance();
    return node;
  }

  /* Function call atau identifier */
  if (check(TokenType::Identifier)) {
    char name[128];
    hal::string::strcpy(name, current.text);
    advance();
    if (check(TokenType::LeftParen)) {
      return parse_function_call(name);
    }
    /* Identifier biasa */
    AstNode *node = alloc_node(NodeKind::StringLiteral);
    hal::string::strcpy(node->str_val, name);
    return node;
  }

  /* Array literal */
  if (check(TokenType::LeftBracket)) {
    return parse_array_literal();
  }

  /* Grouping: ( expr ) */
  if (match(TokenType::LeftParen)) {
    AstNode *expr = parse_expression();
    expect(TokenType::RightParen);
    return expr;
  }

  /* Negative number */
  if (check(TokenType::Minus)) {
    advance();
    if (check(TokenType::Integer)) {
      AstNode *node = alloc_node(NodeKind::IntLiteral);
      node->int_val = -hal::string::atoi(current.text);
      advance();
      return node;
    }
  }

  return nullptr;
}

AstNode *PhpParser::parse_function_call(const char *name) {
  AstNode *node = alloc_node(NodeKind::FunctionCall);
  hal::string::strcpy(node->name, name);

  expect(TokenType::LeftParen);
  while (!check(TokenType::RightParen) && !check(TokenType::Eof)) {
    AstNode *arg = parse_expression();
    if (arg && node->child_count < 64) {
      node->children[node->child_count++] = arg;
    }
    if (!match(TokenType::Comma))
      break;
  }
  expect(TokenType::RightParen);

  return node;
}

AstNode *PhpParser::parse_array_literal() {
  advance(); /* skip [ */
  AstNode *node = alloc_node(NodeKind::ArrayLiteral);

  while (!check(TokenType::RightBracket) && !check(TokenType::Eof)) {
    AstNode *val = parse_expression();

    /* Cek apakah key => value */
    if (check(TokenType::DoubleArrow)) {
      advance();
      AstNode *pair = alloc_node(NodeKind::BinaryOp);
      pair->op = TokenType::DoubleArrow;
      pair->left = val;
      pair->right = parse_expression();
      val = pair;
    }

    if (val && node->child_count < 64) {
      node->children[node->child_count++] = val;
    }
    if (!match(TokenType::Comma))
      break;
  }
  expect(TokenType::RightBracket);

  return node;
}

/* ============================================================
 * PHP INTERPRETER — Menjalankan AST
 * ============================================================ */
void PhpInterpreter::init() {
  current_scope = nullptr;
  functions = nullptr;
  builtins = nullptr;

  /* Inisialisasi value pool */
  pool_initialized = 1;
  free_list = nullptr;
  for (int i = VALUE_POOL_SIZE - 1; i >= 0; i--) {
    value_pool[i].type = PhpType::Null;
    value_pool[i].str_val = nullptr;
    value_pool[i].in_pool = true;
    value_pool[i].ref_count = 0;
    value_pool[i].array_head = nullptr;
    value_pool[i].array_count = 0;
    /* Link free list menggunakan int_val sebagai next index */
    value_pool[i].int_val = (int64_t)(intptr_t)free_list;
    free_list = &value_pool[i];
  }

  /* Inisialisasi string pool */
  str_pool_initialized = 1;
  for (int i = 0; i < STR_POOL_SIZE; i++) {
    str_pool_used[i] = false;
  }

  push_scope(); /* Global scope */
}

/* Scope Management */
void PhpInterpreter::push_scope() {
  PhpScope *scope = (PhpScope *)hal::memory::kcalloc(1, sizeof(PhpScope));
  scope->parent = current_scope;
  scope->variables = nullptr;
  scope->return_value = nullptr;
  scope->has_return = false;
  scope->has_break = false;
  scope->has_continue = false;
  current_scope = scope;
}

void PhpInterpreter::pop_scope() {
  if (current_scope) {
    PhpScope *old = current_scope;
    current_scope = current_scope->parent;
    /* Bebaskan variabel dan nilainya dalam scope */
    PhpVariable *var = old->variables;
    while (var) {
      PhpVariable *next = var->next;
      if (var->value)
        free_value(var->value);
      hal::memory::kfree(var);
      var = next;
    }
    hal::memory::kfree(old);
  }
}

/* Variable Management */
PhpValue *PhpInterpreter::get_variable(const char *name) {
  PhpScope *scope = current_scope;
  while (scope) {
    PhpVariable *var = scope->variables;
    while (var) {
      if (hal::string::strcmp(var->name, name) == 0) {
        /* Bump ref_count — caller gets a reference */
        var->value->ref_count++;
        return var->value;
      }
      var = var->next;
    }
    scope = scope->parent;
  }
  return create_null();
}

void PhpInterpreter::set_variable(const char *name, PhpValue *value) {
  /* Cari di scope saat ini dulu */
  PhpVariable *var = current_scope->variables;
  while (var) {
    if (hal::string::strcmp(var->name, name) == 0) {
      /* Bebaskan nilai lama jika berbeda */
      if (var->value && var->value != value) {
        free_value(var->value);
      }
      var->value = value;
      if (value)
        value->ref_count++; /* Scope takes ownership */
      return;
    }
    var = var->next;
  }

  /* Buat variabel baru */
  PhpVariable *new_var =
      (PhpVariable *)hal::memory::kcalloc(1, sizeof(PhpVariable));
  hal::string::strncpy(new_var->name, name, 127);
  new_var->value = value;
  if (value)
    value->ref_count++; /* Scope takes ownership */
  new_var->next = current_scope->variables;
  current_scope->variables = new_var;
}

/* Function Management */
PhpFunction *PhpInterpreter::find_function(const char *name) {
  PhpFunction *f = functions;
  while (f) {
    if (hal::string::strcmp(f->name, name) == 0)
      return f;
    f = f->next;
  }
  return nullptr;
}

PhpInterpreter::BuiltinEntry *PhpInterpreter::find_builtin(const char *name) {
  BuiltinEntry *b = builtins;
  while (b) {
    if (hal::string::strcmp(b->name, name) == 0)
      return b;
    b = b->next;
  }
  return nullptr;
}

void PhpInterpreter::register_builtin(const char *name, BuiltinFunc func) {
  BuiltinEntry *entry =
      (BuiltinEntry *)hal::memory::kcalloc(1, sizeof(BuiltinEntry));
  hal::string::strncpy(entry->name, name, 127);
  entry->func = func;
  entry->next = builtins;
  builtins = entry;
}

/* ============================================================
 * VALUE POOL MANAGEMENT — menghindari memory leak
 * ============================================================ */
PhpValue *PhpInterpreter::alloc_value() {
  if (free_list) {
    PhpValue *v = free_list;
    free_list = (PhpValue *)(intptr_t)v->int_val;
    v->type = PhpType::Null;
    v->int_val = 0;
    v->bool_val = false;
    v->str_val = nullptr;
    v->str_len = 0;
    v->array_head = nullptr;
    v->array_count = 0;
    v->ref_count = 1;
    v->in_pool = true;
    return v;
  }
  /* Pool exhausted — fallback to kmalloc */
  PhpValue *v = (PhpValue *)hal::memory::kcalloc(1, sizeof(PhpValue));
  if (v) {
    v->in_pool = false;
    v->ref_count = 1;
  }
  return v;
}

void PhpInterpreter::free_value(PhpValue *val) {
  if (!val)
    return;
  /* Decrement ref count — only free when it reaches 0 */
  if (val->ref_count > 1) {
    val->ref_count--;
    return;
  }
  val->ref_count = 0;

  /* Bebaskan string yang dialokasikan */
  if (val->type == PhpType::String && val->str_val) {
    free_str(val->str_val);
    val->str_val = nullptr;
  }

  /* Bebaskan array entries */
  if (val->type == PhpType::Array) {
    PhpArrayEntry *e = val->array_head;
    while (e) {
      PhpArrayEntry *next = e->next;
      if (e->value)
        free_value(e->value);
      hal::memory::kfree(e);
      e = next;
    }
    val->array_head = nullptr;
    val->array_count = 0;
  }

  if (val->in_pool) {
    /* Kembalikan ke free list */
    val->type = PhpType::Null;
    val->int_val = (int64_t)(intptr_t)free_list;
    free_list = val;
  } else {
    hal::memory::kfree(val);
  }
}

char *PhpInterpreter::alloc_str(const char *s, int len) {
  if (!s || len <= 0)
    return nullptr;
  /* Coba dari string pool jika cukup kecil */
  if (len < STR_SLOT_SIZE - 1) {
    for (int i = 0; i < STR_POOL_SIZE; i++) {
      if (!str_pool_used[i]) {
        str_pool_used[i] = true;
        memcpy(str_pool[i], s, len);
        str_pool[i][len] = '\0';
        return str_pool[i];
      }
    }
  }
  /* Fallback to kmalloc */
  char *r = (char *)hal::memory::kmalloc(len + 1);
  if (r) {
    memcpy(r, s, len);
    r[len] = '\0';
  }
  return r;
}

void PhpInterpreter::free_str(char *s) {
  if (!s)
    return;
  /* Cek apakah dari string pool */
  for (int i = 0; i < STR_POOL_SIZE; i++) {
    if (s == str_pool[i]) {
      str_pool_used[i] = false;
      return;
    }
  }
  /* Bukan dari pool — kfree */
  hal::memory::kfree(s);
}

/* ============================================================
 * VALUE CREATION
 * ============================================================ */
PhpValue *PhpInterpreter::create_null() {
  PhpValue *v = alloc_value();
  if (v)
    v->type = PhpType::Null;
  return v;
}

PhpValue *PhpInterpreter::create_bool(bool val) {
  PhpValue *v = alloc_value();
  if (v) {
    v->type = PhpType::Bool;
    v->bool_val = val;
  }
  return v;
}

PhpValue *PhpInterpreter::create_int(int64_t val) {
  PhpValue *v = alloc_value();
  if (v) {
    v->type = PhpType::Int;
    v->int_val = val;
  }
  return v;
}

PhpValue *PhpInterpreter::create_string(const char *str) {
  PhpValue *v = alloc_value();
  if (v) {
    v->type = PhpType::String;
    int len = str ? (int)hal::string::strlen(str) : 0;
    v->str_val = alloc_str(str, len);
    v->str_len = len;
  }
  return v;
}

PhpValue *PhpInterpreter::create_string_n(const char *str, int len) {
  PhpValue *v = alloc_value();
  if (v) {
    v->type = PhpType::String;
    v->str_val = alloc_str(str, len);
    v->str_len = len;
  }
  return v;
}

PhpValue *PhpInterpreter::create_array() {
  PhpValue *v = alloc_value();
  if (v) {
    v->type = PhpType::Array;
    v->array_head = nullptr;
    v->array_count = 0;
  }
  return v;
}

/* ============================================================
 * VALUE CONVERSION
 * ============================================================ */
const char *PhpInterpreter::value_to_cstr(PhpValue *val) {
  if (!val)
    return "";
  switch (val->type) {
  case PhpType::Null:
    return "";
  case PhpType::Bool:
    return val->bool_val ? "1" : "";
  case PhpType::Int: {
    hal::string::itoa(val->int_val, conversion_buf);
    return conversion_buf;
  }
  case PhpType::String:
    return val->str_val ? val->str_val : "";
  case PhpType::Array:
    return "Array";
  default:
    return "";
  }
}

int64_t PhpInterpreter::value_to_int(PhpValue *val) {
  if (!val)
    return 0;
  switch (val->type) {
  case PhpType::Null:
    return 0;
  case PhpType::Bool:
    return val->bool_val ? 1 : 0;
  case PhpType::Int:
    return val->int_val;
  case PhpType::String:
    return hal::string::atoi(val->str_val ? val->str_val : "0");
  case PhpType::Array:
    return val->array_count > 0 ? 1 : 0;
  default:
    return 0;
  }
}

bool PhpInterpreter::value_to_bool(PhpValue *val) {
  if (!val)
    return false;
  switch (val->type) {
  case PhpType::Null:
    return false;
  case PhpType::Bool:
    return val->bool_val;
  case PhpType::Int:
    return val->int_val != 0;
  case PhpType::String:
    return val->str_val && val->str_len > 0 &&
           !(val->str_len == 1 && val->str_val[0] == '0');
  case PhpType::Array:
    return val->array_count > 0;
  default:
    return false;
  }
}

/* ============================================================
 * VALUE OPERATIONS
 * ============================================================ */
PhpValue *PhpInterpreter::value_add(PhpValue *a, PhpValue *b) {
  return create_int(value_to_int(a) + value_to_int(b));
}

PhpValue *PhpInterpreter::value_sub(PhpValue *a, PhpValue *b) {
  return create_int(value_to_int(a) - value_to_int(b));
}

PhpValue *PhpInterpreter::value_mul(PhpValue *a, PhpValue *b) {
  return create_int(value_to_int(a) * value_to_int(b));
}

PhpValue *PhpInterpreter::value_div(PhpValue *a, PhpValue *b) {
  int64_t divisor = value_to_int(b);
  if (divisor == 0) {
    Console::puts_colored("[PHP Warning] Division by zero\n", COLOR_WARNING);
    return create_int(0);
  }
  return create_int(value_to_int(a) / divisor);
}

PhpValue *PhpInterpreter::value_mod(PhpValue *a, PhpValue *b) {
  int64_t divisor = value_to_int(b);
  if (divisor == 0) {
    Console::puts_colored("[PHP Warning] Modulo by zero\n", COLOR_WARNING);
    return create_int(0);
  }
  return create_int(value_to_int(a) % divisor);
}

PhpValue *PhpInterpreter::value_concat(PhpValue *a, PhpValue *b) {
  const char *sa = value_to_cstr(a);
  /* value_to_cstr menggunakan conversion_buf, jadi kita perlu salin dulu */
  char buf_a[512];
  hal::string::strncpy(buf_a, sa, 511);
  buf_a[511] = '\0';

  const char *sb = value_to_cstr(b);

  char *result = str_concat(buf_a, sb);
  PhpValue *v = create_string(result);
  if (result)
    hal::memory::kfree(result);
  return v;
}

bool PhpInterpreter::values_equal(PhpValue *a, PhpValue *b) {
  if (!a && !b)
    return true;
  if (!a || !b)
    return false;

  if (a->type == PhpType::Int && b->type == PhpType::Int) {
    return a->int_val == b->int_val;
  }
  if (a->type == PhpType::String && b->type == PhpType::String) {
    return hal::string::strcmp(a->str_val ? a->str_val : "",
                               b->str_val ? b->str_val : "") == 0;
  }
  if (a->type == PhpType::Bool || b->type == PhpType::Bool) {
    return value_to_bool(a) == value_to_bool(b);
  }
  if (a->type == PhpType::Null && b->type == PhpType::Null)
    return true;
  if (a->type == PhpType::Null || b->type == PhpType::Null)
    return false;

  /* Fallback: bandingkan sebagai string */
  const char *sa = value_to_cstr(a);
  char buf_a[512];
  hal::string::strncpy(buf_a, sa, 511);
  buf_a[511] = '\0';
  const char *sb = value_to_cstr(b);
  return hal::string::strcmp(buf_a, sb) == 0;
}

bool PhpInterpreter::values_less(PhpValue *a, PhpValue *b) {
  if (a->type == PhpType::Int && b->type == PhpType::Int) {
    return a->int_val < b->int_val;
  }
  return value_to_int(a) < value_to_int(b);
}

/* ============================================================
 * STATEMENT EXECUTION
 * ============================================================ */
void PhpInterpreter::execute(AstNode *program) {
  if (!program)
    return;

  if (program->kind == NodeKind::Program || program->kind == NodeKind::Block) {
    for (int i = 0; i < program->child_count; i++) {
      exec_statement(program->children[i]);
      if (current_scope->has_return || current_scope->has_break ||
          current_scope->has_continue)
        break;
    }
  } else {
    exec_statement(program);
  }
}

void PhpInterpreter::exec_statement(AstNode *node) {
  if (!node)
    return;

  switch (node->kind) {
  case NodeKind::EchoStmt:
    exec_echo(node);
    break;
  case NodeKind::IfStmt:
    exec_if(node);
    break;
  case NodeKind::WhileStmt:
    exec_while(node);
    break;
  case NodeKind::ForStmt:
    exec_for(node);
    break;
  case NodeKind::FunctionDecl:
    exec_function_decl(node);
    break;
  case NodeKind::Block:
    exec_block(node);
    break;
  case NodeKind::ReturnStmt: {
    if (node->left) {
      current_scope->return_value = eval(node->left);
    } else {
      current_scope->return_value = create_null();
    }
    current_scope->has_return = true;
    break;
  }
  case NodeKind::BreakStmt:
    current_scope->has_break = true;
    break;
  case NodeKind::ContinueStmt:
    current_scope->has_continue = true;
    break;
  case NodeKind::ExprStmt: {
    PhpValue *tmp = eval(node->left);
    free_value(tmp);
    break;
  }
  default: {
    PhpValue *tmp = eval(node);
    free_value(tmp);
    break;
  }
  }
}

void PhpInterpreter::exec_echo(AstNode *node) {
  if (node->left) {
    PhpValue *val = eval(node->left);
    Console::puts(value_to_cstr(val));
    free_value(val);
  }
  /* Ekspresi tambahan (dipisah koma) */
  for (int i = 0; i < node->child_count; i++) {
    PhpValue *val = eval(node->children[i]);
    Console::puts(value_to_cstr(val));
    free_value(val);
  }
}

void PhpInterpreter::exec_if(AstNode *node) {
  PhpValue *cond = eval(node->condition);
  bool result = value_to_bool(cond);
  free_value(cond);
  if (result) {
    execute(node->body);
  } else if (node->else_body) {
    execute(node->else_body);
  }
}

void PhpInterpreter::exec_while(AstNode *node) {
  while (true) {
    PhpValue *cond = eval(node->condition);
    bool result = value_to_bool(cond);
    free_value(cond);
    if (!result)
      break;
    execute(node->body);
    if (current_scope->has_return)
      break;
    if (current_scope->has_break) {
      current_scope->has_break = false;
      break;
    }
    if (current_scope->has_continue) {
      current_scope->has_continue = false;
    }
  }
}

void PhpInterpreter::exec_for(AstNode *node) {
  /* Init */
  if (node->init_node) {
    PhpValue *tmp = eval(node->init_node);
    free_value(tmp);
  }

  while (true) {
    /* Condition */
    if (node->condition) {
      PhpValue *cond = eval(node->condition);
      bool result = value_to_bool(cond);
      free_value(cond);
      if (!result)
        break;
    }

    /* Body */
    execute(node->body);
    if (current_scope->has_return)
      break;
    if (current_scope->has_break) {
      current_scope->has_break = false;
      break;
    }
    if (current_scope->has_continue) {
      current_scope->has_continue = false;
    }

    /* Update */
    if (node->update_node) {
      PhpValue *tmp = eval(node->update_node);
      free_value(tmp);
    }
  }
}

void PhpInterpreter::exec_function_decl(AstNode *node) {
  PhpFunction *func =
      (PhpFunction *)hal::memory::kcalloc(1, sizeof(PhpFunction));
  hal::string::strcpy(func->name, node->name);
  func->node = node;
  func->next = functions;
  functions = func;
}

void PhpInterpreter::exec_block(AstNode *node) {
  for (int i = 0; i < node->child_count; i++) {
    exec_statement(node->children[i]);
    if (current_scope->has_return || current_scope->has_break ||
        current_scope->has_continue)
      break;
  }
}

/* ============================================================
 * EXPRESSION EVALUATION
 * ============================================================ */
PhpValue *PhpInterpreter::eval(AstNode *node) {
  if (!node)
    return create_null();

  switch (node->kind) {
  case NodeKind::IntLiteral:
    return create_int(node->int_val);
  case NodeKind::StringLiteral:
    return create_string(node->str_val);
  case NodeKind::BoolLiteral:
    return create_bool(node->bool_val);
  case NodeKind::NullLiteral:
    return create_null();
  case NodeKind::Variable:
    return get_variable(node->name);
  case NodeKind::BinaryOp:
    return eval_binary(node);
  case NodeKind::UnaryOp:
    return eval_unary(node);
  case NodeKind::Concat: {
    PhpValue *cl = eval(node->left);
    PhpValue *cr = eval(node->right);
    PhpValue *cc = value_concat(cl, cr);
    free_value(cl);
    free_value(cr);
    return cc;
  }
  case NodeKind::FunctionCall:
    return eval_call(node);
  case NodeKind::Assignment:
    return eval_assignment(node);
  case NodeKind::CompoundAssign:
    return eval_compound_assign(node);
  case NodeKind::ArrayAccess:
    return eval_array_access(node);
  case NodeKind::ArrayLiteral:
    return eval_array_literal(node);
  case NodeKind::ArrayPush:
    return eval(
        node->left); /* $arr[] -> disediakan sebagai target di assignment */
  case NodeKind::EchoStmt:
    exec_echo(node);
    return create_null();
  case NodeKind::ExprStmt:
    return eval(node->left);
  default:
    return create_null();
  }
}

PhpValue *PhpInterpreter::eval_binary(AstNode *node) {
  PhpValue *left = eval(node->left);
  PhpValue *right = eval(node->right);
  PhpValue *result;

  switch (node->op) {
  case TokenType::Plus:
    result = value_add(left, right);
    break;
  case TokenType::Minus:
    result = value_sub(left, right);
    break;
  case TokenType::Star:
    result = value_mul(left, right);
    break;
  case TokenType::Slash:
    result = value_div(left, right);
    break;
  case TokenType::Percent:
    result = value_mod(left, right);
    break;
  case TokenType::Equal:
    result = create_bool(values_equal(left, right));
    break;
  case TokenType::NotEqual:
    result = create_bool(!values_equal(left, right));
    break;
  case TokenType::Less:
    result = create_bool(values_less(left, right));
    break;
  case TokenType::Greater:
    result = create_bool(values_less(right, left));
    break;
  case TokenType::LessEqual:
    result = create_bool(!values_less(right, left));
    break;
  case TokenType::GreaterEqual:
    result = create_bool(!values_less(left, right));
    break;
  case TokenType::And:
    result = create_bool(value_to_bool(left) && value_to_bool(right));
    break;
  case TokenType::Or:
    result = create_bool(value_to_bool(left) || value_to_bool(right));
    break;
  default:
    result = create_null();
    break;
  }

  free_value(left);
  free_value(right);
  return result;
}

PhpValue *PhpInterpreter::eval_unary(AstNode *node) {
  PhpValue *operand = eval(node->left);
  PhpValue *result;

  switch (node->op) {
  case TokenType::Not:
    result = create_bool(!value_to_bool(operand));
    free_value(operand);
    return result;
  case TokenType::Minus:
    result = create_int(-value_to_int(operand));
    free_value(operand);
    return result;
  case TokenType::Increment: {
    /* $var++ */
    int64_t val = value_to_int(operand);
    free_value(operand);
    if (node->left && node->left->kind == NodeKind::Variable) {
      PhpValue *old = create_int(val);
      set_variable(node->left->name, create_int(val + 1));
      return old;
    }
    return create_int(val + 1);
  }
  case TokenType::Decrement: {
    int64_t val = value_to_int(operand);
    free_value(operand);
    if (node->left && node->left->kind == NodeKind::Variable) {
      PhpValue *old = create_int(val);
      set_variable(node->left->name, create_int(val - 1));
      return old;
    }
    return create_int(val - 1);
  }
  default:
    return operand;
  }
}

PhpValue *PhpInterpreter::eval_call(AstNode *node) {
  /* Cek built-in dulu */
  BuiltinEntry *builtin = find_builtin(node->name);
  if (builtin) {
    PhpValue *args[64];
    int argc = 0;
    for (int i = 0; i < node->child_count && i < 64; i++) {
      args[argc++] = eval(node->children[i]);
    }
    PhpValue *result = builtin->func(this, args, argc);
    /* Free arguments — builtin sudah membaca nilainya */
    for (int i = 0; i < argc; i++) {
      free_value(args[i]);
    }
    return result;
  }

  /* Cek user-defined function */
  PhpFunction *func = find_function(node->name);
  if (func) {
    push_scope();

    /* Bind parameter */
    AstNode *decl = func->node;
    for (int i = 0; i < decl->param_count && i < node->child_count; i++) {
      PhpValue *arg_val = eval(node->children[i]);
      set_variable(decl->params[i]->name, arg_val);
    }

    /* Eksekusi body */
    execute(decl->body);

    PhpValue *result = current_scope->return_value;
    if (!result)
      result = create_null();
    /* Detach result dari scope agar tidak di-free oleh pop_scope */
    current_scope->return_value = nullptr;

    pop_scope();
    /* Reset return flag di parent scope */
    if (current_scope)
      current_scope->has_return = false;

    return result;
  }

  Console::state.cursor_row = 0;
  Console::state.cursor_col = 0;
  Console::state.fg_color = TW_BLACK;
  Console::printf("[PHP Error] Fungsi '%s' tidak ditemukan", node->name);
  return create_null();
}

PhpValue *PhpInterpreter::eval_assignment(AstNode *node) {
  PhpValue *val = eval(node->right);

  if (node->left->kind == NodeKind::Variable) {
    set_variable(node->left->name, val);
  } else if (node->left->kind == NodeKind::ArrayAccess) {
    /* $arr[key] = value */
    PhpValue *arr = eval(node->left->left);
    if (arr->type != PhpType::Array) {
      /* Konversi ke array */
      arr->type = PhpType::Array;
      arr->array_head = nullptr;
      arr->array_count = 0;
    }
    PhpValue *key = eval(node->left->right);

    /* Tambah atau update entry */
    char key_str[128];
    if (key->type == PhpType::Int) {
      hal::string::itoa(key->int_val, key_str);
    } else {
      hal::string::strncpy(key_str, value_to_cstr(key), 127);
      key_str[127] = '\0';
    }
    free_value(key);

    PhpArrayEntry *entry = arr->array_head;
    while (entry) {
      if (hal::string::strcmp(entry->key, key_str) == 0) {
        if (entry->value && entry->value != val)
          free_value(entry->value);
        entry->value = val;
        val->ref_count++; /* Array entry takes ownership */
        free_value(arr);
        return val;
      }
      entry = entry->next;
    }

    /* Buat entry baru */
    PhpArrayEntry *new_entry =
        (PhpArrayEntry *)hal::memory::kcalloc(1, sizeof(PhpArrayEntry));
    hal::string::strcpy(new_entry->key, key_str);
    new_entry->value = val;
    val->ref_count++; /* Array entry takes ownership */
    new_entry->is_int_key = (key->type == PhpType::Int);
    new_entry->int_key = key->int_val;
    new_entry->next = nullptr;

    /* Tambahkan di akhir */
    if (!arr->array_head) {
      arr->array_head = new_entry;
    } else {
      PhpArrayEntry *last = arr->array_head;
      while (last->next)
        last = last->next;
      last->next = new_entry;
    }
    arr->array_count++;
    free_value(arr);
  } else if (node->left->kind == NodeKind::ArrayPush) {
    /* $arr[] = value */
    PhpValue *arr = eval(node->left->left);
    if (arr->type != PhpType::Array) {
      if (node->left->left->kind == NodeKind::Variable) {
        free_value(arr);
        arr = create_array();
        set_variable(node->left->left->name, arr);
      }
    }

    PhpArrayEntry *new_entry =
        (PhpArrayEntry *)hal::memory::kcalloc(1, sizeof(PhpArrayEntry));
    hal::string::itoa((int64_t)arr->array_count, new_entry->key);
    new_entry->value = val;
    val->ref_count++; /* Array entry takes ownership */
    new_entry->is_int_key = true;
    new_entry->int_key = arr->array_count;
    new_entry->next = nullptr;

    if (!arr->array_head) {
      arr->array_head = new_entry;
    } else {
      PhpArrayEntry *last = arr->array_head;
      while (last->next)
        last = last->next;
      last->next = new_entry;
    }
    arr->array_count++;
    free_value(arr);
  }

  return val;
}

PhpValue *PhpInterpreter::eval_compound_assign(AstNode *node) {
  PhpValue *current_val = eval(node->left);
  PhpValue *right_val = eval(node->right);
  PhpValue *result = nullptr;

  switch (node->op) {
  case TokenType::PlusAssign:
    result = value_add(current_val, right_val);
    break;
  case TokenType::MinusAssign:
    result = value_sub(current_val, right_val);
    break;
  case TokenType::StarAssign:
    result = value_mul(current_val, right_val);
    break;
  case TokenType::SlashAssign:
    result = value_div(current_val, right_val);
    break;
  case TokenType::DotAssign:
    result = value_concat(current_val, right_val);
    break;
  default:
    result = current_val;
    current_val = nullptr;
    break;
  }

  free_value(current_val);
  free_value(right_val);

  if (node->left->kind == NodeKind::Variable) {
    set_variable(node->left->name, result);
  }

  return result;
}

PhpValue *PhpInterpreter::eval_array_access(AstNode *node) {
  PhpValue *arr = eval(node->left);
  PhpValue *key = eval(node->right);

  if (arr->type == PhpType::String) {
    /* String index access */
    int64_t idx = value_to_int(key);
    free_value(key);
    PhpValue *res;
    if (arr->str_val && idx >= 0 && idx < arr->str_len) {
      char buf[2] = {arr->str_val[idx], '\0'};
      res = create_string(buf);
    } else {
      res = create_string("");
    }
    free_value(arr);
    return res;
  }

  if (arr->type != PhpType::Array) {
    free_value(key);
    free_value(arr);
    return create_null();
  }

  char key_str[128];
  if (key->type == PhpType::Int) {
    hal::string::itoa(key->int_val, key_str);
  } else {
    hal::string::strncpy(key_str, value_to_cstr(key), 127);
    key_str[127] = '\0';
  }
  free_value(key);

  PhpArrayEntry *entry = arr->array_head;
  while (entry) {
    if (hal::string::strcmp(entry->key, key_str) == 0) {
      entry->value->ref_count++; /* Caller gets a reference */
      free_value(arr);
      return entry->value;
    }
    entry = entry->next;
  }

  free_value(arr);
  return create_null();
}

PhpValue *PhpInterpreter::eval_array_literal(AstNode *node) {
  PhpValue *arr = create_array();
  int auto_key = 0;

  for (int i = 0; i < node->child_count; i++) {
    AstNode *child = node->children[i];

    if (child->kind == NodeKind::BinaryOp &&
        child->op == TokenType::DoubleArrow) {
      /* key => value */
      PhpValue *key = eval(child->left);
      PhpValue *val = eval(child->right);

      PhpArrayEntry *entry =
          (PhpArrayEntry *)hal::memory::kcalloc(1, sizeof(PhpArrayEntry));
      if (key->type == PhpType::Int) {
        hal::string::itoa(key->int_val, entry->key);
        entry->is_int_key = true;
        entry->int_key = key->int_val;
        if (key->int_val >= auto_key)
          auto_key = (int)key->int_val + 1;
      } else {
        hal::string::strncpy(entry->key, value_to_cstr(key), 127);
        entry->is_int_key = false;
      }
      free_value(key);
      entry->value = val;
      /* val already has ref_count=1 from creation; entry owns it */
      entry->next = nullptr;

      if (!arr->array_head) {
        arr->array_head = entry;
      } else {
        PhpArrayEntry *last = arr->array_head;
        while (last->next)
          last = last->next;
        last->next = entry;
      }
      arr->array_count++;
    } else {
      /* Auto-indexed value */
      PhpValue *val = eval(child);

      PhpArrayEntry *entry =
          (PhpArrayEntry *)hal::memory::kcalloc(1, sizeof(PhpArrayEntry));
      hal::string::itoa((int64_t)auto_key, entry->key);
      entry->is_int_key = true;
      entry->int_key = auto_key;
      entry->value = val;
      /* val already has ref_count=1 from creation; entry owns it */
      entry->next = nullptr;

      if (!arr->array_head) {
        arr->array_head = entry;
      } else {
        PhpArrayEntry *last = arr->array_head;
        while (last->next)
          last = last->next;
        last->next = entry;
      }
      arr->array_count++;
      auto_key++;
    }
  }

  return arr;
}

/* ============================================================
 * BUILT-IN PHP FUNCTIONS
 * ============================================================ */

/* strlen($str) */
static PhpValue *builtin_strlen(PhpInterpreter *interp, PhpValue **args,
                                int argc) {
  if (argc < 1)
    return interp->create_int(0);
  const char *s = interp->value_to_cstr(args[0]);
  return interp->create_int((int64_t)hal::string::strlen(s));
}

/* substr($str, $start, $length?) */
static PhpValue *builtin_substr(PhpInterpreter *interp, PhpValue **args,
                                int argc) {
  if (argc < 2)
    return interp->create_string("");
  const char *s = interp->value_to_cstr(args[0]);
  /* Kita perlu salin string karena value_to_cstr menggunakan buffer bersama */
  char buf[512];
  hal::string::strncpy(buf, s, 511);
  buf[511] = '\0';
  int64_t start = interp->value_to_int(args[1]);
  int64_t slen = (int64_t)hal::string::strlen(buf);

  if (start < 0)
    start = slen + start;
  if (start < 0)
    start = 0;
  if (start >= slen)
    return interp->create_string("");

  int64_t length = slen - start;
  if (argc >= 3) {
    length = interp->value_to_int(args[2]);
    if (length < 0)
      length = (slen - start) + length;
    if (length < 0)
      length = 0;
  }
  if (start + length > slen)
    length = slen - start;

  return interp->create_string_n(buf + start, (int)length);
}

/* strtoupper($str) */
static PhpValue *builtin_strtoupper(PhpInterpreter *interp, PhpValue **args,
                                    int argc) {
  if (argc < 1)
    return interp->create_string("");
  const char *s = interp->value_to_cstr(args[0]);
  size_t len = hal::string::strlen(s);
  char *buf = (char *)hal::memory::kmalloc(len + 1);
  for (size_t i = 0; i < len; i++)
    buf[i] = hal::string::to_upper(s[i]);
  buf[len] = '\0';
  PhpValue *r = interp->create_string(buf);
  hal::memory::kfree(buf);
  return r;
}

/* strtolower($str) */
static PhpValue *builtin_strtolower(PhpInterpreter *interp, PhpValue **args,
                                    int argc) {
  if (argc < 1)
    return interp->create_string("");
  const char *s = interp->value_to_cstr(args[0]);
  size_t len = hal::string::strlen(s);
  char *buf = (char *)hal::memory::kmalloc(len + 1);
  for (size_t i = 0; i < len; i++)
    buf[i] = hal::string::to_lower(s[i]);
  buf[len] = '\0';
  PhpValue *r = interp->create_string(buf);
  hal::memory::kfree(buf);
  return r;
}

/* intval($val) */
static PhpValue *builtin_intval(PhpInterpreter *interp, PhpValue **args,
                                int argc) {
  if (argc < 1)
    return interp->create_int(0);
  return interp->create_int(interp->value_to_int(args[0]));
}

/* strval($val) */
static PhpValue *builtin_strval(PhpInterpreter *interp, PhpValue **args,
                                int argc) {
  if (argc < 1)
    return interp->create_string("");
  return interp->create_string(interp->value_to_cstr(args[0]));
}

/* count($arr) */
static PhpValue *builtin_count(PhpInterpreter *interp, PhpValue **args,
                               int argc) {
  if (argc < 1)
    return interp->create_int(0);
  if (args[0]->type == PhpType::Array)
    return interp->create_int(args[0]->array_count);
  if (args[0]->type == PhpType::String)
    return interp->create_int(args[0]->str_len);
  return interp->create_int(0);
}

/* isset($var) */
static PhpValue *builtin_isset(PhpInterpreter *interp, PhpValue **args,
                               int argc) {
  if (argc < 1)
    return interp->create_bool(false);
  return interp->create_bool(args[0]->type != PhpType::Null);
}

/* print($str) */
static PhpValue *builtin_print(PhpInterpreter *interp, PhpValue **args,
                               int argc) {
  if (argc >= 1) {
    Console::puts(interp->value_to_cstr(args[0]));
  }
  return interp->create_int(1);
}

/* var_dump($val) */
static PhpValue *builtin_var_dump(PhpInterpreter *interp, PhpValue **args,
                                  int argc) {
  for (int i = 0; i < argc; i++) {
    PhpValue *v = args[i];
    switch (v->type) {
    case PhpType::Null:
      Console::puts("NULL\n");
      break;
    case PhpType::Bool:
      Console::printf("bool(%s)\n", v->bool_val ? "true" : "false");
      break;
    case PhpType::Int:
      Console::printf("int(%d)\n", (int64_t)v->int_val);
      break;
    case PhpType::String:
      Console::printf("string(%d) \"%s\"\n", (int64_t)v->str_len,
                      v->str_val ? v->str_val : "");
      break;
    case PhpType::Array:
      Console::printf("array(%d) {\n", (int64_t)v->array_count);
      {
        PhpArrayEntry *e = v->array_head;
        while (e) {
          Console::printf("  [%s] => ", e->key);
          PhpValue *one_val = e->value;
          /* Sederhana: tampilkan tipe dan nilai */
          if (one_val) {
            Console::puts(interp->value_to_cstr(one_val));
          }
          Console::puts("\n");
          e = e->next;
        }
      }
      Console::puts("}\n");
      break;
    default:
      Console::puts("unknown\n");
      break;
    }
  }
  return interp->create_null();
}

/* str_repeat($str, $times) */
static PhpValue *builtin_str_repeat(PhpInterpreter *interp, PhpValue **args,
                                    int argc) {
  if (argc < 2)
    return interp->create_string("");
  const char *s = interp->value_to_cstr(args[0]);
  char buf[512];
  hal::string::strncpy(buf, s, 511);
  buf[511] = '\0';
  int64_t times = interp->value_to_int(args[1]);
  if (times <= 0)
    return interp->create_string("");

  size_t slen = hal::string::strlen(buf);
  size_t total = slen * (size_t)times;
  if (total > 4096)
    total = 4096;

  char *result = (char *)hal::memory::kmalloc(total + 1);
  size_t pos = 0;
  for (int64_t i = 0; i < times && pos < total; i++) {
    memcpy(result + pos, buf, slen);
    pos += slen;
  }
  result[pos] = '\0';

  PhpValue *r = interp->create_string(result);
  hal::memory::kfree(result);
  return r;
}

/* str_contains($haystack, $needle) */
static PhpValue *builtin_str_contains(PhpInterpreter *interp, PhpValue **args,
                                      int argc) {
  if (argc < 2)
    return interp->create_bool(false);
  const char *hay = interp->value_to_cstr(args[0]);
  char hay_buf[512];
  hal::string::strncpy(hay_buf, hay, 511);
  hay_buf[511] = '\0';
  const char *needle = interp->value_to_cstr(args[1]);
  return interp->create_bool(hal::string::strstr(hay_buf, needle) != nullptr);
}

/* ============================================================
 * KERNEL-SPECIFIC BUILT-IN FUNCTIONS
 * Fungsi khusus yang menghubungkan PHP ke hardware melalui HAL C++
 * ============================================================ */

/* kernel_version() */
static PhpValue *builtin_kernel_version(PhpInterpreter *interp, PhpValue **,
                                        int) {
  return interp->create_string(LITTLEOS_VERSION);
}

/* kernel_name() */
static PhpValue *builtin_kernel_name(PhpInterpreter *interp, PhpValue **, int) {
  return interp->create_string(LITTLEOS_NAME);
}

/* kernel_arch() */
static PhpValue *builtin_kernel_arch(PhpInterpreter *interp, PhpValue **, int) {
  return interp->create_string(LITTLEOS_ARCH);
}

/* memory_total() — total memori dalam bytes */
static PhpValue *builtin_memory_total(PhpInterpreter *interp, PhpValue **,
                                      int) {
  return interp->create_int((int64_t)hal::memory::get_total());
}

/* memory_free() — memori bebas dalam bytes */
static PhpValue *builtin_memory_free(PhpInterpreter *interp, PhpValue **, int) {
  return interp->create_int((int64_t)hal::memory::get_free());
}

/* memory_used() — memori terpakai dalam bytes */
static PhpValue *builtin_memory_used(PhpInterpreter *interp, PhpValue **, int) {
  return interp->create_int((int64_t)hal::memory::get_used());
}

/* uptime() — detik sejak boot */
static PhpValue *builtin_uptime(PhpInterpreter *interp, PhpValue **, int) {
  return interp->create_int((int64_t)hal::timer::get_seconds());
}

/* uptime_ms() — milidetik sejak boot */
static PhpValue *builtin_uptime_ms(PhpInterpreter *interp, PhpValue **, int) {
  return interp->create_int((int64_t)hal::timer::get_ms());
}

/* console_clear() */
static PhpValue *builtin_console_clear(PhpInterpreter *interp, PhpValue **,
                                       int) {
  Console::clear();
  return interp->create_null();
}

/* console_set_color($hex) */
static PhpValue *builtin_console_set_color(PhpInterpreter *interp,
                                           PhpValue **args, int argc) {
  if (argc >= 1) {
    uint32_t color = (uint32_t)interp->value_to_int(args[0]);
    Console::set_fg(color);
  }
  return interp->create_null();
}

/* readline() — baca satu baris dari keyboard */
static PhpValue *builtin_readline(PhpInterpreter *interp, PhpValue **args,
                                  int argc) {
  /* Tampilkan prompt jika ada */
  if (argc >= 1) {
    Console::puts(interp->value_to_cstr(args[0]));
  }

  char buf[512];
  hal::keyboard::read_line(buf, sizeof(buf));
  return interp->create_string(buf);
}

/* sleep($ms) — tunggu milidetik */
static PhpValue *builtin_sleep(PhpInterpreter *interp, PhpValue **args,
                               int argc) {
  if (argc >= 1) {
    uint64_t ms = (uint64_t)interp->value_to_int(args[0]);
    hal::timer::wait_ms(ms);
  }
  return interp->create_null();
}

/* screen_width() */
static PhpValue *builtin_screen_width(PhpInterpreter *interp, PhpValue **,
                                      int) {
  return interp->create_int((int64_t)Console::get_fb_width());
}

/* screen_height() */
static PhpValue *builtin_screen_height(PhpInterpreter *interp, PhpValue **,
                                       int) {
  return interp->create_int((int64_t)Console::get_fb_height());
}

/* console_cols() */
static PhpValue *builtin_console_cols(PhpInterpreter *interp, PhpValue **,
                                      int) {
  return interp->create_int((int64_t)Console::get_width());
}

/* console_rows() */
static PhpValue *builtin_console_rows(PhpInterpreter *interp, PhpValue **,
                                      int) {
  return interp->create_int((int64_t)Console::get_height());
}

/* ============================================================
 * BUILT-IN: MOUSE FUNCTIONS
 * ============================================================ */
static PhpValue *builtin_mouse_x(PhpInterpreter *interp, PhpValue **, int) {
  return interp->create_int((int64_t)hal::mouse::get_x());
}

static PhpValue *builtin_mouse_y(PhpInterpreter *interp, PhpValue **, int) {
  return interp->create_int((int64_t)hal::mouse::get_y());
}

static PhpValue *builtin_mouse_clicked(PhpInterpreter *interp, PhpValue **,
                                       int) {
  return interp->create_bool(hal::mouse::is_left_pressed());
}

static PhpValue *builtin_mouse_event(PhpInterpreter *interp, PhpValue **, int) {
  return interp->create_bool(hal::mouse::has_event());
}

/* ============================================================
 * BUILT-IN: RTC FUNCTIONS
 * ============================================================ */
static PhpValue *builtin_rtc_hour(PhpInterpreter *interp, PhpValue **, int) {
  hal::rtc::DateTime dt = hal::rtc::get_time();
  return interp->create_int((int64_t)dt.hour);
}

static PhpValue *builtin_rtc_minute(PhpInterpreter *interp, PhpValue **, int) {
  hal::rtc::DateTime dt = hal::rtc::get_time();
  return interp->create_int((int64_t)dt.minute);
}

static PhpValue *builtin_rtc_second(PhpInterpreter *interp, PhpValue **, int) {
  hal::rtc::DateTime dt = hal::rtc::get_time();
  return interp->create_int((int64_t)dt.second);
}

static PhpValue *builtin_rtc_day(PhpInterpreter *interp, PhpValue **, int) {
  hal::rtc::DateTime dt = hal::rtc::get_time();
  return interp->create_int((int64_t)dt.day);
}

static PhpValue *builtin_rtc_month(PhpInterpreter *interp, PhpValue **, int) {
  hal::rtc::DateTime dt = hal::rtc::get_time();
  return interp->create_int((int64_t)dt.month);
}

static PhpValue *builtin_rtc_year(PhpInterpreter *interp, PhpValue **, int) {
  hal::rtc::DateTime dt = hal::rtc::get_time();
  return interp->create_int((int64_t)dt.year);
}

/* ============================================================
 * BUILT-IN: DRAWING FUNCTIONS (Tailwind CSS GUI)
 * ============================================================ */
static PhpValue *builtin_draw_pixel(PhpInterpreter *interp, PhpValue **args,
                                    int argc) {
  if (argc >= 3) {
    int32_t x = (int32_t)interp->value_to_int(args[0]);
    int32_t y = (int32_t)interp->value_to_int(args[1]);
    uint32_t c = (uint32_t)interp->value_to_int(args[2]);
    Console::draw_pixel(x, y, c);
  }
  return interp->create_null();
}

static PhpValue *builtin_fill_rect(PhpInterpreter *interp, PhpValue **args,
                                   int argc) {
  if (argc >= 5) {
    int32_t x = (int32_t)interp->value_to_int(args[0]);
    int32_t y = (int32_t)interp->value_to_int(args[1]);
    int32_t w = (int32_t)interp->value_to_int(args[2]);
    int32_t h = (int32_t)interp->value_to_int(args[3]);
    uint32_t c = (uint32_t)interp->value_to_int(args[4]);
    Console::fill_rect(x, y, w, h, c);
  }
  return interp->create_null();
}

static PhpValue *builtin_draw_rect(PhpInterpreter *interp, PhpValue **args,
                                   int argc) {
  if (argc >= 5) {
    int32_t x = (int32_t)interp->value_to_int(args[0]);
    int32_t y = (int32_t)interp->value_to_int(args[1]);
    int32_t w = (int32_t)interp->value_to_int(args[2]);
    int32_t h = (int32_t)interp->value_to_int(args[3]);
    uint32_t c = (uint32_t)interp->value_to_int(args[4]);
    Console::draw_rect(x, y, w, h, c);
  }
  return interp->create_null();
}

static PhpValue *builtin_fill_rounded_rect(PhpInterpreter *interp,
                                           PhpValue **args, int argc) {
  if (argc >= 6) {
    int32_t x = (int32_t)interp->value_to_int(args[0]);
    int32_t y = (int32_t)interp->value_to_int(args[1]);
    int32_t w = (int32_t)interp->value_to_int(args[2]);
    int32_t h = (int32_t)interp->value_to_int(args[3]);
    int32_t r = (int32_t)interp->value_to_int(args[4]);
    uint32_t c = (uint32_t)interp->value_to_int(args[5]);
    Console::fill_rounded_rect(x, y, w, h, r, c);
  }
  return interp->create_null();
}

static PhpValue *builtin_draw_text(PhpInterpreter *interp, PhpValue **args,
                                   int argc) {
  if (argc >= 4) {
    int32_t x = (int32_t)interp->value_to_int(args[0]);
    int32_t y = (int32_t)interp->value_to_int(args[1]);
    const char *s = interp->value_to_cstr(args[2]);
    uint32_t c = (uint32_t)interp->value_to_int(args[3]);
    Console::draw_string(x, y, s, c);
  }
  return interp->create_null();
}

static PhpValue *builtin_draw_line(PhpInterpreter *interp, PhpValue **args,
                                   int argc) {
  if (argc >= 5) {
    int32_t x0 = (int32_t)interp->value_to_int(args[0]);
    int32_t y0 = (int32_t)interp->value_to_int(args[1]);
    int32_t x1 = (int32_t)interp->value_to_int(args[2]);
    int32_t y1 = (int32_t)interp->value_to_int(args[3]);
    uint32_t c = (uint32_t)interp->value_to_int(args[4]);
    Console::draw_line(x0, y0, x1, y1, c);
  }
  return interp->create_null();
}

static PhpValue *builtin_fill_circle(PhpInterpreter *interp, PhpValue **args,
                                     int argc) {
  if (argc >= 4) {
    int32_t cx = (int32_t)interp->value_to_int(args[0]);
    int32_t cy = (int32_t)interp->value_to_int(args[1]);
    int32_t r = (int32_t)interp->value_to_int(args[2]);
    uint32_t c = (uint32_t)interp->value_to_int(args[3]);
    Console::fill_circle(cx, cy, r, c);
  }
  return interp->create_null();
}

static PhpValue *builtin_keyboard_read(PhpInterpreter *interp, PhpValue **,
                                       int) {
  char c = hal::keyboard::read_char_nonblocking();
  if (c == 0)
    return interp->create_null();
  char buf[2] = {c, '\0'};
  return interp->create_string(buf);
}

static PhpValue *builtin_keyboard_has_input(PhpInterpreter *interp, PhpValue **,
                                            int) {
  return interp->create_bool(hal::keyboard::has_input());
}

/* ============================================================
 * BUILT-IN: DESKTOP / WINDOW MANAGER FUNCTIONS
 * ============================================================ */
static PhpValue *builtin_desktop_init(PhpInterpreter *interp, PhpValue **,
                                      int) {
  Desktop::init(Console::get_fb_width(), Console::get_fb_height());
  return interp->create_null();
}

static PhpValue *builtin_desktop_create_window(PhpInterpreter *interp,
                                               PhpValue **args, int argc) {
  if (argc < 6)
    return interp->create_int(-1);
  const char *title = interp->value_to_cstr(args[0]);
  int32_t x = (int32_t)interp->value_to_int(args[1]);
  int32_t y = (int32_t)interp->value_to_int(args[2]);
  int32_t w = (int32_t)interp->value_to_int(args[3]);
  int32_t h = (int32_t)interp->value_to_int(args[4]);
  const char *app_type = interp->value_to_cstr(args[5]);
  int id = Desktop::create_window(title, x, y, w, h, app_type);
  return interp->create_int((int64_t)id);
}

static PhpValue *builtin_desktop_close_window(PhpInterpreter *interp,
                                              PhpValue **args, int argc) {
  if (argc >= 1) {
    int id = (int)interp->value_to_int(args[0]);
    Desktop::close_window(id);
  }
  return interp->create_null();
}

static PhpValue *builtin_desktop_set_text(PhpInterpreter *interp,
                                          PhpValue **args, int argc) {
  if (argc >= 2) {
    int id = (int)interp->value_to_int(args[0]);
    const char *text = interp->value_to_cstr(args[1]);
    Desktop::set_window_text(id, text);
  }
  return interp->create_null();
}

static PhpValue *builtin_desktop_render(PhpInterpreter *interp, PhpValue **,
                                        int) {
  Desktop::render_all();
  return interp->create_null();
}

static PhpValue *builtin_desktop_poll_events(PhpInterpreter *interp,
                                             PhpValue **, int) {
  /* Poll mouse dan update desktop state */
  hal::mouse::MouseEvent ev = hal::mouse::poll_event();
  Desktop::DesktopState &ds = Desktop::get_state();

  int mx = ds.cursor_x = hal::mouse::get_x(),
      my = ds.cursor_y = hal::mouse::get_y();

  int32_t ty = taskbar.get('y');

  /* ---- RIGHT CLICK — Context Menu ---- */
  if (ev.right_clicked) {
    /* Tutup menu lain dulu */
    ds.start_menu_open = false;

    /* Cek klik kanan pada context menu yang sudah terbuka — abaikan */
    if (ds.context_menu.visible) {
      Desktop::close_context_menu();
      return interp->create_null();
    }

    /* Klik kanan di taskbar */
    if (my >= ty) {
      Desktop::open_context_menu(mx, my, "taskbar");
      return interp->create_null();
    }

    /* Klik kanan di window */
    int hit_id = Desktop::hit_test_window(mx, my);
    if (hit_id > 0) {
      Desktop::open_context_menu(mx, my, "window");
      return interp->create_null();
    }

    /* Klik kanan di desktop */
    Desktop::open_context_menu(mx, my, "desktop");
    return interp->create_null();
  }

  /* ---- LEFT CLICK ---- */
  if (ev.clicked) {
    /* Tutup context menu dulu jika visible */
    if (ds.context_menu.visible) {
      Desktop::ContextMenu &cm = ds.context_menu;
      /* Cek klik di dalam context menu */
      int32_t menu_w = 210;
      int32_t item_h = 28;
      int32_t iy = cm.y + 6;
      for (int i = 0; i < cm.item_count; i++) {
        if (cm.items[i].separator) {
          iy += 9;
          continue;
        }
        if (mx >= cm.x + 4 && mx < cm.x + menu_w - 4 && my >= iy &&
            my < iy + item_h) {
          /* Item diklik! */
          char action[64];
          hal::string::strncpy(action, cm.items[i].action, 63);
          action[63] = '\0';
          Desktop::close_context_menu();
          return interp->create_string(action);
        }
        iy += item_h;
      }
      /* Klik di luar context menu — tutup */
      Desktop::close_context_menu();
      return interp->create_null();
    }

    /* Check start menu button */
    // int32_t ty = ds.screen_h - TASKBAR_HEIGHT;
    if (mx >= 4 && mx < 88 && my >= ty + 3 && my < ty + TASKBAR_HEIGHT - 3) {
      ds.start_menu_open = !ds.start_menu_open;
      ds.needs_redraw = true;
      return interp->create_string("start_menu");
    }

    /* Check start menu item clicks */
    if (ds.start_menu_open) {
      int32_t msx = 4;
      int32_t msy = ds.screen_h - TASKBAR_HEIGHT - 310;
      const char *items[] = {"Sistem",    "Terminal", "File Manager",
                             "Browser",   "Setting",  "Task Manager",
                             "Clock Date"};
      for (int i = 0; i < 7; i++) {
        int iy2 = msy + 64 + i * 34;
        if (mx >= msx + 6 && mx < msx + 254 && my >= iy2 && my < iy2 + 32) {
          ds.start_menu_open = false;
          return interp->create_string(items[i]);
        }
      }
      /* Click di luar start menu */
      if (mx > 264 || my < msy) {
        ds.start_menu_open = false;
      }
    }

    /* Check pinned app clicks on taskbar */
    {
      int px = 96;
      /* Terminal pinned button */
      if (mx >= px && mx < px + 36 && my >= ty + 3 &&
          my < ty + TASKBAR_HEIGHT - 3) {
        ds.start_menu_open = false;
        return interp->create_string("Terminal");
      }
      px += 40;
      /* File Manager pinned button */
      if (mx >= px && mx < px + 36 && my >= ty + 3 &&
          my < ty + TASKBAR_HEIGHT - 3) {
        ds.start_menu_open = false;
        return interp->create_string("File Manager");
      }
      px += 44 + 6; /* +6 for separator gap */

      /* Check taskbar window buttons (after pinned apps) */
      int bx = px;
      for (int i = 0; i < ds.z_count && bx < ds.screen_w - 260; i++) {
        int idx = ds.z_order[i];
        Desktop::Window &win = ds.windows[idx];
        if (win.id < 0 || win.state == Desktop::WindowState::Closed)
          continue;

        if (mx >= bx && mx < bx + 150 && my >= ty + 3 &&
            my < ty + TASKBAR_HEIGHT - 3) {
          /* Klik pada taskbar button — restore/focus */
          if (win.state == Desktop::WindowState::Minimized) {
            Desktop::restore_window(win.id);
          }
          Desktop::bring_to_front(win.id);
          return interp->create_string("window_focused");
        }
        bx += 158;
      }
    }

    /* Check window interactions */
    int hit_id = Desktop::hit_test_window(mx, my);
    if (hit_id > 0) {
      /* Check close button */
      if (Desktop::hit_test_close_btn(hit_id, mx, my)) {
        Desktop::close_window(hit_id);
        return interp->create_string("window_closed");
      }
      /* Check maximize button */
      if (Desktop::hit_test_maximize_btn(hit_id, mx, my)) {
        Desktop::maximize_window(hit_id);
        return interp->create_string("window_maximized");
      }
      /* Check minimize button */
      if (Desktop::hit_test_minimize_btn(hit_id, mx, my)) {
        Desktop::minimize_window(hit_id);
        return interp->create_string("window_minimized");
      }
      /* Bring window to front */
      Desktop::bring_to_front(hit_id);

      /* Start dragging if on title bar */
      Desktop::Window *win = Desktop::get_window(hit_id);
      if (win && my >= win->y && my < win->y + Desktop::TITLE_BAR_H) {
        ds.dragging = true;
        ds.drag_window = hit_id;
        ds.drag_offset_x = mx - win->x;
        ds.drag_offset_y = my - win->y;
      }
    }
  }

  if (ev.released) {
    ds.dragging = false;
    ds.drag_window = -1;
  }

  /* Handle dragging */
  if (ds.dragging && ds.drag_window > 0) {
    Desktop::Window *win = Desktop::get_window(ds.drag_window);
    if (win && win->state == Desktop::WindowState::Normal) {
      win->x = mx - ds.drag_offset_x;
      win->y = my - ds.drag_offset_y;
      ds.needs_redraw = true;
    }
  }
  return interp->create_null();
}

PhpValue *kernel_update(PhpInterpreter *irp, PhpValue **, int) {
  // Time Update
  Desktop::get_core().time.update(hal::timer::time_ns());

  // Mouse Handler
  sys::proc_mouse(hal::mouse::getState());

  return irp->create_null();
}

/* ============================================================
 * PHP RUNTIME — Top-Level Engine
 * ============================================================ */
void PhpRuntime::init() {
  interpreter.init();
  register_kernel_builtins();
}

void PhpRuntime::register_kernel_builtins() {
  /* Fungsi string standar PHP */
  interpreter.register_builtin("strlen", builtin_strlen);
  interpreter.register_builtin("substr", builtin_substr);
  interpreter.register_builtin("strtoupper", builtin_strtoupper);
  interpreter.register_builtin("strtolower", builtin_strtolower);
  interpreter.register_builtin("str_repeat", builtin_str_repeat);
  interpreter.register_builtin("str_contains", builtin_str_contains);

  /* Fungsi konversi */
  interpreter.register_builtin("intval", builtin_intval);
  interpreter.register_builtin("strval", builtin_strval);

  /* Fungsi array */
  interpreter.register_builtin("count", builtin_count);
  interpreter.register_builtin("isset", builtin_isset);

  /* Fungsi output */
  interpreter.register_builtin("print", builtin_print);
  interpreter.register_builtin("var_dump", builtin_var_dump);

  /* Fungsi kernel — akses hardware melalui C++ HAL */
  interpreter.register_builtin("kernel_version", builtin_kernel_version);
  interpreter.register_builtin("kernel_name", builtin_kernel_name);
  interpreter.register_builtin("kernel_arch", builtin_kernel_arch);
  interpreter.register_builtin("memory_total", builtin_memory_total);
  interpreter.register_builtin("memory_free", builtin_memory_free);
  interpreter.register_builtin("memory_used", builtin_memory_used);
  interpreter.register_builtin("uptime", builtin_uptime);
  interpreter.register_builtin("uptime_ms", builtin_uptime_ms);

  /* Fungsi console */
  interpreter.register_builtin("console_clear", builtin_console_clear);
  interpreter.register_builtin("console_set_color", builtin_console_set_color);
  interpreter.register_builtin("console_cols", builtin_console_cols);
  interpreter.register_builtin("console_rows", builtin_console_rows);

  /* Fungsi I/O */
  interpreter.register_builtin("readline", builtin_readline);
  interpreter.register_builtin("sleep", builtin_sleep);

  /* Fungsi layar */
  interpreter.register_builtin("screen_width", builtin_screen_width);
  interpreter.register_builtin("screen_height", builtin_screen_height);

  /* Fungsi mouse */
  interpreter.register_builtin("mouse_x", builtin_mouse_x);
  interpreter.register_builtin("mouse_y", builtin_mouse_y);
  interpreter.register_builtin("mouse_clicked", builtin_mouse_clicked);
  interpreter.register_builtin("mouse_event", builtin_mouse_event);

  /* Fungsi RTC (Real-Time Clock) */
  interpreter.register_builtin("rtc_hour", builtin_rtc_hour);
  interpreter.register_builtin("rtc_minute", builtin_rtc_minute);
  interpreter.register_builtin("rtc_second", builtin_rtc_second);
  interpreter.register_builtin("rtc_day", builtin_rtc_day);
  interpreter.register_builtin("rtc_month", builtin_rtc_month);
  interpreter.register_builtin("rtc_year", builtin_rtc_year);

  /* Fungsi drawing (Tailwind CSS GUI) */
  interpreter.register_builtin("draw_pixel", builtin_draw_pixel);
  interpreter.register_builtin("fill_rect", builtin_fill_rect);
  interpreter.register_builtin("draw_rect", builtin_draw_rect);
  interpreter.register_builtin("fill_rounded_rect", builtin_fill_rounded_rect);
  interpreter.register_builtin("draw_text", builtin_draw_text);
  interpreter.register_builtin("draw_line", builtin_draw_line);
  interpreter.register_builtin("fill_circle", builtin_fill_circle);

  /* Fungsi keyboard GUI */
  interpreter.register_builtin("keyboard_read", builtin_keyboard_read);
  interpreter.register_builtin("keyboard_has_input",
                               builtin_keyboard_has_input);

  /* Fungsi Desktop / Window Manager */
  interpreter.register_builtin("desktop_init", builtin_desktop_init);
  interpreter.register_builtin("desktop_create_window",
                               builtin_desktop_create_window);
  interpreter.register_builtin("desktop_close_window",
                               builtin_desktop_close_window);
  interpreter.register_builtin("desktop_set_text", builtin_desktop_set_text);
  interpreter.register_builtin("desktop_render", builtin_desktop_render);
  interpreter.register_builtin("desktop_poll_events",
                               builtin_desktop_poll_events);
  interpreter.register_builtin("xxkernelxx", kernel_update);
}

void PhpRuntime::execute_script(const char *source) {
  lexer.init(source);
  parser.init(&lexer);

  AstNode *program = parser.parse_program();
  if (program) {
    interpreter.execute(program);
  } else {
    Console::puts_colored("[PHP Fatal] Gagal mem-parse script kernel\n",
                          COLOR_ERROR);
  }
}
