#pragma once

#include <Process/LayerView.hpp>
#include <QString>

class QGraphicsItem;
class QPainter;

namespace Mapping
{
class MappingView final : public Process::LayerView
{
    public:
        explicit MappingView(QGraphicsItem *parent);

        void showName(bool b)  { m_showName = b; }

        void setDisplayedName(QString s) { m_displayedName = s; }

    private:
        void paint_impl(QPainter* painter) const override;

        QString m_displayedName;
        bool m_showName{true};
};
}
