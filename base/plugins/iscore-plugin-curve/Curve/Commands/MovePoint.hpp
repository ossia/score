#pragma once
#include <Curve/Commands/CurveCommandFactory.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <Curve/Palette/CurvePoint.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_plugin_curve_export.h>
class CurveModel;
class CurvePointModel;
class DataStreamInput;
class DataStreamOutput;

class ISCORE_PLUGIN_CURVE_EXPORT MovePoint final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(CurveCommandFactoryName(), MovePoint, "Move a point")
        public:
            MovePoint(Path<CurveModel>&& model,
                      const Id<CurvePointModel>& pointId,
                      Curve::Point newPoint);

        void undo() const override;
        void redo() const override;

        void update(Path<CurveModel>&& model,
                    const Id<CurvePointModel>& pointId,
                    const Curve::Point& newPoint);

    protected:
        void serializeImpl(DataStreamInput & s) const override;
        void deserializeImpl(DataStreamOutput & s) override;

    private:
        Path<CurveModel> m_model;
        Id<CurvePointModel> m_pointId;
        Curve::Point m_newPoint;
        Curve::Point m_oldPoint;
};

