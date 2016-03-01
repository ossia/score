#pragma once
#include <QColor>
namespace Curve
{
struct Style
{
        const QColor& Point;//{128, 215, 62}; // Tender3
        const QColor& PointSelected;//{233, 208, 89}; // Emphasis2

        const QColor& Segment;//{199, 31, 44}; // Tender1
        const QColor& SegmentSelected;//{216, 178, 24}; // Tender2
        const QColor& SegmentDisabled;//{127, 127, 127}; // Gray
};
}
