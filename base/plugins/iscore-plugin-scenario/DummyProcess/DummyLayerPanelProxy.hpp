#pragma once
#include <Process/LayerModelPanelProxy.hpp>

#include <iscore_lib_dummyprocess_export.h>

namespace Process { class LayerModel; }
class QObject;

class ISCORE_LIB_DUMMYPROCESS_EXPORT DummyLayerPanelProxy final :
        public Process::GraphicsViewLayerModelPanelProxy
{
    public:
        explicit DummyLayerPanelProxy(
                const Process::LayerModel& vm,
                QObject* parent);
};
