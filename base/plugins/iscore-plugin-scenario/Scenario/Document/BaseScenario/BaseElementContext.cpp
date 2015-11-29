#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>

#include "BaseElementContext.hpp"
#include "core/document/DocumentContext.hpp"

namespace iscore {
class Document;
}  // namespace iscore

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
