#pragma once
#include <Process/LayerView.hpp>
#include <qstring.h>

class QGraphicsItem;
class QPainter;

class DummyLayerView final : public LayerView
{
    public:
        explicit DummyLayerView(QGraphicsItem* parent);

        void setText(const QString& text)
        {
            m_text = text;
            update();
        }

    protected:
        void paint_impl(QPainter*) const override;

    private:
        QString m_text;
};
