#pragma once

/* Reading and writing SpatGRIS speaker-setup files.
 *
 * Three variants exist in the wild, all plain XML (the v4 one is a serialised
 * juce::ValueTree, which is ordinary XML once written out), so Qt's
 * QXmlStreamReader covers all of them and no JUCE is involved:
 *
 *   1. legacy     <SPEAKER_SETUP VERSION="3.x"> + <SPEAKER_n> + <POSITION X Y Z>
 *                 (this is what the Satosphere setup uses)
 *   2. intermediate <SpeakerSetup Name Dimension> + <Ring> + <Speaker PositionX .../>
 *   3. current v4 <SPEAKER_SETUP SPEAKER_SETUP_VERSION> + <SPEAKER_GROUP> + <SPEAKER>
 *
 * Writing always emits v4, which is what current SpatGRIS reads; re-emitting a
 * legacy file would silently drop group information.
 */

#include <Gris/Algo/SpeakerSetup.hpp>

#include <QString>

#include <optional>

namespace Gris
{
enum class SpeakerSetupFormat
{
  unknown,
  legacy,       //! <SPEAKER_SETUP VERSION="3.x">, flat SPEAKER_n list
  intermediate, //! <SpeakerSetup Name Dimension>, <Ring>/<Speaker>
  valueTree     //! current v4, SPEAKER_GROUP nesting
};

struct SpeakerSetupReadResult
{
  std::optional<SpeakerSetup> setup{};
  SpeakerSetupFormat format{SpeakerSetupFormat::unknown};
  /** Empty when the read succeeded. */
  QString error{};

  [[nodiscard]] explicit operator bool() const noexcept { return setup.has_value(); }
};

/** Reads any of the three variants from XML held in memory. */
[[nodiscard]] SpeakerSetupReadResult readSpeakerSetup(QByteArray const& xml);

/** Reads any of the three variants from a file. */
[[nodiscard]] SpeakerSetupReadResult readSpeakerSetupFile(QString const& path);

/** Serialises to the current (v4) format. */
[[nodiscard]] QByteArray writeSpeakerSetup(SpeakerSetup const& setup);

/** Serialises to the current (v4) format and writes it to disk.
 *  @return an error message, empty on success. */
[[nodiscard]] QString writeSpeakerSetupFile(SpeakerSetup const& setup, QString const& path);

} // namespace Gris
