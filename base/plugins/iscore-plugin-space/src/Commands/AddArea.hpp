#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <DeviceExplorer/Address/AddressSettings.hpp>

class SpaceProcess;
class AreaModel;
class DimensionModel;
class AddArea : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL2("SpaceControl", "AddArea", "AddArea")
    public:
            ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR2(AddArea)

          AddArea(ModelPath<SpaceProcess>&& spacProcess,
            int type,
            const QString& area,
                  const QMap<id_type<DimensionModel>, QString>& dimMap,
                  const QMap<QString, iscore::FullAddressSettings>& addrMap);

        void undo() override;
        void redo() override;

    protected:
        void serializeImpl(QDataStream & s) const override;
        void deserializeImpl(QDataStream & s) override;

    private:
        ModelPath<SpaceProcess> m_path;
        id_type<AreaModel> m_createdAreaId;

        int m_areaType{-1};
        QString m_areaFormula;

        QMap<id_type<DimensionModel>, QString> m_dimensionToVarMap;
        QMap<QString, iscore::FullAddressSettings> m_symbolToAddressMap;
};
