#pragma once
#include <QWidget>
class QGridLayout;
class ValueWidget;

class ValueWrapper : public QWidget
{
    public:
        explicit ValueWrapper(QWidget* parent);

        void setWidget(ValueWidget* widg);

        ValueWidget* valueWidget() const;

    private:
        QGridLayout* m_lay{};
        ValueWidget* m_widget{};
};
