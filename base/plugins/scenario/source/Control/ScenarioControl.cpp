#include "ScenarioControl.hpp"

#include <iscore/menu/MenuInterface.hpp>
#include <core/presenter/MenubarManager.hpp>
#include <QAction>

#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <iscore/plugins/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <iscore/document/DocumentInterface.hpp>
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
            [this] ()
    {
        auto& pres = IDocument::presenterDelegate<BaseElementPresenter>(currentDocument());
        pres.selectAll();
    });

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::ViewMenu,
                                       ViewMenuElement::Windows,
                                       selectAll);


    QAction* deselectAll = new QAction {tr("Deselect all"), this};
    connect(deselectAll,	&QAction::triggered,
            [this] ()
    {
        auto& pres = IDocument::presenterDelegate<BaseElementPresenter>(currentDocument());
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
