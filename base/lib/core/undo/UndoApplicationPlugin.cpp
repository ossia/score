// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "UndoApplicationPlugin.hpp"

#include <QIcon>
#include <QKeySequence>
#include <QString>
#include <QToolBar>
#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/presenter/CoreActions.hpp>
#include <core/presenter/CoreApplicationPlugin.hpp>
#include <score/actions/Menu.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/widgets/SetIcons.hpp>

class QObject;

score::UndoApplicationPlugin::UndoApplicationPlugin(
    const score::GUIApplicationContext& app)
    : score::GUIApplicationPlugin{app}
    , m_undoAction{"Undo", nullptr}
    , m_redoAction{"Redo", nullptr}
{
  m_undoAction.setShortcut(QKeySequence::Undo);
  m_undoAction.setEnabled(false);
  m_undoAction.setText(QObject::tr("Nothing to undo"));
  m_undoAction.setToolTip(QObject::tr("Undo (Ctrl+Z)"));

  setIcons(
      &m_undoAction, QStringLiteral(":/icons/prev_on.png"),
      QStringLiteral(":/icons/prev_off.png"),
      QStringLiteral(":/icons/prev_disabled.png") );

  con(m_undoAction, &QAction::triggered,
      [&]() { currentDocument()->commandStack().undo(); });

  m_redoAction.setShortcut(QKeySequence::Redo);
  m_redoAction.setEnabled(false);
  m_redoAction.setText(QObject::tr("Nothing to redo"));
  m_redoAction.setToolTip(QObject::tr("Redo (Ctrl+Shift+Z)"));

  setIcons(
      &m_redoAction, QStringLiteral(":/icons/next_on.png"),
      QStringLiteral(":/icons/next_off.png"),
      QStringLiteral(":/icons/next_disabled.png") );

  con(m_redoAction, &QAction::triggered,
      [&]() { currentDocument()->commandStack().redo(); });
}

score::UndoApplicationPlugin::~UndoApplicationPlugin()
{
  Foreach(
      m_connections, [](auto connection) { QObject::disconnect(connection); });
}

void score::UndoApplicationPlugin::on_documentChanged(
    score::Document* olddoc, score::Document* newDoc)
{
  using namespace score;

  // Cleanup
  Foreach(
      m_connections, [](auto connection) { QObject::disconnect(connection); });
  m_connections.clear();

  if (!newDoc)
  {
    m_undoAction.setEnabled(false);
    m_undoAction.setText(QObject::tr("Nothing to undo"));
    m_redoAction.setEnabled(false);
    m_redoAction.setText(QObject::tr("Nothing to redo"));
    return;
  }

  // Redo the connections
  // TODO maybe use conditions for this ?
  auto stack = &newDoc->commandStack();
  m_connections.push_back(
      QObject::connect(stack, &CommandStack::canUndoChanged, [&](bool b) {
        m_undoAction.setEnabled(b);
      }));
  m_connections.push_back(
      QObject::connect(stack, &CommandStack::canRedoChanged, [&](bool b) {
        m_redoAction.setEnabled(b);
      }));

  m_connections.push_back(QObject::connect(
      stack, &CommandStack::undoTextChanged, [&](const QString& s) {
        m_undoAction.setText(QObject::tr("Undo ") + s);
      }));
  m_connections.push_back(QObject::connect(
      stack, &CommandStack::redoTextChanged, [&](const QString& s) {
        m_redoAction.setText(QObject::tr("Redo ") + s);
      }));

  // Set the correct values for the current document.
  m_undoAction.setEnabled(stack->canUndo());
  m_redoAction.setEnabled(stack->canRedo());

  m_undoAction.setText(stack->undoText());
  m_redoAction.setText(stack->redoText());
}

auto score::UndoApplicationPlugin::makeGUIElements() -> GUIElements
{
  GUIElements e;
  auto& toolbars = e.toolbars;

  toolbars.reserve(1);

  {
    auto bar = new QToolBar;
    bar->addAction(&m_undoAction);
    bar->addAction(&m_redoAction);
    toolbars.emplace_back(bar, StringKey<score::Toolbar>("Undo"), Qt::TopToolBarArea, 500);
  }

  Menu& edit = context.menus.get().at(Menus::Edit());
  edit.menu()->addAction(&m_undoAction);
  edit.menu()->addAction(&m_redoAction);

  e.actions.container.reserve(2);
  e.actions.add<Actions::Undo>(&m_undoAction);
  e.actions.add<Actions::Redo>(&m_redoAction);

  return e;
}
