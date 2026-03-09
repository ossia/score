#include "ExprtkToCpp.hpp"

#include <boost/algorithm/string/replace.hpp>

#include <cmath>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Nodes
{

struct exprtk_strings
{
  std::string pre;
  std::string post;
};

static exprtk_strings exprtk_to_cpp_impl{
    .pre = R"_(
  [](auto& object) {
    using namespace std;
    static %IVECT% px%IVECF% = {};
    static %OVECT% po%OVECF% = {};
    static float pa = 0;
    static float pb = 0;
    static float pc = 0;
    auto& x%IVECF% = object.inputs.in.value;
    auto& o%OVECF% = object.outputs.out.value;
    auto& a = object.inputs.a.value;
    auto& b = object.inputs.b.value;
    auto& c = object.inputs.c.value;
    auto& m1 = object.inputs.m1.value;
    auto& m2 = object.inputs.m2.value;
    auto& m3 = object.inputs.m3.value;
    float t = 0;
    float dt = 0;
    float pos = 0;
    float fs = 44100;
)_",
    .post = R"_(
    value_adapt(px%IVECF%, x%IVECF% %IVECA%);
    value_adapt(po%OVECF%, o%OVECF% %OVECA%);

    pa = a;
    pb = b;
    pc = c;
)_"};

static exprtk_strings exprtk_to_cpp_impl_opt{
    .pre = R"_(
  [](auto& object) {
    using namespace std;

    object.outputs.out.value = std::nullopt;
    if(object.inputs.in.value) {
      static %IVECT% px%IVECF% = {};
      static %OVECT% po%OVECF% = {};
      static float pa = 0;
      static float pb = 0;
      static float pc = 0;
      auto& x%IVECF% = *object.inputs.in.value;
      auto& o%OVECF% = object.outputs.out.value;
      auto& a = object.inputs.a.value;
      auto& b = object.inputs.b.value;
      auto& c = object.inputs.c.value;
      auto& m1 = object.inputs.m1.value;
      auto& m2 = object.inputs.m2.value;
      auto& m3 = object.inputs.m3.value;
      float t = 0;
      float dt = 0;
      float pos = 0;
      float fs = 44100;
)_",
    .post = R"_(
      value_adapt(px%IVECF%, x%IVECF% %IVECA%);
      value_adapt(po%OVECF%, o%OVECF% %OVECA%);

      pa = a;
      pb = b;
      pc = c;
    }
    object.inputs.in.value = std::nullopt;
)_"};

namespace
{

// ============================================================
// Tokenizer
// ============================================================

struct Token
{
  enum Type
  {
    Number,
    Ident,
    LParen,
    RParen,
    LBracket,
    RBracket,
    LBrace,
    RBrace,
    Comma,
    Semi,
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Caret,
    Less,
    LessEq,
    Greater,
    GreaterEq,
    EqualEq,
    NotEq,
    Assign,    // :=
    Equal,     // = (equality in exprtk)
    PlusEq,
    MinusEq,
    StarEq,
    SlashEq,
    Hash,
    Eof
  };
  Type type;
  std::string text;
};

std::vector<Token> tokenize(const std::string& expr)
{
  std::vector<Token> tokens;
  size_t i = 0;
  auto sz = expr.size();

  while(i < sz)
  {
    char c = expr[i];

    // Whitespace
    if(std::isspace(c))
    {
      i++;
      continue;
    }

    // Single-line comment: //
    if(c == '/' && i + 1 < sz && expr[i + 1] == '/')
    {
      while(i < sz && expr[i] != '\n')
        i++;
      continue;
    }

    // Multi-line comment: /* ... */
    if(c == '/' && i + 1 < sz && expr[i + 1] == '*')
    {
      i += 2;
      while(i + 1 < sz && !(expr[i] == '*' && expr[i + 1] == '/'))
        i++;
      i += 2;
      continue;
    }

    // Numbers
    if(std::isdigit(c) || (c == '.' && i + 1 < sz && std::isdigit(expr[i + 1])))
    {
      size_t start = i;
      while(i < sz && (std::isdigit(expr[i]) || expr[i] == '.'))
        i++;
      // Scientific notation
      if(i < sz && (expr[i] == 'e' || expr[i] == 'E'))
      {
        i++;
        if(i < sz && (expr[i] == '+' || expr[i] == '-'))
          i++;
        while(i < sz && std::isdigit(expr[i]))
          i++;
      }
      tokens.push_back({Token::Number, expr.substr(start, i - start)});
      continue;
    }

    // Identifiers
    if(std::isalpha(c) || c == '_')
    {
      size_t start = i;
      while(i < sz && (std::isalnum(expr[i]) || expr[i] == '_'))
        i++;
      tokens.push_back({Token::Ident, expr.substr(start, i - start)});
      continue;
    }

    // Two-character tokens
    if(i + 1 < sz)
    {
      char c2 = expr[i + 1];
      if(c == ':' && c2 == '=')
      {
        tokens.push_back({Token::Assign, ":="});
        i += 2;
        continue;
      }
      if(c == '<' && c2 == '=')
      {
        tokens.push_back({Token::LessEq, "<="});
        i += 2;
        continue;
      }
      if(c == '>' && c2 == '=')
      {
        tokens.push_back({Token::GreaterEq, ">="});
        i += 2;
        continue;
      }
      if(c == '=' && c2 == '=')
      {
        tokens.push_back({Token::EqualEq, "=="});
        i += 2;
        continue;
      }
      if(c == '!' && c2 == '=')
      {
        tokens.push_back({Token::NotEq, "!="});
        i += 2;
        continue;
      }
      if(c == '+' && c2 == '=')
      {
        tokens.push_back({Token::PlusEq, "+="});
        i += 2;
        continue;
      }
      if(c == '-' && c2 == '=')
      {
        tokens.push_back({Token::MinusEq, "-="});
        i += 2;
        continue;
      }
      if(c == '*' && c2 == '=')
      {
        tokens.push_back({Token::StarEq, "*="});
        i += 2;
        continue;
      }
      if(c == '/' && c2 == '=')
      {
        tokens.push_back({Token::SlashEq, "/="});
        i += 2;
        continue;
      }
    }

    // Single-character tokens
    switch(c)
    {
      case '(':
        tokens.push_back({Token::LParen, "("});
        break;
      case ')':
        tokens.push_back({Token::RParen, ")"});
        break;
      case '[':
        tokens.push_back({Token::LBracket, "["});
        break;
      case ']':
        tokens.push_back({Token::RBracket, "]"});
        break;
      case '{':
        tokens.push_back({Token::LBrace, "{"});
        break;
      case '}':
        tokens.push_back({Token::RBrace, "}"});
        break;
      case ',':
        tokens.push_back({Token::Comma, ","});
        break;
      case ';':
        tokens.push_back({Token::Semi, ";"});
        break;
      case '+':
        tokens.push_back({Token::Plus, "+"});
        break;
      case '-':
        tokens.push_back({Token::Minus, "-"});
        break;
      case '*':
        tokens.push_back({Token::Star, "*"});
        break;
      case '/':
        tokens.push_back({Token::Slash, "/"});
        break;
      case '%':
        tokens.push_back({Token::Percent, "%"});
        break;
      case '^':
        tokens.push_back({Token::Caret, "^"});
        break;
      case '<':
        tokens.push_back({Token::Less, "<"});
        break;
      case '>':
        tokens.push_back({Token::Greater, ">"});
        break;
      case '=':
        tokens.push_back({Token::Equal, "="});
        break;
      case '#':
        tokens.push_back({Token::Hash, "#"});
        break;
      default:
        break; // skip unknown
    }
    i++;
  }

  tokens.push_back({Token::Eof, ""});
  return tokens;
}

// ============================================================
// Function / keyword mappings
// ============================================================

static const std::unordered_map<std::string, std::string> function_map = {
    // Single-arg math
    {"abs", "std::abs"},
    {"acos", "std::acos"},
    {"acosh", "std::acosh"},
    {"asin", "std::asin"},
    {"asinh", "std::asinh"},
    {"atan", "std::atan"},
    {"atanh", "std::atanh"},
    {"ceil", "std::ceil"},
    {"cos", "std::cos"},
    {"cosh", "std::cosh"},
    {"cot", "exprtk_cot"},
    {"csc", "exprtk_csc"},
    {"d2g", "exprtk_deg2grad"},
    {"d2r", "exprtk_deg2rad"},
    {"deg2grad", "exprtk_deg2grad"},
    {"deg2rad", "exprtk_deg2rad"},
    {"erf", "std::erf"},
    {"erfc", "std::erfc"},
    {"exp", "std::exp"},
    {"expm1", "std::expm1"},
    {"floor", "std::floor"},
    {"frac", "exprtk_frac"},
    {"g2d", "exprtk_grad2deg"},
    {"grad2deg", "exprtk_grad2deg"},
    {"log", "std::log"},
    {"log10", "std::log10"},
    {"log1p", "std::log1p"},
    {"log2", "std::log2"},
    {"ncdf", "exprtk_ncdf"},
    {"r2d", "exprtk_rad2deg"},
    {"rad2deg", "exprtk_rad2deg"},
    {"round", "std::round"},
    {"sec", "exprtk_sec"},
    {"sgn", "exprtk_sgn"},
    {"sin", "std::sin"},
    {"sinc", "exprtk_sinc"},
    {"sinh", "std::sinh"},
    {"sqrt", "std::sqrt"},
    {"tan", "std::tan"},
    {"tanh", "std::tanh"},
    {"trunc", "std::trunc"},
    // Two-arg math
    {"atan2", "std::atan2"},
    {"hypot", "std::hypot"},
    {"logn", "exprtk_logn"},
    {"max", "std::max"},
    {"min", "std::min"},
    {"mod", "std::fmod"},
    {"pow", "std::pow"},
    {"root", "exprtk_root"},
    {"roundn", "exprtk_roundn"},
    // Three-arg
    {"clamp", "exprtk_clamp"},
    {"iclamp", "exprtk_iclamp"},
    {"inrange", "exprtk_inrange"},
    // Other
    {"swap", "std::swap"},
};

static const std::unordered_set<std::string> known_functions = [] {
  std::unordered_set<std::string> s;
  for(auto& [k, v] : function_map)
    s.insert(k);
  // Control flow keywords that look like function calls
  s.insert("if");
  s.insert("for");
  s.insert("while");
  s.insert("repeat");
  s.insert("switch");
  s.insert("return");
  s.insert("avg");
  s.insert("sum");
  s.insert("mul");
  return s;
}();

static const std::unordered_map<std::string, std::string> constant_map = {
    {"pi", "3.14159265358979323846f"},
    {"epsilon", "std::numeric_limits<float>::epsilon()"},
    {"inf", "std::numeric_limits<float>::infinity()"},
    {"true", "1.f"},
    {"false", "0.f"},
};

bool is_function(const std::string& name)
{
  return known_functions.count(name) > 0;
}

std::string format_number(const std::string& num)
{
  // Append 'f' suffix for float literal
  if(num.find('.') != std::string::npos || num.find('e') != std::string::npos
     || num.find('E') != std::string::npos)
    return num + "f";
  return num + ".f";
}

// ============================================================
// Insert implicit multiplication tokens
// ============================================================
// In exprtk: 2x → 2*x, 2(x) → 2*(x), (a)(b) → (a)*(b), etc.

void insert_implicit_mul(std::vector<Token>& tokens)
{
  // A token is "value-like on the right" if it can START an operand
  auto can_start_operand = [](const Token& t) {
    return t.type == Token::Number || t.type == Token::LParen
           || (t.type == Token::Ident && !is_function(t.text));
  };

  // A token is "value-like on the left" if it ends an operand
  auto ends_operand = [](const Token& t) {
    return t.type == Token::Number || t.type == Token::RParen
           || t.type == Token::RBracket
           || (t.type == Token::Ident && !is_function(t.text));
  };

  std::vector<Token> result;
  for(size_t i = 0; i < tokens.size(); ++i)
  {
    result.push_back(tokens[i]);

    if(i + 1 < tokens.size() && ends_operand(tokens[i]) && can_start_operand(tokens[i + 1]))
    {
      result.push_back({Token::Star, "*"});
    }
  }
  tokens = std::move(result);
}

// ============================================================
// Recursive descent parser → C++ code emitter
// ============================================================

class ExprParser
{
  const std::vector<Token>& tok;
  size_t pos = 0;

  const Token& peek() const { return tok[pos]; }
  const Token& advance() { return tok[pos++]; }
  bool at(Token::Type t) const { return peek().type == t; }
  bool at_ident(const char* s) const { return at(Token::Ident) && peek().text == s; }

  bool match(Token::Type t)
  {
    if(at(t))
    {
      advance();
      return true;
    }
    return false;
  }

public:
  ExprParser(const std::vector<Token>& tokens)
      : tok(tokens)
  {
  }

  // Parse semicolon-separated statements, return each as a string
  std::vector<std::string> parse_statements()
  {
    std::vector<std::string> stmts;
    while(!at(Token::Eof))
    {
      auto s = parse_statement();
      if(!s.empty())
        stmts.push_back(std::move(s));
      if(!match(Token::Semi))
        break;
    }
    return stmts;
  }

private:
  std::string parse_statement()
  {
    // var declaration
    if(at_ident("var"))
    {
      advance();
      if(at(Token::Ident))
      {
        auto name = advance().text;
        if(match(Token::Assign))
          return "exprtk_arithmetic " + name + " = " + parse_expr();
        return "exprtk_arithmetic " + name + " = 0";
      }
    }

    // Check for assignment: ident := expr, ident += expr, etc.
    if(at(Token::Ident) && pos + 1 < tok.size())
    {
      auto& next = tok[pos + 1];
      if(next.type == Token::Assign)
      {
        auto name = advance().text;
        advance(); // :=
        return name + " = " + parse_expr();
      }
      if(next.type == Token::PlusEq || next.type == Token::MinusEq
         || next.type == Token::StarEq || next.type == Token::SlashEq)
      {
        auto name = advance().text;
        auto op = advance().text;
        return name + " " + op + " " + parse_expr();
      }
      // ident[idx] := expr
      if(next.type == Token::LBracket)
      {
        size_t saved = pos;
        auto name = advance().text;
        advance(); // [
        auto idx = parse_expr();
        if(match(Token::RBracket) && match(Token::Assign))
        {
          return name + "[static_cast<int>(" + idx + ")] = " + parse_expr();
        }
        // Not an assignment, backtrack
        pos = saved;
      }
    }

    return parse_expr();
  }

  // ---- Expression precedence levels ----

  std::string parse_expr() { return parse_ternary(); }

  // Ternary: expr ? expr : expr  (not standard exprtk but handle it)
  std::string parse_ternary()
  {
    auto left = parse_or();
    // exprtk doesn't have ?: but just in case
    return left;
  }

  // or / nor / xor / xnor
  std::string parse_or()
  {
    auto left = parse_and();
    while(true)
    {
      if(at_ident("or"))
      {
        advance();
        left = "(" + left + " || " + parse_and() + ")";
      }
      else if(at_ident("nor"))
      {
        advance();
        left = "(!(" + left + " || " + parse_and() + "))";
      }
      else if(at_ident("xor"))
      {
        advance();
        auto r = parse_and();
        left = "(((" + left + ") != 0) != ((" + r + ") != 0))";
      }
      else if(at_ident("xnor"))
      {
        advance();
        auto r = parse_and();
        left = "(((" + left + ") != 0) == ((" + r + ") != 0))";
      }
      else
        break;
    }
    return left;
  }

  // and / nand
  std::string parse_and()
  {
    auto left = parse_not();
    while(true)
    {
      if(at_ident("and"))
      {
        advance();
        left = "(" + left + " && " + parse_not() + ")";
      }
      else if(at_ident("nand"))
      {
        advance();
        left = "(!(" + left + " && " + parse_not() + "))";
      }
      else
        break;
    }
    return left;
  }

  // not
  std::string parse_not()
  {
    if(at_ident("not"))
    {
      advance();
      return "(!" + parse_not() + ")";
    }
    return parse_comparison();
  }

  // comparisons: < <= > >= == != =
  std::string parse_comparison()
  {
    auto left = parse_addition();
    while(true)
    {
      if(at(Token::Less))
      {
        advance();
        left = "(" + left + " < " + parse_addition() + ")";
      }
      else if(at(Token::LessEq))
      {
        advance();
        left = "(" + left + " <= " + parse_addition() + ")";
      }
      else if(at(Token::Greater))
      {
        advance();
        left = "(" + left + " > " + parse_addition() + ")";
      }
      else if(at(Token::GreaterEq))
      {
        advance();
        left = "(" + left + " >= " + parse_addition() + ")";
      }
      else if(at(Token::EqualEq))
      {
        advance();
        left = "(" + left + " == " + parse_addition() + ")";
      }
      else if(at(Token::NotEq))
      {
        advance();
        left = "(" + left + " != " + parse_addition() + ")";
      }
      else if(at(Token::Equal))
      {
        // In exprtk, bare '=' is equality
        advance();
        left = "(" + left + " == " + parse_addition() + ")";
      }
      else
        break;
    }
    return left;
  }

  // + -
  std::string parse_addition()
  {
    auto left = parse_multiplication();
    while(true)
    {
      if(at(Token::Plus))
      {
        advance();
        left = "(" + left + " + " + parse_multiplication() + ")";
      }
      else if(at(Token::Minus))
      {
        advance();
        left = "(" + left + " - " + parse_multiplication() + ")";
      }
      else
        break;
    }
    return left;
  }

  // * / %
  std::string parse_multiplication()
  {
    auto left = parse_power();
    while(true)
    {
      if(at(Token::Star))
      {
        advance();
        left = "(" + left + " * " + parse_power() + ")";
      }
      else if(at(Token::Slash))
      {
        advance();
        left = "(" + left + " / " + parse_power() + ")";
      }
      else if(at(Token::Percent))
      {
        advance();
        left = "std::fmod(" + left + ", " + parse_power() + ")";
      }
      else
        break;
    }
    return left;
  }

  // ^ (right-associative)
  std::string parse_power()
  {
    auto left = parse_unary();
    if(at(Token::Caret))
    {
      advance();
      auto right = parse_power(); // right-associative
      return "std::pow(" + left + ", " + right + ")";
    }
    return left;
  }

  // unary: - + not
  std::string parse_unary()
  {
    if(at(Token::Minus))
    {
      advance();
      return "(-" + parse_unary() + ")";
    }
    if(at(Token::Plus))
    {
      advance();
      return parse_unary();
    }
    return parse_postfix();
  }

  // postfix: primary, then [] indexing
  std::string parse_postfix()
  {
    auto left = parse_primary();
    while(at(Token::LBracket))
    {
      advance();
      auto idx = parse_expr();
      match(Token::RBracket);
      left = left + "[static_cast<int>(" + idx + ")]";
    }
    return left;
  }

  // primary: number, ident, function call, parenthesized expr,
  //          if-expression, for/while/repeat, { block }
  std::string parse_primary()
  {
    // Number literal
    if(at(Token::Number))
    {
      return format_number(advance().text);
    }

    // Parenthesized expression
    if(at(Token::LParen))
    {
      advance();
      auto inner = parse_expr();
      match(Token::RParen);
      return "(" + inner + ")";
    }

    // Block { stmts }
    if(at(Token::LBrace))
    {
      advance();
      std::string block = "([&]{ ";
      while(!at(Token::RBrace) && !at(Token::Eof))
      {
        block += parse_statement() + "; ";
        match(Token::Semi);
      }
      match(Token::RBrace);
      block += "return 0.f; })()";
      return block;
    }

    // Identifier: could be variable, constant, or function call
    if(at(Token::Ident))
    {
      auto name = advance().text;

      // Constants
      if(auto it = constant_map.find(name); it != constant_map.end())
        return it->second;

      // if(cond, then, else) or if(cond) { ... } else { ... }
      if(name == "if")
        return parse_if();

      // for(init; cond; inc) { body }
      if(name == "for")
        return parse_for();

      // while(cond) { body }
      if(name == "while")
        return parse_while();

      // repeat { body } until(cond)
      if(name == "repeat")
        return parse_repeat();

      // switch { case cond : expr; ... default: expr; }
      if(name == "switch")
        return parse_switch();

      // return [expr]
      if(name == "return")
      {
        if(!at(Token::Semi) && !at(Token::Eof) && !at(Token::RBracket)
           && !at(Token::RParen) && !at(Token::RBrace))
          return parse_expr();
        return "0.f";
      }

      // avg(a, b, c, ...) → sum/count
      if(name == "avg" && at(Token::LParen))
        return parse_avg();

      // sum(a, b, c, ...) → (a + b + c + ...)
      if(name == "sum" && at(Token::LParen))
        return parse_vararg_op(" + ");

      // mul(a, b, c, ...) → (a * b * c * ...)
      if(name == "mul" && at(Token::LParen))
        return parse_vararg_op(" * ");

      // Known function call
      if(at(Token::LParen))
      {
        if(auto it = function_map.find(name); it != function_map.end())
        {
          advance(); // (
          auto args = parse_args();
          match(Token::RParen);
          std::string result = it->second + "(";
          for(size_t i = 0; i < args.size(); ++i)
          {
            if(i > 0)
              result += ", ";
            result += args[i];
          }
          result += ")";
          return result;
        }
      }

      // Plain variable
      return name;
    }

    // Fallback: skip the token
    if(!at(Token::Eof))
      advance();
    return "0.f";
  }

  std::vector<std::string> parse_args()
  {
    std::vector<std::string> args;
    if(!at(Token::RParen) && !at(Token::Eof))
    {
      args.push_back(parse_expr());
      while(match(Token::Comma))
        args.push_back(parse_expr());
    }
    return args;
  }

  // if(cond, then, else) → (cond ? then : else)
  // if(cond) { then } else { else_block }
  std::string parse_if()
  {
    if(!match(Token::LParen))
      return "0.f";
    auto cond = parse_expr();

    if(match(Token::Comma))
    {
      // Ternary form: if(cond, then_expr, else_expr)
      auto then_expr = parse_expr();
      std::string else_expr = "0.f";
      if(match(Token::Comma))
        else_expr = parse_expr();
      match(Token::RParen);
      return "(" + cond + " ? " + then_expr + " : " + else_expr + ")";
    }

    match(Token::RParen);

    // Block form: if(cond) { ... } else { ... }
    std::string result = "([&]{ if(" + cond + ") ";
    if(at(Token::LBrace))
    {
      advance();
      result += "{ ";
      while(!at(Token::RBrace) && !at(Token::Eof))
      {
        result += parse_statement() + "; ";
        match(Token::Semi);
      }
      match(Token::RBrace);
      result += "} ";
    }
    else
    {
      result += "{ " + parse_statement() + "; } ";
      match(Token::Semi);
    }

    if(at_ident("else"))
    {
      advance();
      if(at(Token::LBrace))
      {
        advance();
        result += "else { ";
        while(!at(Token::RBrace) && !at(Token::Eof))
        {
          result += parse_statement() + "; ";
          match(Token::Semi);
        }
        match(Token::RBrace);
        result += "} ";
      }
      else
      {
        result += "else { " + parse_statement() + "; } ";
      }
    }

    result += "return 0.f; })()";
    return result;
  }

  std::string parse_for()
  {
    match(Token::LParen);
    auto init = parse_statement();
    match(Token::Semi);
    auto cond = parse_expr();
    match(Token::Semi);
    auto inc = parse_statement();
    match(Token::RParen);

    std::string result = "([&]{ for(" + init + "; " + cond + "; " + inc + ") ";
    if(at(Token::LBrace))
    {
      advance();
      result += "{ ";
      while(!at(Token::RBrace) && !at(Token::Eof))
      {
        result += parse_statement() + "; ";
        match(Token::Semi);
      }
      match(Token::RBrace);
      result += "} ";
    }
    else
    {
      result += "{ " + parse_statement() + "; } ";
    }
    result += "return 0.f; })()";
    return result;
  }

  std::string parse_while()
  {
    match(Token::LParen);
    auto cond = parse_expr();
    match(Token::RParen);

    std::string result = "([&]{ while(" + cond + ") ";
    if(at(Token::LBrace))
    {
      advance();
      result += "{ ";
      while(!at(Token::RBrace) && !at(Token::Eof))
      {
        result += parse_statement() + "; ";
        match(Token::Semi);
      }
      match(Token::RBrace);
      result += "} ";
    }
    else
    {
      result += "{ " + parse_statement() + "; } ";
    }
    result += "return 0.f; })()";
    return result;
  }

  std::string parse_repeat()
  {
    std::string result = "([&]{ do ";
    if(at(Token::LBrace))
    {
      advance();
      result += "{ ";
      while(!at(Token::RBrace) && !at(Token::Eof))
      {
        result += parse_statement() + "; ";
        match(Token::Semi);
      }
      match(Token::RBrace);
      result += "} ";
    }
    else
    {
      result += "{ " + parse_statement() + "; } ";
    }
    // until(cond)
    if(at_ident("until"))
    {
      advance();
      match(Token::LParen);
      auto cond = parse_expr();
      match(Token::RParen);
      result += "while(!(" + cond + ")); ";
    }
    result += "return 0.f; })()";
    return result;
  }

  std::string parse_switch()
  {
    // switch { case cond0 : expr0; case cond1 : expr1; default: exprN; }
    match(Token::LBrace);
    std::string result;
    bool first = true;

    while(!at(Token::RBrace) && !at(Token::Eof))
    {
      if(at_ident("case"))
      {
        advance();
        auto cond = parse_expr();
        match(Token::Hash); // : in exprtk switch is actually a colon... we use # as placeholder
        auto expr = parse_expr();
        match(Token::Semi);
        if(first)
          result += "(" + cond + " ? " + expr;
        else
          result += " : " + cond + " ? " + expr;
        first = false;
      }
      else if(at_ident("default"))
      {
        advance();
        match(Token::Hash);
        auto expr = parse_expr();
        match(Token::Semi);
        result += " : " + expr;
      }
      else
      {
        advance(); // skip unknown
      }
    }
    match(Token::RBrace);
    result += ")";
    return result;
  }

  // avg(a, b, c) → ((a + b + c) / 3.f)
  std::string parse_avg()
  {
    advance(); // (
    auto args = parse_args();
    match(Token::RParen);

    if(args.empty())
      return "0.f";

    std::string sum = "(" + args[0];
    for(size_t i = 1; i < args.size(); ++i)
      sum += " + " + args[i];
    sum += ")";

    return "(" + sum + " / " + std::to_string(args.size()) + ".f)";
  }

  // sum(...) → (a + b + ...), mul(...) → (a * b * ...)
  std::string parse_vararg_op(const char* op)
  {
    advance(); // (
    auto args = parse_args();
    match(Token::RParen);

    if(args.empty())
      return "0.f";

    std::string result = "(" + args[0];
    for(size_t i = 1; i < args.size(); ++i)
      result += op + args[i];
    result += ")";
    return result;
  }
};

std::vector<std::string> parse_exprtk_to_statements(const std::string& expr)
{
  auto tokens = tokenize(expr);
  insert_implicit_mul(tokens);
  ExprParser parser(tokens);
  return parser.parse_statements();
}

} // anonymous namespace

std::string exprtk_to_cpp(std::string exprtk_expr, bool optional) noexcept
{
  const bool in_vector = exprtk_expr.find("xv[") != std::string::npos;
  const bool ret_vector = exprtk_expr.find("return") != std::string::npos;

  if(exprtk_expr.empty())
    return "[] (auto& object) { }";

  auto statements = parse_exprtk_to_statements(exprtk_expr);

  std::string code;

  // Original expression as comment
  code += "/* original expression:\n";
  code += exprtk_expr;
  code += "\n*/\n";

  // All but last statement are side-effect statements
  for(size_t i = 0; i + 1 < statements.size(); ++i)
    code += "    " + statements[i] + ";\n";

  // Last statement is the output value
  if(!statements.empty())
  {
    if(!ret_vector)
      code += "    value_adapt(o, " + statements.back() + ");\n";
    else
      code += "    " + statements.back() + ";\n";
  }

  // --- Build the lambda with pre/post templates ---
  const auto& strs = optional ? exprtk_to_cpp_impl_opt : exprtk_to_cpp_impl;
  auto pre = strs.pre;
  auto post = strs.post;

  if(in_vector)
  {
    boost::replace_all(pre, "%IVECT%", "std::vector<float>");
    boost::replace_all(pre, "%IVECF%", "v");
    boost::replace_all(pre, "%IVECA%", "");
    boost::replace_all(post, "%IVECT%", "std::vector<float>");
    boost::replace_all(post, "%IVECF%", "v");
    boost::replace_all(post, "%IVECA%", "");
  }
  else
  {
    boost::replace_all(pre, "%IVECT%", "float");
    boost::replace_all(pre, "%IVECF%", "");
    boost::replace_all(pre, "%IVECA%", "");
    boost::replace_all(post, "%IVECT%", "float");
    boost::replace_all(post, "%IVECF%", "");
    boost::replace_all(post, "%IVECA%", "");
  }

  if(ret_vector)
  {
    boost::replace_all(pre, "%OVECT%", "std::vector<float>");
    boost::replace_all(pre, "%OVECF%", "v");
    boost::replace_all(pre, "%OVECA%", "");
    boost::replace_all(post, "%OVECT%", "std::vector<float>");
    boost::replace_all(post, "%OVECF%", "v");
    boost::replace_all(post, "%OVECA%", "");
  }
  else
  {
    boost::replace_all(pre, "%OVECT%", "float");
    boost::replace_all(pre, "%OVECF%", "");
    boost::replace_all(pre, "%OVECA%", "");
    boost::replace_all(post, "%OVECT%", "float");
    boost::replace_all(post, "%OVECF%", "");
    boost::replace_all(post, "%OVECA%", "");
  }

  return pre + "\n" + code + "\n" + post + "\n}\n";
}

}
