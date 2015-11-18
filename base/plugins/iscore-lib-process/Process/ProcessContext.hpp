#pragma once
#include <core/document/DocumentContext.hpp>
#include <Process/Focus/FocusDispatcher.hpp>

class LayerPresenter;

struct LayerContext : public iscore::DocumentContext
{
        LayerContext(
                iscore::Document& doc,
                LayerPresenter& pres,
                FocusDispatcher& d):
            iscore::DocumentContext{doc},
            layerPresenter{pres},
            focusDispatcher{d}
        {

        }

        LayerContext(
                iscore::DocumentContext& doc,
                LayerPresenter& pres,
                FocusDispatcher& d):
            LayerContext{doc.document, pres, d}
        {

        }



        LayerPresenter& layerPresenter;
        FocusDispatcher& focusDispatcher;
};
