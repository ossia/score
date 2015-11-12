#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <src/Commands/SpaceCommandFactory.hpp>


class SpaceProcess;
class AreaModel;
class DimensionModel;
class AddArea : public iscore::SerializableCommand
{
        ISCORE_SERIALIZABLE_COMMAND_DECL(SpaceCommandFactoryName(), AddArea, "AddArea")
    public:

          AddArea(Path<SpaceProcess>&& spacProcess,
            int type,
            const QString& area,
                  const QMap<Id<DimensionModel>, QString>& dimMap,
                  const QMap<QString, iscore::FullAddressSettings>& addrMap);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(QDataStream & s) const override;
        void deserializeImpl(QDataStream & s) override;

    private:
        Path<SpaceProcess> m_path;
        Id<AreaModel> m_createdAreaId;

        int m_areaType{-1};
        QString m_areaFormula;

        QMap<Id<DimensionModel>, QString> m_dimensionToVarMap;
        QMap<QString, iscore::FullAddressSettings> m_symbolToAddressMap;
};
