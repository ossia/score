#include "ScenarioControl.hpp"
#include "Document/BaseElement/BaseElementModel.hpp"
#include "Document/BaseElement/BaseElementPresenter.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"
#include "Process/ScenarioModel.hpp"
#include "Process/ScenarioGlobalCommandManager.hpp"

#include <iscore/menu/MenuInterface.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/presenter/MenubarManager.hpp>

#include "Control/OldFormatConversion.hpp"

#include <QAction>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>

using namespace iscore;

ScenarioControl::ScenarioControl(QObject* parent) :
    PluginControlInterface {"ScenarioControl", parent},
    m_processList {new ProcessList{this}}
{

}

void ScenarioControl::populateMenus(iscore::MenubarManager* menu)
{
    // File

    // Export in old format
    auto toZeroTwo = new QAction("To i-score 0.2", this);
    connect(toZeroTwo, &QAction::triggered,
            [this]()
    {
        auto savename = QFileDialog::getSaveFileName(nullptr, tr("Save"));

        // TODO Operate at the document level instead.
        if(!savename.isEmpty())
        {
            QFile f(savename);
            f.open(QIODevice::WriteOnly);
            f.write(JSONToZeroTwo(currentDocument()->saveAsJson()).toLatin1().constData());
        }
    });

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                       FileMenuElement::Separator_Quit,
                                       toZeroTwo);


    // Edit
    QAction* removeElements = new QAction {tr("Remove scenario elements"), this};
    connect(removeElements, &QAction::triggered,
            [this] ()
    {
        auto& model = IDocument::modelDelegate<BaseElementModel>(*currentDocument());
        if(auto sm = dynamic_cast<ScenarioModel*>(model.focusedViewModel()->sharedProcessModel()))
        {
            ScenarioGlobalCommandManager mgr{currentDocument()->commandStack()};
            mgr.deleteSelection(*sm);
        }
    });
    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::EditMenu,
                                       removeElements);


    QAction* clearElements = new QAction {tr("Clear scenario elements"), this};
    connect(clearElements, &QAction::triggered,
            [this] ()
    {
        auto& model = IDocument::modelDelegate<BaseElementModel>(*currentDocument());
        if(auto sm = dynamic_cast<ScenarioModel*>(model.focusedViewModel()->sharedProcessModel()))
        {
            ScenarioGlobalCommandManager mgr{currentDocument()->commandStack()};
            mgr.clearContentFromSelection(*sm);
        }
    });
    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::EditMenu,
                                       clearElements);

    // View
    QAction* selectAll = new QAction {tr("Select all"), this};
    connect(selectAll,	&QAction::triggered,
            [this] ()
    {
        auto& pres = IDocument::presenterDelegate<BaseElementPresenter>(*currentDocument());
        pres.selectAll();
    });

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::ViewMenu,
                                       ViewMenuElement::Windows,
                                       selectAll);


    QAction* deselectAll = new QAction {tr("Deselect all"), this};
    connect(deselectAll,	&QAction::triggered,
            [this] ()
    {
        auto& pres = IDocument::presenterDelegate<BaseElementPresenter>(*currentDocument());
        pres.deselectAll();
    });

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::ViewMenu,
                                       ViewMenuElement::Windows,
                                       deselectAll);
}

void ScenarioControl::populateToolbars()
{
}

// Defined in CommandNames.cpp
iscore::SerializableCommand* makeCommandByName(const QString& name);

iscore::SerializableCommand* ScenarioControl::instantiateUndoCommand(const QString& name, const QByteArray& data)
{
    iscore::SerializableCommand* cmd = makeCommandByName(name);
    if(!cmd)
    {
        qDebug() << Q_FUNC_INFO << "Warning : command" << name << "received, but it could not be read.";
        return nullptr;
    }

    cmd->deserialize(data);
    return cmd;
}
