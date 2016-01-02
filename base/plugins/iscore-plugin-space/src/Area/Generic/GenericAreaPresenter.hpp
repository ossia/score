#pragma once
#include "src/Area/AreaPresenter.hpp"

namespace Space
{
class GenericAreaView;
class GenericAreaModel;

class GenericAreaPresenter : public AreaPresenter
{
    public:
        using model_type = GenericAreaModel;
        using view_type = GenericAreaView;

        GenericAreaPresenter(
                GenericAreaView *view,
                const GenericAreaModel &model,
                QObject* parent);

        void update() override;
        void on_areaChanged(ValMap) override;

};
}
