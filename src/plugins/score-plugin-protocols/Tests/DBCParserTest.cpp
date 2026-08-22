/**
 * Tests for the DBC parser and the signal bit extraction.
 *
 * Expected values are hand-computed from the bit-numbering rules and shown in
 * the comments, never taken from a run of the code under test - a wrong answer
 * here looks exactly like a right one.
 *
 * The numbering, restated so the derivations can be checked:
 *  - Bit position `p` is bit `p % 8` of byte `p / 8`, counting bit 0 as the
 *    least significant bit of the byte.
 *  - An Intel (`@1`, little-endian) signal's start bit is its LSB, and the
 *    value's bit `k` sits at position `start + k`.
 *  - A Motorola (`@0`, big-endian) signal's start bit is its MSB. Its bits are
 *    contiguous on the "flipped" axis `flip(p) = (p & ~7) | (7 - (p & 7))`, on
 *    which index 0 is the MSB of byte 0, index 7 its LSB, index 8 the MSB of
 *    byte 1, and so on -- so flipped index `f` is bit `7 - (f % 8)` of byte
 *    `f / 8`.
 */

#include <Protocols/CAN/DBCParser.hpp>

#include <catch2/catch_all.hpp>

#include <cstring>
#include <fstream>
#include <string>

using namespace Protocols::CAN;

namespace
{
//! A payload with every nibble distinct, so that a misplaced shift shows up as
//! a wrong digit rather than as a plausible number.
constexpr uint8_t payload[8] = {0x21, 0x43, 0x65, 0x87, 0xA9, 0xCB, 0xED, 0x0F};

uint64_t le(int start, int len)
{
  return extractRawBits(payload, 8, start, len, ByteOrder::LittleEndian);
}
uint64_t be(int start, int len)
{
  return extractRawBits(payload, 8, start, len, ByteOrder::BigEndian);
}

std::string dataPath(const char* name)
{
  return std::string{SCORE_CAN_TEST_DATA} + "/" + name;
}

Database parseString(std::string_view s)
{
  return parseDBC(s);
}
}

TEST_CASE("flipBitPos is an involution and maps bytes MSB-first", "[can][dbc]")
{
  // Within byte 0: position 7 (the MSB) is flipped index 0, position 0 (the
  // LSB) is flipped index 7.
  REQUIRE(flipBitPos(0) == 7);
  REQUIRE(flipBitPos(7) == 0);
  REQUIRE(flipBitPos(1) == 6);
  REQUIRE(flipBitPos(6) == 1);

  // Byte 1 follows byte 0 on the flipped axis.
  REQUIRE(flipBitPos(15) == 8);
  REQUIRE(flipBitPos(8) == 15);
  REQUIRE(flipBitPos(12) == 11);

  // Byte 4.
  REQUIRE(flipBitPos(39) == 32);
  REQUIRE(flipBitPos(32) == 39);

  for(int p = 0; p < 512; ++p)
    REQUIRE(flipBitPos(flipBitPos(p)) == p);
}

TEST_CASE("Little-endian bit extraction", "[can][dbc]")
{
  // Byte-aligned, one byte: bits 0..7 are byte 0 verbatim.
  REQUIRE(le(0, 8) == 0x21);
  REQUIRE(le(8, 8) == 0x43);
  REQUIRE(le(16, 8) == 0x65);
  REQUIRE(le(56, 8) == 0x0F);

  // Byte-aligned, two bytes: little-endian, so byte 1 is the high half.
  //   value = byte0 | (byte1 << 8) = 0x21 | 0x4300
  REQUIRE(le(0, 16) == 0x4321);
  //   value = byte2 | (byte3 << 8) = 0x65 | 0x8700
  REQUIRE(le(16, 16) == 0x8765);

  // The whole frame, little-endian.
  REQUIRE(le(0, 64) == 0x0FEDCBA987654321ull);

  // Single bits. 0x21 == 0b0010'0001, so only bits 0 and 5 are set.
  REQUIRE(le(0, 1) == 1);
  REQUIRE(le(1, 1) == 0);
  REQUIRE(le(5, 1) == 1);
  REQUIRE(le(6, 1) == 0);

  // Straddling one byte boundary, nibble-aligned:
  //   bits 4..11 = high nibble of byte0 (0x2) as the low 4 bits,
  //                low nibble of byte1 (0x3) as the high 4 bits
  //   value = 0x2 | (0x3 << 4) = 0x32
  REQUIRE(le(4, 8) == 0x32);

  // Straddling three bytes, not nibble-aligned in length:
  //   bits 12..31 = high nibble of byte1 (0x4)  -> value bits 0..3
  //                 byte2 (0x65)                -> value bits 4..11
  //                 byte3 (0x87)                -> value bits 12..19
  //   value = 0x4 | (0x65 << 4) | (0x87 << 12) = 0x87654
  REQUIRE(le(12, 20) == 0x87654);

  // A 3-bit field inside one byte: bits 1..3 of 0b0010'0001 are 0,0,0.
  REQUIRE(le(1, 3) == 0);
  // Bits 4..6 of 0b0010'0001 are b4=0, b5=1, b6=0 -> value 0b010 = 2.
  REQUIRE(le(4, 3) == 2);
}

TEST_CASE("Big-endian bit extraction", "[can][dbc]")
{
  // The canonical byte-aligned Motorola signal: start bit 7 is the MSB of
  // byte 0, so flip(7) == 0 and the signal is flipped indices 0..7 == byte 0.
  REQUIRE(be(7, 8) == 0x21);
  REQUIRE(be(15, 8) == 0x43);
  REQUIRE(be(63, 8) == 0x0F);

  // Two bytes, big-endian: flip(7) == 0, flipped 0..15 == byte0 then byte1.
  REQUIRE(be(7, 16) == 0x2143);
  // flip(15) == 8, flipped 8..23 == byte1 then byte2.
  REQUIRE(be(15, 16) == 0x4365);
  // flip(39) == 32, flipped 32..47 == byte4 then byte5.
  REQUIRE(be(39, 16) == 0xA9CB);

  // The whole frame, big-endian.
  REQUIRE(be(7, 64) == 0x21436587A9CBED0Full);

  // Sub-byte, MSB-first: flip(7) == 0, flipped 0..3 are byte0 bits 7,6,5,4,
  // i.e. the high nibble of 0x21 == 0x2.
  REQUIRE(be(7, 4) == 0x2);

  // Straddling a byte boundary. start 3 -> flip(3) == 4, so the signal is
  // flipped indices 4..11:
  //   flipped  4,5,6,7   = byte0 bits 3,2,1,0 = low nibble of 0x21 = 0b0001
  //   flipped  8,9,10,11 = byte1 bits 7,6,5,4 = high nibble of 0x43 = 0b0100
  //   value (MSB first)  = 0b0001'0100 = 0x14
  REQUIRE(be(3, 8) == 0x14);

  // A 4-bit field straddling the boundary. start 1 -> flip(1) == 6,
  // flipped 6..9:
  //   flipped 6 = byte0 bit 1 -> 0x21 == 0b0010'0001, b1 = 0
  //   flipped 7 = byte0 bit 0 ->                      b0 = 1
  //   flipped 8 = byte1 bit 7 -> 0x43 == 0b0100'0011, b7 = 0
  //   flipped 9 = byte1 bit 6 ->                      b6 = 1
  //   value (MSB first) = 0b0101 = 5
  REQUIRE(be(1, 4) == 5);

  // A 12-bit field, straddling and aligned to neither nibble nor byte.
  // start 4 -> flip(4) == 3, so flipped 3..14:
  //   flipped 3..7   = byte0 bits 4,3,2,1,0 -> 0x21 = 0b0010'0001 -> 0,0,0,0,1
  //   flipped 8..14  = byte1 bits 7,6,5,4,3,2,1 -> 0x43 = 0b0100'0011
  //                    -> 0,1,0,0,0,0,1
  //   value = 0b00001'0100001 = (1 << 7) | 0b0100001 = 128 + 33 = 161 = 0x0A1
  REQUIRE(be(4, 12) == 0x0A1);

  // Single bits: flipped index 0 is byte0's MSB, which is 0 for 0x21.
  REQUIRE(be(7, 1) == 0);
  // start 5 is byte0 bit 5, which is set in 0x21.
  REQUIRE(be(5, 1) == 1);
  // start 0 is byte0 bit 0, also set.
  REQUIRE(be(0, 1) == 1);
}

TEST_CASE("Extraction past the end of the frame reads as zero", "[can][dbc]")
{
  // A device may send fewer bytes than the database declares; the signals that
  // *are* present must still decode.
  const uint8_t two[2] = {0xFF, 0xFF};
  REQUIRE(extractRawBits(two, 2, 0, 16, ByteOrder::LittleEndian) == 0xFFFF);
  // Bits 16..31 do not exist -> zero, and the low half still reads.
  REQUIRE(extractRawBits(two, 2, 0, 32, ByteOrder::LittleEndian) == 0x0000FFFF);
  REQUIRE(extractRawBits(two, 2, 16, 8, ByteOrder::LittleEndian) == 0);

  // Degenerate lengths are refused rather than read.
  REQUIRE(extractRawBits(payload, 8, 0, 0, ByteOrder::LittleEndian) == 0);
  REQUIRE(extractRawBits(payload, 8, 0, 65, ByteOrder::LittleEndian) == 0);
}

TEST_CASE("Sign extension", "[can][dbc]")
{
  // 16-bit edges.
  REQUIRE(signExtend(0x0000, 16) == 0);
  REQUIRE(signExtend(0x7FFF, 16) == 32767);
  REQUIRE(signExtend(0x8000, 16) == -32768);
  REQUIRE(signExtend(0xFFFF, 16) == -1);

  // 8-bit edges.
  REQUIRE(signExtend(0x7F, 8) == 127);
  REQUIRE(signExtend(0x80, 8) == -128);
  REQUIRE(signExtend(0xFF, 8) == -1);

  // A 1-bit signed signal: the only bit is the sign bit, so it is 0 or -1.
  REQUIRE(signExtend(0, 1) == 0);
  REQUIRE(signExtend(1, 1) == -1);

  // 2-bit: 0, 1, -2, -1.
  REQUIRE(signExtend(0b00, 2) == 0);
  REQUIRE(signExtend(0b01, 2) == 1);
  REQUIRE(signExtend(0b10, 2) == -2);
  REQUIRE(signExtend(0b11, 2) == -1);

  // 32-bit edges.
  REQUIRE(signExtend(0x7FFFFFFFull, 32) == 2147483647ll);
  REQUIRE(signExtend(0x80000000ull, 32) == -2147483648ll);
  REQUIRE(signExtend(0xFFFFFFFFull, 32) == -1);

  // 64 bits is already the full width: the pattern is reinterpreted, not
  // extended.
  REQUIRE(signExtend(0xFFFFFFFFFFFFFFFFull, 64) == -1);
  REQUIRE(signExtend(0x8000000000000000ull, 64) == INT64_MIN);
}

TEST_CASE("Factor and offset scaling", "[can][dbc]")
{
  Signal sig;
  sig.startBit = 0;
  sig.length = 16;
  sig.byteOrder = ByteOrder::LittleEndian;
  sig.valueType = ValueType::Unsigned;

  // 0x2696 == 9878, the worked example from the LPMS documentation.
  const uint8_t data[2] = {0x96, 0x26};

  sig.factor = 1.;
  sig.offset = 0.;
  REQUIRE(decodeSignal(sig, data, 2) == Catch::Approx(9878.));

  sig.factor = 0.0001;
  sig.offset = 0.;
  REQUIRE(decodeSignal(sig, data, 2) == Catch::Approx(0.9878));

  // Offset is applied after the factor: raw * factor + offset.
  sig.factor = 0.5;
  sig.offset = 100.;
  REQUIRE(decodeSignal(sig, data, 2) == Catch::Approx(9878. * 0.5 + 100.));

  // A negative factor is legal and inverts the scale.
  sig.factor = -1.;
  sig.offset = 0.;
  REQUIRE(decodeSignal(sig, data, 2) == Catch::Approx(-9878.));

  // Signed reading of the same bits: 0x2696 is positive, but 0x9626 is not.
  const uint8_t negative[2] = {0x26, 0x96}; // little-endian -> 0x9626
  sig.valueType = ValueType::Signed;
  sig.factor = 1.;
  REQUIRE(decodeSignal(sig, negative, 2) == Catch::Approx(double(int16_t(0x9626))));
  REQUIRE(int16_t(0x9626) == -27098); // 0x9626 == 38438; 38438 - 65536 == -27098
}

TEST_CASE("IEEE 754 signals", "[can][dbc]")
{
  Signal sig;
  sig.startBit = 0;
  sig.length = 32;
  sig.byteOrder = ByteOrder::LittleEndian;
  sig.valueType = ValueType::Float32;
  sig.factor = 1.;
  sig.offset = 0.;

  // 1.0f == 0x3F800000, little-endian on the wire.
  const uint8_t one[4] = {0x00, 0x00, 0x80, 0x3F};
  REQUIRE(decodeSignal(sig, one, 4) == Catch::Approx(1.0));

  // -2.5f == 0xC0200000.
  const uint8_t negTwoFive[4] = {0x00, 0x00, 0x20, 0xC0};
  REQUIRE(decodeSignal(sig, negTwoFive, 4) == Catch::Approx(-2.5));

  // Big-endian float: same bits, reversed byte order on the wire, start bit at
  // the MSB of byte 0.
  sig.byteOrder = ByteOrder::BigEndian;
  sig.startBit = 7;
  const uint8_t oneBE[4] = {0x3F, 0x80, 0x00, 0x00};
  REQUIRE(decodeSignal(sig, oneBE, 4) == Catch::Approx(1.0));

  // double: 1.0 == 0x3FF0000000000000.
  Signal dbl;
  dbl.startBit = 0;
  dbl.length = 64;
  dbl.byteOrder = ByteOrder::LittleEndian;
  dbl.valueType = ValueType::Double64;
  dbl.factor = 1.;
  dbl.offset = 0.;
  const uint8_t oneD[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x3F};
  REQUIRE(decodeSignal(dbl, oneD, 8) == Catch::Approx(1.0));
}

// ---------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------

TEST_CASE("The real LPMS3 16-bit database", "[can][dbc]")
{
  auto db = parseDBCFile(dataPath("LPMS3_16bit.dbc"));

  INFO([&] {
    std::string s;
    for(auto& w : db.warnings)
      s += w + "\n";
    return s;
  }());
  REQUIRE(db.warnings.empty());

  // Exactly the five real frames. The vendor file also holds a sixth BO_ --
  // VECTOR__INDEPENDENT_SIG_MSG, id 3221225472 -- which is not a message but
  // Vector's pool of unattached signals, and must not be in the tree.
  REQUIRE(db.messages.size() == 5);

  for(const auto& m : db.messages)
    REQUIRE(m.name != std::string{independentSignalMessage});

  // CANopen: start id + node id, with the file written for node 1.
  //   PDO1 0x180+1 = 0x181 = 385     PDO3 0x380+1 = 0x381 = 897
  //   PDO2 0x280+1 = 0x281 = 641     PDO4 0x480+1 = 0x481 = 1153
  //   heartbeat 0x700+1 = 0x701 = 1793
  REQUIRE(db.findMessage(0x181, false) != nullptr);
  REQUIRE(db.findMessage(0x281, false) != nullptr);
  REQUIRE(db.findMessage(0x381, false) != nullptr);
  REQUIRE(db.findMessage(0x481, false) != nullptr);
  REQUIRE(db.findMessage(0x701, false) != nullptr);

  REQUIRE(db.findMessage(0x181, false)->name == "PDO1_Transmit");
  REQUIRE(db.findMessage(0x481, false)->name == "PDO4_Transmit");

  // All of them are standard (11-bit) identifiers.
  for(const auto& m : db.messages)
    REQUIRE(m.extended == false);

  // The vendor file misspells the heartbeat message; the parser must not
  // "correct" or reject it.
  REQUIRE(db.findMessage(0x701, false)->name == "Heatbeat");
  // ...and it declares no signal at all.
  REQUIRE(db.findMessage(0x701, false)->signals.empty());
  REQUIRE(db.findMessage(0x701, false)->size == 8);

  // The attribute at the end of the file.
  const auto* bus = db.attributes.find("BusType");
  REQUIRE(bus != nullptr);
  REQUIRE(*bus == "CAN");
}

TEST_CASE("Decoding a synthetic LPMS PDO4 quaternion frame", "[can][dbc]")
{
  auto db = parseDBCFile(dataPath("LPMS3_16bit.dbc"));
  const auto* pdo4 = db.findMessage(0x481, false);
  REQUIRE(pdo4 != nullptr);
  REQUIRE(pdo4->signals.size() == 4);

  // The four components are 16-bit signed Intel values with a 0.0001 factor,
  // at start bits 0, 16, 32 and 48. Note the vendor's typo on the Z component.
  const auto* w = pdo4->findSignal("QuaternionW");
  const auto* x = pdo4->findSignal("QuaternionX");
  const auto* y = pdo4->findSignal("QuaternionY");
  const auto* z = pdo4->findSignal("QuatetnionZ");
  REQUIRE(w != nullptr);
  REQUIRE(x != nullptr);
  REQUIRE(y != nullptr);
  REQUIRE(z != nullptr);

  REQUIRE(w->startBit == 0);
  REQUIRE(x->startBit == 16);
  REQUIRE(y->startBit == 32);
  REQUIRE(z->startBit == 48);
  REQUIRE(w->length == 16);
  REQUIRE(w->byteOrder == ByteOrder::LittleEndian);
  REQUIRE(w->valueType == ValueType::Signed);
  REQUIRE(w->factor == Catch::Approx(0.0001));
  REQUIRE(w->unit.empty());
  REQUIRE(w->hasRange);
  REQUIRE(w->min == Catch::Approx(-3.2768));
  REQUIRE(w->max == Catch::Approx(3.2767));

  // W =  9878 = 0x2696 -> bytes 96 26   (the documented worked example)
  // X = -1234        -> 0x10000 - 1234 = 64302 = 0xFB2E -> bytes 2E FB
  // Y =     0        -> 0x0000            -> bytes 00 00
  // Z = 32767 = 0x7FFF -> bytes FF 7F
  const uint8_t frame[8] = {0x96, 0x26, 0x2E, 0xFB, 0x00, 0x00, 0xFF, 0x7F};

  REQUIRE(decodeSignal(*w, frame, 8) == Catch::Approx(0.9878));
  REQUIRE(decodeSignal(*x, frame, 8) == Catch::Approx(-0.1234));
  REQUIRE(decodeSignal(*y, frame, 8) == Catch::Approx(0.0));
  REQUIRE(decodeSignal(*z, frame, 8) == Catch::Approx(3.2767));

  // And the raw bits, independently of the scaling.
  REQUIRE(rawSignalBits(*w, frame, 8) == 9878);
  REQUIRE(rawSignalBits(*z, frame, 8) == 32767);
}

TEST_CASE("The node id offset shifts message ids", "[can][dbc]")
{
  auto db = parseDBCFile(dataPath("LPMS3_16bit.dbc"));
  REQUIRE(db.findMessage(0x181, false) != nullptr);

  // The same sensor configured as CANopen node 2 puts the identical signal
  // layout one identifier higher.
  applyNodeIdOffset(db, 1);

  REQUIRE(db.findMessage(0x181, false) == nullptr);
  REQUIRE(db.findMessage(0x182, false) != nullptr);
  REQUIRE(db.findMessage(0x182, false)->name == "PDO1_Transmit");
  REQUIRE(db.findMessage(0x282, false) != nullptr);
  REQUIRE(db.findMessage(0x382, false) != nullptr);
  REQUIRE(db.findMessage(0x482, false) != nullptr);
  REQUIRE(db.findMessage(0x702, false) != nullptr);

  // The signals are untouched: only the frame they arrive in moves.
  REQUIRE(db.findMessage(0x482, false)->findSignal("QuaternionW") != nullptr);

  // A negative offset pulls the database back down.
  applyNodeIdOffset(db, -1);
  REQUIRE(db.findMessage(0x181, false) != nullptr);
  REQUIRE(db.findMessage(0x182, false) == nullptr);

  // Zero is a no-op.
  const auto before = db.messages.front().id;
  applyNodeIdOffset(db, 0);
  REQUIRE(db.messages.front().id == before);
}

TEST_CASE("The node id offset refuses to leave the identifier range", "[can][dbc]")
{
  // A standard identifier is 11 bits: shifting 0x181 up by 0x700 would leave
  // it. Rather than wrap onto some other device's frame, the message stays put
  // and a warning is recorded.
  auto db = parseDBCFile(dataPath("LPMS3_16bit.dbc"));
  applyNodeIdOffset(db, 0x700);

  REQUIRE(!db.warnings.empty());
  // 0x181 + 0x700 = 0x881 > 0x7FF, so PDO1 is left where it was.
  REQUIRE(db.findMessage(0x181, false) != nullptr);

  auto db2 = parseDBCFile(dataPath("LPMS3_16bit.dbc"));
  applyNodeIdOffset(db2, -0x2000);
  REQUIRE(!db2.warnings.empty());
  REQUIRE(db2.findMessage(0x181, false) != nullptr);
}

TEST_CASE("The 32-bit float override", "[can][dbc]")
{
  auto db = parseDBCFile(dataPath("LPMS3_32bit.dbc"));
  REQUIRE(db.messages.size() == 5);

  const auto* pdo1 = db.findMessage(0x181, false);
  REQUIRE(pdo1 != nullptr);
  const auto* accX = pdo1->findSignal("Acc_CalibratedX");
  REQUIRE(accX != nullptr);

  // As written the file declares a signed 32-bit integer with an identity
  // scaling, and states no range.
  REQUIRE(accX->length == 32);
  REQUIRE(accX->valueType == ValueType::Signed);
  REQUIRE(accX->factor == Catch::Approx(1.0));
  REQUIRE(accX->hasRange == false);

  // 1.0f == 0x3F800000, little-endian on the wire.
  const uint8_t frame[8] = {0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F};

  // Decoded as the file says: the bit pattern read as an integer.
  REQUIRE(decodeSignal(*accX, frame, 8) == Catch::Approx(1065353216.0));

  // Decoded with the override: what the sensor actually sends.
  applyFloat32Override(db);
  const auto* accX2 = db.findMessage(0x181, false)->findSignal("Acc_CalibratedX");
  REQUIRE(accX2->valueType == ValueType::Float32);
  REQUIRE(decodeSignal(*accX2, frame, 8) == Catch::Approx(1.0));
}

TEST_CASE("The float override leaves non-32-bit and explicit signals alone", "[can][dbc]")
{
  // 16-bit signals must not be touched.
  auto db16 = parseDBCFile(dataPath("LPMS3_16bit.dbc"));
  applyFloat32Override(db16);
  const auto* w = db16.findMessage(0x481, false)->findSignal("QuaternionW");
  REQUIRE(w->valueType == ValueType::Signed);

  // A signal with an explicit SIG_VALTYPE_ double is not downgraded to a float.
  auto db = parseString(R"(VERSION ""

BO_ 100 Msg: 8 Vector__XXX
 SG_ Wide : 0|64@1+ (1,0) [0|0] "" Vector__XXX
 SG_ Narrow : 0|32@1+ (1,0) [0|0] "" Vector__XXX

SIG_VALTYPE_ 100 Wide : 2;
)");
  REQUIRE(db.messages.size() == 1);
  applyFloat32Override(db);
  REQUIRE(db.messages[0].findSignal("Wide")->valueType == ValueType::Double64);
  REQUIRE(db.messages[0].findSignal("Narrow")->valueType == ValueType::Float32);
}

TEST_CASE("The extended identifier flag", "[can][dbc]")
{
  // Bit 31 of the BO_ id marks a 29-bit identifier; the identifier itself is
  // the low 29 bits. 0x98765432 -> extended, id 0x18765432.
  auto db = parseString(R"(VERSION ""

BO_ 2557891634 Extended: 8 Vector__XXX
 SG_ A : 0|8@1+ (1,0) [0|0] "" Vector__XXX

BO_ 291 Standard: 8 Vector__XXX
 SG_ B : 0|8@1+ (1,0) [0|0] "" Vector__XXX
)");

  REQUIRE(db.messages.size() == 2);

  const auto* ext = db.findMessage(0x18765432, true);
  REQUIRE(ext != nullptr);
  REQUIRE(ext->name == "Extended");
  REQUIRE(ext->extended == true);

  const auto* std_ = db.findMessage(0x123, false);
  REQUIRE(std_ != nullptr);
  REQUIRE(std_->name == "Standard");
  REQUIRE(std_->extended == false);

  // The pair is the key: the same number as a standard frame is a different
  // message, and must not be found.
  REQUIRE(db.findMessage(0x18765432, false) == nullptr);
  REQUIRE(db.findMessage(0x123, true) == nullptr);
}

TEST_CASE("A standard and an extended message may share a number", "[can][dbc]")
{
  // 0x80000064 is extended id 100; 100 is standard id 100. Keying on the raw
  // value alone would make one of them shadow the other.
  auto db = parseString(R"(VERSION ""

BO_ 100 AsStandard: 8 Vector__XXX
 SG_ A : 0|8@1+ (1,0) [0|0] "" Vector__XXX

BO_ 2147483748 AsExtended: 8 Vector__XXX
 SG_ B : 0|8@1+ (1,0) [0|0] "" Vector__XXX
)");

  REQUIRE(db.messages.size() == 2);
  REQUIRE(db.findMessage(100, false) != nullptr);
  REQUIRE(db.findMessage(100, true) != nullptr);
  REQUIRE(db.findMessage(100, false)->name == "AsStandard");
  REQUIRE(db.findMessage(100, true)->name == "AsExtended");
}

TEST_CASE("VECTOR__INDEPENDENT_SIG_MSG is skipped", "[can][dbc]")
{
  // 3221225472 == 0xC0000000: the extended flag is set and the masked id is 0,
  // which is exactly why it must be matched by name and dropped -- keyed on the
  // raw number it would look like a legitimate extended message.
  auto db = parseString(R"(VERSION ""

BO_ 3221225472 VECTOR__INDEPENDENT_SIG_MSG: 0 Vector__XXX
 SG_ Unused1 : 0|16@1- (0.01,0) [-327.68|327.67] "d" Vector__XXX
 SG_ Unused2 : 0|16@1- (0.01,0) [-327.68|327.67] "d" Vector__XXX

BO_ 100 Real: 8 Vector__XXX
 SG_ A : 0|8@1+ (1,0) [0|0] "" Vector__XXX

CM_ BO_ 3221225472 "This is a message for not used signals.";
)");

  REQUIRE(db.messages.size() == 1);
  REQUIRE(db.messages[0].name == "Real");
  REQUIRE(db.findMessage(0, true) == nullptr);

  // The CM_ that refers to it must not produce a warning: the reference is
  // expected to dangle.
  REQUIRE(db.warnings.empty());
}

TEST_CASE("Value tables become enumerations", "[can][dbc]")
{
  auto db = parseString(R"(VERSION ""

BO_ 100 Msg: 8 Vector__XXX
 SG_ Gear : 0|4@1+ (1,0) [0|15] "" Vector__XXX
 SG_ Mode : 8|4@1+ (1,0) [0|15] "" Vector__XXX

VAL_TABLE_ ModeTable 0 "Idle" 1 "Run" 2 "Fault" ;
VAL_ 100 Gear 0 "Neutral" 1 "First" 2 "Second" -1 "Reverse" ;
VAL_ 100 Mode ModeTable ;
)");

  REQUIRE(db.messages.size() == 1);
  const auto* gear = db.messages[0].findSignal("Gear");
  REQUIRE(gear != nullptr);
  REQUIRE(gear->valueTable.size() == 4);
  REQUIRE(gear->valueTable[0].value == 0);
  REQUIRE(gear->valueTable[0].name == "Neutral");
  REQUIRE(gear->valueTable[2].name == "Second");
  // Negative enumerators are legal.
  REQUIRE(gear->valueTable[3].value == -1);
  REQUIRE(gear->valueTable[3].name == "Reverse");

  // A VAL_ may name a VAL_TABLE_ instead of spelling the pairs out.
  const auto* mode = db.messages[0].findSignal("Mode");
  REQUIRE(mode != nullptr);
  REQUIRE(mode->valueTable.size() == 3);
  REQUIRE(mode->valueTable[1].name == "Run");
}

TEST_CASE("Comments", "[can][dbc]")
{
  auto db = parseString("VERSION \"\"\r\n"
                        "\r\n"
                        "BO_ 100 Msg: 8 Vector__XXX\r\n"
                        " SG_ A : 0|8@1+ (1,0) [0|0] \"\" Vector__XXX\r\n"
                        "\r\n"
                        "CM_ \"A database comment\";\r\n"
                        "CM_ BO_ 100 \"A message comment\";\r\n"
                        "CM_ SG_ 100 A \"A signal comment\nspanning two lines\";\r\n");

  REQUIRE(db.messages.size() == 1);
  REQUIRE(db.comment == "A database comment");
  REQUIRE(db.messages[0].comment == "A message comment");
  // Comments may contain newlines: CANdb++ writes them verbatim.
  REQUIRE(db.messages[0].findSignal("A")->comment == "A signal comment\nspanning two lines");
}

TEST_CASE("CRLF line endings", "[can][dbc]")
{
  // The vendor files really are CRLF. A '\r' treated as a blank rather than as
  // end-of-line turns the receiver list of the first signal into a construct
  // that runs to the end of the file, and the database silently comes back
  // empty -- which is exactly what happened before this was handled.
  const std::string crlf = "VERSION \"\"\r\n"
                           "\r\n"
                           "BO_ 100 Msg: 8 Vector__XXX\r\n"
                           " SG_ A : 0|8@1+ (1,0) [0|0] \"\" Vector__XXX\r\n"
                           " SG_ B : 8|8@1+ (1,0) [0|0] \"\" Vector__XXX\r\n"
                           "\r\n"
                           "BO_ 200 Msg2: 8 Vector__XXX\r\n"
                           " SG_ C : 0|8@1+ (1,0) [0|0] \"\" Vector__XXX\r\n";

  auto db = parseString(crlf);
  REQUIRE(db.messages.size() == 2);
  REQUIRE(db.messages[0].signals.size() == 2);
  REQUIRE(db.messages[1].signals.size() == 1);
  REQUIRE(db.warnings.empty());

  // The same content with LF endings must parse identically.
  std::string lf;
  for(char c : crlf)
    if(c != '\r')
      lf.push_back(c);

  auto db2 = parseString(lf);
  REQUIRE(db2.messages.size() == db.messages.size());
  REQUIRE(db2.messages[0].signals.size() == db.messages[0].signals.size());
  REQUIRE(db2.messages[0].signals[0].name == db.messages[0].signals[0].name);
}

TEST_CASE("Multiplexing indicators", "[can][dbc]")
{
  auto db = parseString(R"(VERSION ""

BO_ 100 Muxed: 8 Vector__XXX
 SG_ Selector M : 0|8@1+ (1,0) [0|0] "" Vector__XXX
 SG_ WhenZero m0 : 8|16@1+ (1,0) [0|0] "" Vector__XXX
 SG_ WhenOne m1 : 8|16@1+ (1,0) [0|0] "" Vector__XXX
 SG_ Always : 24|8@1+ (1,0) [0|0] "" Vector__XXX
)");

  REQUIRE(db.messages.size() == 1);
  const auto& m = db.messages[0];
  REQUIRE(m.signals.size() == 4);

  const auto* sel = m.findSignal("Selector");
  REQUIRE(sel->isMultiplexer);
  REQUIRE(!sel->isMultiplexed);

  const auto* zero = m.findSignal("WhenZero");
  REQUIRE(zero->isMultiplexed);
  REQUIRE(!zero->isMultiplexer);
  REQUIRE(zero->multiplexValue == 0);

  const auto* one = m.findSignal("WhenOne");
  REQUIRE(one->isMultiplexed);
  REQUIRE(one->multiplexValue == 1);

  const auto* always = m.findSignal("Always");
  REQUIRE(!always->isMultiplexed);
  REQUIRE(!always->isMultiplexer);
}

TEST_CASE("Extended multiplexing is reported, not silently dropped", "[can][dbc]")
{
  // SG_MUL_VAL_ is not implemented. A database that uses it would decode its
  // multiplexed signals unconditionally, so the user has to be told.
  auto db = parseString(R"(VERSION ""

BO_ 100 Muxed: 8 Vector__XXX
 SG_ Selector M : 0|8@1+ (1,0) [0|0] "" Vector__XXX
 SG_ Sub m0 : 8|16@1+ (1,0) [0|0] "" Vector__XXX

SG_MUL_VAL_ 100 Sub Selector 1-3, 5-7;
)");

  REQUIRE(db.messages.size() == 1);
  REQUIRE(!db.warnings.empty());

  bool mentioned = false;
  for(const auto& w : db.warnings)
    if(w.find("SG_MUL_VAL_") != std::string::npos)
      mentioned = true;
  REQUIRE(mentioned);
}

TEST_CASE("A degenerate [0|0] range is not a domain", "[can][dbc]")
{
  auto db = parseString(R"(VERSION ""

BO_ 100 Msg: 8 Vector__XXX
 SG_ NoRange : 0|8@1+ (1,0) [0|0] "" Vector__XXX
 SG_ Ranged : 8|8@1+ (1,0) [0|255] "" Vector__XXX
 SG_ NegRange : 16|8@1- (1,0) [-128|127] "" Vector__XXX
)");

  const auto& m = db.messages[0];
  REQUIRE(m.findSignal("NoRange")->hasRange == false);
  REQUIRE(m.findSignal("Ranged")->hasRange == true);
  REQUIRE(m.findSignal("Ranged")->max == Catch::Approx(255.));
  REQUIRE(m.findSignal("NegRange")->hasRange == true);
  REQUIRE(m.findSignal("NegRange")->min == Catch::Approx(-128.));
}

TEST_CASE("Malformed input is survivable", "[can][dbc]")
{
  // Empty input.
  REQUIRE(parseString("").messages.empty());
  REQUIRE(parseString("   \n\n  \n").messages.empty());

  // Only a header.
  REQUIRE(parseString("VERSION \"1.0\"\n").messages.empty());
  REQUIRE(parseString("VERSION \"1.0\"\n").version == "1.0");

  // A signal with an impossible length is dropped, the message survives.
  auto db = parseString(R"(VERSION ""

BO_ 100 Msg: 8 Vector__XXX
 SG_ TooWide : 0|65@1+ (1,0) [0|0] "" Vector__XXX
 SG_ Zero : 0|0@1+ (1,0) [0|0] "" Vector__XXX
 SG_ Fine : 0|8@1+ (1,0) [0|0] "" Vector__XXX
)");
  REQUIRE(db.messages.size() == 1);
  REQUIRE(db.messages[0].signals.size() == 1);
  REQUIRE(db.messages[0].signals[0].name == "Fine");
  REQUIRE(db.warnings.size() >= 2);

  // An unterminated string does not hang or read out of bounds.
  auto db2 = parseString("VERSION \"unterminated\n");
  REQUIRE(!db2.warnings.empty());

  // A truncated SG_ line.
  auto db3 = parseString(R"(VERSION ""

BO_ 100 Msg: 8 Vector__XXX
 SG_ Broken :
 SG_ Fine : 0|8@1+ (1,0) [0|0] "" Vector__XXX
)");
  REQUIRE(db3.messages.size() == 1);
  REQUIRE(!db3.warnings.empty());
}

TEST_CASE("Duplicates are rejected with a diagnostic", "[can][dbc]")
{
  auto db = parseString(R"(VERSION ""

BO_ 100 First: 8 Vector__XXX
 SG_ A : 0|8@1+ (1,0) [0|0] "" Vector__XXX

BO_ 100 Second: 8 Vector__XXX
 SG_ B : 0|8@1+ (1,0) [0|0] "" Vector__XXX
)");
  // Two messages with the same (id, extended): the first wins, and the clash is
  // reported rather than resolved by chance.
  REQUIRE(db.messages.size() == 1);
  REQUIRE(db.messages[0].name == "First");
  REQUIRE(!db.warnings.empty());

  auto db2 = parseString(R"(VERSION ""

BO_ 100 Msg: 8 Vector__XXX
 SG_ A : 0|8@1+ (1,0) [0|0] "" Vector__XXX
 SG_ A : 8|8@1+ (1,0) [0|0] "" Vector__XXX
)");
  REQUIRE(db2.messages[0].signals.size() == 1);
  REQUIRE(db2.messages[0].signals[0].startBit == 0);
  REQUIRE(!db2.warnings.empty());
}

TEST_CASE("Attributes", "[can][dbc]")
{
  auto db = parseString(R"(VERSION ""

BO_ 100 Msg: 8 Vector__XXX
 SG_ A : 0|8@1+ (1,0) [0|0] "" Vector__XXX

BA_DEF_ BO_ "GenMsgCycleTime" INT 0 65535;
BA_DEF_ SG_ "GenSigStartValue" FLOAT 0 100;
BA_DEF_ "BusType" STRING ;
BA_DEF_ BO_ "MsgKind" ENUM "Cyclic","Event","Mixed";
BA_DEF_DEF_ "GenMsgCycleTime" 100;
BA_DEF_DEF_ "BusType" "CAN";
BA_ "GenMsgCycleTime" BO_ 100 20;
BA_ "GenSigStartValue" SG_ 100 A 3.5;
BA_ "MsgKind" BO_ 100 1;
BA_ "BusType" "CAN FD";
)");

  REQUIRE(db.messages.size() == 1);

  const auto* cycle = db.messages[0].attributes.find("GenMsgCycleTime");
  REQUIRE(cycle != nullptr);
  REQUIRE(*cycle == "20");

  const auto* start = db.messages[0].signals[0].attributes.find("GenSigStartValue");
  REQUIRE(start != nullptr);
  REQUIRE(*start == "3.5");

  // An ENUM attribute is written as an index into the BA_DEF_ list; it is
  // resolved back to the name it stands for.
  const auto* kind = db.messages[0].attributes.find("MsgKind");
  REQUIRE(kind != nullptr);
  REQUIRE(*kind == "Event");

  // A database-level attribute.
  const auto* bus = db.attributes.find("BusType");
  REQUIRE(bus != nullptr);
  REQUIRE(*bus == "CAN FD");
}

TEST_CASE("Known but unused constructs are reported once each", "[can][dbc]")
{
  auto db = parseString(R"(VERSION ""

BU_: Alpha Beta Gamma

BO_ 100 Msg: 8 Vector__XXX
 SG_ A : 0|8@1+ (1,0) [0|0] "" Vector__XXX

BO_TX_BU_ 100 : Alpha,Beta;
SIG_GROUP_ 100 GroupA 1 : A;
SIG_GROUP_ 100 GroupB 1 : A;
EV_ SomeVar: 0 [0|100] "" 0 1 DUMMY_NODE_VECTOR0 Vector__XXX;
)");

  // The message still parses despite the constructs around it.
  REQUIRE(db.messages.size() == 1);
  REQUIRE(db.messages[0].signals.size() == 1);

  // Two SIG_GROUP_ records, but only one diagnostic about them: a large file
  // must not drown the user in identical warnings.
  int sigGroupWarnings = 0;
  for(const auto& w : db.warnings)
    if(w.find("SIG_GROUP_") != std::string::npos)
      ++sigGroupWarnings;
  REQUIRE(sigGroupWarnings == 1);
}

TEST_CASE("The NS_ block is not mistaken for content", "[can][dbc]")
{
  // The NS_ section lists the very same tokens as the real constructs. A parser
  // that dispatches on the leading keyword without knowing about NS_ would try
  // to read `CM_`, `VAL_` and `SIG_VALTYPE_` as records.
  auto db = parseString(R"(VERSION ""


NS_ :
	NS_DESC_
	CM_
	BA_DEF_
	BA_
	VAL_
	CAT_DEF_
	CAT_
	FILTER
	BA_DEF_DEF_
	EV_DATA_
	ENVVAR_DATA_
	SGTYPE_
	SGTYPE_VAL_
	BA_DEF_SGTYPE_
	BA_SGTYPE_
	SIG_TYPE_REF_
	VAL_TABLE_
	SIG_GROUP_
	SIG_VALTYPE_
	SIGTYPE_VALTYPE_
	BO_TX_BU_
	BA_DEF_REL_
	BA_REL_
	BA_DEF_DEF_REL_
	BU_SG_REL_
	BU_EV_REL_
	BU_BO_REL_
	SG_MUL_VAL_

BS_:

BU_:

BO_ 100 Msg: 8 Vector__XXX
 SG_ A : 0|8@1+ (1,0) [0|0] "" Vector__XXX
)");

  REQUIRE(db.messages.size() == 1);
  REQUIRE(db.messages[0].name == "Msg");
  REQUIRE(db.warnings.empty());
}

TEST_CASE("Signal syntax variants", "[can][dbc]")
{
  auto db = parseString(R"(VERSION ""

BO_ 100 Msg: 8 Vector__XXX
 SG_ NoSpaceColon: 0|8@1+ (1,0) [0|0] "" Vector__XXX
 SG_ BigEndian : 15|16@0- (0.5,-40) [-40|100] "degC" Vector__XXX
 SG_ MultiRx : 32|8@1+ (1,0) [0|0] "" Alpha,Beta
 SG_ Exponent : 40|8@1+ (1e-3,0) [0|0] "" Vector__XXX
)");

  REQUIRE(db.messages.size() == 1);
  const auto& m = db.messages[0];
  REQUIRE(m.signals.size() == 4);

  // A missing space before the colon is accepted.
  REQUIRE(m.findSignal("NoSpaceColon") != nullptr);
  REQUIRE(m.findSignal("NoSpaceColon")->startBit == 0);

  const auto* bigE = m.findSignal("BigEndian");
  REQUIRE(bigE != nullptr);
  REQUIRE(bigE->byteOrder == ByteOrder::BigEndian);
  REQUIRE(bigE->valueType == ValueType::Signed);
  REQUIRE(bigE->startBit == 15);
  REQUIRE(bigE->factor == Catch::Approx(0.5));
  REQUIRE(bigE->offset == Catch::Approx(-40.));
  REQUIRE(bigE->unit == "degC");

  // Several receivers, comma-separated; the Vector__XXX placeholder is dropped
  // but real node names are kept.
  const auto* rx = m.findSignal("MultiRx");
  REQUIRE(rx->receivers.size() == 2);
  REQUIRE(rx->receivers[0] == "Alpha");
  REQUIRE(rx->receivers[1] == "Beta");
  REQUIRE(m.findSignal("NoSpaceColon")->receivers.empty());

  // Exponent notation in the factor.
  REQUIRE(m.findSignal("Exponent")->factor == Catch::Approx(0.001));
}

TEST_CASE("SIG_VALTYPE_ with and without the colon", "[can][dbc]")
{
  auto db = parseString(R"(VERSION ""

BO_ 100 Msg: 8 Vector__XXX
 SG_ F : 0|32@1+ (1,0) [0|0] "" Vector__XXX
 SG_ G : 32|32@1+ (1,0) [0|0] "" Vector__XXX

SIG_VALTYPE_ 100 F : 1;
SIG_VALTYPE_ 100 G 1;
)");

  REQUIRE(db.messages.size() == 1);
  REQUIRE(db.messages[0].findSignal("F")->valueType == ValueType::Float32);
  // The colon is optional in some dialects.
  REQUIRE(db.messages[0].findSignal("G")->valueType == ValueType::Float32);
}

TEST_CASE("Escaped and doubled quotes in strings", "[can][dbc]")
{
  auto db = parseString("VERSION \"\"\n"
                        "\n"
                        "BO_ 100 Msg: 8 Vector__XXX\n"
                        " SG_ A : 0|8@1+ (1,0) [0|0] \"\" Vector__XXX\n"
                        "\n"
                        "CM_ BO_ 100 \"a \\\"quoted\\\" word\";\n");
  REQUIRE(db.messages.size() == 1);
  REQUIRE(db.messages[0].comment == "a \"quoted\" word");

  auto db2 = parseString("VERSION \"\"\n"
                         "\n"
                         "BO_ 100 Msg: 8 Vector__XXX\n"
                         " SG_ A : 0|8@1+ (1,0) [0|0] \"\" Vector__XXX\n"
                         "\n"
                         "CM_ BO_ 100 \"a \"\"doubled\"\" word\";\n");
  REQUIRE(db2.messages.size() == 1);
  REQUIRE(db2.messages[0].comment == "a \"doubled\" word");
}

TEST_CASE("A one-bit and a 64-bit signal round-trip through the parser", "[can][dbc]")
{
  auto db = parseString(R"(VERSION ""

BO_ 100 Msg: 8 Vector__XXX
 SG_ Flag : 7|1@1+ (1,0) [0|1] "" Vector__XXX
 SG_ Whole : 0|64@1+ (1,0) [0|0] "" Vector__XXX
)");

  const auto& m = db.messages[0];
  const auto* flag = m.findSignal("Flag");
  const auto* whole = m.findSignal("Whole");
  REQUIRE(flag->length == 1);
  REQUIRE(whole->length == 64);

  const uint8_t frame[8] = {0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

  // Bit 7 of byte 0 is set in 0x80.
  REQUIRE(rawSignalBits(*flag, frame, 8) == 1);
  // The whole frame, little-endian: byte 7 is the most significant.
  REQUIRE(rawSignalBits(*whole, frame, 8) == 0x0100000000000080ull);
}

// ---------------------------------------------------------------------------
// Cross-checks against independent implementations
//
// The two blocks below are the strongest oracles available for the big-endian
// case, because neither expectation was produced by this code: the first comes
// from Vector CANdb++ itself and the second from decoding the same payloads
// through cantools and canmatrix, which agreed bit for bit.
// ---------------------------------------------------------------------------

TEST_CASE("Big-endian start bits agree with Vector CANdb++", "[can][dbc][oracle]")
{
  // Expected LSB/MSB positions as reported by Vector CANdb++ for a set of
  // Motorola signals (the table canmatrix pins in test_candbpp_startbit).
  // "MSB" is the DBC start bit itself; "LSB" is the position of the signal's
  // least significant bit, which is what the flipped-axis walk has to land on.
  struct Case
  {
    int start;
    int length;
    int expectedLsb;
  };

  static constexpr Case cases[] = {
      {39, 4, 36},  {52, 1, 52},  {51, 12, 56}, {6, 1, 6},   {5, 1, 5},
      {23, 3, 21},  {7, 1, 7},    {34, 11, 40}, {18, 11, 24}, {4, 13, 8},
  };

  for(const auto& c : cases)
  {
    INFO("start " << c.start << " length " << c.length);
    // The signal runs from flip(start) for `length` bits along the flipped
    // axis; its LSB is the last of those, mapped back.
    const int lsb = flipBitPos(flipBitPos(c.start) + c.length - 1);
    REQUIRE(lsb == c.expectedLsb);
  }
}

TEST_CASE("Big-endian extraction agrees with cantools and canmatrix", "[can][dbc][oracle]")
{
  // Signals and expected raw values obtained by decoding the payloads below
  // through cantools and through canmatrix; the two agreed on every cell.
  //
  //  BE_7_16  :  7|16@0+     BE_39_1  : 39|1@0+
  //  BE_23_4  : 23|4@0+      BE_38_11 : 38|11@0+
  //  BE_19_4  : 19|4@0+      BE_43_12 : 43|12@0+
  //  BE_31_8  : 31|8@0+      LE_56_8  : 56|8@1+
  auto db = parseString(R"(VERSION ""

BO_ 100 M: 8 ECU1
 SG_ BE_7_16 : 7|16@0+ (1,0) [0|0] "" ECU2
 SG_ BE_23_4 : 23|4@0+ (1,0) [0|0] "" ECU2
 SG_ BE_19_4 : 19|4@0+ (1,0) [0|0] "" ECU2
 SG_ BE_31_8 : 31|8@0+ (1,0) [0|0] "" ECU2
 SG_ BE_39_1 : 39|1@0+ (1,0) [0|0] "" ECU2
 SG_ BE_38_11 : 38|11@0+ (1,0) [0|0] "" ECU2
 SG_ BE_43_12 : 43|12@0+ (1,0) [0|0] "" ECU2
 SG_ LE_56_8 : 56|8@1+ (1,0) [0|0] "" ECU2
)");

  REQUIRE(db.messages.size() == 1);
  const auto& m = db.messages[0];
  REQUIRE(m.signals.size() == 8);

  struct Expected
  {
    const char* name;
    uint64_t values[3];
  };

  // payload 0: 01 02 03 04 05 06 07 08
  // payload 1: ff 00 ff 00 ff 00 ff 00
  // payload 2: 12 34 56 78 9a bc de f0
  static constexpr uint8_t payloads[3][8]
      = {{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08},
         {0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00},
         {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0}};

  static const Expected expected[] = {
      {"BE_7_16", {258, 65280, 4660}}, {"BE_23_4", {0, 15, 5}},
      {"BE_19_4", {3, 15, 6}},         {"BE_31_8", {4, 0, 120}},
      {"BE_39_1", {0, 1, 1}},          {"BE_38_11", {80, 2032, 427}},
      {"BE_43_12", {1543, 255, 3294}}, {"LE_56_8", {8, 0, 240}},
  };

  for(const auto& e : expected)
  {
    const auto* sig = m.findSignal(e.name);
    REQUIRE(sig != nullptr);
    for(int p = 0; p < 3; ++p)
    {
      INFO(e.name << " on payload " << p);
      REQUIRE(rawSignalBits(*sig, payloads[p], 8) == e.values[p]);
    }
  }
}

TEST_CASE("Line and block comments are stripped outside strings", "[can][dbc]")
{
  // Not in the Vector format, but they occur in the wild and two of the major
  // readers accept them. Crucially, "//" inside a quoted string is data.
  auto db = parseString(R"(VERSION "" // trailing comment

/* a block comment
   spanning lines */
BO_ 100 Msg: 8 Vector__XXX
 SG_ A : 0|8@1+ (1,0) [0|0] "u//nit" Vector__XXX

// a whole-line comment
CM_ BO_ 100 "text // not a comment";
)");

  REQUIRE(db.messages.size() == 1);
  REQUIRE(db.messages[0].name == "Msg");
  // The unit really does contain a double slash.
  REQUIRE(db.messages[0].findSignal("A")->unit == "u//nit");
  REQUIRE(db.messages[0].comment == "text // not a comment");
  REQUIRE(db.warnings.empty());
}

TEST_CASE("Latin-1 text is transcoded to UTF-8", "[can][dbc]")
{
  // CANdb++ writes cp1252/ISO-8859-1: a degree sign is the byte 0xB0, which is
  // not valid UTF-8 on its own and would render as nothing downstream.
  std::string src = "VERSION \"\"\n\nBO_ 100 Msg: 8 Vector__XXX\n SG_ A : 0|8@1+ (1,0) [0|0] \"";
  src += char(0xB0);
  src += "C\" Vector__XXX\n";

  auto db = parseString(src);
  REQUIRE(db.messages.size() == 1);

  // U+00B0 as UTF-8 is C2 B0.
  const auto& unit = db.messages[0].findSignal("A")->unit;
  REQUIRE(unit.size() == 3);
  REQUIRE(uint8_t(unit[0]) == 0xC2);
  REQUIRE(uint8_t(unit[1]) == 0xB0);
  REQUIRE(unit[2] == 'C');

  // Text that already is valid UTF-8 must survive untouched.
  std::string utf8 = "VERSION \"\"\n\nBO_ 100 Msg: 8 Vector__XXX\n SG_ A : 0|8@1+ (1,0) [0|0] \"\xC2\xB0" "C\" Vector__XXX\n";
  auto db2 = parseString(utf8);
  REQUIRE(db2.messages[0].findSignal("A")->unit == "\xC2\xB0" "C");
}

TEST_CASE("A signal running past the DLC is a warning, not a rejection", "[can][dbc]")
{
  // Real files do this; refusing them would lose whole databases. The bits
  // past the end of a received frame read as zero.
  auto db = parseString(R"(VERSION ""

BO_ 100 Msg: 2 Vector__XXX
 SG_ Fits : 0|8@1+ (1,0) [0|0] "" Vector__XXX
 SG_ Overruns : 8|32@1+ (1,0) [0|0] "" Vector__XXX
)");

  REQUIRE(db.messages.size() == 1);
  REQUIRE(db.messages[0].signals.size() == 2);

  bool mentioned = false;
  for(const auto& w : db.warnings)
    if(w.find("Overruns") != std::string::npos)
      mentioned = true;
  REQUIRE(mentioned);

  // And it still decodes what is there.
  const uint8_t frame[2] = {0xAA, 0xBB};
  REQUIRE(rawSignalBits(*db.messages[0].findSignal("Fits"), frame, 2) == 0xAA);
  REQUIRE(rawSignalBits(*db.messages[0].findSignal("Overruns"), frame, 2) == 0xBB);
}

TEST_CASE("Both VECTOR__INDEPENDENT_SIG_MSG identifiers are skipped", "[can][dbc]")
{
  // The pseudo-message appears with 0xC0000000 and with 0x40000000 in real
  // corpora, which is why it is matched by name rather than by id.
  for(const char* id : {"3221225472", "1073741824"})
  {
    std::string src = "VERSION \"\"\n\nBO_ ";
    src += id;
    src += " VECTOR__INDEPENDENT_SIG_MSG: 0 Vector__XXX\n"
           " SG_ Orphan : 0|16@1- (0.01,0) [0|0] \"\" Vector__XXX\n"
           "\nBO_ 100 Real: 8 Vector__XXX\n"
           " SG_ A : 0|8@1+ (1,0) [0|0] \"\" Vector__XXX\n";

    INFO("id " << id);
    auto db = parseString(src);
    REQUIRE(db.messages.size() == 1);
    REQUIRE(db.messages[0].name == "Real");
  }
}
