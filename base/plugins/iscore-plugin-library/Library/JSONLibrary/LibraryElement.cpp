#include "LibraryElement.hpp"
#include <QObject>

static const QMap<Category, QString> map{
    {Category::StateNode, QObject::tr("State")},
    {Category::MessageList, QObject::tr("MessageList")},
    {Category::ScenarioData, QObject::tr("ScenarioData")},
    {Category::Process, QObject::tr("Process")},
    {Category::Device, QObject::tr("Device")},
    {Category::Address, QObject::tr("Address")}
};

const QMap<Category, QString>& categoryPrettyName()
{
    return map;
}
