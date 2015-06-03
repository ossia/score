#include "ObjectMenuActions.hpp"

#include "Process/ScenarioGlobalCommandManager.hpp"
#include "iscore/menu/MenuInterface.hpp"

#include <QJsonDocument>
#include <QApplication>
#include <QClipboard>

#include <QTextEdit>
#include <QGridLayout>
#include <QDialogButtonBox>
#include <QTextBlock>
#include <QDialog>

class TextDialog : public QDialog
{
    public:
        TextDialog(QString s)
        {
            this->setLayout(new QGridLayout);
            auto textEdit = new QTextEdit;
            textEdit->setPlainText(s);
            layout()->addWidget(textEdit);
            auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);
            layout()->addWidget(buttonBox);

            connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));

        }
};

ObjectMenuActions::ObjectMenuActions(ScenarioControl* parent) :
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
    m_copyContent = new QAction{tr("Copy"), this};
    m_copyContent->setShortcut(QKeySequence::Copy);
    connect(m_copyContent, &QAction::triggered,
            [this]()
    {
        QJsonDocument doc{m_parent->copySelectedElementsToJson()};
        auto clippy = QApplication::clipboard();
        clippy->setText(doc.toJson(QJsonDocument::Indented));
    });

    m_cutContent = new QAction{tr("Cut"), this};
    m_cutContent->setShortcut(QKeySequence::Cut);
    connect(m_cutContent, &QAction::triggered,
            [this]()
    {
        QJsonDocument doc{m_parent->cutSelectedElementsToJson()};
        auto clippy = QApplication::clipboard();
        clippy->setText(doc.toJson(QJsonDocument::Indented));
    });

    m_pasteContent = new QAction{tr("Paste"), this};
    m_pasteContent->setShortcut(QKeySequence::Paste);
    connect(m_pasteContent, &QAction::triggered,
            [this]()
    {
        m_parent->writeJsonToSelectedElements(
                    QJsonDocument::fromJson(
                        QApplication::clipboard()->text().toLatin1()).object());
    });

    // JSON
    m_elementsToJson = new QAction{tr("Convert selection to JSON"), this};
    connect(m_elementsToJson, &QAction::triggered,
            [this]()
    {
        QJsonDocument doc{m_parent->copySelectedElementsToJson()};
        auto s = new TextDialog(doc.toJson(QJsonDocument::Indented));

        s->show();
    });

}

void ObjectMenuActions::fillMenuBar(iscore::MenubarManager *menu)
{
    menu->insertActionIntoToplevelMenu(iscore::ToplevelMenuElement::ObjectMenu,
                                       m_elementsToJson);
    menu->insertActionIntoToplevelMenu(iscore::ToplevelMenuElement::ObjectMenu,
                                       m_removeElements);
    menu->insertActionIntoToplevelMenu(iscore::ToplevelMenuElement::ObjectMenu,
                                       m_clearElements);
    menu->addSeparatorIntoToplevelMenu(iscore::ToplevelMenuElement::ObjectMenu, iscore::EditMenuElement::Separator_Copy);
    menu->insertActionIntoToplevelMenu(iscore::ToplevelMenuElement::ObjectMenu,
                                       m_copyContent);
    menu->insertActionIntoToplevelMenu(iscore::ToplevelMenuElement::ObjectMenu,
                                       m_cutContent);
    menu->insertActionIntoToplevelMenu(iscore::ToplevelMenuElement::ObjectMenu,
                                       m_pasteContent);

}

void ObjectMenuActions::fillContextMenu(QMenu *menu)
{
    menu->addAction(m_elementsToJson);
    menu->addAction(m_removeElements);
    menu->addAction(m_clearElements);
    menu->addSeparator();
    menu->addAction(m_copyContent);
    menu->addAction(m_cutContent);
    menu->addAction(m_pasteContent);
}

QList<QAction *> ObjectMenuActions::actions()
{
    QList<QAction*> list{m_elementsToJson, m_removeElements, m_clearElements, m_copyContent, m_cutContent, m_pasteContent};
    return list;
}

