#include <QApplication>
#include <QDebug>
#include <qnamespace.h>
#include <QSet>
#include <QSettings>
#include <QStandardItemModel>
#include <QStringList>
#include <QVariant>

#include <iscore/tools/std/Algorithms.hpp>
#include "PluginSettingsModel.hpp"
#include "commands/BlacklistCommand.hpp"
#include <iscore/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <boost/iterator/filter_iterator.hpp>

namespace PluginSettings
{
struct AddonVectorWrapper
{
        struct not_core_addon {
          bool operator()(const iscore::Addon& e) { return !e.corePlugin; }
        };

        const std::vector<iscore::Addon>& vec;


        using iterator = boost::filter_iterator<not_core_addon, std::vector<iscore::Addon>::iterator>;
        using const_iterator = boost::filter_iterator<not_core_addon, std::vector<iscore::Addon>::const_iterator>;

        auto size() const
        { return count_if(vec, [] (auto& e) { return !e.corePlugin; }); }

        auto begin() const
        { return const_iterator{vec.begin()}; }
        auto end() const
        { return const_iterator{vec.end()}; }
        auto cbegin() const
        { return const_iterator{vec.cbegin()}; }
        auto cend() const
        { return const_iterator{vec.cend()}; }

        auto& operator[](int i)
        {
            auto it = begin();
            std::advance(it, i);
            return *it;
        }
        auto& operator[](int i) const
        {
            auto it = begin();
            std::advance(it, i);
            return *it;
        }

};

class PluginItemModel : public QAbstractItemModel
{
        AddonVectorWrapper m_vec;
    public:
        enum class Column {
            Name, ShortDesc, Path
        };
        static constexpr const int ColumnCount = 5;

        PluginItemModel(const std::vector<iscore::Addon>& vec):
            m_vec{vec}
        {
        }

        QModelIndex index(int row, int column, const QModelIndex &parent) const override
        {
            if(row >= (int)m_vec.size() || row < 0)
                return {};

            if(column >= ColumnCount || column < 0)
                return {};

            return createIndex(row, column, nullptr);
        }

        QModelIndex parent(const QModelIndex &child) const override
        {
            return {};
        }

        int rowCount(const QModelIndex &parent) const override
        {
            return m_vec.size();
        }

        int columnCount(const QModelIndex &parent) const override
        {
            return ColumnCount;
        }

        QVariant data(const QModelIndex &index, int role) const override
        {
            auto row = index.row();
            auto column = (Column) index.column();
            if(row >= m_vec.size() || row < 0)
                return {};

            if(index.column() >= ColumnCount || index.column() < 0)
                return {};

            const iscore::Addon& addon = m_vec[row];

            switch(role)
            {
                case Qt::DisplayRole:
                {
                    switch(column)
                    {
                        case Column::Name:
                            return addon.name;
                            break;
                        case Column::ShortDesc:
                            return addon.shortDescription;
                            break;
                        case Column::Path:
                            return addon.path;
                            break;
                        default:
                            break;
                    }

                    return {};
                    break;
                }

                case Qt::FontRole:
                {
                    QFont f;
                    if(column == Column::Name)
                    {
                        f.setBold(true);
                    }
                    return f;
                }

                case Qt::DecorationRole:
                {
                    switch(column)
                    {
                        case Column::Name:
                        {
                            if(!addon.smallImage.isNull())
                            {
                                return QPixmap::fromImage(addon.smallImage);
                            }
                            return {};
                        }
                        default:
                            return {};
                    }

                    break;
                }
                case Qt::CheckStateRole:
                {
                    if(column == Column::Name)
                    {
                        return addon.enabled ? Qt::Checked : Qt::Unchecked;
                    }
                    else
                    {
                        return QVariant{};
                    }
                    break;
                }
            }

            return {};
        }
/*
        QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
        {
            if(orientation != Qt::Horizontal)
                return {};
            if(role != Qt::DisplayRole)
                return {};

            switch(section)
            {
                case 0:
                    return tr("Enabled");
                case 1:
                    return tr("Image");
                case 2:
                    return tr("Name");
                case 3:
                    return tr("Description");
                case 4:
                    return tr("Path");
                default:
                    return {};
            }
        }
*/
        Qt::ItemFlags flags(const QModelIndex &index) const override
        {
            Qt::ItemFlags flags = Qt::ItemIsEnabled;
            if(index.column() == 0)
                flags |= Qt::ItemIsUserCheckable;

            return flags;
        }
};

PluginSettingsModel::PluginSettingsModel(const iscore::ApplicationContext& ctx) :
    iscore::SettingsDelegateModel {}
{
    this->setObjectName("PluginSettingsModel");

    QSettings s;
    auto blacklist = s.value("PluginSettings/Blacklist", QStringList{}).toStringList();
    blacklist.sort();

    QStringList systemlist;
    for(auto& add : ctx.components.addons())
    {
        if(!add.path.isEmpty() && !add.corePlugin)
            systemlist.push_back(add.path);
    }
    systemlist.sort();

    m_plugins = new PluginItemModel(ctx.components.addons());
/*
    int i = 0;

    for(auto& plugin_name : systemlist)
    {
        QStandardItem* item = new QStandardItem(plugin_name);
        item->setCheckable(true);
        item->setCheckState(blacklist.contains(plugin_name) ? Qt::Unchecked : Qt::Checked);

        m_plugins->setItem(i++, 0, item);
    }

    auto diff = blacklist.toSet() - systemlist.toSet(); // The ones in the blacklist but not in the systemlist

    for(auto& plugin_name : diff)
    {
        QStandardItem* item = new QStandardItem(plugin_name);
        item->setCheckable(true);
        item->setCheckState(Qt::Checked);
    }

    connect(m_plugins,  &QStandardItemModel::itemChanged,
            this,		&PluginSettingsModel::on_itemChanged);
            */
}


void PluginSettingsModel::setFirstTimeSettings()
{
}
}
