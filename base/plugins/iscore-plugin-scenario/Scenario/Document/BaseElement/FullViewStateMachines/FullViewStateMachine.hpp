#pragma once
#include <Scenario/Document/BaseElement/BaseScenario/StateMachine/BaseMoveSlot.hpp>
#include <Scenario/Application/ScenarioEditionSettings.hpp>

#include <iscore/statemachine/BaseStateMachine.hpp>

class BaseElementPresenter;
class QGraphicsItem;
class DisplayedElementsPresenter;
class DisplayedElementsModel;

// RENAME FILE
class FullViewToolPalette final : public GraphicsSceneToolPalette
{
    public:
        FullViewToolPalette(
                const iscore::DocumentContext& ctx,
                const DisplayedElementsModel&,
                const DisplayedElementsPresenter&,
                QGraphicsItem&);

        QGraphicsItem& view() const;
        const DisplayedElementsPresenter& presenter() const;
        const DisplayedElementsModel& model() const;
        const iscore::DocumentContext& context() const;
        const Scenario::EditionSettings& editionSettings() const;

    private:
        const iscore::DocumentContext& m_context;
        const DisplayedElementsModel& m_model;
        const DisplayedElementsPresenter& m_presenter;
        QGraphicsItem& m_view;
        const Scenario::EditionSettings& m_editionSettings;
};

