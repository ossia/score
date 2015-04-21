#pragma once
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
namespace iscore{
class ElementPluginModel;
}
template<typename T>
iscore::ElementPluginModel* deserializeElementPluginModel(
        Deserializer<T>& deserializer,
        const QString& elementName,
        QObject* parent);
