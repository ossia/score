#pragma once
#include <Process/ZoomHelper.hpp>
#include <iscore/tools/NamedObject.hpp>
#include <iscore_lib_process_export.h>

#include <iscore/tools/SettableIdentifier.hpp>
#include <Process/ProcessContext.hpp>
class QMenu;
class QPoint;
class QPointF;
namespace Process
{
class ProcessModel;
class LayerModel;
class LayerContextMenuManager;
class ISCORE_LIB_PROCESS_EXPORT LayerPresenter : public QObject
{
        Q_OBJECT

    public:
        LayerPresenter(
                const ProcessPresenterContext& ctx,
                QObject* parent):
            QObject{parent},
            m_context{ctx, *this}
        {

        }

        virtual ~LayerPresenter();

        const Process::LayerContext& context() const
        { return m_context; }

        bool focused() const;
        void setFocus(bool focus);
        virtual void on_focusChanged();


        virtual void setWidth(qreal width) = 0;
        virtual void setHeight(qreal height) = 0;

        virtual void putToFront() = 0;
        virtual void putBehind() = 0;

        virtual void on_zoomRatioChanged(ZoomRatio) = 0;
        virtual void parentGeometryChanged() = 0;

        virtual const LayerModel& layerModel() const = 0;
        virtual const Id<ProcessModel>& modelId() const = 0;

        virtual void fillContextMenu(QMenu&,
                                     QPoint pos,
                                     QPointF scenepos,
                                     const LayerContextMenuManager&) const = 0;
    signals:
        void contextMenuRequested(const QPoint&, const QPointF&);

    protected:
        Process::LayerContext m_context;

    private:
        bool m_focus{false};

};

}
