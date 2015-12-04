#pragma once
#include <Process/LayerView.hpp>
#include <QString>
#include <iscore_lib_dummyprocess_export.h>


class QGraphicsItem;
class QPainter;

class ISCORE_LIB_DUMMYPROCESS_EXPORT DummyLayerView final : public LayerView
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
