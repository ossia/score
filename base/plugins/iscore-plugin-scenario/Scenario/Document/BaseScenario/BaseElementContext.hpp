#pragma once
#include <iscore/document/DocumentContext.hpp>
#include <Process/ProcessContext.hpp>

namespace Process { class ProcessFocusManager; }
namespace iscore {
class Document;
}  // namespace iscore

namespace Scenario
{
class ScenarioDocumentPresenter;

class BaseElementContext
{
    public:
        const Process::ProcessPresenterContext& context;
        ScenarioDocumentPresenter& layerPresenter;
};
}
