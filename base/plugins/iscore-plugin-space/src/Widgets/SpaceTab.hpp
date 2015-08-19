#pragma once
#include <QtWidgets>

#include <iscore/widgets/MarginLess.hpp>
class SpaceModel;
class SpaceTab : public QWidget
{
        Q_OBJECT
    public:
        SpaceTab(const SpaceModel& space, QWidget *parent);

    private:
        const SpaceModel& m_space;
        QVBoxLayout* m_dimensionLayout{};
        QVBoxLayout* m_viewportLayout{};
        QPushButton* m_addDim{}, *m_addViewport{};
};
