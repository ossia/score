#pragma once
#include <Process/ZoomHelper.hpp>
#include <iscore/tools/NamedObject.hpp>

class LayerModel;
class Process;
class QMenu;
class QPoint;
class QPointF;
template <typename tag, typename impl> class id_base_t;

namespace iscore
{
}
class LayerPresenter : public NamedObject
{
        Q_OBJECT
        bool m_focus{false};

    public:
        using NamedObject::NamedObject;

        virtual ~LayerPresenter();

        bool focused() const;
        void setFocus(bool focus);
        virtual void on_focusChanged();


        virtual void setWidth(int width) = 0;
        virtual void setHeight(int height) = 0;

        virtual void putToFront() = 0;
        virtual void putBehind() = 0;

        virtual void on_zoomRatioChanged(ZoomRatio) = 0;
        virtual void parentGeometryChanged() = 0;

        virtual const LayerModel& layerModel() const = 0;
        virtual const Id<Process>& modelId() const = 0;

        virtual void fillContextMenu(QMenu*,
                                     const QPoint& pos,
                                     const QPointF& scenepos) const = 0;
    signals:
        void contextMenuRequested(const QPoint&, const QPointF&);

};
