#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

class AddPoint : public iscore::SerializableCommand
{
    public:
        AddPoint();
        AddPoint(ObjectPath&& pointPath, double x, double y);

        virtual void undo() override;
        virtual void redo() override;
        virtual bool mergeWith(const Command* other) override;

    protected:
        virtual void serializeImpl(QDataStream&) const override;
        virtual void deserializeImpl(QDataStream&) override;

    private:
        ObjectPath m_path;

        double m_x {};
        double m_y {};
};
