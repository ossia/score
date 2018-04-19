#pragma once
#include <QImage>
#include <QObject>
#include <score/plugins/customfactory/UuidKey.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

namespace score
{
class Plugin_QtInterface;
/**
 * @brief The Addon struct
 *
 * Metadata for score addons
 */
struct SCORE_LIB_BASE_EXPORT Addon
{
  score::Plugin_QtInterface* plugin{};
  UuidKey<Plugin> key; // Can be the same as plug-in's
  QString path;        // Path to the addon folder

  QString name;
  QString version;
  QString
      latestVersionAddress; // URL to a file containing the current version.

  QString shortDescription;
  QString longDescription;
  QImage smallImage;
  QImage largeImage;
  bool enabled = true;
  bool corePlugin = false; // For plug-ins shipped with score
};

/**
 * @brief addonArchitecture
 * @return Architecture for the system score is compiled for.
 */
SCORE_LIB_BASE_EXPORT
QString addonArchitecture();
}

/**
 * Addons are located on-disk in $DOCUMENTS/score/plugins
 * We have multiple files for the addon system.
 *
 * A local file that describes the on-disk, installed addon : localaddon.json.
 * It roughly maps to the Addon struct.
 * File paths are given relatively to localaddon.json's folder.
 *
 * A remote file that describes the metadata for the add-on browser.
 * Both have the same keys, however the remote file has URLs instead of local
 * paths,
 * and instead of pointing to dynamic libraries directly, points to compressed
 * add-on packages
 * which can be downloaded.
 *
 *
 */
