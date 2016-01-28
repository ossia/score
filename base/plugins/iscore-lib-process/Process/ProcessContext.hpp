#pragma once
#include <iscore/document/DocumentContext.hpp>
#include <Process/Focus/FocusDispatcher.hpp>

namespace Process { class LayerPresenter; }

struct LayerContext : public iscore::DocumentContext
{
        LayerContext(
                iscore::Document& doc,
                Process::LayerPresenter& pres,
                FocusDispatcher& d):
            iscore::DocumentContext{doc},
            layerPresenter{pres},
            focusDispatcher{d}
        {

        }

        LayerContext(
                const iscore::DocumentContext& doc,
                Process::LayerPresenter& pres,
                FocusDispatcher& d):
            LayerContext{doc.document, pres, d}
        {

        }



        Process::LayerPresenter& layerPresenter;
        FocusDispatcher& focusDispatcher;
};
