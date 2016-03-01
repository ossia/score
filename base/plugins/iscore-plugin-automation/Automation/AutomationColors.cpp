#include <QByteArray>
#include <QColor>
#include <QFile>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include "AutomationColors.hpp"
#include <Curve/CurveStyle.hpp>
#include <Process/Style/Skin.hpp>
class QString;

namespace Automation
{
Colors::Colors():
    m_style{
        Skin::instance().Tender3,
        Skin::instance().Emphasis2,
        Skin::instance().Tender1,
        Skin::instance().Tender2,
        Skin::instance().Gray}
{
}
}
