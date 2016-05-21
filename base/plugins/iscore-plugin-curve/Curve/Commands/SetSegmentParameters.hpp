#pragma once
#include <Curve/Commands/CurveCommandFactory.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <QMap>
#include <QPair>

#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_plugin_curve_export.h>

struct DataStreamInput;
struct DataStreamOutput;

namespace Curve
{
class Model;
class SegmentModel;
using SegmentParameterMap = QMap<Id<SegmentModel>, QPair<double, double>>;
class ISCORE_PLUGIN_CURVE_EXPORT SetSegmentParameters final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(CommandFactoryName(), SetSegmentParameters, "Set segment parameters")
    public:
        SetSegmentParameters(Path<Model>&& model, SegmentParameterMap&& parameters);

        void undo() const override;
        void redo() const override;

        void update(Path<Model>&& model, SegmentParameterMap&&  segments);

    protected:
        void serializeImpl(DataStreamInput & s) const override;
        void deserializeImpl(DataStreamOutput & s) override;

    private:
        Path<Model> m_model;
        SegmentParameterMap m_new;
        QMap<
            Id<SegmentModel>,
            QPair<
                optional<double>,
                optional<double>
            >
        > m_old;
};
}
