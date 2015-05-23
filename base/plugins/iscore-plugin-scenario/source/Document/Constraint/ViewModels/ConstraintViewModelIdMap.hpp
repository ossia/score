#pragma once
#include<QHash>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
class AbstractConstraintViewModel;
using ConstraintViewModelIdMap = QHash<ObjectPath, id_type<AbstractConstraintViewModel>>;
using SerializedConstraintViewModels = QVector<QPair<ObjectPath, QPair<QString, QByteArray>>>;
