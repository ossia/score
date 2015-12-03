#pragma once
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <QString>

#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <memory>

class ClientSession;
class ClientSessionBuilder;
namespace iscore {

class Document;
class MenubarManager;
class Presenter;
}  // namespace iscore
struct VisitorVariant;

namespace iscore
{
}

#ifdef USE_ZEROCONF
class ZeroconfBrowser;
#endif

class NetworkApplicationPlugin : public QObject, public iscore::GUIApplicationContextPlugin
{
        Q_OBJECT

    public:
        NetworkApplicationPlugin(const iscore::ApplicationContext& app);
        void populateMenus(iscore::MenubarManager*) override;

    public slots:
        void setupClientConnection(QString ip, int port);

    private:
        virtual iscore::DocumentPluginModel* loadDocumentPlugin(
                const QString& name,
                const VisitorVariant& var,
                iscore::Document* parent) override;

        std::unique_ptr<ClientSessionBuilder> m_sessionBuilder;

#ifdef USE_ZEROCONF
        ZeroconfBrowser* m_zeroconfBrowser{};
#endif
};
