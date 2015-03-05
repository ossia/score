#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

class RemovePoint : public iscore::SerializableCommand
{
    public:
        RemovePoint();
        RemovePoint(ObjectPath&& pointPath, double x);

        virtual void undo() override;
        virtual void redo() override;
        virtual bool mergeWith(const Command* other) override;

    protected:
        virtual void serializeImpl(QDataStream&) const override;
        virtual void deserializeImpl(QDataStream&) override;

    private:
        ObjectPath m_path;

        double m_x {};
        double m_oldY {};
};
