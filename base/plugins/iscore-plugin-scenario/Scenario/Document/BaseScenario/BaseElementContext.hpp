#pragma once
#include <iscore/document/DocumentContext.hpp>
#include <Process/ProcessContext.hpp>

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
