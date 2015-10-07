#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>

class CurveSegmentList;
class AutomationControl : public iscore::PluginControlInterface
{
    public:
        explicit AutomationControl(iscore::Presenter* pres);
        virtual ~AutomationControl() = default;

        virtual iscore::SerializableCommand* instantiateUndoCommand(const QString& name,
                                                                    const QByteArray& data) override;
    private:
        void setupCommands();
        void initColors();
};
