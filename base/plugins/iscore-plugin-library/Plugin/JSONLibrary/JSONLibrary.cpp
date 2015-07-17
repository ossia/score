#include "JSONLibrary.hpp"
#include <QJsonDocument>
#include <QDebug>
#include <QMimeData>
#include <QMap>

#include <State/StateMimeTypes.hpp>
#include "Plugin/Panel/DeviceExplorerMimeTypes.hpp"
enum class LibraryColumns
{
    Name, Category, Tags, Json
};


JSONModel::JSONModel()
{
    elements.append(LibraryElement{"dada", Category::State, {"da di do", "yada"}, {}});
}

QModelIndex JSONModel::index(int row, int column, const QModelIndex &parent) const
{
    if(row >= elements.size() || column >= 4 || row < 0 || column < 0)
        return {};

    switch(column)
    {
        case (int)LibraryColumns::Name:
            return createIndex(row, column, (void*)&elements[row].name);
        case (int)LibraryColumns::Category:
            return createIndex(row, column, (void*)&elements[row].category);
        case (int)LibraryColumns::Tags:
            return createIndex(row, column, (void*)&elements[row].tags);
        case (int)LibraryColumns::Json:
            return createIndex(row, column, (void*)&elements[row].obj);
        default:
            return {};
    }
}

QModelIndex JSONModel::parent(const QModelIndex &child) const
{
    return {};
}

int JSONModel::rowCount(const QModelIndex &parent) const
{
    return elements.size();
}

int JSONModel::columnCount(const QModelIndex &parent) const
{
    return 4;
}

QVariant JSONModel::data(const QModelIndex &index, int role) const
{
    if(role == Qt::CheckStateRole)
        return {};
    const auto& e = elements.at(index.row());
    switch(index.column())
    {
        case (int)LibraryColumns::Name:
            return e.name;
        case (int)LibraryColumns::Category:
            return categoryPrettyName()[e.category];
        case (int)LibraryColumns::Tags:
            return e.tags.join(", ");
        case (int)LibraryColumns::Json:
            return e.obj;
        default:
            return {};
    }
}


QVariant JSONModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Vertical)
        return {};
    if(!(role == Qt::DisplayRole || role == Qt::ToolTipRole || role ==Qt::WhatsThisRole))
        return {};

    switch(section)
    {
        case (int)LibraryColumns::Name:
            return tr("Name");
            break;
        case (int)LibraryColumns::Category:
            return tr("Category");
            break;
        case (int)LibraryColumns::Tags:
            return tr("Tags");
            break;
        case (int)LibraryColumns::Json:
            return tr("Data");
            break;
        default:
            break;
    }

    return {};
}

Qt::ItemFlags JSONModel::flags(const QModelIndex &index) const
{
    if (index.isValid())
    {
        qDebug() << "yeah";
        return Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }
    else
        return Qt::ItemIsDropEnabled | Qt::ItemIsEnabled;
}


bool JSONModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if(row > rowCount(parent) || row < 0)
        return false;

    beginRemoveRows(parent, row, row+count);
    for(; count --> 0; )
        elements.removeAt(row);
    endRemoveRows();

    return true;
}

bool JSONModel::moveRows(
        const QModelIndex &sourceParent,
        int sourceRow,
        int count,
        const QModelIndex &destinationParent,
        int destinationChild)
{
    return false;
}


// TODO refactor with device explorer
static const QString MimeTypeScenarioData = "application/x-iscore-scenariodata";
static const QString MimeTypeProcess = "application/x-iscore-processdata";
static const QMap<Category, QString> mimeTypeMap{
	{Category::State, iscore::mime::state()},
    {Category::ScenarioData, MimeTypeScenarioData},
    {Category::Process, MimeTypeProcess},
	{Category::Device, iscore::mime::device()},
	{Category::Address, iscore::mime::address()}
};

static const QMap<Category, QString>& categoryMimeTypeMap()
{
    return mimeTypeMap;
}

QStringList JSONModel::mimeTypes() const
{
    return {
		iscore::mime::state(),
        MimeTypeScenarioData,
        MimeTypeProcess,
		iscore::mime::device(),
		iscore::mime::address()
    };
}

QMimeData *JSONModel::mimeData(const QModelIndexList &indexes) const
{
    // Only 1 index for now
    QMimeData *mimeData = new QMimeData;

    const auto& index = indexes.first();
    const auto& elt = elements.at(index.row());

    QJsonDocument doc{elt.obj};
    auto text = doc.toJson(QJsonDocument::Indented);

    mimeData->setData(categoryMimeTypeMap()[elt.category], text);
    return mimeData;
}
#include <iostream>
bool JSONModel::canDropMimeData(
        const QMimeData *data,
        Qt::DropAction action,
        int row, int column,
        const QModelIndex &parent) const
{
    for(auto& elt : data->formats())
    {
        std::cerr << elt.toStdString() << std::endl;
        if(mimeTypes().contains(elt))
        {
            return true;
        }
    }

    return false;
}

bool JSONModel::dropMimeData(
        const QMimeData *data,
        Qt::DropAction action,
        int row,
        int column,
        const QModelIndex &parent)
{
    beginInsertRows(parent, elements.size(), elements.size());
    elements.push_back(
                LibraryElement
                    {"Name me!",
                     categoryMimeTypeMap().key(data->formats().first(), Category(-1)),
                     {"no tags"},
                     QJsonObject{}});
    endInsertRows();
    return false;
}

Qt::DropActions JSONModel::supportedDropActions() const
{
    return Qt::CopyAction;
}

Qt::DropActions JSONModel::supportedDragActions() const
{
    return Qt::CopyAction;
}
