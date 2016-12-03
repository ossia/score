#pragma once
#include <QHash>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
namespace Process
{
class LayerModel;
}
namespace Scenario
{
class ConstraintViewModel;
class AbstractScenarioLayer;
using ConstraintViewModelIdMap
    = QHash<Path<Process::LayerModel>, Id<ConstraintViewModel>>;
using SerializedConstraintViewModels
    = QVector<QPair<Path<Process::LayerModel>, QPair<QString, QByteArray>>>;
}
