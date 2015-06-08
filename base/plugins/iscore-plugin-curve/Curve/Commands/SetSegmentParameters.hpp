#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

class CurveSegmentModel;
using SegmentParameterMap = QMap<id_type<CurveSegmentModel>, QPair<double, double>>;
class SetSegmentParameters : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("SetSegmentParameters", "SetSegmentParameters")
    public:
        ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(SetSegmentParameters, "AutomationControl")
        SetSegmentParameters(ObjectPath&& model, SegmentParameterMap&& parameters);

        void undo() override;
        void redo() override;

        void update(ObjectPath&& model, SegmentParameterMap&&  segments);

    protected:
        void serializeImpl(QDataStream & s) const override;
        void deserializeImpl(QDataStream & s) override;

    private:
        ObjectPath m_model;
        SegmentParameterMap m_new;
        QMap<
            id_type<CurveSegmentModel>,
            QPair<
                boost::optional<double>,
                boost::optional<double>
            >
        > m_old;
};
