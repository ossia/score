#pragma once
#include <QWidget>

// Set the dimensions and bounds of the space we're in.
// If > 2, set a value for the other dims.
class SpaceWidget : public QWidget
{
    public:
        SpaceWidget(QWidget* parent);
};
