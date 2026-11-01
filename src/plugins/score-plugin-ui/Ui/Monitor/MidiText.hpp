#pragma once
#include <libremidi/message.hpp>

#include <QString>

#include <cstdint>

/**
 * MIDI messages as the monitoring views name them: note names, what a
 * status byte is, and one line of text per message.
 */
namespace Ui::Monitor
{
//! The name of a MIDI note, middle C (60) being C4; "?" out of 0 to 127.
inline QString midiNoteName(int pitch)
{
  static constexpr const char* names[12]
      = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  if(pitch < 0 || pitch > 127)
    return QStringLiteral("?");
  return QString::fromLatin1(names[pitch % 12]) + QString::number(pitch / 12 - 1);
}

/**
 * What a message is. A note-on of velocity 0 is a note-off, as the MIDI
 * specification has it. A status byte below 0x80, or a channel message with a
 * data byte of 0x80 or more, is malformed: INVALID.
 */
constexpr libremidi::message_type
midiMessageType(uint8_t status, uint8_t d1, uint8_t d2) noexcept
{
  using T = libremidi::message_type;
  if(status < 0x80)
    return T::INVALID;
  if(status >= 0xF0)
    return T(status);
  if(d1 >= 0x80 || d2 >= 0x80)
    return T::INVALID;
  const auto t = T(status & 0xF0);
  if(t == T::NOTE_ON && d2 == 0)
    return T::NOTE_OFF;
  return t;
}

//! Whether the message belongs to a channel, as opposed to the system ones.
constexpr bool isChannelMessage(libremidi::message_type t) noexcept
{
  return uint8_t(t) >= 0x80 && uint8_t(t) < 0xF0;
}

/**
 * One line describing a message. `aligned` pads the fields into columns, for
 * a list in a monospace font; otherwise the line is as short as it gets.
 * `size` is the size of the whole message, which only matters for a SysEx.
 */
inline QString
describeMidi(uint8_t status, uint8_t d1, uint8_t d2, uint32_t size, bool aligned)
{
  using T = libremidi::message_type;
  const auto type = midiMessageType(status, d1, d2);
  const QString sep = aligned ? QStringLiteral("  ") : QStringLiteral(" ");
  const int vw = aligned ? 3 : 0;
  // Explicitly QString: an operator+ chain is a QStringBuilder, which would
  // refer to the lambda's locals once returned
  const auto line = [&](const char* kind, const QString& args) -> QString {
    QString out;
    if(isChannelMessage(type))
      out = QStringLiteral("ch%1").arg((status & 0x0F) + 1, aligned ? 2 : 0) + sep;
    else if(aligned)
      out = QStringLiteral("    ") + sep;
    QString k = QString::fromLatin1(kind);
    if(aligned)
      k = k.leftJustified(10);
    out += k;
    out += sep;
    out += args;
    return out;
  };
  const auto note = [&]() -> QString {
    return QStringLiteral("%1 (%2)").arg(int(d1), vw).arg(midiNoteName(d1));
  };

  switch(type)
  {
    case T::NOTE_ON:
      return line("Note On", note() + QStringLiteral(" vel %1").arg(int(d2)));
    case T::NOTE_OFF:
      return line("Note Off", note() + QStringLiteral(" vel %1").arg(int(d2)));
    case T::POLY_PRESSURE:
      return line("Poly Aft.", note() + QStringLiteral(" -> %1").arg(int(d2)));
    case T::CONTROL_CHANGE:
      return line("CC", QStringLiteral("%1 -> %2").arg(int(d1), vw).arg(int(d2)));
    case T::PROGRAM_CHANGE:
      return line("Program", QString::number(int(d1)).rightJustified(vw));
    case T::AFTERTOUCH:
      return line("Aftertouch", QString::number(int(d1)).rightJustified(vw));
    case T::PITCH_BEND:
      return line("Pitch Bend", QString::number(int(d1) + (int(d2) << 7) - 8192));
    case T::SYSTEM_EXCLUSIVE:
      return line("SysEx", QStringLiteral("%1 bytes").arg(size));
    case T::TIME_CODE:
      return line("Time code", QString::number(int(d1)));
    case T::SONG_POS_POINTER:
      return line("Song pos.", QString::number(int(d1) + (int(d2) << 7)));
    case T::SONG_SELECT:
      return line("Song", QString::number(int(d1)));
    case T::TUNE_REQUEST:
      return line("Tune req.", {});
    case T::TIME_CLOCK:
      return line("Clock", {});
    case T::START:
      return line("Start", {});
    case T::CONTINUE:
      return line("Continue", {});
    case T::STOP:
      return line("Stop", {});
    case T::ACTIVE_SENSING:
      return line("Sensing", {});
    case T::SYSTEM_RESET:
      return line("Reset", {});
    case T::INVALID:
      return line(
          "Invalid", QStringLiteral("%1 %2 %3")
                         .arg(int(status), 2, 16, QLatin1Char('0'))
                         .arg(int(d1), 2, 16, QLatin1Char('0'))
                         .arg(int(d2), 2, 16, QLatin1Char('0')));
    default:
      return line(
          "System", QStringLiteral("%1 %2 %3")
                        .arg(int(status), 2, 16, QLatin1Char('0'))
                        .arg(int(d1), 3)
                        .arg(int(d2), 3));
  }
}

//! The bytes of a message in hexadecimal, or its size for a SysEx.
inline QString midiBytesText(uint8_t status, uint8_t d1, uint8_t d2, uint32_t size)
{
  if(status == 0xF0)
    return QStringLiteral("SysEx %1 bytes").arg(size);
  return QStringLiteral("%1 %2 %3")
      .arg(int(status), 2, 16, QLatin1Char('0'))
      .arg(int(d1), 2, 16, QLatin1Char('0'))
      .arg(int(d2), 2, 16, QLatin1Char('0'))
      .toUpper();
}
}
