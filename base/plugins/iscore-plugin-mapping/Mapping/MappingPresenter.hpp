#pragma once
#include <Curve/Process/CurveProcessPresenter.hpp>
#include <Curve/CurveStyle.hpp>


#include "MappingModel.hpp"
#include "MappingLayerModel.hpp"
#include "MappingView.hpp"
class MappingPresenter :
        public CurveProcessPresenter<
            MappingLayerModel,
            MappingView>
{
    public:
        MappingPresenter(
                Curve::EditionSettings& set,
                const CurveStyle& style,
                const MappingLayerModel& layer,
                MappingView* view,
                QObject* parent):
            CurveProcessPresenter{set, style, layer, view, parent}
        {
          ISCORE_TODO;
        }
};
