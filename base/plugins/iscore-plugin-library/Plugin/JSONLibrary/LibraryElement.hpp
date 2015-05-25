#pragma once
#include <QJsonObject>
#include <QStringList>

struct LibraryElement
{
        QString name;
        QStringList tags;
        QJsonObject obj;
};
