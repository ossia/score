#include "MappingControl.hpp"

#include <Curve/CurveStyle.hpp>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
MappingControl::MappingControl(
        iscore::Presenter* pres) :
    PluginControlInterface {pres, "AutomationControl", nullptr}
{
    initColors();
}

void MappingControl::initColors()
{
    CurveStyle& instance = CurveStyle::instance();
#ifdef ISCORE_IEEE_SKIN
    QFile cols(":/CurveColors-IEEE.json");
#else
    QFile cols(":/CurveColors.json");
#endif
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
