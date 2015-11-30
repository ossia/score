#pragma once
#include <core/document/DocumentContext.hpp>

class ProcessFocusManager;
class ScenarioDocumentPresenter;
namespace iscore {
class Document;
}  // namespace iscore

class BaseElementContext : public iscore::DocumentContext
{
    public:
        BaseElementContext(
                iscore::Document& doc,
                ScenarioDocumentPresenter& pres,
                ProcessFocusManager& d);

        BaseElementContext(
                const iscore::DocumentContext& doc,
                ScenarioDocumentPresenter& pres);

        ScenarioDocumentPresenter& layerPresenter;
        ProcessFocusManager& focusDispatcher;
};
