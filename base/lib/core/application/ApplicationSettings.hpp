#pragma once
#include <QStringList>
namespace iscore
{
struct ApplicationSettings
{
        bool tryToRestore = true;
        bool gui = true;
        bool autoplay = false;
        QStringList loadList;

        void parse();
};
}
