#pragma once
#include <QPointF>
#include <QMetaType>

namespace Process { class LayerPresenter; }

namespace Scenario
{
struct ScenarioRecordInitData
{
        ScenarioRecordInitData() {}
        ScenarioRecordInitData(const Process::LayerPresenter* lp, QPointF p):
            presenter{lp},
            point{p}
        {
        }

        const Process::LayerPresenter* presenter{};
        QPointF point;
};
}
Q_DECLARE_METATYPE(ScenarioRecordInitData)
