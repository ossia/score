#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

class RemovePoint : public iscore::SerializableCommand
{
    public:
        RemovePoint();
        RemovePoint(ObjectPath&& pointPath, double x);

        virtual void undo() override;
        virtual void redo() override;

    protected:
        virtual void serializeImpl(QDataStream&) const override;
        virtual void deserializeImpl(QDataStream&) override;

    private:
        ObjectPath m_path;

        double m_x {};
        double m_oldY {};
};
