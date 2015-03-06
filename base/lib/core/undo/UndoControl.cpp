#include "UndoControl.hpp"
#include <interface/plugincontrol/MenuInterface.hpp>
#include <core/presenter/command/CommandQueue.hpp>
#include <core/document/DocumentPresenter.hpp>

iscore::UndoControl::UndoControl(QObject* parent):
    iscore::PluginControlInterface{"UndoControl", parent}
{
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setEnabled(false);
    m_undoAction->setText(tr("Nothing to undo"));
    connect(m_undoAction, &QAction::triggered,
            [&] ()
    {
        presenter()->currentDocument()->commandStack()->undoAndNotify();
    });

    m_redoAction->setShortcut(QKeySequence::Redo);
    m_redoAction->setEnabled(false);
    m_redoAction->setText(tr("Nothing to redo"));
    connect(m_redoAction, &QAction::triggered,
            [&] ()
    {
        presenter()->currentDocument()->commandStack()->redoAndNotify();
    });
}

iscore::UndoControl::~UndoControl()
{
    for(auto& connection : m_connections)
    {
        disconnect(connection);
    }
}

void iscore::UndoControl::populateMenus(iscore::MenubarManager* menu)
{
    ////// Edit //////
    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::EditMenu,
                                       EditMenuElement::Undo,
                                       m_undoAction);
    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::EditMenu,
                                       EditMenuElement::Redo,
                                       m_redoAction);
}

void iscore::UndoControl::populateToolbars()
{
}

void iscore::UndoControl::on_presenterChanged()
{
    connect(presenter(), &Presenter::currentDocumentChanged,
            this, &UndoControl::on_documentChanged);
}


void iscore::UndoControl::on_documentChanged(Document* newDoc)
{
    using namespace iscore;
    // Cleanup
    for(auto& connection : m_connections)
        disconnect(connection);
    m_connections.clear();

    // Redo the connections
    m_connections.push_back(
                connect(newDoc->commandStack(), &CommandStack::canUndoChanged,
                        [&] (bool b) { m_undoAction->setEnabled(b); }));
    m_connections.push_back(
                connect(newDoc->commandStack(), &CommandStack::canRedoChanged,
                        [&] (bool b) { m_redoAction->setEnabled(b); }));

    m_connections.push_back(
                connect(newDoc->commandStack(), &CommandStack::undoTextChanged,
                        [&] (const QString& s) { m_undoAction->setText(tr("Undo ") + s);}));
    m_connections.push_back(
                connect(newDoc->commandStack(), &CommandStack::redoTextChanged,
                        [&] (const QString& s) { m_redoAction->setText(tr("Redo ") + s);}));

    // Set the correct values for the current document.
    m_undoAction->setEnabled(newDoc->commandStack()->canUndo());
    m_redoAction->setEnabled(newDoc->commandStack()->canRedo());

    m_undoAction->setText(newDoc->commandStack()->undoText());
    m_redoAction->setText(newDoc->commandStack()->redoText());
}
