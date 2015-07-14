#pragma once
#include<QHash>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
class ConstraintViewModel;
using ConstraintViewModelIdMap = QHash<ObjectPath, id_type<ConstraintViewModel>>;
using SerializedConstraintViewModels = QVector<QPair<ObjectPath, QPair<QString, QByteArray>>>;
