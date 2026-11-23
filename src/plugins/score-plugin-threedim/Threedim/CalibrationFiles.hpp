#pragma once
#include <Gfx/Graph/FloatImageIO.hpp>
#include <Gfx/Graph/PhotometricMaps.hpp>
#include <Threedim/CameraLens.hpp>

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Threedim
{
//! A projector calibration `name`: `name.json` and the files its "files" list
//! names, as the multi-window outputs read them (Gfx::calibrationListing): a
//! listed file is used, an unlisted one never is; with no list, any
//! `name.<suffix>` on disk is.
//!   name.xyz.pfm + name.valid.pfm  XYZ map, w = valid
//!   name.lum.pfm, name.black.pfm   white - black and black, RGB
//! The camera a calibration was measured with: name.json's "camera".
struct CalibrationCamera
{
  std::array<float, 4> intrinsics{};
  int width{}, height{};
  CameraLens lens;
};

struct CalibrationFile
{
  //! `dir/name`.
  QString base;
  int width{}, height{};
  //! RGBA, rows top to bottom.
  std::vector<float> xyz;
  std::array<float, 4> intrinsics{};
  std::array<float, 16> pose{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  bool hasPose{};
  bool viewerMode{};
  //! RGB, empty when not measured.
  std::vector<float> lum, black;
  std::optional<CalibrationCamera> camera;
  //! n.xyz, d with n . p + d = 0.
  std::vector<std::array<float, 4>> planes;
  //! Its index in its set; -1 when name.json does not say.
  int projectorIndex{-1};
  //! An emissive screen (LED wall, monitor), not a projector: "display" in
  //! name.json. Projector Blend Maps leaves screens out.
  bool screen{};
  //! name.json as read: the keys of the fields above and all the others.
  QJsonObject json;
  QString error;

  bool ok() const noexcept
  {
    return width > 0 && height > 0 && xyz.size() == std::size_t(width) * height * 4;
  }
  bool hasModel() const noexcept
  {
    return hasPose && intrinsics[0] > 0.f && intrinsics[1] > 0.f;
  }
};

//! A map of a calibration: `base + suffix`. A 4-channel image with the suffix
//! ".xyz.pfm" is written as name.xyz.pfm + name.valid.pfm.
struct CalibrationMap
{
  QString suffix;
  score::gfx::FloatImage image;
};

//! One write of a calibration: its maps first, each replaced atomically, then
//! name.json, replaced atomically, listing them.
struct CalibrationWrite
{
  QString base;
  std::vector<CalibrationMap> maps;
  //! Set over the keys of the existing name.json, which are kept otherwise.
  QJsonObject fields;
  //! Keys of the existing name.json this write makes stale.
  QStringList removeKeys;
  //! Other files, each replaced atomically, after the maps and before name.json.
  std::vector<score::gfx::EncodedFile> files;
  //! Suffixes of the files the existing name.json lists that stay listed when
  //! this write does not replace them; the others are no longer listed.
  QStringList keepListed;
  //! Refuse when name.json exists.
  bool createOnly{};
};

//! Writes one map, replaced atomically.
bool writeCalibrationMap(const QString& base, const CalibrationMap& m, QString* error = nullptr);

//! False and `error` set when nothing was written: the maps or name.json could
//! not be written, or name.json exists and either `createOnly` is set or it
//! describes a projector of another size than the maps.
bool writeCalibration(const CalibrationWrite& w, QString* error = nullptr);

//! The file of `base` for `suffix` its last run wrote: the listed one, or
//! `base + suffix` when name.json lists nothing and it exists; empty otherwise.
QString calibrationMapFile(const QString& base, const QString& suffix);

//! A calibration from any of its files, or its name.
CalibrationFile loadCalibration(const QString& path);

//! The write of `c`: its fields (size, intrinsics, pose, mode, camera, planes,
//! projector index) over `c.json`, its xyz map and the lum / black maps it
//! has; the photometric maps the existing name.json lists stay listed.
CalibrationWrite calibrationWrite(const CalibrationFile& c);
bool saveCalibration(const CalibrationFile& c, QString* error = nullptr);

//! Adds files already written next to the calibration `base` to its
//! name.json's "files", replaced atomically; false, with `error`, when
//! name.json is missing or not a JSON object.
bool listCalibrationFiles(const QString& base, const QStringList& names, QString* error = nullptr);
//! Takes files out of name.json's "files" and deletes them: products a
//! later computation no longer makes. True when there was none to forget.
bool forgetCalibrationFiles(
    const QString& base, const QStringList& names, QString* error = nullptr);

//! Replaces `path` atomically.
bool writeJsonFile(const QString& path, const QJsonObject& o, QString* error = nullptr);
//! Empty when missing or not a JSON object.
QJsonObject readJsonFile(const QString& path);
//! nullopt, with `error`, when missing or not a JSON object.
std::optional<QJsonObject> parseJsonFile(const QString& path, QString* error = nullptr);

//! `base` with `tag` before its projector index, so that it stays in a set
//! (Gfx::calibrationSetBase): room_p2 + "_reg" -> room_reg_p2.
QString calibrationSetRenamed(const QString& base, const QString& tag);

//! Splits a list of paths on new lines, ';' or '|'.
std::vector<std::string> splitPaths(std::string_view list);

//! The finite numbers of a list split on ',', ';' or white space; nullopt
//! when an entry is not one.
std::optional<std::vector<float>> parseNumbers(std::string_view list);
}
