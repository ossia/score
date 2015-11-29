#pragma once
#include <qstringlist.h>

struct ApplicationSettings
{
        bool tryToRestore = true;
        bool gui = true;
        bool autoplay = false;
        QStringList loadList;

        void parse();
};
