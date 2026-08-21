#pragma once

/**
 * @file A small, dependency-free reader for Vector DBC (CAN database) files.
 *
 * Scope: what a DBC needs to *decode* a bus. See DBCParser.cpp for the list of
 * constructs that are parsed, tolerated-and-skipped, or unsupported.
 *
 * This header is deliberately free of Qt, of ossia and of anything Linux, so
 * that the parser and the bit extraction can be unit-tested on their own -- the
 * protocol side of the CAN device is Linux-only, the file format is not.
 */

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace Protocols::CAN
{

//! Bit numbering of a signal inside a frame.
enum class ByteOrder
{
  //! `@0` in the file. The start bit is the position of the signal's *most*
  //! significant bit, and the following bits run towards the end of the frame.
  BigEndian,

  //! `@1` in the file. The start bit is the position of the signal's *least*
  //! significant bit, and the following bits run towards the end of the frame.
  LittleEndian
};

//! How the extracted bits are to be read as a number.
enum class ValueType
{
  Unsigned,  //!< `+` in the file
  Signed,    //!< `-` in the file, two's complement
  Float32,   //!< SIG_VALTYPE_ ... 1
  Double64   //!< SIG_VALTYPE_ ... 2
};

//! One entry of a VAL_ / VAL_TABLE_ enumeration.
struct ValueDescription
{
  int64_t value{};
  std::string name;
};

/**
 * A BA_ attribute, resolved to text.
 *
 * Attributes are a free-form extension mechanism: the type of each one is
 * declared by a BA_DEF_ elsewhere in the file and there is no fixed vocabulary,
 * so they are kept verbatim and left for the caller to interpret. The common
 * ones are "GenMsgCycleTime" on a message and "BusType" on the database.
 */
struct Attribute
{
  std::string name;
  std::string value;
};

//! Attributes attached to one object, with the BA_DEF_DEF_ defaults folded in.
struct AttributeSet
{
  std::vector<Attribute> attributes;

  //! nullptr when the attribute is neither set nor defaulted.
  const std::string* find(std::string_view name) const noexcept;
};

struct Signal
{
  std::string name;
  std::string unit;
  std::string comment;
  std::vector<std::string> receivers;

  //! Position of the signal's LSB (little-endian) or MSB (big-endian) in the
  //! DBC bit numbering: bit `p` is bit `p % 8` (LSB = 0) of byte `p / 8`.
  int startBit{};

  //! 1 to 64.
  int length{1};

  ByteOrder byteOrder{ByteOrder::LittleEndian};
  ValueType valueType{ValueType::Unsigned};

  double factor{1.};
  double offset{0.};
  double min{};
  double max{};

  //! `[min|max]` was `[0|0]`, i.e. the file states no range. Very common: a
  //! degenerate domain must not be pushed onto the parameter.
  bool hasRange{};

  //! `M` in the file: this signal selects which multiplexed signals of the
  //! message are present in a given frame.
  bool isMultiplexer{};

  //! `m<N>` in the file: this signal is only present in frames whose
  //! multiplexer signal carries the value `multiplexValue`.
  bool isMultiplexed{};
  int64_t multiplexValue{};

  std::vector<ValueDescription> valueTable;

  AttributeSet attributes;
};

struct Message
{
  //! Identifier with the DBC extended-id flag (bit 31) removed and masked to
  //! 29 bits. Never compare this alone: `{id, extended}` is the key, as a
  //! standard and an extended frame may carry the same numeric identifier.
  uint32_t id{};

  //! Bit 31 of the raw DBC identifier.
  bool extended{};

  std::string name;
  std::string comment;
  std::string transmitter;

  //! Declared payload length in bytes (the DBC "DLC").
  int size{};

  std::vector<Signal> signals;

  AttributeSet attributes;

  const Signal* findSignal(std::string_view name) const noexcept;
};

struct Database
{
  std::vector<Message> messages;

  //! `CM_ "..."` at file scope.
  std::string comment;

  //! DBC "VERSION" record.
  std::string version;

  AttributeSet attributes;

  //! Non-fatal problems: unknown constructs, references to unknown messages or
  //! signals, out-of-range values. Parsing never fails on these -- real vendor
  //! files contain typos and dialect quirks, and refusing the file over one
  //! bad line would be worse than decoding the other 99% of it.
  std::vector<std::string> warnings;

  const Message* findMessage(uint32_t id, bool extended) const noexcept;
};

//! The magic message name under which Vector CANdb++ parks the signals that are
//! defined in the database but not assigned to any frame. It is not a message
//! and must never become part of the node tree.
inline constexpr const char* independentSignalMessage = "VECTOR__INDEPENDENT_SIG_MSG";

//! Bit 31 of a raw DBC identifier marks a 29-bit (extended) identifier.
inline constexpr uint32_t dbcExtendedFlag = 0x80000000u;
inline constexpr uint32_t dbcIdentifierMask = 0x1FFFFFFFu;

//! Parse a DBC held in memory. Never throws; see Database::warnings.
Database parseDBC(std::string_view content);

//! Parse a DBC from disk. On failure the database is empty and a warning
//! explains why.
Database parseDBCFile(const std::string& path);

/**
 * Convert a DBC bit position to its "MSB-first sequential" index and back.
 *
 * DBC numbers bits inside a byte from the LSB (0) to the MSB (7), but a
 * big-endian signal runs from its MSB *downwards* inside a byte and then
 * continues in the next byte. Flipping the low three bits of the position maps
 * that discontinuous walk onto plain increments: in flipped space, index 0 is
 * the MSB of byte 0, index 7 its LSB, index 8 the MSB of byte 1, and so on.
 *
 * The mapping is an involution: flipBitPos(flipBitPos(p)) == p.
 */
constexpr int flipBitPos(int p) noexcept
{
  return (p & ~7) | (7 - (p & 7));
}

/**
 * Extract `length` raw bits of a signal, right-aligned and zero-extended.
 *
 * Bits that fall outside the frame read as zero rather than out of bounds: a
 * device may legitimately send a frame shorter than the DLC the database
 * declares, and dropping the whole frame over it would lose the signals that
 * *are* present.
 */
inline uint64_t
extractRawBits(const uint8_t* data, int dataSize, int startBit, int length, ByteOrder order) noexcept
{
  if(length <= 0 || length > 64)
    return 0;

  const int frameBits = dataSize * 8;
  uint64_t out = 0;

  // Walk the value from its MSB down to its LSB, so that each step is a plain
  // shift-in. `k` is the significance of the bit being read.
  for(int k = length - 1; k >= 0; --k)
  {
    int pos;
    if(order == ByteOrder::LittleEndian)
    {
      // The start bit is the LSB and significance grows with the position.
      pos = startBit + k;
    }
    else
    {
      // The start bit is the MSB; significance grows *backwards* along the
      // flipped axis, on which the signal's bits are contiguous.
      pos = flipBitPos(flipBitPos(startBit) + (length - 1 - k));
    }

    out <<= 1;
    if(pos >= 0 && pos < frameBits)
      out |= uint64_t((data[pos >> 3] >> (pos & 7)) & 1u);
  }

  return out;
}

//! Interpret `length` right-aligned bits as a two's-complement number.
constexpr int64_t signExtend(uint64_t raw, int length) noexcept
{
  if(length <= 0 || length >= 64)
    return int64_t(raw);

  const uint64_t signBit = uint64_t(1) << (length - 1);
  if(raw & signBit)
    return int64_t(raw | ~((uint64_t(1) << length) - 1));
  return int64_t(raw);
}

//! Extract one signal from a frame and apply its value type, factor and offset.
inline double decodeSignal(const Signal& sig, const uint8_t* data, int dataSize) noexcept
{
  const uint64_t raw = extractRawBits(data, dataSize, sig.startBit, sig.length, sig.byteOrder);

  double physical{};
  switch(sig.valueType)
  {
    case ValueType::Float32: {
      const auto bits = uint32_t(raw);
      float f{};
      std::memcpy(&f, &bits, sizeof(f));
      physical = double(f);
      break;
    }
    case ValueType::Double64: {
      double d{};
      std::memcpy(&d, &raw, sizeof(d));
      physical = d;
      break;
    }
    case ValueType::Signed:
      physical = double(signExtend(raw, sig.length));
      break;
    case ValueType::Unsigned:
      physical = double(raw);
      break;
  }

  return physical * sig.factor + sig.offset;
}

//! The raw, unscaled bits of a signal -- what a multiplexer switch is compared
//! against, and what the tests use as an oracle.
inline uint64_t rawSignalBits(const Signal& sig, const uint8_t* data, int dataSize) noexcept
{
  return extractRawBits(data, dataSize, sig.startBit, sig.length, sig.byteOrder);
}

/**
 * Shift every message identifier of the database by `offset`.
 *
 * See CANSpecificSettings::nodeIdOffset: one vendor DBC hardcodes one node id,
 * and the same file has to serve every device of a chain.
 *
 * Messages that would leave the identifier space are left untouched and a
 * warning is recorded, rather than silently wrapping onto another device.
 */
void applyNodeIdOffset(Database& db, int32_t offset);

/**
 * Reinterpret every 32-bit integer signal as an IEEE 754 single.
 *
 * Opt-in and off by default: it exists for databases that declare a float
 * payload as `32@1-` with a `(1,0)` scaling and no SIG_VALTYPE_ record, which
 * decodes to garbage as written. Signals that already carry an explicit
 * SIG_VALTYPE_ float type are untouched.
 */
void applyFloat32Override(Database& db);

}
