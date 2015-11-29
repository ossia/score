#pragma once

#include <qstring.h>

namespace iscore
{
/**
 * @brief The OpenDocumentsFile struct
 *
 * This file contains the list of the documents backup files
 * that are currently open,
 * and will be used to reload them in case of crash.
 */
struct OpenDocumentsFile
{
        static bool exists();
        static QString path();
};

}
