#pragma once
#include <QLabel>

class ClickableLabel : public QLabel
{
        Q_OBJECT

    public:
        explicit ClickableLabel(QString text, QWidget* parent);

    signals:
        void clicked(ClickableLabel* self);

    protected:
        virtual void enterEvent(QEvent*) override;
        virtual void leaveEvent(QEvent*) override;

        virtual void mousePressEvent(QMouseEvent* event) override;
};
