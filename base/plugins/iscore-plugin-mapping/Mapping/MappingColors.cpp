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
#include <Process/Style/Skin.hpp>
class QString;

namespace Mapping
{
MappingColors::MappingColors():
    m_style{
        Skin::instance().Tender3,
        Skin::instance().Emphasis2,
        Skin::instance().Emphasis3,
        Skin::instance().Tender2,
        Skin::instance().Gray}
{
}
}
