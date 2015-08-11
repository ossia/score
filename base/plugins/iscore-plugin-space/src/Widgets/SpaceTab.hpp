#pragma once
#include <QtWidgets>

#include <iscore/widgets/MarginLess.hpp>
class SpaceTab : public QWidget
{
        Q_OBJECT
    public:
        SpaceTab(QWidget *parent);

    private:
        QVBoxLayout* m_dimensionLayout{};
};
