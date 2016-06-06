#pragma once
#include <QObject>
#include <QImage>
#include <iscore/plugins/customfactory/UuidKey.hpp>
namespace iscore
{
class Plugin_QtInterface;
/**
 * @brief The Addon struct
 *
 * Metadata for i-score addons
 */

struct ISCORE_LIB_BASE_EXPORT Addon
{
        iscore::Plugin_QtInterface* plugin{};
        UuidKey<Addon> key; // Can be the same as plug-in's
        QString path; // Path to the addon folder

        QString name;
        QString version;
        QString latestVersionAddress; // URL to a file containing the current version.

        QString shortDescription;
        QString longDescription;
        QImage smallImage;
        QImage largeImage;
        bool enabled = true;
        bool corePlugin = false; // For plug-ins shipped with i-score
};

/**
 * @brief addonArchitecture
 * @return Architecture for the system i-score is compiled for.
 */
ISCORE_LIB_BASE_EXPORT
QString addonArchitecture();
}

/**
 * Addons are located on-disk in $DOCUMENTS/i-score/plugins
 * We have multiple files for the addon system.
 *
 * A local file that describes the on-disk, installed addon : localaddon.json.
 * It roughly maps to the Addon struct.
 * File paths are given relatively to localaddon.json's folder.
 *
 * A remote file that describes the metadata for the add-on browser.
 * Both have the same keys, however the remote file has URLs instead of local paths,
 * and instead of pointing to dynamic libraries directly, points to compressed add-on packages
 * which can be downloaded.
 *
 *
 */
