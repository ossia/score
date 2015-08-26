#pragma once
#include <QWidget>
class QGridLayout;
class ValueWidget;

/**
 * @brief The ValueWrapper class
 *
 * This wrapper is used to easily wrap a ValueWidget
 * so that the ValueWidget can be replaced without clearing all the layout.
 */
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
