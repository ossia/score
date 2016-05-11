#include <QKeySequence>
#include <QString>
#include <QToolBar>

#include "UndoApplicationPlugin.hpp"
#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/presenter/MenubarManager.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <iscore/widgets/OrderedToolbar.hpp>
#include <iscore/widgets/SetIcons.hpp>
#include <QIcon>

class QObject;

iscore::UndoApplicationPlugin::UndoApplicationPlugin(
        const iscore::ApplicationContext& app,
        QObject* parent):
    iscore::GUIApplicationContextPlugin{app, "UndoApplicationPlugin", parent},
    m_undoAction{"Undo", nullptr},
    m_redoAction{"Redo", nullptr}
{
    m_undoAction.setShortcut(QKeySequence::Undo);
    m_undoAction.setEnabled(false);
    m_undoAction.setText(QObject::tr("Nothing to undo"));
    m_undoAction.setToolTip(QObject::tr("Ctrl+Z"));

    setIcons(&m_undoAction, QString(":/icones/prev_on.png"), QString(":/icones/prev_off.png"));

    con(m_undoAction, &QAction::triggered,
            [&] ()
    {
        currentDocument()->commandStack().undo();
    });

    m_redoAction.setShortcut(QKeySequence::Redo);
    m_redoAction.setEnabled(false);
    m_redoAction.setText(QObject::tr("Nothing to redo"));
    m_redoAction.setToolTip(QObject::tr("Ctrl+Shift+Z"));

    setIcons(&m_redoAction, QString(":/icones/next_on.png"), QString(":/icones/next_off.png"));

    con(m_redoAction, &QAction::triggered,
            [&] ()
    {
        currentDocument()->commandStack().redo();
    });
}

iscore::UndoApplicationPlugin::~UndoApplicationPlugin()
{
    Foreach(m_connections, [] (auto connection)
    {
        QObject::disconnect(connection);
    });
}

void iscore::UndoApplicationPlugin::populateMenus(iscore::MenubarManager* menu)
{
    ////// Edit //////
    menu->addSeparatorIntoToplevelMenu(ToplevelMenuElement::EditMenu,
                                       EditMenuElement::Separator_Undo);
    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::EditMenu,
                                       EditMenuElement::Undo,
                                       &m_undoAction);
    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::EditMenu,
                                       EditMenuElement::Redo,
                                       &m_redoAction);
}

std::vector<iscore::OrderedToolbar> iscore::UndoApplicationPlugin::makeToolbars()
{
    QToolBar* bar = new QToolBar;
    bar->addAction(&m_undoAction);
    bar->addAction(&m_redoAction);

    return std::vector<OrderedToolbar>{OrderedToolbar(3, bar)};
}

void iscore::UndoApplicationPlugin::on_documentChanged(
        iscore::Document* olddoc,
        iscore::Document* newDoc)
{
    using namespace iscore;

    // Cleanup
    Foreach(m_connections, [] (auto connection) {
        QObject::disconnect(connection);
    });
    m_connections.clear();

    if(!newDoc)
    {
        m_undoAction.setEnabled(false);
        m_undoAction.setText(QObject::tr("Nothing to undo"));
        m_redoAction.setEnabled(false);
        m_redoAction.setText(QObject::tr("Nothing to redo"));
        return;
    }

    // Redo the connections
    auto stack = &newDoc->commandStack();
    m_connections.push_back(
                QObject::connect(stack, &CommandStack::canUndoChanged,
                        [&] (bool b) { m_undoAction.setEnabled(b); }));
    m_connections.push_back(
                QObject::connect(stack, &CommandStack::canRedoChanged,
                        [&] (bool b) { m_redoAction.setEnabled(b); }));

    m_connections.push_back(
                QObject::connect(stack, &CommandStack::undoTextChanged,
                        [&] (const QString& s) { m_undoAction.setText(QObject::tr("Undo ") + s);}));
    m_connections.push_back(
                QObject::connect(stack, &CommandStack::redoTextChanged,
                        [&] (const QString& s) { m_redoAction.setText(QObject::tr("Redo ") + s);}));

    // Set the correct values for the current document.
    m_undoAction.setEnabled(stack->canUndo());
    m_redoAction.setEnabled(stack->canRedo());

    m_undoAction.setText(stack->undoText());
    m_redoAction.setText(stack->redoText());
}
