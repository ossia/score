#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <DeviceExplorer/Address/AddressSettings.hpp>

class SpaceProcess;
class AreaModel;
class AddArea : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL2("AreaPlugin", "AddArea", "AddArea")
    public:
            ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR2(AddArea)

        AddArea(ModelPath<SpaceProcess>&& spacProcess,
            const QString& area,
                  const QMap<QString, QString>& dimMap,
                  const QMap<QString, QPair<bool, iscore::FullAddressSettings>>& addrMap);

        void undo() override;
        void redo() override;

    protected:
        void serializeImpl(QDataStream & s) const override;
        void deserializeImpl(QDataStream & s) override;

    private:
        ModelPath<SpaceProcess> m_path;
        id_type<AreaModel> m_createdAreaId;

        QString m_areaFormula;

        QMap<QString, QString> m_varToDimensionMap;
        QMap<QString, QPair<bool, iscore::FullAddressSettings>> m_symbolToAddressMap;
};
