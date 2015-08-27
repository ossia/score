#pragma once
#include<QHash>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

class ConstraintViewModel;
class AbstractScenarioLayerModel;
using ConstraintViewModelIdMap = QHash<ModelPath<AbstractScenarioLayerModel>, id_type<ConstraintViewModel>>;
using SerializedConstraintViewModels = QVector<QPair<ModelPath<AbstractScenarioLayerModel>, QPair<QString, QByteArray>>>;
