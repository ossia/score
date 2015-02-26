#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

class AddPoint : public iscore::SerializableCommand
{
    public:
        AddPoint();
        AddPoint(ObjectPath&& pointPath, double x, double y);

        virtual void undo() override;
        virtual void redo() override;
        virtual int id() const override;
        virtual bool mergeWith(const QUndoCommand* other) override;

    protected:
        virtual void serializeImpl(QDataStream&) const override;
        virtual void deserializeImpl(QDataStream&) override;

    private:
        ObjectPath m_path;

        double m_x {};
        double m_y {};
};
