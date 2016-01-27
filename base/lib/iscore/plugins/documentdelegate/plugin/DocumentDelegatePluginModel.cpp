#include "DocumentDelegatePluginModel.hpp"
#include <iscore/tools/NamedObject.hpp>

class QObject;
namespace iscore {
class Document;

DocumentPlugin::DocumentPlugin(
        iscore::Document& ctx,
        const QString& name,
        QObject* parent):
    NamedObject{name, parent},
    m_context{ctx}
{

}

DocumentPlugin::~DocumentPlugin()
{

}

SerializableDocumentPlugin::~SerializableDocumentPlugin()
{

}

DocumentPluginFactory::~DocumentPluginFactory()
{

}

}

