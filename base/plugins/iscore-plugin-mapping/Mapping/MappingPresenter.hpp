#pragma once
#include <Curve/Process/CurveProcessPresenter.hpp>
#include <Curve/CurveStyle.hpp>

#include <Mapping/MappingModel.hpp>
#include <Mapping/MappingLayerModel.hpp>
#include <Mapping/MappingView.hpp>

#include <Process/ProcessContext.hpp>

namespace Mapping
{
class MappingPresenter :
        public Curve::CurveProcessPresenter<
            LayerModel,
            MappingView>
{
    public:
        MappingPresenter(
                const iscore::DocumentContext& context,
                const Curve::Style& style,
                const LayerModel& layer,
                MappingView* view,
                QObject* parent):
            CurveProcessPresenter{context, style, layer, view, parent}
        {
          ISCORE_TODO;
        }
};
}
