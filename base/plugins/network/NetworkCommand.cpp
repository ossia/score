#include "NetworkCommand.hpp"
#include <core/presenter/Presenter.hpp>
#include <core/document/DocumentPresenter.hpp>
#include <core/document/DocumentModel.hpp>

#include "NetworkDocumentPlugin.hpp"

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
}

void NetworkControl::populateToolbars()
{
}

void NetworkControl::on_newDocument(Document* doc)
{
    doc->model()->addPluginModel(new NetworkDocumentPlugin{this, doc});
}
