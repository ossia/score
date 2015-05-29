#include "EditMenuActions.hpp"

#include "Process/ScenarioGlobalCommandManager.hpp"
#include "iscore/menu/MenuInterface.hpp"

#include <QJsonDocument>
#include <QApplication>
#include <QClipboard>

EditMenuActions::EditMenuActions(ScenarioControl* parent) :
        m_parent{parent}
{
    // remove
    m_removeElements = new QAction{tr("Remove selected elements"), this};
    m_removeElements->setShortcut(QKeySequence::Delete);
    connect(m_removeElements, &QAction::triggered,
            [this]()
    {
        if (auto sm = m_parent->focusedScenarioModel())
        {
            ScenarioGlobalCommandManager mgr{m_parent->currentDocument()->commandStack()};
            mgr.deleteSelection(*sm);
        }
    });

    m_clearElements = new QAction{tr("Clear selected elements"), this};
    m_clearElements->setShortcut(Qt::Key_Backspace);
    connect(m_clearElements, &QAction::triggered,
            [this]()
    {
        if (auto sm = m_parent->focusedScenarioModel())
        {
            ScenarioGlobalCommandManager mgr{m_parent->currentDocument()->commandStack()};
            mgr.clearContentFromSelection(*sm);
        }
    });

    // copy/cut
    m_copyConstraintContent = new QAction{tr("Copy"), this};
    m_copyConstraintContent->setShortcut(QKeySequence::Copy);
    connect(m_copyConstraintContent, &QAction::triggered,
            [this]()
    {
        QJsonDocument doc{m_parent->copySelectedElementsToJson()};
        auto clippy = QApplication::clipboard();
        clippy->setText(doc.toJson(QJsonDocument::Indented));
    });

    m_cutConstraintContent = new QAction{tr("Cut"), this};
    m_cutConstraintContent->setShortcut(QKeySequence::Cut);
    connect(m_cutConstraintContent, &QAction::triggered,
            [this]()
    {
        QJsonDocument doc{m_parent->cutSelectedElementsToJson()};
        auto clippy = QApplication::clipboard();
        clippy->setText(doc.toJson(QJsonDocument::Indented));
    });

    m_pasteConstraintContent = new QAction{tr("Paste"), this};
    m_pasteConstraintContent->setShortcut(QKeySequence::Paste);
    connect(m_pasteConstraintContent, &QAction::triggered,
            [this]()
    {
        m_parent->writeJsonToSelectedElements(
                    QJsonDocument::fromJson(
                        QApplication::clipboard()->text().toLatin1()).object());
    });

}

void EditMenuActions::fillMenuBar(iscore::MenubarManager *menu)
{
    menu->insertActionIntoToplevelMenu(iscore::ToplevelMenuElement::EditMenu,
                                       m_removeElements);
    menu->insertActionIntoToplevelMenu(iscore::ToplevelMenuElement::EditMenu,
                                       m_clearElements);
    menu->addSeparatorIntoToplevelMenu(iscore::ToplevelMenuElement::EditMenu, iscore::EditMenuElement::Separator_Copy);
    menu->insertActionIntoToplevelMenu(iscore::ToplevelMenuElement::EditMenu,
                                       m_copyConstraintContent);
    menu->insertActionIntoToplevelMenu(iscore::ToplevelMenuElement::EditMenu,
                                       m_cutConstraintContent);
    menu->insertActionIntoToplevelMenu(iscore::ToplevelMenuElement::EditMenu,
                                       m_pasteConstraintContent);

}

QList<QAction *> EditMenuActions::actions()
{
    QList<QAction*> list{m_removeElements, m_clearElements, m_copyConstraintContent, m_cutConstraintContent, m_pasteConstraintContent};
    return list;
}

