#pragma once

#include <QObject>
#include <iscore_plugin_curve_export.h>

namespace Curve
{
enum class Tool {
    Disabled, Select, Create, SetSegment, CreatePen, RemovePen, Playing
};
enum class AddPointBehaviour {
    LinearBefore, LinearAfter, DuplicateSegment
};

class ISCORE_PLUGIN_CURVE_EXPORT  EditionSettings : public QObject
{
        Q_OBJECT
        Q_PROPERTY(bool lockBetweenPoints READ lockBetweenPoints WRITE setLockBetweenPoints NOTIFY lockBetweenPointsChanged)
        Q_PROPERTY(bool suppressOnOverlap READ suppressOnOverlap WRITE setSuppressOnOverlap NOTIFY suppressOnOverlapChanged)
        Q_PROPERTY(bool stretchBothBounds READ stretchBothBounds WRITE setStretchBothBounds NOTIFY stretchBothBoundsChanged)
        Q_PROPERTY(Curve::AddPointBehaviour addPointBehaviour READ addPointBehaviour WRITE setAddPointBehaviour NOTIFY addPointBehaviourChanged)
        Q_PROPERTY(Curve::Tool tool READ tool WRITE setTool NOTIFY toolChanged)

        bool m_lockBetweenPoints{true};
        bool m_suppressOnOverlap{true};
        bool m_stretchBothBounds{false};
        Curve::AddPointBehaviour m_addPointBehaviour{AddPointBehaviour::DuplicateSegment};
        Curve::Tool m_tool{Curve::Tool::Disabled};

    public:
        bool lockBetweenPoints() const;
        bool suppressOnOverlap() const;
        bool stretchBothBounds() const;
        Curve::AddPointBehaviour addPointBehaviour() const;
        Curve::Tool tool() const;

        void setLockBetweenPoints(bool);
        void setSuppressOnOverlap(bool);
        void setStretchBothBounds(bool);
        void setAddPointBehaviour(Curve::AddPointBehaviour);
        void setTool(Curve::Tool tool);

    signals:
        void lockBetweenPointsChanged(bool);
        void suppressOnOverlapChanged(bool);
        void stretchBothBoundsChanged(bool);
        void addPointBehaviourChanged(Curve::AddPointBehaviour);
        void toolChanged(Curve::Tool tool);
};
}
