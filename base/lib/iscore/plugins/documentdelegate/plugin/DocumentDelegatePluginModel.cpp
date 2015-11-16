#include "DocumentDelegatePluginModel.hpp"
#include <core/document/Document.hpp>
iscore::DocumentDelegatePluginModel::DocumentDelegatePluginModel(
        iscore::Document& ctx,
        const QString& name,
        QObject* parent):
    NamedObject{name, parent},
    m_context{ctx}
{

}

iscore::DocumentDelegatePluginModel::~DocumentDelegatePluginModel()
{

}
