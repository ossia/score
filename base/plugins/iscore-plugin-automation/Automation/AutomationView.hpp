#pragma once

#include <Process/LayerView.hpp>
#include <QString>
#include <QTextLayout>

class QGraphicsItem;
class QPainter;

namespace Automation
{
class LayerView final : public Process::LayerView
{
    public:
        explicit LayerView(QGraphicsItem *parent);

        void setDisplayedName(const QString& s);
        void showName(bool b)
        {
            m_showName = b;
            update();
        }

    protected:
        void paint_impl(QPainter* painter) const override;

    private:
        QString m_displayedName{};
        bool m_showName{true};

        QTextLayout m_textcache;
};
}
