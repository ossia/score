#pragma once
#include <QJsonObject>
#include <QStringList>
#include <QMap>


enum Category
{
    State, ScenarioData, Process, Device, Address
};
const QMap<Category, QString>& categoryPrettyName();


struct LibraryElement
{
        QString name;
        Category category;
        QStringList tags;
        QJsonObject obj;
};
