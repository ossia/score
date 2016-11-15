#include <Process/Style/ScenarioStyle.hpp>
#include <iscore/model/Skin.hpp>
#include <QColor>

#include <Scenario/Document/TimeRuler/AbstractTimeRulerView.hpp>
#include "TimeRulerView.hpp"
#include <iscore/tools/Todo.hpp>
namespace Scenario
{
TimeRulerView::TimeRulerView() :
    AbstractTimeRulerView{}
{
    this->setCacheMode(QGraphicsItem::ItemCoordinateCache);
    m_height = -3 * m_graduationHeight;
    m_textPosition = 1.15 * m_graduationHeight;
    m_color = ScenarioStyle::instance().TimeRuler;


    auto& skin = iscore::Skin::instance();
    con(skin, &iscore::Skin::changed,
        this, [&] () {
        auto& skin = ScenarioStyle::instance();
        m_color = skin.TimeRuler;
    });
}

QRectF TimeRulerView::boundingRect() const
{
    return QRectF{0, -m_height, m_width * 2, m_height};
}
}
