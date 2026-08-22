#pragma once
#include <Media/Sound/BW64/AdmData.hpp>

#include <QString>

#include <optional>

namespace Media::BW64
{

/**
 * @brief Reader for BW64 files with ADM (Audio Definition Model) metadata
 *
 * BW64 is an extended Broadcast Wave Format that can contain ADM metadata
 * for object-based 3D audio. The ADM data includes:
 * - Audio objects with their associated audio channels
 * - Position automation (azimuth/elevation/distance or x/y/z over time)
 * - Gain and other parameter automation
 */
class Bw64AdmReader
{
public:
  /**
   * @brief Check if a file is a BW64/WAV file with ADM metadata
   * @param path Path to the audio file
   * @return true if the file contains axml and chna chunks
   */
  static bool isBw64WithAdm(const QString& path);

  /**
   * @brief Parse a BW64 file and extract ADM automation data
   * @param path Path to the BW64 file
   * @return Parsed ADM data, or nullopt if parsing failed
   */
  static std::optional<Bw64AdmData> parse(const QString& path);
};

}
