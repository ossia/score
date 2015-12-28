#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <src/Commands/SpaceCommandFactory.hpp>
#include <src/Area/AreaFactoryKey.hpp>
namespace Space { class ProcessModel; }
class AreaModel;
class DimensionModel;
class AddArea : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(SpaceCommandFactoryName(), AddArea, "AddArea")
    public:

          AddArea(Path<Space::ProcessModel>&& spacProcess,
            AreaFactoryKey type,
            const QString& area,
                  const QMap<Id<DimensionModel>, QString>& dimMap,
                  const QMap<QString, iscore::FullAddressSettings>& addrMap);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput & s) const override;
        void deserializeImpl(DataStreamOutput & s) override;

    private:
        Path<Space::ProcessModel> m_path;
        Id<AreaModel> m_createdAreaId;

        AreaFactoryKey m_areaType{"Generic"};
        QString m_areaFormula;

        QMap<Id<DimensionModel>, QString> m_dimensionToVarMap;
        QMap<QString, iscore::FullAddressSettings> m_symbolToAddressMap;
};
