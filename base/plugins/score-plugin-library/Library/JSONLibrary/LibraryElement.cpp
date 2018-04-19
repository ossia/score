// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LibraryElement.hpp"

#include <QObject>
namespace Library
{
static const QMap<Category, QString> map{
    {Category::StateNode, QObject::tr("State")},
    {Category::MessageList, QObject::tr("MessageList")},
    {Category::ScenarioData, QObject::tr("ScenarioData")},
    {Category::Process, QObject::tr("Process")},
    {Category::Device, QObject::tr("Device")},
    {Category::Address, QObject::tr("Address")}};

const QMap<Category, QString>& categoryPrettyName()
{
  return map;
}
}
