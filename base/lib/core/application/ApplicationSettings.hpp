#pragma once
#include <QStringList>
#include <iscore_lib_base_export.h>
namespace iscore
{
struct ISCORE_LIB_BASE_EXPORT ApplicationSettings
{
        bool tryToRestore = true;
        bool gui = true;
        bool autoplay = false;
        QStringList loadList;

        void parse();
};
}
