#include "EditMenuActions.hpp"

#include "Process/ScenarioGlobalCommandManager.hpp"

#include <QJsonDocument>
#include <QApplication>
#include <QClipboard>

EditMenuActions::EditMenuActions(ScenarioControl* parent) :
        m_parent{parent}
{
    m_editActions = new QActionGroup{this};
    m_editActions->setExclusive(false);

    // remove
    QAction *removeElements = new QAction{tr("Remove scenario elements"), this};
    removeElements->setShortcut(QKeySequence::Delete);
    connect(removeElements, &QAction::triggered,
            [this]()
    {
        if (auto sm = m_parent->focusedScenarioModel())
        {
            ScenarioGlobalCommandManager mgr{m_parent->currentDocument()->commandStack()};
            mgr.deleteSelection(*sm);
        }
    });
    m_editActions->addAction(removeElements);

    QAction *clearElements = new QAction{tr("Clear scenario elements"), this};
    clearElements->setShortcut(Qt::Key_Backspace);
    connect(clearElements, &QAction::triggered,
            [this]()
    {
        if (auto sm = m_parent->focusedScenarioModel())
        {
            ScenarioGlobalCommandManager mgr{m_parent->currentDocument()->commandStack()};
            mgr.clearContentFromSelection(*sm);
        }
    });
    m_editActions->addAction(clearElements);

    // copy/cut
    QAction *copyConstraintContent = new QAction{tr("Copy"), this};
    copyConstraintContent->setShortcut(QKeySequence::Copy);
    connect(copyConstraintContent, &QAction::triggered,
            [this]()
    {
        QJsonDocument doc{m_parent->copySelectedElementsToJson()};
        auto clippy = QApplication::clipboard();
        clippy->setText(doc.toJson(QJsonDocument::Indented));
    });
    m_editActions->addAction(copyConstraintContent);

    QAction *cutConstraintContent = new QAction{tr("Cut"), this};
    cutConstraintContent->setShortcut(QKeySequence::Cut);
    connect(cutConstraintContent, &QAction::triggered,
            [this]()
    {
        QJsonDocument doc{m_parent->cutSelectedElementsToJson()};
        auto clippy = QApplication::clipboard();
        clippy->setText(doc.toJson(QJsonDocument::Indented));
    });
    m_editActions->addAction(cutConstraintContent);

    QAction *pasteConstraintContent = new QAction{tr("Paste"), this};
    pasteConstraintContent->setShortcut(QKeySequence::Paste);
    connect(pasteConstraintContent, &QAction::triggered,
            [this]()
    {
        m_parent->writeJsonToSelectedElements(
                    QJsonDocument::fromJson(
                        QApplication::clipboard()->text().toLatin1()).object());
    });
    m_editActions->addAction(pasteConstraintContent);

}

QActionGroup *EditMenuActions::editActions()
{
    return m_editActions;
}

