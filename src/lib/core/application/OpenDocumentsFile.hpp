#pragma once

#include <QString>
#include <score_lib_base_export.h>
namespace score
{
/**
 * @brief File used for crash restore
 *
 * This file contains the list of the documents backup files
 * that are currently open,
 * and will be used to reload them in case of crash.
 */
struct SCORE_LIB_BASE_EXPORT OpenDocumentsFile
{
  static bool exists();
  static QString path();
};
}
