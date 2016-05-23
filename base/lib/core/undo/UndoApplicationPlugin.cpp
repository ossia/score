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
#include <core/presenter/CoreApplicationPlugin.hpp>
#include <iscore/widgets/SetIcons.hpp>
#include <QIcon>

class QObject;

iscore::UndoApplicationPlugin::UndoApplicationPlugin(
        const iscore::ApplicationContext& app):
    iscore::GUIApplicationContextPlugin{app},
    m_undoAction{"Undo", nullptr},
    m_redoAction{"Redo", nullptr}
{
    m_undoAction.setShortcut(QKeySequence::Undo);
    m_undoAction.setEnabled(false);
    m_undoAction.setText(QObject::tr("Nothing to undo"));
    m_undoAction.setToolTip(QObject::tr("Undo (Ctrl+Z)"));

    setIcons(&m_undoAction, QString(":/icons/prev_on.png"), QString(":/icons/prev_off.png"));

    con(m_undoAction, &QAction::triggered,
            [&] ()
    {
        currentDocument()->commandStack().undo();
    });

    m_redoAction.setShortcut(QKeySequence::Redo);
    m_redoAction.setEnabled(false);
    m_redoAction.setText(QObject::tr("Nothing to redo"));
    m_redoAction.setToolTip(QObject::tr("Redo (Ctrl+Shift+Z)"));

    setIcons(&m_redoAction, QString(":/icons/next_on.png"), QString(":/icons/next_off.png"));

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

auto iscore::UndoApplicationPlugin::makeGUIElements() -> GUIElements
{
    std::vector<Toolbar> toolbars;
    toolbars.reserve(1);

    {
        auto bar = new QToolBar;
        bar->addAction(&m_undoAction);
        bar->addAction(&m_redoAction);
        toolbars.emplace_back(bar, StringKey<Toolbar>("Undo"), 0, 3);
    }

    Menu& edit = context.menus.get().at(Menus::Edit());
    edit.menu()->addAction(m_undoAction);
    edit.menu()->addAction(m_redoAction);

    return std::make_tuple({}, toolbars, std::vector<Action>{m_undoAction, m_redoAction});
}
