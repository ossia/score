#include "ImageListChooser.hpp"

#include <Process/Commands/SetControlValue.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/graphics/RectItem.hpp>
#include <score/graphics/TextItem.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QFileDialog>
#include <QHBoxLayout>
#include <QListView>
#include <QPushButton>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <wobjectimpl.h>

#include <verdigris>
namespace Gfx::Images
{

ImageListChooser::~ImageListChooser() { }
ImageListChooser::ImageListChooser(
    const std::vector<QString>& init, const QString& name, Id<Port> id, QObject* parent)
    : ControlInlet{id, parent}
{
  hidden = true;
  std::vector<ossia::value> init_v;
  for(auto& v : init)
    init_v.push_back(v.toStdString());
  setValue(std::move(init_v));
  setName(name);
}

}

class EditableTable : public QWidget
{
  W_OBJECT(EditableTable)
public:
  QListView* table{};
  QStandardItemModel* model{};

  EditableTable(QWidget* parent = nullptr)
      : QWidget{parent}
  {
    setContentsMargins(0, 0, 0, 0);
    auto lay = new QVBoxLayout{this};
    model = new QStandardItemModel{};
    model->insertColumns(0, 1);
    model->setHorizontalHeaderLabels({tr("Path")});

    table = new QListView;

    table->setModel(model);
    lay->addWidget(table);

    auto btn_lay = new QHBoxLayout;
    auto add = new QPushButton{tr("+")};
    connect(add, &QPushButton::clicked, this, &EditableTable::on_addItems);
    auto rm = new QPushButton{tr("-")};
    connect(rm, &QPushButton::clicked, this, &EditableTable::on_removeItem);
    btn_lay->addWidget(add);
    btn_lay->addWidget(rm);
    lay->addLayout(btn_lay);
  }

  void addCheckedItem(QString f)
  {
    auto path = new QStandardItem{f};
    model->insertRow(model->rowCount(), QList<QStandardItem*>{path});
  }

  void addItem(QString f)
  {
    auto file = QImage(f);
    if(file.width() > 0 && file.height() > 0)
    {
      addCheckedItem(f);
    }
  }

  void clear()
  {
    while(model->rowCount() > 0)
      model->removeRow(model->rowCount() - 1);
  }

  void setItems(const std::vector<ossia::value>& v)
  {
    clear();

    for(auto& val : v)
    {
      const std::string& str = ossia::convert<std::string>(val);
      addCheckedItem(QString::fromStdString(str));
    }
  }

  ossia::value value() const noexcept
  {
    std::vector<ossia::value> vec;

    for(int i = 0; i < model->rowCount(); i++)
    {
      vec.push_back(model->item(i, 0)->text().toStdString());
    }

    return vec;
  }

  void itemsChanged() W_SIGNAL(itemsChanged);

private:
  void on_addItems()
  {
    auto files = QFileDialog::getOpenFileNames(
        this, tr("Choose images..."), QString{},
        QString{"Images (*.png *.jpg *.jpeg *.gif *.bmp *.tiff)"});
    for(auto f : files)
    {
      addItem(f);
    }
    itemsChanged();
  }

  void on_removeItem()
  {
    auto indices = table->selectionModel()->selectedIndexes();
    fc::flat_set<ossia::small_pod_vector<int, 16>, std::greater<>> rows;
    for(auto& index : indices)
      rows.insert(index.row());
    for(int index : rows)
      model->removeRow(index);
    itemsChanged();
  }
};

QWidget* WidgetFactory::ImageListChooserItems::make_widget(
    const Gfx::Images::ImageListChooser& slider,
    const Gfx::Images::ImageListChooser& inlet, const score::DocumentContext& ctx,
    QWidget* parent, QObject* context)
{
  auto widg = new EditableTable;
  auto vec = ossia::convert<std::vector<ossia::value>>(inlet.value());
  for(auto& val : vec)
  {
    if(const std::string* str = val.target<std::string>())
    {
      widg->addCheckedItem(QString::fromStdString(*str));
    }
  }

  QObject::connect(widg, &EditableTable::itemsChanged, context, [widg, &inlet, &ctx]() {
    CommandDispatcher<> disp{ctx.commandStack};
    disp.submit<Process::SetControlValue>(inlet, widg->value());
  });

  QObject::connect(
      &inlet, &Gfx::Images::ImageListChooser::valueChanged, widg,
      [widg](const ossia::value& val) {
    widg->setItems(ossia::convert<std::vector<ossia::value>>(val));
      });

  return widg;
}

QGraphicsItem* WidgetFactory::ImageListChooserItems::make_item(
    const Gfx::Images::ImageListChooser& slider,
    const Gfx::Images::ImageListChooser& inlet, const score::DocumentContext& ctx,
    QGraphicsItem* parent, QObject* context)
{
  return new score::EmptyItem{parent};
}

W_OBJECT_IMPL(EditableTable)
