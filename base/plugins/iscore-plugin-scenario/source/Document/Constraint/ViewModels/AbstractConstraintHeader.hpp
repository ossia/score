#pragma once
#include <QGraphicsObject>
#include <QFont>

class ConstraintView;
class ConstraintHeader : public QGraphicsObject
{
    public:
        using QGraphicsObject::QGraphicsObject;
        static constexpr int headerHeight() { return 25; }

        void setWidth(double width);
        void setText(const QString &text);

    protected:
        double m_width{};
        QString m_text;

        static const QFont m_font;
};
