#include "DocumentDelegatePluginModel.hpp"
#include <iscore/tools/NamedObject.hpp>

class QObject;
namespace iscore {
class Document;
}  // namespace iscore

iscore::DocumentPluginModel::DocumentPluginModel(
        iscore::Document& ctx,
        const QString& name,
        QObject* parent):
    NamedObject{name, parent},
    m_context{ctx}
{

}

iscore::DocumentPluginModel::~DocumentPluginModel()
{

}
