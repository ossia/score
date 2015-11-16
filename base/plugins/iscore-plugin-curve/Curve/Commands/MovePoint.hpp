#pragma once
#include <Curve/Commands/CurveCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include "Curve/Point/CurvePointModel.hpp"
#include "Curve/CurveModel.hpp"

class CurveSegmentModel;
class CurvePointModel;

class MovePoint final : public iscore::SerializableCommand
{
	ISCORE_SERIALIZABLE_COMMAND_DECL(CurveCommandFactoryName(), MovePoint, "MovePoint")
    public:
	MovePoint(Path<CurveModel>&& model,
		  const Id<CurvePointModel>& pointId,
          CurvePoint newPoint);

	void undo() const override;
	void redo() const override;

	void update(Path<CurveModel>&& model,
		    const Id<CurvePointModel>& pointId,
		    const CurvePoint& newPoint);

    protected:
	void serializeImpl(QDataStream & s) const override;
	void deserializeImpl(QDataStream & s) override;

    private:
	Path<CurveModel> m_model;
	Id<CurvePointModel> m_pointId;
    CurvePoint m_newPoint;
	CurvePoint m_oldPoint;
};

