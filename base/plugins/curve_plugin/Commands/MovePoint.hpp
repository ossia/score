#pragma once
#include <public_interface/command/SerializableCommand.hpp>
#include <public_interface/tools/ObjectPath.hpp>

class MovePoint : public iscore::SerializableCommand
{
    public:
        MovePoint();
        MovePoint(ObjectPath&& pointPath,
                  double oldx, double newx, double newy);

        virtual void undo() override;
        virtual void redo() override;
        virtual bool mergeWith(const Command* other) override;

    protected:
        virtual void serializeImpl(QDataStream&) const override;
        virtual void deserializeImpl(QDataStream&) override;

    private:
        ObjectPath m_path;

        double m_oldX {};
        double m_oldY {};

        double m_newX {};
        double m_newY {};
};
