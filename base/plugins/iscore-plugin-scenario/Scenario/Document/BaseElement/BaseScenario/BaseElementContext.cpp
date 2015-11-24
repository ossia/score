#include "BaseElementContext.hpp"
#include <Scenario/Document/BaseElement/BaseElementModel.hpp>
#include <Scenario/Document/BaseElement/BaseElementPresenter.hpp>
BaseElementContext::BaseElementContext(
        iscore::Document& doc,
        BaseElementPresenter& pres,
        ProcessFocusManager& d):
    iscore::DocumentContext{doc},
    layerPresenter{pres},
    focusDispatcher{d}
{

}

BaseElementContext::BaseElementContext(
        const iscore::DocumentContext& doc,
        BaseElementPresenter& pres):
    BaseElementContext{
        doc.document,
        pres,
        const_cast<BaseElementModel&>(pres.model()).focusManager()}
{

}
