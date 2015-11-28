#include "BaseElementContext.hpp"
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
BaseElementContext::BaseElementContext(
        iscore::Document& doc,
        ScenarioDocumentPresenter& pres,
        ProcessFocusManager& d):
    iscore::DocumentContext{doc},
    layerPresenter{pres},
    focusDispatcher{d}
{

}

BaseElementContext::BaseElementContext(
        const iscore::DocumentContext& doc,
        ScenarioDocumentPresenter& pres):
    BaseElementContext{
        doc.document,
        pres,
        const_cast<ScenarioDocumentModel&>(pres.model()).focusManager()}
{

}
