#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <Process/ZoomHelper.hpp>
#include <Process/TimeValue.hpp>
#include <iscore/tools/std/StdlibWrapper.hpp>

#include <Scenario/Document/BaseScenario/BaseScenarioPresenter.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintPresenter.hpp>

class ConstraintModel;
class StateModel;
class StatePresenter;
class EventModel;
class EventPresenter;
class TimeNodeModel;
class TimeNodePresenter;
class LayerPresenter;
class ScenarioDocumentPresenter;
class BaseGraphicsObject;

// Contains the elements that are shown (not necessarily the ones in
// BaseScenarioModel)
class DisplayedElementsPresenter final :
        public QObject,
        public BaseScenarioPresenter<DisplayedElementsModel, FullViewConstraintPresenter>
{
        Q_OBJECT
    public:
        DisplayedElementsPresenter(ScenarioDocumentPresenter* parent);
        using BaseScenarioPresenter<DisplayedElementsModel, FullViewConstraintPresenter>::event;

        void on_displayedConstraintChanged(const ConstraintModel &m);
        void showConstraint();

        void on_zoomRatioChanged(ZoomRatio r);

    signals:
        void requestFocusedPresenterChange(LayerPresenter*);

    public slots:
        void on_displayedConstraintDurationChanged(TimeValue);
        void on_displayedConstraintHeightChanged(double);

    private:
        void updateLength(double);

        ScenarioDocumentPresenter* m_model{};

        std::vector<QMetaObject::Connection> m_connections;
};
