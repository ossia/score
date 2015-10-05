#include "AutomationControl.hpp"

#include "Curve/Commands/UpdateCurve.hpp"
#include "Curve/Commands/SetSegmentParameters.hpp"

#include "Commands/ChangeAddress.hpp"
#include "Commands/SetCurveMin.hpp"
#include "Commands/SetCurveMax.hpp"

#include "Commands/InitAutomation.hpp"

#include <iscore/command/CommandGeneratorMap.hpp>
AutomationControl::AutomationControl(
        iscore::Presenter* pres) :
    PluginControlInterface {pres, "AutomationControl", nullptr}
{
    setupCommands();
}

namespace {
struct AutomationCommandFactory
{
        static CommandGeneratorMap map;
};

CommandGeneratorMap AutomationCommandFactory::map;
}

void AutomationControl::setupCommands()
{
    boost::mpl::for_each<
            boost::mpl::list<
                UpdateCurve,
                UpdateCurveFast,
                SetSegmentParameters,
                ChangeAddress,
                SetCurveMin,
                SetCurveMax,
                InitAutomation
            >,
            boost::type<boost::mpl::_>
    >(CommandGeneratorMapInserter<AutomationCommandFactory>());
}

iscore::SerializableCommand* AutomationControl::instantiateUndoCommand(
        const QString& name,
        const QByteArray& data)
{
    return PluginControlInterface::instantiateUndoCommand<AutomationCommandFactory>(name, data);
}

#include <Curve/CurveStyle.hpp>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
void AutomationControl::initColors()
{
    CurveStyle& instance = CurveStyle::instance();
    QFile cols(":/CurveColors.json");

    if(cols.open(QFile::ReadOnly))
    {
        // TODO refactor with ScenarioControl
        auto obj = QJsonDocument::fromJson(cols.readAll()).object();
        auto fromColor = [&] (const QString& key) {
          auto arr = obj[key].toArray();
          if(arr.size() == 3)
            return QColor(arr[0].toInt(), arr[1].toInt(), arr[2].toInt());
          else if(arr.size() == 4)
              return QColor(arr[0].toInt(), arr[1].toInt(), arr[2].toInt(), arr[3].toInt());
          return QColor{};
        };

        instance.Point = fromColor("Point");
        instance.PointSelected = fromColor("PointSelected");

        instance.Segment = fromColor("Segment");
        instance.SegmentSelected = fromColor("SegmentSelected");
        instance.SegmentDisabled = fromColor("SegmentDisabled");
    }
}
