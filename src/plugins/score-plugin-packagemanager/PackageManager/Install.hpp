#pragma once
#include <QString>
#include <QStringList>
#include <QUuid>

#include <score_plugin_packagemanager_export.h>

#include <vector>

namespace PM
{
/**
 * @brief The single folder all the extracted files live in, if there is one.
 *
 * Archives are most often wrapped in one folder named after the repository
 * they are built from; packages must be installed without it.
 *
 * @param extractedIn folder the archive has been extracted in
 * @param extracted the paths the extraction wrote
 * @return the wrapping folder's name, empty if the archive has no single root
 */
SCORE_PLUGIN_PACKAGEMANAGER_EXPORT
QString
archiveRootFolder(const QString& extractedIn, const std::vector<QString>& extracted);

/**
 * @brief Put a freshly extracted archive at its final place.
 *
 * @p destination ends up with the content of the wrapping folder if the
 * archive has one, and with the whole of @p extractedIn otherwise. A previous
 * install is only removed once the new one is in place; if anything fails,
 * both @p destination and @p extractedIn are left as they were.
 */
SCORE_PLUGIN_PACKAGEMANAGER_EXPORT
bool moveExtractedPackage(
    const QString& extractedIn, const std::vector<QString>& extracted,
    const QString& destination, QString& error);

/**
 * @brief Removes the other installs of a package.
 *
 * A new version of a package can be installed under another folder name than
 * the previous one; both would then be on disk, and for support packages both
 * on the library search path. Every folder directly under one of @p roots whose
 * package.json has @p key is removed, except @p keep. Folders which cannot be
 * removed now are removed at the next start.
 *
 * @return how many other installs there were
 */
SCORE_PLUGIN_PACKAGEMANAGER_EXPORT
int removeOtherInstalls(const QUuid& key, const QString& keep, const QStringList& roots);
}
