#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <QByteArray>
#include <QFile>
#include <QFlags>
#include <QIODevice>
#include <QStringList>
#include <stdlib.h>

#include "RandomNameProvider.hpp"

struct WordList :
        public QStringList
{
        WordList()
        {
            QFile f(":/dict.txt");
            if(f.open(QFile::Text | QFile::ReadOnly))
            {
                QString list = f.readAll();
                static_cast<QStringList&>(*this) = list.split("\n");
            }
            else
            {
                append("some");
                append("basic");
                append("words");
                append("lambda");
                append("default");
            }
        }
};

QString RandomNameProvider::generateRandomName()
{
    static WordList words;

    return
            words.at(std::abs(iscore::random_id_generator::getRandomId() % (words.size() - 1))) +
            QString::number(std::abs(iscore::random_id_generator::getRandomId() % 99)) +
            words.at(std::abs(iscore::random_id_generator::getRandomId() % (words.size() - 1))) +
            QString::number(std::abs(iscore::random_id_generator::getRandomId() % 99));
}
