#pragma once
#include "iscore/serialization/VisitorInterface.hpp"

class QObject;

namespace iscore{
class ElementPluginModel;
}
template<typename T>
iscore::ElementPluginModel* deserializeElementPluginModel(
        Deserializer<T>& deserializer,
        const QObject* element,
        QObject* parent);
