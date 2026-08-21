#include "DBCParser.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

/**
 * Why this is a hand-written recursive-descent parser and not a Spirit X3
 * grammar, given that X3 is available and used elsewhere in the tree
 * (ossia/protocols/coap/link_format_parser.cpp, ValueParser.hpp...):
 *
 *  - The NS_ block makes the file non-dispatchable by leading keyword. It is a
 *    bare list of the *same* tokens as the top-level constructs -- a real file
 *    opens with `NS_ :` and then lines reading `CM_`, `BA_DEF_`, `VAL_`,
 *    `SIG_VALTYPE_`, `SG_MUL_VAL_`... An alternation over the constructs would
 *    start parsing the NS_ entries as records and fail. Handling it needs a
 *    stateful "am I inside NS_" switch in the top-level loop, which is exactly
 *    the thing a declarative grammar is supposed to remove.
 *
 *  - Per-record error recovery is a requirement here, not a nicety: vendor
 *    files carry typos and dialect variants, and the contract for this parser
 *    is to keep going and *name* what it could not read (Database::warnings).
 *    X3 reports failure as one boolean plus an iterator; getting a diagnostic
 *    per record means either an on_error handler on every rule or invoking the
 *    parser once per record from a hand-written loop.
 *
 *  - DBC is line-sensitive in two places (the NS_ block ends at a blank line,
 *    a BU_ node list and a SG_ receiver list end at the newline) and free-form
 *    everywhere else, so no single X3 skipper fits.
 *
 *  - There is no recursion in the grammar. Message/signal nesting is one level
 *    deep, everything else is flat, so a parser generator has no structure to
 *    exploit -- and X3's compile-time cost is real.
 *
 * The lexical layer below is the only genuinely fiddly part, and it is ~120
 * lines.
 */

namespace Protocols::CAN
{

const std::string* AttributeSet::find(std::string_view name) const noexcept
{
  for(auto& a : attributes)
    if(a.name == name)
      return &a.value;
  return nullptr;
}

const Signal* Message::findSignal(std::string_view name) const noexcept
{
  for(auto& s : signals)
    if(s.name == name)
      return &s;
  return nullptr;
}

const Message* Database::findMessage(uint32_t id, bool extended) const noexcept
{
  for(auto& m : messages)
    if(m.id == id && m.extended == extended)
      return &m;
  return nullptr;
}

namespace
{

constexpr bool isIdentChar(char c) noexcept
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
         || c == '_';
}

//! Note: a carriage return is deliberately *not* a blank. parseDBC() strips
//! CRLF before parsing, so a '\r' reaching this point means the input was not
//! normalized, and treating it as a blank would hide the end of a line.
constexpr bool isBlank(char c) noexcept
{
  return c == ' ' || c == '\t';
}

constexpr bool isSpace(char c) noexcept
{
  return isBlank(c) || c == '\n';
}

/**
 * Make a string from the file safe to hand to Qt and ossia as UTF-8.
 *
 * Real DBC files are not UTF-8. CANdb++ writes cp1252 and the reference
 * readers default to cp1252 or ISO-8859-1; degree signs in units ("\xB0C") and
 * umlauts in comments are everywhere. Passing those bytes on unchanged would
 * produce invalid UTF-8 in a node description, which Qt renders as nothing.
 *
 * So: text that already is valid UTF-8 is left exactly as it is (a file that
 * really is UTF-8 must not be mangled), and text that is not is transcoded
 * from Latin-1, which is the best guess and never fails.
 */
std::string toUtf8(std::string s)
{
  const auto* p = reinterpret_cast<const unsigned char*>(s.data());
  const std::size_t n = s.size();

  bool valid = true;
  for(std::size_t i = 0; i < n && valid;)
  {
    const unsigned char c = p[i];
    int extra = 0;
    if(c < 0x80)
      extra = 0;
    else if((c & 0xE0) == 0xC0)
      extra = 1;
    else if((c & 0xF0) == 0xE0)
      extra = 2;
    else if((c & 0xF8) == 0xF0)
      extra = 3;
    else
      valid = false;

    if(!valid)
      break;

    if(i + extra >= n)
    {
      valid = false;
      break;
    }
    for(int k = 1; k <= extra; ++k)
      if((p[i + k] & 0xC0) != 0x80)
      {
        valid = false;
        break;
      }
    i += extra + 1;
  }

  if(valid)
    return s;

  std::string out;
  out.reserve(s.size() + 8);
  for(std::size_t i = 0; i < n; ++i)
  {
    const unsigned char c = p[i];
    if(c < 0x80)
    {
      out.push_back(char(c));
    }
    else
    {
      out.push_back(char(0xC0 | (c >> 6)));
      out.push_back(char(0x80 | (c & 0x3F)));
    }
  }
  return out;
}

//! The top-level keywords, so that statement recovery can tell where the next
//! record starts when the current one is missing its terminator.
const std::unordered_set<std::string_view>& topLevelKeywords()
{
  static const std::unordered_set<std::string_view> k{
      "VERSION",     "NS_",           "BS_",         "BU_",         "VAL_TABLE_",
      "BO_",         "SG_",           "BO_TX_BU_",   "EV_",         "ENVVAR_DATA_",
      "CM_",         "BA_DEF_",       "BA_DEF_DEF_", "BA_",         "VAL_",
      "SIG_VALTYPE_", "SIG_GROUP_",   "SGTYPE_",     "SIG_TYPE_REF_", "SG_MUL_VAL_",
      "CAT_DEF_",    "CAT_",          "FILTER",      "BA_DEF_REL_", "BA_REL_",
      "BA_DEF_DEF_REL_", "BU_SG_REL_", "BU_EV_REL_", "BU_BO_REL_",  "NS_DESC_",
      "BA_DEF_SGTYPE_", "BA_SGTYPE_", "SIGTYPE_VALTYPE_"};
  return k;
}

//! A BA_DEF_ declaration, kept only so that BA_ enum assignments (which carry
//! the *index* into the enum, not its text) can be resolved to their name.
struct AttributeDefinition
{
  std::string name;
  std::string objectKind; // "", "BU_", "BO_", "SG_", "EV_"
  std::string type;       // INT, HEX, FLOAT, STRING, ENUM
  std::vector<std::string> enumValues;
};

struct Parser
{
  std::string_view s;
  std::size_t i = 0;
  Database db;

  std::unordered_map<std::string, std::vector<ValueDescription>> valueTables;
  std::unordered_map<std::string, AttributeDefinition> attributeDefs;
  std::unordered_set<std::string> warnedKinds;

  static constexpr std::size_t maxWarnings = 200;

  // ---------------------------------------------------------------- warnings

  int lineAt(std::size_t pos) const noexcept
  {
    int line = 1;
    for(std::size_t k = 0; k < pos && k < s.size(); ++k)
      if(s[k] == '\n')
        ++line;
    return line;
  }

  void warn(std::string text) { warnAt(i, std::move(text)); }

  void warnAt(std::size_t pos, std::string text)
  {
    if(db.warnings.size() >= maxWarnings)
    {
      if(db.warnings.size() == maxWarnings)
        db.warnings.push_back("too many problems: further ones are not reported");
      return;
    }
    db.warnings.push_back(
        "line " + std::to_string(lineAt(pos)) + ": " + std::move(text));
  }

  //! One diagnostic per unsupported construct kind, not one per occurrence: a
  //! large file may hold thousands of SIG_GROUP_ records and the user only
  //! needs to be told once that they are being dropped.
  void warnKindOnce(const std::string& kind, const std::string& text)
  {
    if(warnedKinds.insert(kind).second)
      warn(text);
  }

  // ----------------------------------------------------------------- lexing

  bool eof() const noexcept { return i >= s.size(); }
  char cur() const noexcept { return i < s.size() ? s[i] : '\0'; }

  void skipBlanks() noexcept
  {
    while(i < s.size() && isBlank(s[i]))
      ++i;
  }

  void skipSpace() noexcept
  {
    while(i < s.size() && isSpace(s[i]))
      ++i;
  }

  void skipLine() noexcept
  {
    while(i < s.size() && s[i] != '\n')
      ++i;
    if(i < s.size())
      ++i;
  }

  //! True when the rest of the current line holds nothing but whitespace.
  bool restOfLineBlank() const noexcept
  {
    for(std::size_t k = i; k < s.size() && s[k] != '\n'; ++k)
      if(!isBlank(s[k]))
        return false;
    return true;
  }

  std::string_view readIdent() noexcept
  {
    const std::size_t start = i;
    while(i < s.size() && isIdentChar(s[i]))
      ++i;
    return s.substr(start, i - start);
  }

  //! A run of non-whitespace, used for keyword dispatch and for the mixed
  //! tokens (`m3M`, `1-`) that are not identifiers.
  std::string_view readToken() noexcept
  {
    const std::size_t start = i;
    while(i < s.size() && !isSpace(s[i]))
      ++i;
    return s.substr(start, i - start);
  }

  std::string_view peekToken() noexcept
  {
    const std::size_t save = i;
    skipSpace();
    const std::size_t start = i;
    while(i < s.size() && !isSpace(s[i]))
      ++i;
    auto tok = s.substr(start, i - start);
    i = save;
    return tok;
  }

  bool consumeChar(char c) noexcept
  {
    skipBlanks();
    if(cur() == c)
    {
      ++i;
      return true;
    }
    return false;
  }

  bool expectChar(char c, std::string_view what)
  {
    if(consumeChar(c))
      return true;
    warn(std::string{"expected '"} + c + "' in " + std::string(what));
    return false;
  }

  /**
   * A quoted string. Newlines are allowed inside: CANdb++ writes multi-line
   * CM_ comments verbatim. Both escaping dialects are accepted -- backslash
   * escapes, and the doubled `""` that some tools emit.
   */
  std::string readQuoted(bool* ok = nullptr)
  {
    skipSpace();
    if(ok)
      *ok = false;
    if(cur() != '"')
      return {};
    ++i;

    std::string out;
    while(i < s.size())
    {
      const char c = s[i];
      if(c == '\\' && i + 1 < s.size())
      {
        const char n = s[i + 1];
        // Only the escapes that actually occur; anything else keeps both
        // characters, since a lone backslash in a comment is not an error.
        switch(n)
        {
          case '"':
            out.push_back('"');
            i += 2;
            continue;
          case '\\':
            out.push_back('\\');
            i += 2;
            continue;
          case 'n':
            out.push_back('\n');
            i += 2;
            continue;
          case 't':
            out.push_back('\t');
            i += 2;
            continue;
          default:
            out.push_back(c);
            ++i;
            continue;
        }
      }
      if(c == '"')
      {
        // A doubled quote is a literal quote, not the end of the string.
        if(i + 1 < s.size() && s[i + 1] == '"')
        {
          out.push_back('"');
          i += 2;
          continue;
        }
        ++i;
        if(ok)
          *ok = true;
        return toUtf8(std::move(out));
      }
      out.push_back(c);
      ++i;
    }

    warn("unterminated string");
    return toUtf8(std::move(out));
  }

  //! The characters that can occur in a DBC number, including exponents.
  static constexpr bool isNumberChar(char c) noexcept
  {
    return (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e'
           || c == 'E';
  }

  std::string_view readNumberToken() noexcept
  {
    skipBlanks();
    const std::size_t start = i;
    while(i < s.size() && isNumberChar(s[i]))
      ++i;
    return s.substr(start, i - start);
  }

  double readDouble(bool* ok = nullptr)
  {
    const auto tok = readNumberToken();
    if(ok)
      *ok = !tok.empty();
    if(tok.empty())
      return 0.;
    // strtod needs a terminator; the tokens here are a handful of characters.
    char buf[64];
    const std::size_t n = std::min(tok.size(), sizeof(buf) - 1);
    std::memcpy(buf, tok.data(), n);
    buf[n] = '\0';
    return std::strtod(buf, nullptr);
  }

  int64_t readInt(bool* ok = nullptr)
  {
    const auto tok = readNumberToken();
    if(ok)
      *ok = !tok.empty();
    if(tok.empty())
      return 0;
    char buf[64];
    const std::size_t n = std::min(tok.size(), sizeof(buf) - 1);
    std::memcpy(buf, tok.data(), n);
    buf[n] = '\0';
    return std::strtoll(buf, nullptr, 10);
  }

  /**
   * Skip the remainder of a statement, stopping at its `;`.
   *
   * Guarded against a missing terminator: if a newline is reached whose next
   * token is a top-level keyword, the statement is assumed to have ended there
   * rather than swallowing the rest of the file.
   */
  void skipStatement()
  {
    while(i < s.size())
    {
      const char c = s[i];
      if(c == '"')
      {
        readQuoted();
        continue;
      }
      if(c == ';')
      {
        ++i;
        return;
      }
      if(c == '\n')
      {
        ++i;
        const auto next = peekToken();
        // A keyword at the start of a line means the previous record is over.
        if(!next.empty() && topLevelKeywords().count(next) > 0)
          return;
        continue;
      }
      ++i;
    }
  }

  // -------------------------------------------------------------- constructs

  void parseVersion()
  {
    db.version = readQuoted();
    skipBlanks();
  }

  /**
   * The NS_ block: `NS_ :` followed by one bare keyword per line, terminated by
   * a blank line. Those keywords are the same tokens as the real constructs,
   * so this must consume them rather than let the dispatcher see them.
   */
  void parseNamespaceSection()
  {
    consumeChar(':');
    skipLine();

    while(i < s.size())
    {
      const std::size_t lineStart = i;
      skipBlanks();
      if(restOfLineBlank())
      {
        // Blank line: end of the block.
        skipLine();
        return;
      }

      const auto tok = peekToken();
      // Defensive: some generators omit the blank line. A token that is not a
      // bare NS_ entry (i.e. it is followed by something else on its line)
      // means the block is over.
      if(tok.empty())
      {
        skipLine();
        continue;
      }

      skipSpace();
      readToken();
      if(!restOfLineBlank())
      {
        // `BS_:` / `BU_:` and friends carry content: rewind, the block ended.
        i = lineStart;
        return;
      }
      skipLine();
    }
  }

  void parseNodes()
  {
    // `BU_: Node1 Node2 ...`, terminated by the end of the line. The node list
    // itself is not needed for decoding, so it is consumed and dropped.
    consumeChar(':');
    skipLine();
  }

  std::vector<ValueDescription> parseValueDescriptionList()
  {
    std::vector<ValueDescription> out;
    while(i < s.size())
    {
      skipSpace();
      if(cur() == ';' || cur() == '\0')
        break;
      if(!isNumberChar(cur()))
        break;

      bool ok = false;
      const int64_t v = readInt(&ok);
      if(!ok)
        break;

      bool strOk = false;
      auto name = readQuoted(&strOk);
      if(!strOk)
      {
        warn("value description without a name");
        break;
      }
      out.push_back(ValueDescription{v, std::move(name)});
    }
    return out;
  }

  void parseValueTable()
  {
    skipSpace();
    const auto name = std::string(readIdent());
    auto values = parseValueDescriptionList();
    skipStatement();
    if(name.empty())
    {
      warn("VAL_TABLE_ without a name");
      return;
    }
    valueTables[name] = std::move(values);
  }

  void parseMessage()
  {
    const std::size_t recordStart = i;

    skipSpace();
    bool ok = false;
    const int64_t rawId = readInt(&ok);
    if(!ok || rawId < 0)
    {
      warnAt(recordStart, "BO_ without a valid identifier");
      skipLine();
      return;
    }

    skipSpace();
    Message msg;
    msg.name = std::string(readIdent());
    // The extended flag lives in bit 31 of the identifier; the identifier
    // itself is 29 bits. Masking is not optional: VECTOR__INDEPENDENT_SIG_MSG
    // is 0xC0000000, i.e. flag set and a masked value of 0, which would
    // otherwise collide with a legitimate id.
    const auto raw = uint32_t(rawId);
    msg.extended = (raw & dbcExtendedFlag) != 0;
    msg.id = raw & dbcIdentifierMask;

    if(!expectChar(':', "BO_ header"))
    {
      skipLine();
      return;
    }

    msg.size = int(readInt());
    skipBlanks();
    msg.transmitter = std::string(readIdent());
    skipLine();

    if(msg.name.empty())
    {
      warnAt(recordStart, "BO_ without a name");
      msg.name = "Message_" + std::to_string(msg.id);
    }

    // The signals belong to the message header that precedes them, so they are
    // parsed here rather than as a top-level construct.
    while(i < s.size())
    {
      const std::size_t save = i;
      skipSpace();
      if(peekToken() != "SG_")
      {
        i = save;
        break;
      }
      skipSpace();
      readToken();
      parseSignal(msg);
    }

    // A signal that runs past the declared payload is a warning, never a
    // rejection: it is common enough in real files (and universal in the
    // VECTOR__INDEPENDENT_SIG_MSG body) that refusing it would lose whole
    // databases. Bits past the end of a received frame simply read as zero.
    // Reported once per message rather than once per signal.
    {
      int worst = 0;
      const char* worstName = nullptr;
      for(const auto& sg : msg.signals)
      {
        const int lastBit
            = (sg.byteOrder == ByteOrder::LittleEndian)
                  ? sg.startBit + sg.length - 1
                  : flipBitPos(flipBitPos(sg.startBit) + sg.length - 1);
        const int neededBytes = lastBit / 8 + 1;
        if(neededBytes > msg.size && neededBytes > worst)
        {
          worst = neededBytes;
          worstName = sg.name.c_str();
        }
      }
      if(worstName && msg.name != independentSignalMessage)
        warnAt(
            recordStart, "message " + msg.name + " declares " + std::to_string(msg.size)
                             + " bytes but signal " + worstName + " needs "
                             + std::to_string(worst));
    }

    // Not a message: Vector's holding pen for the signals that are defined in
    // the database but attached to no frame. Its identifier is meaningless and
    // its signals all start at bit 0, so letting it through would create a
    // bogus node full of overlapping parameters.
    if(msg.name == independentSignalMessage)
      return;

    if(const auto* dup = db.findMessage(msg.id, msg.extended))
    {
      warnAt(
          recordStart, "duplicate message id " + std::to_string(msg.id) + " ("
                           + msg.name + " and " + dup->name + "); keeping the first");
      return;
    }

    db.messages.push_back(std::move(msg));
  }

  void parseSignal(Message& msg)
  {
    const std::size_t recordStart = i;
    Signal sig;

    skipBlanks();
    sig.name = std::string(readIdent());
    if(sig.name.empty())
    {
      warnAt(recordStart, "SG_ without a name");
      skipLine();
      return;
    }

    skipBlanks();
    // Optional multiplexing indicator between the name and the colon:
    // `M` for the multiplexer switch, `m<n>` for a multiplexed signal, and
    // `m<n>M` for a signal that is both (extended multiplexing).
    if(cur() != ':')
    {
      const auto tok = readToken();
      if(!tok.empty() && tok != ":")
      {
        std::string_view t = tok;
        // Tolerate `m3:` written without a space before the colon.
        if(!t.empty() && t.back() == ':')
        {
          t.remove_suffix(1);
          --i; // give the colon back to expectChar below
        }

        if(t == "M")
        {
          sig.isMultiplexer = true;
        }
        else if(t.size() >= 2 && t.front() == 'm')
        {
          sig.isMultiplexed = true;
          if(t.back() == 'M')
          {
            sig.isMultiplexer = true;
            t.remove_suffix(1);
          }
          sig.multiplexValue = std::strtoll(std::string(t.substr(1)).c_str(), nullptr, 10);
        }
        else if(!t.empty())
        {
          warnAt(recordStart, "unknown multiplexing indicator '" + std::string(t)
                                  + "' on signal " + sig.name);
        }
      }
    }

    if(!expectChar(':', "SG_ " + sig.name))
    {
      skipLine();
      return;
    }

    bool ok = false;
    sig.startBit = int(readInt(&ok));
    if(!ok)
    {
      warnAt(recordStart, "SG_ " + sig.name + ": missing start bit");
      skipLine();
      return;
    }
    expectChar('|', "SG_ " + sig.name);
    sig.length = int(readInt(&ok));
    if(!ok)
    {
      warnAt(recordStart, "SG_ " + sig.name + ": missing length");
      skipLine();
      return;
    }
    expectChar('@', "SG_ " + sig.name);

    skipBlanks();
    const char order = cur();
    if(order == '0' || order == '1')
    {
      sig.byteOrder = (order == '0') ? ByteOrder::BigEndian : ByteOrder::LittleEndian;
      ++i;
    }
    else
    {
      warnAt(recordStart, "SG_ " + sig.name + ": unknown byte order, assuming Intel");
    }

    const char sign = cur();
    if(sign == '+' || sign == '-')
    {
      sig.valueType = (sign == '-') ? ValueType::Signed : ValueType::Unsigned;
      ++i;
    }
    else
    {
      warnAt(recordStart, "SG_ " + sig.name + ": missing sign, assuming unsigned");
    }

    expectChar('(', "SG_ " + sig.name);
    sig.factor = readDouble();
    expectChar(',', "SG_ " + sig.name);
    sig.offset = readDouble();
    expectChar(')', "SG_ " + sig.name);

    expectChar('[', "SG_ " + sig.name);
    sig.min = readDouble();
    expectChar('|', "SG_ " + sig.name);
    sig.max = readDouble();
    expectChar(']', "SG_ " + sig.name);

    sig.unit = readQuoted();

    // Receivers run to the end of the line, comma-separated. `Vector__XXX` is
    // the placeholder CANdb++ writes for "nobody in particular".
    skipBlanks();
    while(i < s.size() && s[i] != '\n' && s[i] != '\r')
    {
      auto r = readIdent();
      if(r.empty())
      {
        // A comma or a stray character. Stepping over it must never step over
        // the end of the line, or the receiver list would swallow the file.
        ++i;
      }
      else if(r != "Vector__XXX")
      {
        sig.receivers.emplace_back(r);
      }
      // Only spaces and tabs: a CR is the end of the line here, not a blank.
      while(i < s.size() && (s[i] == ' ' || s[i] == '\t'))
        ++i;
    }
    skipLine();

    // Validation. None of these are fatal for the file, only for the signal.
    if(sig.length <= 0 || sig.length > 64)
    {
      warnAt(
          recordStart, "SG_ " + sig.name + ": length " + std::to_string(sig.length)
                           + " out of the 1..64 range; signal dropped");
      return;
    }
    if(sig.startBit < 0)
    {
      warnAt(recordStart, "SG_ " + sig.name + ": negative start bit; signal dropped");
      return;
    }
    if(sig.factor == 0.)
    {
      // Legal, and occasionally deliberate, but it collapses every raw value
      // onto the offset -- worth saying out loud.
      warnAt(recordStart, "SG_ " + sig.name + ": factor is 0, every value decodes to the offset");
    }

    // `[0|0]` is how CANdb++ spells "no range given"; it is not a real domain
    // and must not be pushed onto a parameter as one.
    sig.hasRange = !(sig.min == 0. && sig.max == 0.);
    if(sig.hasRange && sig.min > sig.max)
    {
      warnAt(recordStart, "SG_ " + sig.name + ": min > max; range ignored");
      sig.hasRange = false;
    }

    if(msg.findSignal(sig.name))
    {
      warnAt(
          recordStart,
          "duplicate signal name " + sig.name + " in message " + msg.name
              + "; keeping the first");
      return;
    }

    msg.signals.push_back(std::move(sig));
  }

  //! Look up a message by its raw (unmasked) DBC identifier, as written in the
  //! CM_/VAL_/BA_/SIG_VALTYPE_ back-references.
  Message* messageByRawId(int64_t rawId)
  {
    const auto raw = uint32_t(rawId);
    const bool ext = (raw & dbcExtendedFlag) != 0;
    const uint32_t id = raw & dbcIdentifierMask;
    for(auto& m : db.messages)
      if(m.id == id && m.extended == ext)
        return &m;
    return nullptr;
  }

  Signal* signalByRawId(int64_t rawId, std::string_view name)
  {
    if(auto* m = messageByRawId(rawId))
      for(auto& sg : m->signals)
        if(sg.name == name)
          return &sg;
    return nullptr;
  }

  //! True when the reference points at the independent-signal pseudo-message,
  //! which was deliberately dropped -- so a failed lookup is expected and must
  //! not produce a warning.
  bool isIndependentSignalRef(int64_t rawId) const noexcept
  {
    return uint32_t(rawId) == (dbcExtendedFlag | 0x40000000u);
  }

  void parseComment()
  {
    const std::size_t recordStart = i;
    skipSpace();
    const auto kind = peekToken();

    if(kind == "BO_")
    {
      skipSpace();
      readToken();
      skipSpace();
      const int64_t id = readInt();
      auto text = readQuoted();
      skipStatement();
      if(auto* m = messageByRawId(id))
        m->comment = std::move(text);
      else if(!isIndependentSignalRef(id))
        warnAt(recordStart, "CM_ BO_ for unknown message " + std::to_string(id));
    }
    else if(kind == "SG_")
    {
      skipSpace();
      readToken();
      skipSpace();
      const int64_t id = readInt();
      skipSpace();
      const auto name = std::string(readIdent());
      auto text = readQuoted();
      skipStatement();
      if(auto* sg = signalByRawId(id, name))
        sg->comment = std::move(text);
      else if(!isIndependentSignalRef(id))
        warnAt(
            recordStart,
            "CM_ SG_ for unknown signal " + name + " in message " + std::to_string(id));
    }
    else if(kind == "BU_" || kind == "EV_")
    {
      // Node and environment-variable comments: consumed, nothing in the node
      // tree corresponds to them.
      skipStatement();
    }
    else
    {
      // `CM_ "text";` -- a comment on the database itself.
      db.comment = readQuoted();
      skipStatement();
    }
  }

  void parseValues()
  {
    const std::size_t recordStart = i;
    skipSpace();

    // `VAL_ <msgid> <signal> ...` for a signal, `VAL_ <envvar> ...` for an
    // environment variable. Only the former starts with a number.
    if(!isNumberChar(cur()))
    {
      skipStatement(); // environment variable: no node tree counterpart
      return;
    }

    const int64_t id = readInt();
    skipSpace();
    const auto name = std::string(readIdent());

    skipSpace();
    std::vector<ValueDescription> values;
    if(isNumberChar(cur()))
    {
      values = parseValueDescriptionList();
    }
    else
    {
      // Dialect: the enumeration can be given by VAL_TABLE_ name instead of
      // being spelled out.
      const auto table = std::string(readIdent());
      if(auto it = valueTables.find(table); it != valueTables.end())
        values = it->second;
      else if(!table.empty())
        warnAt(recordStart, "VAL_ refers to unknown value table " + table);
    }
    skipStatement();

    if(auto* sg = signalByRawId(id, name))
      sg->valueTable = std::move(values);
    else if(!isIndependentSignalRef(id))
      warnAt(
          recordStart,
          "VAL_ for unknown signal " + name + " in message " + std::to_string(id));
  }

  void parseSignalValueType()
  {
    const std::size_t recordStart = i;
    skipSpace();
    const int64_t id = readInt();
    skipSpace();
    const auto name = std::string(readIdent());
    // The colon is written by most tools but is absent in some dialects.
    consumeChar(':');
    skipSpace();
    const int64_t type = readInt();
    skipStatement();

    auto* sg = signalByRawId(id, name);
    if(!sg)
    {
      if(!isIndependentSignalRef(id))
        warnAt(
            recordStart, "SIG_VALTYPE_ for unknown signal " + name + " in message "
                             + std::to_string(id));
      return;
    }

    switch(type)
    {
      case 0:
        break; // integer: keep the sign from the SG_ line
      case 1:
        if(sg->length != 32)
          warnAt(
              recordStart,
              "SIG_VALTYPE_ declares " + name + " a float but its length is "
                  + std::to_string(sg->length) + ", not 32");
        sg->valueType = ValueType::Float32;
        break;
      case 2:
        if(sg->length != 64)
          warnAt(
              recordStart,
              "SIG_VALTYPE_ declares " + name + " a double but its length is "
                  + std::to_string(sg->length) + ", not 64");
        sg->valueType = ValueType::Double64;
        break;
      default:
        warnAt(recordStart, "SIG_VALTYPE_ with unknown type " + std::to_string(type));
        break;
    }
  }

  void parseAttributeDefinition(bool relation)
  {
    const std::size_t recordStart = i;
    AttributeDefinition def;

    skipSpace();
    if(cur() != '"')
      def.objectKind = std::string(readIdent());

    def.name = readQuoted();
    skipSpace();
    def.type = std::string(readIdent());

    if(def.type == "ENUM")
    {
      while(i < s.size())
      {
        skipSpace();
        if(cur() != '"')
          break;
        def.enumValues.push_back(readQuoted());
        skipSpace();
        if(cur() == ',')
          ++i;
        else
          break;
      }
    }
    skipStatement();

    if(def.name.empty())
    {
      warnAt(recordStart, "BA_DEF_ without a name");
      return;
    }
    // Relation attributes (BA_DEF_REL_) describe node-to-message relations,
    // which have no counterpart here; their definitions are kept out of the
    // table so that a BA_REL_ cannot be mistaken for an object attribute.
    if(!relation)
      attributeDefs[def.name] = std::move(def);
  }

  //! Read an attribute value, which is either a quoted string or a number.
  std::string readAttributeValue()
  {
    skipSpace();
    if(cur() == '"')
      return readQuoted();

    const auto tok = readNumberToken();
    if(!tok.empty())
      return std::string(tok);

    return std::string(readIdent());
  }

  void parseAttributeDefault()
  {
    skipSpace();
    const auto name = readQuoted();
    auto value = readAttributeValue();
    skipStatement();
    if(name.empty())
      return;

    if(auto it = attributeDefs.find(name); it != attributeDefs.end())
      defaults[name] = std::move(value);
  }

  std::unordered_map<std::string, std::string> defaults;

  //! Resolve a BA_ value: for an ENUM attribute the file carries the *index*
  //! into the enumeration, not the text, so it is mapped back here.
  std::string resolveAttributeValue(const std::string& name, std::string value)
  {
    auto it = attributeDefs.find(name);
    if(it == attributeDefs.end() || it->second.type != "ENUM")
      return value;
    if(value.empty() || !isNumberChar(value.front()))
      return value; // already textual

    const long idx = std::strtol(value.c_str(), nullptr, 10);
    if(idx >= 0 && std::size_t(idx) < it->second.enumValues.size())
      return it->second.enumValues[std::size_t(idx)];
    return value;
  }

  void parseAttribute()
  {
    const std::size_t recordStart = i;
    skipSpace();
    const auto name = readQuoted();

    skipSpace();
    const auto kind = peekToken();

    if(kind == "BO_")
    {
      skipSpace();
      readToken();
      skipSpace();
      const int64_t id = readInt();
      auto value = resolveAttributeValue(name, readAttributeValue());
      skipStatement();
      if(auto* m = messageByRawId(id))
        m->attributes.attributes.push_back({name, std::move(value)});
      else if(!isIndependentSignalRef(id))
        warnAt(recordStart, "BA_ for unknown message " + std::to_string(id));
    }
    else if(kind == "SG_")
    {
      skipSpace();
      readToken();
      skipSpace();
      const int64_t id = readInt();
      skipSpace();
      const auto sigName = std::string(readIdent());
      auto value = resolveAttributeValue(name, readAttributeValue());
      skipStatement();
      if(auto* sg = signalByRawId(id, sigName))
        sg->attributes.attributes.push_back({name, std::move(value)});
      else if(!isIndependentSignalRef(id))
        warnAt(recordStart, "BA_ for unknown signal " + sigName);
    }
    else if(kind == "BU_" || kind == "EV_")
    {
      skipStatement();
    }
    else
    {
      auto value = resolveAttributeValue(name, readAttributeValue());
      skipStatement();
      db.attributes.attributes.push_back({name, std::move(value)});
    }
  }

  void parseExtendedMultiplexing()
  {
    // `SG_MUL_VAL_ <msgid> <multiplexed> <multiplexor> <a>-<b>, <c>-<d>;`
    //
    // Extended multiplexing allows several multiplexor signals per message and
    // value *ranges* per multiplexed signal. Decoding it is not implemented, so
    // the record is reported rather than dropped in silence: a message that
    // uses it would otherwise decode its multiplexed signals unconditionally
    // and produce plausible-looking rubbish.
    const std::size_t recordStart = i;
    skipSpace();
    const int64_t id = readInt();
    skipSpace();
    const auto sigName = std::string(readIdent());
    skipStatement();

    warnAt(
        recordStart,
        "SG_MUL_VAL_ (extended multiplexing) on signal " + sigName + " of message "
            + std::to_string(id)
            + " is not supported; that signal will be decoded unconditionally");
  }

  // ------------------------------------------------------------------- drive

  void run()
  {
    while(i < s.size())
    {
      skipSpace();
      if(i >= s.size())
        break;

      const std::size_t recordStart = i;
      const auto kw = readIdent();

      if(kw.empty())
      {
        // Not an identifier at all: a stray character. Skip the line rather
        // than spin.
        skipLine();
        continue;
      }

      if(kw == "VERSION")
        parseVersion();
      else if(kw == "NS_")
        parseNamespaceSection();
      else if(kw == "BS_")
        skipLine(); // bit timing, obsolete and always empty in practice
      else if(kw == "BU_")
        parseNodes();
      else if(kw == "VAL_TABLE_")
        parseValueTable();
      else if(kw == "BO_")
        parseMessage();
      else if(kw == "CM_")
        parseComment();
      else if(kw == "VAL_")
        parseValues();
      else if(kw == "SIG_VALTYPE_")
        parseSignalValueType();
      else if(kw == "BA_DEF_")
        parseAttributeDefinition(false);
      else if(kw == "BA_DEF_REL_")
        parseAttributeDefinition(true);
      else if(kw == "BA_DEF_DEF_" || kw == "BA_DEF_DEF_REL_")
        parseAttributeDefault();
      else if(kw == "BA_")
        parseAttribute();
      else if(kw == "SG_MUL_VAL_")
        parseExtendedMultiplexing();
      else if(kw == "SG_")
      {
        warnAt(recordStart, "SG_ outside of a message");
        skipLine();
      }
      else if(
          kw == "BO_TX_BU_" || kw == "EV_" || kw == "ENVVAR_DATA_" || kw == "SIG_GROUP_"
          || kw == "SGTYPE_" || kw == "SIG_TYPE_REF_" || kw == "BA_REL_"
          || kw == "BA_DEF_SGTYPE_" || kw == "BA_SGTYPE_" || kw == "SIGTYPE_VALTYPE_"
          || kw == "CAT_DEF_" || kw == "CAT_" || kw == "FILTER" || kw == "BU_SG_REL_"
          || kw == "BU_EV_REL_" || kw == "BU_BO_REL_")
      {
        // Known, understood, and irrelevant to decoding a frame into a node
        // tree. Reported once per kind so that the user is never left guessing
        // why part of their file had no effect.
        warnKindOnce(
            std::string(kw),
            std::string(kw) + " records are parsed but not used for decoding");
        skipStatement();
      }
      else
      {
        warnKindOnce(
            std::string(kw), "unknown DBC construct '" + std::string(kw) + "', skipped");
        skipStatement();
      }
    }
  }
};

}

/**
 * Preprocess the file: drop carriage returns and source comments.
 *
 * Both are done here, once, rather than taught to every loop in the parser.
 *
 * CRLF: vendor files really are CRLF -- both LPMS files are -- and a '\r'
 * treated as a blank rather than as end-of-line turns a line-terminated
 * construct (a BU_ node list, a SG_ receiver list) into one that runs to the
 * end of the file.
 *
 * Comments: line comments and block comments are not in the Vector
 * format, but they occur in real files and two of the major readers accept
 * them. They cannot be stripped with a plain scan, because a unit or a comment
 * string may legitimately contain "//" -- so this walks strings and comments
 * with the same state machine.
 *
 * Every removed byte is either dropped or replaced by nothing *except*
 * newlines, which are always kept: the line numbers in the diagnostics must
 * stay those of the file the user is looking at.
 */
static std::string preprocess(std::string_view content)
{
  std::string out;
  out.reserve(content.size());

  enum
  {
    Code,
    Str,
    LineComment,
    BlockComment
  } state
      = Code;

  for(std::size_t i = 0; i < content.size(); ++i)
  {
    const char c = content[i];
    const char n = (i + 1 < content.size()) ? content[i + 1] : '\0';

    switch(state)
    {
      case Code:
        if(c == '"')
        {
          state = Str;
          out.push_back(c);
        }
        else if(c == '/' && n == '/')
        {
          state = LineComment;
          ++i;
        }
        else if(c == '/' && n == '*')
        {
          state = BlockComment;
          ++i;
        }
        else if(c != '\r')
        {
          out.push_back(c);
        }
        break;

      case Str:
        // Inside a string nothing is a comment and nothing is an escape except
        // a backslash, which is copied together with whatever follows it so
        // that \" does not end the string here.
        if(c == '\\' && n != '\0')
        {
          out.push_back(c);
          out.push_back(n);
          ++i;
        }
        else if(c == '"')
        {
          state = Code;
          out.push_back(c);
        }
        else if(c != '\r')
        {
          out.push_back(c);
        }
        break;

      case LineComment:
        if(c == '\n')
        {
          state = Code;
          out.push_back(c);
        }
        break;

      case BlockComment:
        if(c == '*' && n == '/')
        {
          state = Code;
          ++i;
        }
        else if(c == '\n')
        {
          // Keep the newline so the line numbering survives.
          out.push_back(c);
        }
        break;
    }
  }

  return out;
}

Database parseDBC(std::string_view content)
{
  const std::string normalized = preprocess(content);
  content = normalized;

  Parser p{.s = content};
  p.run();

  // Fold in the BA_DEF_DEF_ defaults for the attributes that no BA_ set. Done
  // once at the end rather than per record so that a default cannot overwrite
  // an explicit value regardless of the order they appear in the file.
  auto applyDefaults = [&](AttributeSet& set) {
    for(auto& [name, value] : p.defaults)
      if(!set.find(name))
        set.attributes.push_back({name, value});
  };
  applyDefaults(p.db.attributes);

  return std::move(p.db);
}

Database parseDBCFile(const std::string& path)
{
  std::ifstream f{path, std::ios::binary};
  if(!f)
  {
    Database db;
    db.warnings.push_back("could not open " + path);
    return db;
  }

  std::ostringstream ss;
  ss << f.rdbuf();
  const auto content = ss.str();

  auto db = parseDBC(content);
  if(db.messages.empty() && db.warnings.empty())
    db.warnings.push_back(path + " defines no message");
  return db;
}

void applyNodeIdOffset(Database& db, int32_t offset)
{
  if(offset == 0)
    return;

  for(auto& m : db.messages)
  {
    const int64_t shifted = int64_t(m.id) + offset;
    // A standard identifier is 11 bits, an extended one 29. Wrapping or
    // truncating would silently point the message at a different device, which
    // is worse than leaving it where the file put it.
    const int64_t limit = m.extended ? int64_t(dbcIdentifierMask) : 0x7FF;
    if(shifted < 0 || shifted > limit)
    {
      db.warnings.push_back(
          "node id offset " + std::to_string(offset) + " puts message " + m.name
          + " outside the identifier range; left at " + std::to_string(m.id));
      continue;
    }
    m.id = uint32_t(shifted);
  }
}

void applyFloat32Override(Database& db)
{
  for(auto& m : db.messages)
    for(auto& sg : m.signals)
    {
      // Only integer signals: a SIG_VALTYPE_ record is an explicit statement by
      // the file's author and outranks this heuristic.
      const bool isInteger
          = sg.valueType == ValueType::Signed || sg.valueType == ValueType::Unsigned;
      if(isInteger && sg.length == 32)
        sg.valueType = ValueType::Float32;
    }
}

}
