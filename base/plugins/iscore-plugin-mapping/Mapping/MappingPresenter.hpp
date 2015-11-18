#pragma once
#include <Curve/Process/CurveProcessPresenter.hpp>
#include <Curve/CurveStyle.hpp>

#include <Mapping/MappingModel.hpp>
#include <Mapping/MappingLayerModel.hpp>
#include <Mapping/MappingView.hpp>

#include <Process/ProcessContext.hpp>

class MappingPresenter :
        public CurveProcessPresenter<
            MappingLayerModel,
            MappingView>
{
    public:
        MappingPresenter(
                iscore::DocumentContext& context,
                const Curve::Style& style,
                const MappingLayerModel& layer,
                MappingView* view,
                QObject* parent):
            CurveProcessPresenter{context, style, layer, view, parent}
        {
          ISCORE_TODO;
        }
};
