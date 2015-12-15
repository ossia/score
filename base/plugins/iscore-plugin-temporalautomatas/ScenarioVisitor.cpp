#include <Automation/AutomationModel.hpp>
#include <Curve/CurveModel.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <iscore/menu/MenuInterface.hpp>
#include <QAction>
#include <QChar>
#include <QDebug>

#include <QString>
#include <sstream>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include <Process/Process.hpp>
#include <Process/State/MessageNode.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include "ScenarioVisitor.hpp"
#include <State/Message.hpp>
#include <State/Value.hpp>
#include <core/presenter/MenubarManager.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/std/Algorithms.hpp>
#include <core/document/Document.hpp>

#include <QApplication>
#include <QJsonDocument>
#include <QFile>
#include "TAConversion.hpp"
#include <Scenario/Application/Menus/TextDialog.hpp>
#include <ScenarioMetrics.hpp>
#include <ScenarioGenerator.hpp>
TemporalAutomatas::ApplicationPlugin::ApplicationPlugin(const iscore::ApplicationContext& app):
    iscore::GUIApplicationContextPlugin(app, "TemporalAutomatasApplicationPlugin", nullptr)
{
    m_generate = new QAction{tr("Generate random score"), nullptr};
    connect(m_generate, &QAction::triggered, [&] () {
        auto doc = currentDocument();
        if(!doc)
            return;
        ScenarioDocumentModel& base = iscore::IDocument::get<ScenarioDocumentModel>(*doc);

        const auto& baseConstraint = base.baseScenario().constraint();
        if(baseConstraint.processes.size() == 0)
            return;

        auto firstScenario = dynamic_cast<Scenario::ScenarioModel*>(&*baseConstraint.processes.begin());
        if(!firstScenario)
            return;

        CommandDispatcher<> disp(doc->context().commandStack);
        for(int set = 0; set < 100; set++)
        {
            for(int n = 5; n < 80; n++)
            {
                while(doc->commandStack().canUndo())
                    doc->commandStack().undo();

                Scenario::generateScenario(*firstScenario, n, disp);

                QString baseName = "data/" + QString::number(set) + "/" + QString::number(n);
                {
                    QFile savefile(baseName + ".scorejson");
                    savefile.open(QFile::WriteOnly);
                    QJsonDocument jdoc(doc->saveAsJson());

                    savefile.write(jdoc.toJson());
                    savefile.close();
                }

                {
                    QString text = TA::makeScenario(base.baseScenario().constraint());
                    QFile f(baseName + ".xml");
                    f.open(QFile::WriteOnly);
                    f.write(text.toUtf8());
                    f.close();
                }
            }
        }
    });
    m_convert = new QAction{tr("Convert to Temporal Automatas"), nullptr};
    connect(m_convert, &QAction::triggered, [&] () {
        auto doc = currentDocument();
        if(!doc)
            return;
        ScenarioDocumentModel& base = iscore::IDocument::get<ScenarioDocumentModel>(*doc);

        QString text = TA::makeScenario(base.baseScenario().constraint());
        TextDialog dial(text, qApp->activeWindow());
        dial.exec();

        QFile f("model-output.xml");
        f.open(QFile::WriteOnly);
        f.write(text.toUtf8());
        f.close();
    } );

    m_metrics = new QAction{tr("Scenario metrics"), nullptr};
    connect(m_metrics, &QAction::triggered, [&] () {

        auto doc = currentDocument();
        if(!doc)
            return;

        ScenarioDocumentModel& base = iscore::IDocument::get<ScenarioDocumentModel>(*doc);
        auto& baseScenario = static_cast<Scenario::ScenarioModel&>(*base.baseScenario().constraint().processes.begin());

        using namespace Scenario::Metrics;
        // Language
        QString str = toScenarioLanguage(baseScenario);

        // Halstead
        {
            auto factors = Halstead::ComputeFactors(baseScenario);
            str += "Difficulty = " + QString::number(Halstead::Difficulty(factors)) + "\n";
            str += "Volume = " + QString::number(Halstead::Volume(factors)) + "\n";
            str += "Effort = " + QString::number(Halstead::Effort(factors)) + "\n";
            str += "TimeRequired = " + QString::number(Halstead::TimeRequired(factors)) + "\n";
            str += "Bugs2 = " + QString::number(Halstead::Bugs2(factors)) + "\n";
        }
        // Cyclomatic
        {
            auto factors = Cyclomatic::ComputeFactors(baseScenario);
            str += "Cyclomatic1 = " + QString::number(Cyclomatic::Complexity(factors));
        }
        // Display
        TextDialog dial(str, qApp->activeWindow());
        dial.exec();
    });
}

void TemporalAutomatas::ApplicationPlugin::populateMenus(iscore::MenubarManager* menus)
{
    menus->insertActionIntoToplevelMenu(
                ToplevelMenuElement::FileMenu,
                m_generate);
    menus->insertActionIntoToplevelMenu(
                ToplevelMenuElement::FileMenu,
                m_convert);
    menus->insertActionIntoToplevelMenu(
                ToplevelMenuElement::FileMenu,
                m_metrics);
}
