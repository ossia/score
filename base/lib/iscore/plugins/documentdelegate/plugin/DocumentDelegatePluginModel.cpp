#include "DocumentDelegatePluginModel.hpp"

iscore::DocumentDelegatePluginModel::DocumentDelegatePluginModel(
        const iscore::DocumentContext& ctx,
        const QString& name,
        QObject* parent):
    NamedObject{name, parent},
    m_context{ctx}
{

}

iscore::DocumentDelegatePluginModel::~DocumentDelegatePluginModel()
{

}
