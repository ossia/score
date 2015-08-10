#pragma once
#include "src/Area/AreaPresenter.hpp"
class CircleAreaView;
class CircleAreaModel;

class CircleAreaPresenter : public AreaPresenter
{
    public:
        using model_type = CircleAreaModel;
        using view_type = CircleAreaView;
        CircleAreaPresenter(CircleAreaView *view,
                const CircleAreaModel &model,
                QObject* parent);

        void update() override;
        void on_areaChanged() override;

};
