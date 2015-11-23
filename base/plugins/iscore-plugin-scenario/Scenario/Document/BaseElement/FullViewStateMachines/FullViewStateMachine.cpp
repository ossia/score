#include "FullViewStateMachine.hpp"

#include <Scenario/Document/BaseElement/DisplayedElements/DisplayedElementsPresenter.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>

#include <core/application/ApplicationContext.hpp>
#include <core/document/Document.hpp>

FullViewToolPalette::FullViewToolPalette(
        const iscore::DocumentContext& ctx,
        const DisplayedElementsModel& model,
        const DisplayedElementsPresenter& pres,
        QGraphicsItem& view):
    GraphicsSceneToolPalette{*view.scene()},
    m_context{ctx},
    m_model{model},
    m_presenter{pres},
    m_view{view},
    m_editionSettings{m_context.app.components.applicationPlugin<ScenarioApplicationPlugin>().editionSettings()}
{
    // Scenario::SelectionAndMoveTool<BaseScenario, SubScenarioToolPalette, QGraphicsItem> teupeu(*this);
}

QGraphicsItem& FullViewToolPalette::view() const
{
    return m_view;
}

const DisplayedElementsPresenter&FullViewToolPalette::presenter() const
{
    return m_presenter;
}

const DisplayedElementsModel& FullViewToolPalette::model() const
{
    return m_model;
}

const iscore::DocumentContext& FullViewToolPalette::context() const
{
    return m_context;
}

const Scenario::EditionSettings&FullViewToolPalette::editionSettings() const
{
    return m_editionSettings;
}
