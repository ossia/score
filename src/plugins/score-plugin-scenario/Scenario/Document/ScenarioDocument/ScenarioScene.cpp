// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioScene.hpp"

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::ScenarioScene)
namespace Scenario
{
ScenarioScene::ScenarioScene(QObject* parent) : QGraphicsScene{parent}
{
  setItemIndexMethod(QGraphicsScene::NoIndex);
}
}
