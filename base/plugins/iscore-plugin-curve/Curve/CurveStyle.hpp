#pragma once
#include <QColor>
namespace Curve
{
struct Style
{
        QColor Point{128, 215, 62}; // Tender3
        QColor PointSelected{233, 208, 89}; // Emphasis2

        QColor Segment{199, 31, 44}; // Tender1
        QColor SegmentSelected{216, 178, 24}; // Tender2
        QColor SegmentDisabled{127, 127, 127}; // Gray
};
}
