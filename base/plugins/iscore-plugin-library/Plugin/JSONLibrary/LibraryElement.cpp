#include "LibraryElement.hpp"


static const QMap<Category, QString> map{
    {Category::State, QObject::tr("State")},
    {Category::ScenarioData, QObject::tr("ScenarioData")},
    {Category::Process, QObject::tr("Process")},
    {Category::Device, QObject::tr("Device")},
    {Category::Address, QObject::tr("Address")}
};

const QMap<Category, QString>& categoryPrettyName()
{
    return map;
}
