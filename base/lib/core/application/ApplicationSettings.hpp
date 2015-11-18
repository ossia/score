#pragma once
#include <QStringList>
struct ApplicationSettings
{
        bool tryToRestore = true;
        bool gui = true;
        bool autoplay = false;
        QStringList loadList;

        void parse();
};
