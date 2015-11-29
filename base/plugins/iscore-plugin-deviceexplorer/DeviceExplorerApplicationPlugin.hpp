#pragma once
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <qstring.h>

#include "iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp"

namespace iscore {
class Application;
class Document;
}  // namespace iscore
struct VisitorVariant;

class DeviceExplorerApplicationPlugin final : public iscore::GUIApplicationContextPlugin
{
    public:
        DeviceExplorerApplicationPlugin(iscore::Application& app);

        virtual iscore::DocumentDelegatePluginModel* loadDocumentPlugin(
                const QString& name,
                const VisitorVariant& var,
                iscore::Document *parent) override;

    protected:
        void on_newDocument(iscore::Document* doc) override;
        void on_documentChanged(
                iscore::Document* olddoc,
                iscore::Document* newdoc) override;
};
