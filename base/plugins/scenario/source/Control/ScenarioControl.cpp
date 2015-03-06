#include "ScenarioControl.hpp"

#include "Commands/Constraint/Box/Deck/AddProcessViewModelToDeck.hpp"
#include "Commands/Constraint/Box/Deck/CopyProcessViewModel.hpp"
#include "Commands/Constraint/Box/Deck/MoveProcessViewModel.hpp"
#include "Commands/Constraint/Box/Deck/RemoveProcessViewModelFromDeck.hpp"
#include "Commands/Constraint/Box/Deck/ResizeDeckVertically.hpp"
#include "Commands/Constraint/Box/AddDeckToBox.hpp"
#include "Commands/Constraint/Box/RemoveDeckFromBox.hpp"
#include "Commands/Constraint/Box/CopyDeck.hpp"
#include "Commands/Constraint/Box/MoveDeck.hpp"
#include "Commands/Constraint/Box/MergeDecks.hpp"
#include "Commands/Constraint/AddBoxToConstraint.hpp"
#include "Commands/Constraint/AddProcessToConstraint.hpp"
#include "Commands/Constraint/MergeBoxes.hpp"
#include "Commands/Constraint/RemoveBoxFromConstraint.hpp"
#include "Commands/Constraint/RemoveProcessFromConstraint.hpp"
#include "Commands/Constraint/SetMinDuration.hpp"
#include "Commands/Constraint/SetMaxDuration.hpp"
#include "Commands/Constraint/SetRigidity.hpp"
#include "Commands/Event/AddStateToEvent.hpp"
#include "Commands/Event/SetCondition.hpp"
#include "Commands/Scenario/ClearConstraint.hpp"
#include "Commands/Scenario/ClearEvent.hpp"
#include "Commands/Scenario/RemoveEvent.hpp"
#include "Commands/Scenario/RemoveConstraint.hpp"
#include "Commands/Scenario/CreateEvent.hpp"
#include "Commands/Scenario/CreateEventAfterEvent.hpp"
#include "Commands/Scenario/CreateEventAfterEventOnTimeNode.hpp"
#include "Commands/Scenario/HideBoxInViewModel.hpp"
#include "Commands/Scenario/MoveEvent.hpp"
#include "Commands/Scenario/MoveTimeNode.hpp"
#include "Commands/Scenario/MoveConstraint.hpp"
#include "Commands/Scenario/ResizeConstraint.hpp"
#include "Commands/ResizeBaseConstraint.hpp"
#include "Commands/Scenario/ShowBoxInViewModel.hpp"
#include "Commands/RemoveMultipleElements.hpp"

#include <interface/plugincontrol/MenuInterface.hpp>
#include <core/presenter/MenubarManager.hpp>
#include <QAction>
#include <QApplication>

#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <interface/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <core/interface/document/DocumentInterface.hpp>
#include <QJsonDocument>


#include "Document/BaseElement/BaseElementModel.hpp"
#include "Document/BaseElement/BaseElementPresenter.hpp"
#include <core/presenter/Presenter.hpp>

#include "Control/OldFormatConversion.hpp"
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

        if(!savename.isEmpty())
        {
            auto& bem = IDocument::modelDelegate<BaseElementModel>(currentDocument());

            QFile f(savename);
            f.open(QIODevice::WriteOnly);
            f.write(JSONToZeroTwo(bem.toJson()).toLatin1().constData());
        }
    });

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                       FileMenuElement::Separator_Quit,
                                       toZeroTwo);

    // Save as json
    // TODO this should go in the global presenter instead.
    auto toJson = new QAction("To JSON", this);
    connect(toJson, &QAction::triggered,
            [this]()
    {
        auto savename = QFileDialog::getSaveFileName(nullptr, tr("Save"));

        if(!savename.isEmpty())
        {
            QJsonDocument doc;
            auto& bem = IDocument::modelDelegate<BaseElementModel>(currentDocument());
            doc.setObject(bem.toJson());

            QFile f(savename);
            f.open(QIODevice::WriteOnly);
            f.write(doc.toJson());
        }
    });
    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                       FileMenuElement::Separator_Quit,
                                       toJson);


    // View
    QAction* selectAll = new QAction {tr("Select all"), this};
    connect(selectAll,	&QAction::triggered,
            this,		&ScenarioControl::selectAll);

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::ViewMenu,
                                       ViewMenuElement::Windows,
                                       selectAll);


    QAction* deselectAll = new QAction {tr("Deselect all"), this};
    connect(deselectAll,	&QAction::triggered,
            this,			&ScenarioControl::deselectAll);

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
    using namespace Scenario::Command;

    iscore::SerializableCommand* cmd = makeCommandByName(name);
    if(!cmd)
    {
        qDebug() << Q_FUNC_INFO << "Warning : command" << name << "received, but it could not be read.";
        return nullptr;
    }

    cmd->deserialize(data);
    return cmd;
}

void ScenarioControl::selectAll()
{
    auto& pres = IDocument::presenterDelegate<BaseElementPresenter>(currentDocument());
    pres.selectAll();
}


void ScenarioControl::deselectAll()
{
    auto& pres = IDocument::presenterDelegate<BaseElementPresenter>(currentDocument());
    pres.deselectAll();
}
