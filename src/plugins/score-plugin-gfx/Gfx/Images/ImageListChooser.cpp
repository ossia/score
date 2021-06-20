#include "ImageListChooser.hpp"
#include <score/document/DocumentContext.hpp>

#include <QStandardItemModel>
#include <QTableView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <verdigris>

#include <ossia/network/value/value_conversion.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <Process/Commands/SetControlValue.hpp>
namespace Gfx::Images
{


ImageListChooser::~ImageListChooser() { }
ImageListChooser::ImageListChooser(
    const std::vector<QString>& init,
    const QString& name,
    Id<Port> id,
    QObject* parent)
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
  QTableView* table{};
  QStandardItemModel* model{};

  EditableTable(QWidget* parent = nullptr)
      : QWidget{parent}
  {
    setContentsMargins(0, 0, 0, 0);
    auto lay = new QVBoxLayout{this};
    model = new QStandardItemModel{};
    model->insertColumns(0, 2);
    model->setHeaderData(0, Qt::Vertical, QObject::tr("Image"), Qt::DisplayRole);
    model->setHeaderData(1, Qt::Vertical, QObject::tr("Path"), Qt::DisplayRole);
    table = new QTableView;
    table->setModel(model);
    lay->addWidget(table);

    auto btn_lay = new QHBoxLayout;
    auto add = new QPushButton{tr("+")};
    connect(add, &QPushButton::clicked, this, &EditableTable::addItems);
    auto rm = new QPushButton{tr("-")};
    connect(add, &QPushButton::clicked, this, &EditableTable::removeItem);
    btn_lay->addWidget(add);
    btn_lay->addWidget(rm);
    lay->addLayout(btn_lay);
  }

  void addItems()
  {
    auto files = QFileDialog::getOpenFileNames(this, tr("Choose images..."), QString{}, QString{"Images (*.png *.jpg *.jpeg *.gif *.bmp *.tiff)"});
    for(auto f : files)
    {
      addItem(f);
    }
  }

  void addItem(QString f)
  {
    auto file = QImage(f);
    if(file.width() > 0 && file.height() > 0)
    {
      auto path = new QStandardItem{f};
      model->insertRow(model->rowCount(), QList<QStandardItem*>{path});
    }
  }
  void removeItem()
  {
    auto indices = table->selectionModel()->selectedIndexes();
    std::vector<int> rows;
    for(auto& index : indices)
      rows.push_back(index.row());
    std::sort(rows.begin(), rows.end());
    for(auto it = rows.rbegin(); it != rows.rend(); ++it)
    {
      qDebug() << "removing row:" << *it;
      model->removeRow(*it);
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

      auto path = new QStandardItem{QString::fromStdString(str)};
      model->insertRow(model->rowCount(), QList<QStandardItem*>{path});
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

};

QWidget* WidgetFactory::ImageListChooserItems::make_widget(
    const Gfx::Images::ImageListChooser& slider,
    const Gfx::Images::ImageListChooser& inlet,
    const score::DocumentContext& ctx,
    QWidget* parent,
    QObject* context)
{
  auto widg = new EditableTable;
  auto vec = ossia::convert<std::vector<ossia::value>>(inlet.value());
  for(auto& val : vec)
  {
    if(const std::string* str = val.target<std::string>())
    {
      widg->addItem(QString::fromStdString(*str));
    }
  }

  QObject::connect(
      widg, &EditableTable::itemsChanged, context, [widg, &inlet, &ctx]() {
        CommandDispatcher<> disp{ctx.commandStack};
        disp.submit<Process::SetControlValue>(inlet, widg->value());
      });

  QObject::connect(
      &inlet, &Gfx::Images::ImageListChooser::valueChanged, widg, [widg] (const ossia::value& val) {

        widg->setItems(ossia::convert<std::vector<ossia::value>>(val));
      });

  return widg;
}

QGraphicsItem* WidgetFactory::ImageListChooserItems::make_item(const Gfx::Images::ImageListChooser& slider, const Gfx::Images::ImageListChooser& inlet, const score::DocumentContext& ctx, QGraphicsItem* parent, QObject* context)
{
  return nullptr;
  /*
    auto sl = new LineEditItem{};
    sl->setTextWidth(180.);
    sl->setDefaultTextColor(QColor{"#E0B01E"});
    sl->setCursor(Qt::IBeamCursor);

  sl->setPlainText(
      QString::fromStdString(ossia::convert<std::string>(inlet.value())));

  auto doc = sl->document();
  QObject::connect(
      doc, &QTextDocument::contentsChanged, context, [=, &inlet, &ctx] {
        auto cur_str = ossia::convert<std::string>(inlet.value());
        if (cur_str != doc->toPlainText().toStdString())
        {
          CommandDispatcher<>{ctx.commandStack}
              .submit<SetControlValue<Control_T>>(
                  inlet, doc->toPlainText().toStdString());
        }
      });
  QObject::connect(
      &inlet, &Control_T::valueChanged, sl, [=](const ossia::value& val) {
        auto str = QString::fromStdString(ossia::convert<std::string>(val));
        if (str != doc->toPlainText())
          doc->setPlainText(str);
      });

  return sl;
  */
}

W_OBJECT_IMPL(EditableTable)
