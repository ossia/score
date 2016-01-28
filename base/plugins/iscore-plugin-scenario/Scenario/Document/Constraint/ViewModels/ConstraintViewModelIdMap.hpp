#pragma once
#include<QHash>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/ModelPath.hpp>

namespace Scenario
{
class ConstraintViewModel;
class AbstractScenarioLayerModel;
using ConstraintViewModelIdMap = QHash<Path<AbstractScenarioLayerModel>, Id<ConstraintViewModel>>;
using SerializedConstraintViewModels = QVector<QPair<Path<AbstractScenarioLayerModel>, QPair<QString, QByteArray>>>;
}
