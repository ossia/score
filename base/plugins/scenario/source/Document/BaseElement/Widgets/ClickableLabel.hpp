#pragma once
#include <QLabel>

class ClickableLabel : public QLabel
{
        Q_OBJECT

        Q_PROPERTY(int index READ index WRITE setIndex)

        int m_index;

    public:
        explicit ClickableLabel(QString text, QWidget* parent);

    int index() const
    {
        return m_index;
    }

    public slots:
    void setIndex(int arg)
    {
        m_index = arg;
    }

    signals:
        void clicked(ClickableLabel* self);

    protected:
        virtual void enterEvent(QEvent*) override;
        virtual void leaveEvent(QEvent*) override;

        virtual void mousePressEvent(QMouseEvent* event) override;
};
