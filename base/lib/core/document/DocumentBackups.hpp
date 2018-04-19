#pragma once
#include <QByteArray>
#include <QString>
#include <score_lib_base_export.h>
#include <utility>
#include <vector>

namespace score
{

struct RestorableDocument
{
  QString filePath;
  QByteArray doc;
  QByteArray commands;
};

/**
 * @brief Manages the list of documents that can be restored after a crash.
 *
 * There are multiple files, all located in the system's temp folder :
 * * The OpenDocumentsFile which contains a list of the file corresponding
 * to each loaded document.
 * * For each document, the files referred to by DocumentBackupManager.
 *
 * Please note that currently, the backup system is not safe in case of power
 * outage.
 *
 * \todo implement periodic saves in an on-disk folder for this case.
 */
struct SCORE_LIB_BASE_EXPORT DocumentBackups
{
  // Check if there are available backup files.
  static bool canRestoreDocuments();

  // First is the data, second is the commands.
  static std::vector<RestorableDocument> restorableDocuments();

  // Removes all the on-disk files that contains document backups.
  static void clear();
};
}
