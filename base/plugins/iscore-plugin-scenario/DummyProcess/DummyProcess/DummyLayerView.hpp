#pragma once
#include <Process/LayerView.hpp>
#include <QString>
#include <iscore_lib_dummyprocess_export.h>


class QGraphicsItem;
class QPainter;
//class QQuickWidget;

namespace Dummy
{
class ISCORE_LIB_DUMMYPROCESS_EXPORT DummyLayerView final : public Process::LayerView
{
        Q_OBJECT
    public:
        explicit DummyLayerView(QGraphicsItem* parent);

        void setText(const QString& text)
        {
            m_text = text;
            update();
        }

    signals:
        void pressed();


    private:
        void paint_impl(QPainter*) const override;
        void mousePressEvent(QGraphicsSceneMouseEvent*) override;
        QString m_text;
        //QQuickWidget* m_widg{};
};
}
