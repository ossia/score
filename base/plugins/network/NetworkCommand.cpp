#include "NetworkCommand.hpp"
#include <core/presenter/Presenter.hpp>
#include <core/document/DocumentPresenter.hpp>
#include <core/document/DocumentModel.hpp>

#include "NetworkDocumentPlugin.hpp"
#include "Serialization/NetworkSerialization.hpp"
#include <QAction>


using namespace iscore;

NetworkControl::NetworkControl() :
    PluginControlInterface {"NetworkCommand", nullptr}
{
}

void NetworkControl::populateMenus(MenubarManager* menu)
{
    QAction* joinSession = new QAction {tr("Join"), this};
    connect(joinSession, &QAction::triggered,
            [&] () {
        auto plug = static_cast<NetworkDocumentPlugin*>(
                        currentDocument()->model()->pluginModel("NetworkDocumentPlugin"));
        plug->createZeroconfSelectionDialog();
    });

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                       FileMenuElement::Separator_Load,
                                       joinSession);

    QAction* connectServer = new QAction {tr("Server"), this};
    connect(connectServer, &QAction::triggered,
            [&] () {
        new NetworkSerializationServer(9876, this);
    });

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                       FileMenuElement::Separator_Load,
                                       connectServer);

    QAction* connectLocal = new QAction {tr("Client"), this};
    connect(connectLocal, &QAction::triggered,
            [&] () {
        new NetworkSerializationSocket("127.0.0.1", 9876, this);
    });

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                       FileMenuElement::Separator_Load,
                                       connectLocal);
}

void NetworkControl::populateToolbars()
{
}

void NetworkControl::on_newDocument(Document* doc)
{
    // TODO FIXME
    //return;
    doc->model()->addPluginModel(new NetworkDocumentPlugin{this, doc});
}
