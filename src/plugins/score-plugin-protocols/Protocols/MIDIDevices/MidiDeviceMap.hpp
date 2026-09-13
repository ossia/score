#pragma once

/**
 * @file The in-memory form of a `.midimap.json` device description: what each
 * control of one MIDI device sends, what it accepts back, and what the values
 * mean.
 *
 * The format is specified in the midi-device-maps repository, `spec/FORMAT.md`.
 * It describes both kinds of device with one structure, because the difference
 * between them is direction rather than shape: an instrument that responds to
 * CC and NRPN, and a controller whose knobs and pads send them.
 *
 * Free of Qt and ossia, like Protocols/CAN/DBCParser.hpp, so that it can be
 * tested without an application, a MIDI port or a device tree.
 */

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Protocols::MIDIDevices
{

enum class MessageType
{
  CC,
  CC14,
  NRPN,
  RPN,
  Note,
  PitchBend,
  Aftertouch,
  PolyAftertouch,
  Program
};

enum class Kind
{
  Fader,
  Knob,
  Encoder,
  Button,
  Pad,
  Key,
  Wheel,
  Ribbon,
  XY,
  Parameter,
  Other
};

enum class Direction
{
  //! Device to host: a knob the hardware sends.
  In,
  //! Host to device: an instrument parameter, or an LED.
  Out,
  Both
};

enum class ValueMode
{
  Absolute,
  Relative
};

/**
 * How an endless encoder encodes a delta. Reading one as another turns the knob
 * backwards or, read as absolute, produces 1 and 127 forever instead of a
 * delta; @see decodeRelative.
 */
enum class Encoding
{
  TwosComplement,
  SignedBit,
  //! SignedBit with the sign inverted: bit 6 set means increment.
  SignedBit2,
  BinaryOffset
};

//! The CC 0 / CC 32 pair a program change must be preceded by. Either half may
//! be absent; both absent means the device's currently selected bank.
struct Bank
{
  int msb{-1};
  int lsb{-1};

  bool empty() const noexcept { return msb < 0 && lsb < 0; }
  bool operator==(const Bank&) const noexcept = default;
};

struct Message
{
  MessageType type{MessageType::CC};

  /**
   * 1-16. Empty means "whichever channel the device is set to"; several means
   * the source states the same control on each of them, which an instrument's
   * name set routinely does ("every channel but 10").
   */
  std::vector<int> channels;

  //! CC number, note number, or the NRPN/RPN parameter MSB. -1 when the message
  //! addresses nothing, as pitch bend, aftertouch and program change do not.
  int number{-1};

  //! The second CC of a CC14, or the NRPN/RPN parameter LSB.
  int lsb{-1};

  //! `program` only.
  Bank bank;

  //! A note control covering several notes: the note becomes the value rather
  //! than a fixed address. -1 when the control addresses one note.
  int rangeFrom{-1};
  int rangeTo{-1};

  bool hasRange() const noexcept { return rangeFrom >= 0 && rangeTo >= rangeFrom; }
};

//! A named value range. `from == to` names a single value.
struct Label
{
  int from{};
  int to{};
  std::string name;
};

/**
 * What the numbers mean in the parameter's own terms, when that differs from
 * what is sent: a knob sends 0-127 while the parameter reads -64..+63, or 0 to
 * 100 %, or simply Left to Right.
 *
 * Never changes what goes on the wire -- Value::min and Value::max stay the
 * only values a host may send. This is for what it shows.
 */
struct Scale
{
  //! The logical range the wire range maps onto, linearly and in order, so
  //! `min` may exceed `max` for a parameter that counts backwards.
  std::optional<double> min;
  std::optional<double> max;

  //! The names of the two ends, for a scale the source names rather than
  //! numbers.
  std::string from;
  std::string to;

  //! Verbatim as the source wrote it: "%", "dB", "Hz", "semitones".
  std::string unit;

  bool empty() const noexcept
  {
    return !min && !max && from.empty() && to.empty();
  }
};

struct Value
{
  ValueMode mode{ValueMode::Absolute};

  //! Set whenever @ref mode is Relative; the format requires it there.
  std::optional<Encoding> encoding;

  //! Absent means the message type's natural range. @see naturalRange
  std::optional<int> min;
  std::optional<int> max;
  std::optional<int> def;

  //! The complete set of data values the control sends, when it sends fixed
  //! values rather than a range. Empty for a continuous control.
  std::vector<int> sends;

  //! The midpoint is neutral: pan, detune, pitch bend.
  bool bipolar{};

  //! Empty unless the source says what the numbers mean.
  Scale scale;

  std::vector<Label> labels;

  //! Buttons and pads: set when the source says whether the control sends a
  //! release. Unset means the source did not say, which is not "not a button".
  std::optional<bool> momentary;

  //! The rest position is the maximum, so the numbers run backwards. Recorded
  //! rather than corrected: @ref min and @ref max stay what is on the wire.
  bool inverted{};

  //! The control needs soft takeover: ignore it until it passes the current
  //! value.
  bool pickup{};
};

/**
 * A second use of a physical control the surface already has, reached by
 * holding a modifier.
 *
 * The two addresses are otherwise unrelated -- nothing in either message says
 * they are one piece of hardware -- so without this a host shows two knobs
 * where the player has one.
 */
struct Layer
{
  //! The modifier as printed on the panel: "Shift", "Shift+Hotcue".
  std::string name;

  //! The `group/name` path of the control it shares hardware with. Empty when
  //! the modifier selects a whole-surface mode rather than pairing two.
  std::string of;

  bool empty() const noexcept { return name.empty(); }
};

//! The control exists only while this program is selected.
struct Condition
{
  int program{};
  Bank bank;
};

struct Control
{
  std::string name;
  Kind kind{Kind::Other};

  //! Already split into levels, whichever form the file used.
  std::vector<std::string> group;

  Direction direction{Direction::In};

  Message message;

  //! A different message the host sends back to light an LED or move a
  //! motorised fader. Unset when it is identical to @ref message.
  std::optional<Message> feedback;

  Value value;

  //! Empty when the control is always present; several conditions mean the
  //! control exists while any one of them holds.
  std::vector<Condition> when;

  //! Empty unless the control is a modifier layer of another.
  Layer layer;

  std::string description;
};

//! What puts the device into the configuration the document describes.
struct Preset
{
  //! The name the user selects on the hardware.
  std::string name;

  /**
   * The messages that enter the mode, lowercase hex bytes. A byte written `??`
   * depends on the individual unit rather than the model -- a SysEx device ID
   * -- and has to be filled in before sending, so a document carrying one is
   * not sendable as it stands.
   */
  std::vector<std::string> sysex;

  bool empty() const noexcept { return name.empty() && sysex.empty(); }
};

//! Attribution and provenance. Carried so that a map can credit its source;
//! none of it affects the tree.
struct Source
{
  //! `ardour-midi-binding-map`, `midnam`, `mixxx-controller-mapping`, ...
  std::string format;

  //! An SPDX identifier, or `NOASSERTION` where no grant was ever made.
  std::string license;

  /**
   * Present on a NOASSERTION document that is published anyway:
   * `factual-documentation` records that the content states facts about
   * hardware rather than anyone's authorship.
   */
  std::string redistribution;

  //! Copyright and permission notices found in the source, verbatim.
  std::vector<std::string> notices;

  //! Everyone credited, as the source credited them.
  std::vector<std::string> authors;
};

struct DeviceMap
{
  std::string manufacturer;
  std::string model;
  std::string description;

  //! A reconfiguration the map depends on that is not a selectable preset.
  std::string requirement;

  Preset preset;

  //! Substrings to match against a live port's name, most specific first.
  std::vector<std::string> match;

  Source source;

  std::vector<Control> controls;

  //! What the reader could not use, and why.
  std::vector<std::string> warnings;

  //! "Arturia: MiniLab mkII", or the model alone when the manufacturer is not
  //! stated.
  std::string label() const;

  bool empty() const noexcept { return controls.empty(); }
};

//! The range a message type carries when the document states no bounds.
std::pair<int, int> naturalRange(MessageType t) noexcept;

//! @p v's bounds, resolved against the natural range of @p t. Meaningless for a
//! relative control, whose bounds belong to whatever it drives.
std::pair<int, int> resolvedRange(const Value& v, MessageType t) noexcept;

/**
 * One data byte of an endless encoder, as a signed delta.
 *
 * The four encodings disagree about which halves of 0-127 mean which sign, so
 * the same byte is +1 under one and -63 under another; there is nothing in the
 * message itself to tell them apart.
 */
int decodeRelative(int byte, Encoding e) noexcept;

/**
 * One step of an endless encoder as the data byte that carries it.
 *
 * The inverse of decodeRelative, and the only thing a relative control may put
 * on the wire: the absolute position exists only here, so sending it would be
 * read as a delta of that size.
 *
 * @p delta is clamped to what one byte of @p e can express.
 */
int encodeRelative(int delta, Encoding e) noexcept;

/**
 * A document's own fields, without its controls: what a library listing needs.
 *
 * Kept separate because the controls are the bulk of a file -- an instrument's
 * patch names run to hundreds of kilobytes -- and listing a library must not
 * read them.
 */
struct DeviceHeader
{
  std::string manufacturer;
  std::string model;
  std::string description;
  std::string requirement;
  Preset preset;
  std::vector<std::string> match;

  std::string label() const;
};

/**
 * Read only the header, stopping at the first control.
 *
 * @p text may therefore be a prefix of the file rather than the whole of it.
 * std::nullopt when the document ends before its header does, which is how a
 * caller learns that its prefix was too short.
 */
std::optional<DeviceHeader> parseDeviceMapHeader(std::string_view text);

/**
 * Read a `.midimap.json`. std::nullopt when @p text is not a document of this
 * format at all -- unparseable, or a `format` field naming something else.
 *
 * A document that parses keeps every control the reader understood; a control
 * it could not use is dropped and described in DeviceMap::warnings, so a single
 * malformed entry does not cost the file.
 */
std::optional<DeviceMap> parseDeviceMap(std::string_view text);

std::string_view toString(Kind) noexcept;
std::string_view toString(MessageType) noexcept;

}
