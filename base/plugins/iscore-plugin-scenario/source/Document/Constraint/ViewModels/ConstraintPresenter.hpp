#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/NamedObject.hpp>
#include <iscore/selection/Selection.hpp>
#include <vector>

#include "ProcessInterface/TimeValue.hpp"
#include "ProcessInterface/ZoomHelper.hpp"

class ConstraintViewModel;
class ConstraintView;
class RackPresenter;
class RackModel;
class ConstraintModel;
class Process;
class ConstraintHeader;
namespace iscore
{
    class SerializableCommand;
}
class LayerPresenter;

class ConstraintPresenter : public NamedObject
{
        Q_OBJECT

    public:
        ConstraintPresenter(
                const QString& name,
                const ConstraintViewModel& model,
                ConstraintView* view,
                ConstraintHeader* header,
                QObject* parent);
        virtual ~ConstraintPresenter() = default;
        virtual void updateScaling();

        bool isSelected() const;

        RackPresenter* rack() const;

        const ConstraintModel& model() const;
        const ConstraintViewModel& abstractConstraintViewModel() const;

        ConstraintView* view() const;

        void on_zoomRatioChanged(ZoomRatio val);
        ZoomRatio zoomRatio() const { return m_zoomRatio; }

        const Id<ConstraintModel>& id() const;

    signals:
        void pressed(const QPointF&);
        void moved(const QPointF&);
        void released(const QPointF&);

        void askUpdate();
        void heightChanged(); // The vertical size
        void heightPercentageChanged(); // The vertical position

    public slots:
        void on_defaultDurationChanged(const TimeValue&);
        void on_minDurationChanged(const TimeValue&);
        void on_maxDurationChanged(const TimeValue&);

        void on_playPercentageChanged(double t);

        void on_rackShown(const Id<RackModel>&);
        void on_rackHidden();
        void on_noRacks();

        void updateHeight();

    protected:
        // Process presenters are in the slot presenters.
        const ConstraintViewModel& m_viewModel;
        ConstraintView* m_view {};
        ConstraintHeader* m_header{};

    private:
        void updateChildren();
        void createRackPresenter(const RackModel&);
        void clearRackPresenter();

        ZoomRatio m_zoomRatio {};
        RackPresenter* m_rack {};
};

// TODO concept: constraint view model.

template<typename T>
const typename T::viewmodel_type* viewModel(const T* obj)
{
    return static_cast<const typename T::viewmodel_type*>(&obj->abstractConstraintViewModel());
}

template<typename T>
const typename T::view_type* view(const T* obj)
{
    return static_cast<const typename T::view_type*>(obj->view());
}


template<typename T>
typename T::view_type* view(T* obj)
{
    return static_cast<typename T::view_type*>(obj->view());
}



template<typename T>
const typename T::viewmodel_type& viewModel(const T& obj)
{
    return static_cast<const typename T::viewmodel_type&>(obj.abstractConstraintViewModel());
}

template<typename T>
typename T::view_type& view(const T& obj)
{
    return static_cast<typename T::view_type&>(*obj.view());
}

template<typename T>
typename T::view_type& view(T& obj)
{
    return static_cast<typename T::view_type&>(*obj.view());
}
