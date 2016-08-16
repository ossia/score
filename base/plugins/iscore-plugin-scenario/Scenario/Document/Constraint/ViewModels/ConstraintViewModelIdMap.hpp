#pragma once
#include<QHash>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/ModelPath.hpp>

namespace Scenario
{
class ConstraintViewModel;
class AbstractScenarioLayer;
using ConstraintViewModelIdMap = QHash<Path<AbstractScenarioLayer>, Id<ConstraintViewModel>>;
using SerializedConstraintViewModels = QVector<QPair<Path<AbstractScenarioLayer>, QPair<QString, QByteArray>>>;
}
