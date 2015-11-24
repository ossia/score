#pragma once
#include <core/document/DocumentContext.hpp>
class BaseElementPresenter;
class ProcessFocusManager;

class BaseElementContext : public iscore::DocumentContext
{
    public:
        BaseElementContext(
                iscore::Document& doc,
                BaseElementPresenter& pres,
                ProcessFocusManager& d);

        BaseElementContext(
                iscore::DocumentContext& doc,
                BaseElementPresenter& pres);

        BaseElementPresenter& layerPresenter;
        ProcessFocusManager& focusDispatcher;
};
