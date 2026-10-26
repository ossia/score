#pragma once
#include <QString>
#include <QStringList>
#include <QUuid>

#include <score_plugin_library_export.h>

#include <vector>

namespace Library
{
//! A package folder in {RootPath}/packages or {RootPath}/support, as the
//! package.json the package manager writes at install time describes it.
struct InstalledPackage
{
  QString path; //!< Absolute path of the package folder
  QUuid key;
  int version{};
};

//! The package folders directly under @p root which have a package.json with a
//! key. Hidden folders (an install in progress) are not packages yet.
SCORE_PLUGIN_LIBRARY_EXPORT
std::vector<InstalledPackage> installedPackages(const QString& root);

/**
 * @brief Removes a package folder.
 *
 * When it cannot be removed now, typically because a library in it is loaded in
 * this process on Windows, the folder is marked instead: it is then neither
 * loaded nor listed, and is removed at the next start.
 *
 * @return true if the folder is gone now
 */
SCORE_PLUGIN_LIBRARY_EXPORT
bool removePackageFolder(const QString& path);

//! Whether removePackageFolder could only mark @p path for removal.
SCORE_PLUGIN_LIBRARY_EXPORT
bool isMarkedForRemoval(const QString& path);

//! Removes the folders under @p root which removePackageFolder marked.
SCORE_PLUGIN_LIBRARY_EXPORT
void removeMarkedPackageFolders(const QString& root);

/**
 * @brief The support package folders whose libraries are to be loaded.
 *
 * Every folder in @p supportDir, except those marked for removal and, among
 * folders holding the same package (the same key), all but the newest version.
 * Two versions of a package typically ship libraries under the same names
 * (cudnn64_9.dll, libcudnn.so.9...), and which one a process would get is then
 * up to the loader's search order.
 */
SCORE_PLUGIN_LIBRARY_EXPORT
QStringList supportFoldersToLoad(const QString& supportDir);

/**
 * @brief Makes the native libraries of the given support package folders
 * loadable by the addons which dlopen / LoadLibrary them by bare name.
 *
 * Libraries which depend on each other across folders (a runtime and the
 * libraries built on it) must be passed in the same call.
 */
SCORE_PLUGIN_LIBRARY_EXPORT
void loadSupportLibraries(const QStringList& folders);
}
