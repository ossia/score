#include <QByteArray>
#include <QColor>
#include <QFile>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <Curve/CurveStyle.hpp>
#include "MappingColors.hpp"

class QString;

namespace Mapping
{
MappingColors::MappingColors()
{
#ifdef ISCORE_IEEE_SKIN
    QFile cols(":/MappingColors-IEEE.json");
#else
    QFile cols(":/MappingColors.json");
#endif
    if(cols.open(QFile::ReadOnly))
    {
        // TODO refactor with ScenarioApplicationPlugin
        auto obj = QJsonDocument::fromJson(cols.readAll()).object();
        auto fromColor = [&] (const QString& key) {
            auto arr = obj[key].toArray();
            if(arr.size() == 3)
                return QColor(arr[0].toInt(), arr[1].toInt(), arr[2].toInt());
            else if(arr.size() == 4)
                return QColor(arr[0].toInt(), arr[1].toInt(), arr[2].toInt(), arr[3].toInt());
            return QColor{};
        };

        m_style.Point = fromColor("Point");
        m_style.PointSelected = fromColor("PointSelected");

        m_style.Segment = fromColor("Segment");
        m_style.SegmentSelected = fromColor("SegmentSelected");
        m_style.SegmentDisabled = fromColor("SegmentDisabled");
    }
}
}
