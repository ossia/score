#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <Process/ZoomHelper.hpp>

class QMenu;
class Process;
class LayerModel;
namespace iscore
{
    class SerializableCommand;
}
class LayerPresenter : public NamedObject
{
        Q_OBJECT
        bool m_focus{false};

    public:
        using NamedObject::NamedObject;

        virtual ~LayerPresenter() = default;

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
