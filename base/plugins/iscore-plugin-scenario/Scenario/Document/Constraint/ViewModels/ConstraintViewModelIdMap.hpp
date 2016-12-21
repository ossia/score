#pragma once
#include <QHash>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/Identifier.hpp>
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
