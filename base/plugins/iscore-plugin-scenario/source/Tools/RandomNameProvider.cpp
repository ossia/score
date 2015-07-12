#include "RandomNameProvider.hpp"
#include <QFile>
#include <QVector>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
// Do something better... one day
static bool vec_init = false;

static QStringList words = QStringList();
static void initWordList()
{
    words.clear();
    QFile f(":/dict.txt");
    f.open(QFile::Text | QFile::ReadOnly);
    QString list = f.readAll();
    words = list.split("\n");

    vec_init = true;
}

QString RandomNameProvider::generateRandomName()
{
    if(!vec_init)
        initWordList();

    return
            words.at(std::abs(getNextId() % (words.size() - 1))) +
            QString::number(std::abs(getNextId() % 99)) +
            words.at(std::abs(getNextId() % (words.size() - 1))) +
            QString::number(std::abs(getNextId() % 99));
}
