#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <DeviceExplorer/Address/AddressSettings.hpp>

class SpaceProcess;
class AreaModel;
class AddCircle : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL2("AreaPlugin", "AddCircle", "AddCircle")
    public:
            ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR2(AddCircle)

          AddCircle(
            ModelPath<SpaceProcess>&& spacProcess,
            const QMap<QString, QString>& dimMap,
            const QMap<QString, iscore::FullAddressSettings>& addrMap);

        void undo() override;
        void redo() override;

    protected:
        void serializeImpl(QDataStream & s) const override;
        void deserializeImpl(QDataStream & s) override;

    private:
        ModelPath<SpaceProcess> m_path;
        id_type<AreaModel> m_createdAreaId;

        QMap<QString, QString> m_varToDimensionMap;
        QMap<QString, iscore::FullAddressSettings> m_symbolToAddressMap;
};
