#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_ARTNET)
#include "ArtnetProtocolFactory.hpp"
#include "ArtnetProtocolSettingsWidget.hpp"
#include "ArtnetSpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Library/LibrarySettings.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/model/tree/TreeNodeItemModel.hpp>
#include <score/tools/FindStringInFile.hpp>
#include <score/tools/ListNetworkAddresses.hpp>

#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/math.hpp>
#include <ossia/detail/string_algorithms.hpp>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QTableWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QVariant>

#include <re2/re2.h>

#include <ctre.hpp>
#include <wobjectimpl.h>

W_OBJECT_IMPL(Protocols::ArtnetProtocolSettingsWidget)

namespace Protocols
{

struct Pixel
{
  int16_t x{}, y{}, z{};
  QString name;
};

struct PixelGroup
{
  QString name;
  std::vector<int> pixels; // Indices in pixels array of PixelMatrix
};

struct PixelMatrix
{
  std::vector<Pixel> pixels;
  std::vector<PixelGroup> groups;
};

struct FixtureMode
{
  QString name;
  std::vector<QString> allChannels;
  std::vector<Artnet::Channel> channels;

  QString content() const noexcept
  {
    QString str;
    str.reserve(500);
    int k = 0;
    for(auto& chan : allChannels)
    {
      str += QString::number(k);
      str += ": \t";
      str += chan.isEmpty() ? "<No function>" : chan;
      str += "\n";
      k++;
    }
    return str;
  }
};

static constexpr auto leq_rexp_str = ctll::fixed_string{R"_(<=([0-9]+))_"};
static constexpr auto geq_rexp_str = ctll::fixed_string{R"_(>=([0-9]+))_"};
static constexpr auto lst_rexp_str = ctll::fixed_string{R"_(<([0-9]+))_"};
static constexpr auto gst_rexp_str = ctll::fixed_string{R"_(>([0-9]+))_"};
static constexpr auto eq_rexp_str = ctll::fixed_string{R"_(=([0-9]+))_"};
static constexpr auto arith1_rexp_str = ctll::fixed_string{R"_(([0-9]+)n)_"};
static constexpr auto arith_rexp_str = ctll::fixed_string{R"_(([0-9]+)n\+([0-9]+))_"};

static constexpr auto leq_rex = ctre::match<leq_rexp_str>;
static constexpr auto geq_rex = ctre::match<geq_rexp_str>;
static constexpr auto lst_rex = ctre::match<lst_rexp_str>;
static constexpr auto gst_rex = ctre::match<gst_rexp_str>;
static constexpr auto eq_rex = ctre::match<eq_rexp_str>;
static constexpr auto arith1_rex = ctre::match<arith1_rexp_str>;
static constexpr auto arith_rex = ctre::match<arith_rexp_str>;

class FixtureData
{
public:
  using ChannelMap = ossia::hash_map<QString, Artnet::Channel>;
  QString name{};
  QStringList tags{};
  QIcon icon{};

  PixelMatrix matrix{};

  std::vector<FixtureMode> modes{};

  // Function returns false if the item must be filtered
  static std::function<bool(int)> constraint_pos(std::string_view cst) noexcept
  {
    // Invalid constraint : we keep the item
    if(cst.empty())
      return [](int p) { return true; };

    if(cst == "even")
      return [](int p) { return (p % 2) == 0; };
    else if(cst == "odd")
      return [](int p) { return (p % 2) == 1; };
    else if(auto [whole, n] = leq_rex(cst); whole)
      return [num = n.to_number()](int p) { return p <= num; };
    else if(auto [whole, n] = geq_rex(cst); whole)
      return [num = n.to_number()](int p) { return p >= num; };
    else if(auto [whole, n] = lst_rex(cst); whole)
      return [num = n.to_number()](int p) { return p < num; };
    else if(auto [whole, n] = gst_rex(cst); whole)
      return [num = n.to_number()](int p) { return p > num; };
    else if(auto [whole, n] = eq_rex(cst); whole)
      return [num = n.to_number()](int p) { return p == num; };
    else if(auto [whole, n, r] = arith_rex(cst); whole)
    {
      if(int num = n.to_number(); num > 0)
      {
        return [num, rem = r.to_number()](int p) { return (p % num) == rem; };
      }
    }
    else if(auto [whole, n] = arith1_rex(cst); whole)
    {
      if(int num = n.to_number(); num > 0)
      {
        return [num](int p) { return (p % num) == 0; };
      }
    }

    return [](int p) { return false; };
  }

  static std::function<bool(const QString&)>
  constraint_name(std::string_view cst) noexcept
  {
    // Invalid constraint : we keep the item
    if(cst.empty())
      return [](const QString& p) { return true; };

    return [rexp = std::make_shared<RE2>(cst)](const QString& p) {
      return RE2::FullMatch(p.toStdString(), *rexp);
    };
  }

  static std::vector<int> filter_constraints(
      const std::vector<Pixel>& pixels, const std::vector<std::string_view>& x_cst,
      const std::vector<std::string_view>& y_cst,
      const std::vector<std::string_view>& z_cst,
      const std::vector<std::string_view>& name_cst) noexcept
  {
    std::vector<int> matching;
    std::vector<std::function<bool(const Pixel&)>> filters;

    for(auto& c : x_cst)
      if(auto f = constraint_pos(c))
        filters.push_back([f](const Pixel& p) { return f(p.x); });
    for(auto& c : y_cst)
      if(auto f = constraint_pos(c))
        filters.push_back([f](const Pixel& p) { return f(p.y); });
    for(auto& c : z_cst)
      if(auto f = constraint_pos(c))
        filters.push_back([f](const Pixel& p) { return f(p.z); });
    for(auto& c : name_cst)
      if(auto f = constraint_name(c))
        filters.push_back([f](const Pixel& p) { return f(p.name); });

    for(int i = 0; i < pixels.size(); i++)
    {
      if(pixels[i].name.isEmpty())
        continue;

      bool filtered = false;
      for(auto& func : filters)
        if(filtered = !func(pixels[i]))
          break;

      if(!filtered)
        matching.push_back(i);
    }

    return matching;
  }

  void loadMatrix(const rapidjson::Value& mat)
  {
    using namespace std::literals;
    // First parse the pixels
    if(auto pk_it = mat.FindMember("pixelKeys");
       pk_it != mat.MemberEnd() && pk_it->value.IsArray())
    {
      const auto& pk_z = pk_it->value.GetArray();
      const int16_t dim_z = pk_z.Size();
      for(int16_t z = 0; z < dim_z; ++z)
      {
        if(pk_z[z].IsArray())
        {
          const auto& pk_y = pk_z[z].GetArray();
          const int16_t dim_y = pk_y.Size();
          for(int16_t y = 0; y < dim_y; ++y)
          {
            if(pk_y[y].IsArray())
            {
              const auto& pk_x = pk_y[y].GetArray();
              const int16_t dim_x = pk_x.Size();
              for(int16_t x = 0; x < dim_x; ++x)
              {
                if(pk_x[x].IsString())
                {
                  matrix.pixels.push_back(
                      Pixel{.x = x, .y = y, .z = z, .name = pk_x[x].GetString()});
                }
              }
            }
          }
        }
      }
    }
    else if(auto pc_it = mat.FindMember("pixelCount");
            pc_it != mat.MemberEnd() && pc_it->value.IsArray())
    {
      const auto& pc = pc_it->value.GetArray();
      if(pc.Size() != 3)
        return;
      if(!pc[0].IsInt() || !pc[1].IsInt() || !pc[2].IsInt())
        return;
      const int16_t dim_x = pc[0].GetInt();
      const int16_t dim_y = pc[1].GetInt();
      const int16_t dim_z = pc[2].GetInt();

      matrix.pixels.reserve(dim_x * dim_y * dim_z);

      auto get_name = (dim_y == 1 && dim_z == 1)
                          ? [](int x, int y, int z) { return QString::number(x + 1); }
                      : dim_z == 1
                          ? [](int x, int y,
                               int z) { return QString("%1 %2").arg(x + 1).arg(y + 1); }
                          : [](int x, int y, int z) {
        return QString("%1 %2 %3").arg(x + 1).arg(y + 1).arg(z + 1);
                        };
      for(int16_t x = 0; x < dim_x; x++)
        for(int16_t y = 0; y < dim_y; y++)
          for(int16_t z = 0; z < dim_z; z++)
            matrix.pixels.push_back(
                Pixel{.x = x, .y = y, .z = z, .name = get_name(x, y, z)});
    }

    if(matrix.pixels.empty())
      return;

    // Then parse the groups
    if(auto gg_it = mat.FindMember("pixelGroups");
       gg_it != mat.MemberEnd() && gg_it->value.IsObject())
    {
      for(auto g_it = gg_it->value.MemberBegin(); g_it != gg_it->value.MemberEnd();
          ++g_it)
      {
        PixelGroup gp;
        gp.name = g_it->name.GetString();

        auto& g = g_it->value;

        if(g.IsString())
        {
          if(g.GetString() == "all"sv)
          {
            // All the pixels are in the group
            gp.pixels.reserve(matrix.pixels.size());
            for(int i = 0; i < matrix.pixels.size(); i++)
              gp.pixels.push_back(i);
          }
        }
        else if(g.IsArray())
        {
          // A specific list of pixels is in the group
          const auto& pxg = g.GetArray();
          for(auto& px : pxg)
          {
            if(px.IsString())
            {
              QString px_name = px.GetString();
              auto px_it = ossia::find_if(
                  matrix.pixels, [=](auto& pix) { return pix.name == px_name; });
              if(px_it != matrix.pixels.end())
              {
                gp.pixels.push_back(std::distance(matrix.pixels.begin(), px_it));
              }
            }
          }
        }
        else if(g.IsObject())
        {
          // Filter constraints
          std::vector<std::string_view> x_cst, y_cst, z_cst, name_cst;
          if(auto x_cst_it = g.FindMember("x");
             x_cst_it != g.MemberEnd() && x_cst_it->value.IsArray())
          {
            for(const auto& v : x_cst_it->value.GetArray())
              if(v.IsString())
                x_cst.push_back(v.GetString());
          }
          if(auto y_cst_it = g.FindMember("y");
             y_cst_it != g.MemberEnd() && y_cst_it->value.IsArray())
          {
            for(const auto& v : y_cst_it->value.GetArray())
              if(v.IsString())
                y_cst.push_back(v.GetString());
          }
          if(auto z_cst_it = g.FindMember("z");
             z_cst_it != g.MemberEnd() && z_cst_it->value.IsArray())
          {
            for(const auto& v : z_cst_it->value.GetArray())
              if(v.IsString())
                z_cst.push_back(v.GetString());
          }
          if(auto n_cst_it = g.FindMember("name");
             n_cst_it != g.MemberEnd() && n_cst_it->value.IsArray())
          {
            for(const auto& v : n_cst_it->value.GetArray())
              if(v.IsString())
                name_cst.push_back(v.GetString());
          }
          gp.pixels = filter_constraints(matrix.pixels, x_cst, y_cst, z_cst, name_cst);
        }

        matrix.groups.push_back(std::move(gp));
      }
    }
  }

  ChannelMap loadChannels(const rapidjson::Value& val)
  {
    ChannelMap channels;
    for(auto chan_it = val.MemberBegin(); chan_it != val.MemberEnd(); ++chan_it)
    {
      Artnet::Channel chan;
      chan.name = chan_it->name.GetString();
      auto& jchan = chan_it->value;

      if(jchan.IsObject())
      {
        if(auto default_it = jchan.FindMember("defaultValue");
           default_it != jchan.MemberEnd())
        {
          if(default_it->value.IsNumber())
          {
            chan.defaultValue = default_it->value.GetDouble();
          }
          else if(default_it->value.IsString())
          {
            // TODO parse strings...
            // From a quick grep in the library the only used string so far is "50%" so we optimize on that.
            // PRs accepted :D
            std::string_view str = default_it->value.GetString();
            if(str == "50%")
              chan.defaultValue = 127;
          }
        }

        if(auto fineChannels_it = jchan.FindMember("fineChannelAliases");
           fineChannels_it != jchan.MemberEnd())
        {
          const auto& fineChannels = fineChannels_it->value;
          if(fineChannels.IsArray())
          {
            const auto& fc = fineChannels.GetArray();
            for(auto& val : fc)
            {
              if(val.IsString())
              {
                chan.fineChannels.push_back(
                    QString::fromUtf8(val.GetString(), val.GetStringLength()));
              }
              else
              {
                chan.fineChannels.clear();
                break;
              }
            }
          }
        }

        if(auto capability_it = jchan.FindMember("capability");
           capability_it != jchan.MemberEnd())
        {
          Artnet::SingleCapability cap;
          if(auto effectname_it = capability_it->value.FindMember("effectName");
             effectname_it != capability_it->value.MemberEnd())
            cap.effectName = effectname_it->value.GetString();

          if(auto comment_it = capability_it->value.FindMember("comment");
             comment_it != capability_it->value.MemberEnd())
            cap.comment = comment_it->value.GetString();

          cap.type = capability_it->value["type"].GetString();
          chan.capabilities = std::move(cap);
        }
        else if(auto capabilities_it = jchan.FindMember("capabilities");
                capabilities_it != jchan.MemberEnd())
        {
          std::vector<Artnet::RangeCapability> caps;
          for(const auto& capa : capabilities_it->value.GetArray())
          {
            QString type = capa["type"].GetString();
            if(type != "NoFunction")
            {
              Artnet::RangeCapability cap;
              if(auto effectname_it = capa.FindMember("effectName");
                 effectname_it != capa.MemberEnd())
                cap.effectName = effectname_it->value.GetString();

              if(auto comment_it = capa.FindMember("comment");
                 comment_it != capa.MemberEnd())
                cap.comment = comment_it->value.GetString();

              cap.type = std::move(type);
              {
                const auto& range_arr = capa["dmxRange"].GetArray();
                cap.range = {range_arr[0].GetInt(), range_arr[1].GetInt()};
              }
              caps.push_back(std::move(cap));
            }
          }

          chan.capabilities = std::move(caps);
        }
      }

      channels[chan.name] = std::move(chan);
    }
    return channels;
  }

  void addTemplateToMode(
      FixtureMode& m, const rapidjson::Value& repeatFor, std::string_view channelOrder,
      const std::vector<QString>& templateChannels, const ChannelMap& templates)
  {
    // FIXME TODO
    if(channelOrder != "perPixel")
      return;

    auto addTemplates = [&m, &templates, &templateChannels](const QString& pixelKey) {
      for(const QString& channel : templateChannels)
      {
        // Locate the template for each channel
        auto template_it = templates.find(channel);
        if(template_it != templates.end())
        {
          QString name = template_it->first;
          name.replace("$pixelKey", pixelKey);

          // Write the channel
          m.channels.push_back(template_it->second);
          m.channels.back().name = name;
          m.allChannels.push_back(name);
        }
      }
    };

    if(repeatFor.IsString())
    {
      std::string_view rf = repeatFor.GetString();
      if(rf == "eachPixelABC")
      {
        auto sortedPixels = this->matrix.pixels;
        ossia::sort(sortedPixels, [](const Pixel& p1, const Pixel& p2) {
          return p1.name < p2.name;
        });

        for(const Pixel& pixel : sortedPixels)
        {
          addTemplates(pixel.name);
        }
      }
      else if(rf == "eachPixelGroup")
      {
        for(const PixelGroup& group : this->matrix.groups)
        {
          addTemplates(group.name);
        }
      }
      else if(ossia::string_starts_with(rf, "eachPixel"))
      {
        rf = rf.substr(strlen("eachPixel"));
        if(!rf.size() == 3)
          return;
        decltype(&Pixel::x) accessors[3];
        for(int i = 0; i < 3; i++)
        {
          switch(rf[i])
          {
            case 'x':
            case 'X':
              accessors[i] = &Pixel::x;
              break;
            case 'y':
            case 'Y':
              accessors[i] = &Pixel::y;
              break;
            case 'z':
            case 'Z':
              accessors[i] = &Pixel::z;
              break;
            default:
              return;
          }
        }

        auto sortedPixels = this->matrix.pixels;
        ossia::sort(sortedPixels, [=](const Pixel& p1, const Pixel& p2) {
          return std::array<int16_t, 3>{
                     p1.*(accessors[2]), p1.*(accessors[1]), p1.*(accessors[0])}
                 < std::array<int16_t, 3>{
                     p2.*(accessors[2]), p2.*(accessors[1]), p2.*(accessors[0])};
        });

        for(const Pixel& pixel : sortedPixels)
        {
          addTemplates(pixel.name);
        }
      }
    }
    else if(repeatFor.IsArray())
    {
      // It's specific pixel groups or pixels
      for(const auto& entity_v : repeatFor.GetArray())
      {
        if(!entity_v.IsString())
          continue;

        addTemplates(entity_v.GetString());
      }
    }
  }

  void loadModes(const rapidjson::Document& doc)
  {
    modes.clear();
    ChannelMap channels;
    if(auto it = doc.FindMember("availableChannels"); it != doc.MemberEnd())
      if(it->value.IsObject())
        channels = loadChannels(it->value);

    ChannelMap templateChannels;
    if(auto it = doc.FindMember("templateChannels"); it != doc.MemberEnd())
      if(it->value.IsObject())
        templateChannels = loadChannels(it->value);

    if(channels.empty() && templateChannels.empty())
      return;

    if(auto it = doc.FindMember("matrix"); it != doc.MemberEnd())
      if(it->value.IsObject())
        loadMatrix(it->value);

    using namespace std::literals;
    {
      auto it = doc.FindMember("modes");
      if(it == doc.MemberEnd())
        return;

      if(!it->value.IsArray())
        return;

      for(auto& mode : it->value.GetArray())
      {
        auto name_it = mode.FindMember("name");
        auto channels_it = mode.FindMember("channels");
        if(name_it != mode.MemberEnd() && channels_it != mode.MemberEnd())
        {
          FixtureMode m;
          if(name_it->value.IsString())
            m.name = name_it->value.GetString();

          if(channels_it->value.IsArray())
          {
            for(auto& channel : channels_it->value.GetArray())
            {
              if(channel.IsString())
              {
                auto matched_channel_it = channels.find(channel.GetString());
                if(matched_channel_it != channels.end())
                {
                  m.channels.push_back(matched_channel_it->second);
                }
                m.allChannels.push_back(
                    QString::fromUtf8(channel.GetString(), channel.GetStringLength()));
              }
              else if(channel.IsObject())
              {
                if(auto insert_it = channel.FindMember("insert");
                   insert_it != channel.MemberEnd() && insert_it->value.IsString()
                   && insert_it->value.GetString() == "matrixChannels"sv)
                {
                  auto repeatFor_it = channel.FindMember("repeatFor");
                  if(repeatFor_it == channel.MemberEnd())
                    continue;
                  auto channelOrder_it = channel.FindMember("channelOrder");
                  auto templateChannels_it = channel.FindMember("templateChannels");
                  std::string channelOrder = channelOrder_it != channel.MemberEnd()
                                                     && channelOrder_it->value.IsString()
                                                 ? channelOrder_it->value.GetString()
                                                 : "";

                  std::vector<QString> modeChannels;
                  if(templateChannels_it != channel.MemberEnd()
                     && templateChannels_it->value.IsArray())
                  {
                    const auto& arr = templateChannels_it->value.GetArray();
                    for(auto& c : arr)
                    {
                      if(c.IsString())
                        modeChannels.push_back(c.GetString());
                      else
                        modeChannels.push_back({});
                    }
                  }

                  addTemplateToMode(
                      m, repeatFor_it->value, channelOrder, modeChannels,
                      templateChannels);
                }
              }
              else
              {
                m.allChannels.push_back({});
              }
            }
          }
          modes.push_back(std::move(m));
        }
      }
    }
  }
};
using FixtureNode = TreeNode<FixtureData>;

std::vector<QString> fixturesLibraryPaths()
{
  auto libPath
      = score::AppContext().settings<Library::Settings::Model>().getPackagesPath();
  QDirIterator it{
      libPath,
      {"fixtures"},
      QDir::Dirs,
      QDirIterator::Subdirectories | QDirIterator::FollowSymlinks};

  std::vector<QString> fixtures;
  while(it.hasNext())
  {
    QDir dirpath = it.next();
    if(!dirpath.entryList({"manufacturers.json"}, QDir::Filter::Files).isEmpty())
    {
      fixtures.push_back(dirpath.absolutePath());
    }
  }
  return fixtures;
}

ossia::flat_map<QString, QString> readManufacturers(const rapidjson::Document& doc)
{
  ossia::flat_map<QString, QString> map;
  map.reserve(100);
  if(!doc.IsObject())
    return map;

  // TODO coroutines
  for(auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it)
  {
    if(it->value.IsObject())
    {
      if(auto name_it = it->value.FindMember("name"); name_it != it->value.MemberEnd())
      {
        map.insert(
            {QString::fromUtf8(it->name.GetString()),
             QString::fromUtf8(name_it->value.GetString())});
      }
    }
  }
  return map;
}

class FixtureDatabase : public TreeNodeBasedItemModel<FixtureNode>
{
public:
  std::vector<QString> m_paths;
  struct Scan
  {
    explicit Scan(QString dir, FixtureNode& manufacturer) noexcept
        : iterator{std::move(dir), QDirIterator::Subdirectories | QDirIterator::FollowSymlinks}
        , manufacturer{manufacturer}
    {
    }
    QDirIterator iterator;
    FixtureNode& manufacturer;
  };

  FixtureDatabase()
      : m_paths{fixturesLibraryPaths()}
  {
    if(!m_paths.empty())
    {
      for(auto& fixtures_dir : m_paths)
      {
        // First read the manufacturers
        {
          QFile f{fixtures_dir + "/manufacturers.json"};
          if(!f.open(QIODevice::ReadOnly))
            continue;
          {
            auto data = f.map(0, f.size());
            rapidjson::Document doc;
            doc.Parse(reinterpret_cast<const char*>(data), f.size());
            if(doc.HasParseError())
            {
              qDebug() << "Invalid manufacturers.json !";
              continue;
            }

            QModelIndex rootIndex;
            auto manufacturers = readManufacturers(doc);
            if(m_root.childCount() == 0)
            {
              // Fast-path since we know that everything is already sorted
              int k = 0;
              for(auto it = manufacturers.begin(); it != manufacturers.end(); ++it, ++k)
              {
                beginInsertRows(rootIndex, k, k);
                auto& child = m_root.emplace_back(FixtureData{it->second}, &m_root);
                endInsertRows();

                QModelIndex manufacturerIndex = createIndex(k, 0, &child);
                next(
                    std::make_shared<Scan>(fixtures_dir + "/" + it->first, child),
                    manufacturerIndex);
              }
            }
            else
            {
              for(auto it = manufacturers.begin(); it != manufacturers.end(); ++it)
              {
                auto manufacturer_node_it = ossia::find_if(
                    m_root, [&](const FixtureNode& n) { return n.name == it->second; });
                if(manufacturer_node_it == m_root.end())
                {
                  // We add it sorted to the model
                  int newRowPosition = 0;
                  auto other_manufacturer_it = m_root.begin();
                  while(other_manufacturer_it != m_root.end()
                        && QString::compare(
                               it->second, other_manufacturer_it->name,
                               Qt::CaseInsensitive)
                               >= 0)
                  {
                    other_manufacturer_it++;
                    newRowPosition++;
                  }

                  beginInsertRows(rootIndex, newRowPosition, newRowPosition);
                  auto& child = m_root.emplace(
                      other_manufacturer_it, FixtureData{it->second}, &m_root);
                  endInsertRows();

                  QModelIndex manufacturerIndex = createIndex(newRowPosition, 0, &child);
                  next(
                      std::make_shared<Scan>(fixtures_dir + "/" + it->first, child),
                      manufacturerIndex);
                }
                else
                {
                  int distance
                      = std::abs(std::distance(manufacturer_node_it, m_root.begin()));
                  QModelIndex manufacturerIndex
                      = createIndex(distance, 0, &*manufacturer_node_it);
                  next(
                      std::make_shared<Scan>(
                          fixtures_dir + "/" + it->first, *manufacturer_node_it),
                      manufacturerIndex);
                }
              }
            }

            f.unmap(data);
          }
        }
      }
    }
  }

  void loadFixture(
      std::string_view fixture_data, FixtureNode& manufacturer,
      const QModelIndex& manufacturerIndex)
  {
    rapidjson::Document doc;
    doc.Parse(fixture_data.data(), fixture_data.size());
    if(doc.HasParseError())
    {
      qDebug() << "Invalid JSON document !";
      return;
    }
    if(auto it = doc.FindMember("name"); it != doc.MemberEnd())
    {
      QString name = it->value.GetString();

      int newRowPosition = 0;
      auto other_fixture_it = manufacturer.begin();
      while(other_fixture_it != manufacturer.end()
            && QString::compare(name, other_fixture_it->name, Qt::CaseInsensitive) >= 0)
      {
        other_fixture_it++;
        newRowPosition++;
      }

      beginInsertRows(manufacturerIndex, newRowPosition, newRowPosition);
      auto& data
          = manufacturer.emplace(other_fixture_it, FixtureData{name}, &manufacturer);
      endInsertRows();

      data.loadModes(doc);

      if(auto it = doc.FindMember("categories"); it != doc.MemberEnd())
      {
        for(auto& category : it->value.GetArray())
        {
          data.tags.push_back(category.GetString());
        }
      }
    }
  }

  // Note: we could use make_unique here but on old Ubuntus stdlibc++-7 does not seem to support it (or Qt 5.9)
  // -> QTimer::singleShot calls copy ctor
  void next(std::shared_ptr<Scan> scan, QModelIndex manufacturerIndex)
  {
    auto& iterator = scan->iterator;
    if(iterator.hasNext())
    {
      const auto filepath = iterator.next();
      if(QFileInfo fi{filepath}; fi.suffix() == "json")
      {
        const std::string_view req{"fixture.json"};
        score::findStringInFile(filepath, req, [&](QFile& f) {
          unsigned char* data = f.map(0, f.size());

          const char* cbegin = reinterpret_cast<char*>(data);

          loadFixture(
              std::string_view(cbegin, f.size()), scan->manufacturer, manufacturerIndex);
        });
      }

      QTimer::singleShot(
          1, this,
          [this, scan = std::move(scan), idx = std::move(manufacturerIndex)]() mutable {
        next(std::move(scan), std::move(idx));
          });
    }
  }

  FixtureNode& rootNode() override { return m_root; }

  const FixtureNode& rootNode() const override { return m_root; }

  int columnCount(const QModelIndex& parent) const override { return 2; }

  QVariant data(const QModelIndex& index, int role) const override
  {
    const auto& node = nodeFromModelIndex(index);
    if(index.column() == 0)
    {
      switch(role)
      {
        case Qt::DisplayRole:
          return node.name;
          //        case Qt::DecorationRole:
          //          return node.icon;
      }
    }
    else if(index.column() == 1)
    {
      switch(role)
      {
        case Qt::DisplayRole:
          return node.tags.join(", ");
      }
    }
    return QVariant{};
  }

  QVariant headerData(int section, Qt::Orientation orientation, int role) const override
  {
    if(role != Qt::DisplayRole)
      return TreeNodeBasedItemModel<FixtureNode>::headerData(section, orientation, role);
    switch(section)
    {
      case 0:
        return tr("Fixture");
      case 1:
        return tr("Tags");
      default:
        return {};
    }
  }
  Qt::ItemFlags flags(const QModelIndex& index) const override
  {
    Qt::ItemFlags f;

    f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    return f;
  }

  static FixtureDatabase& instance()
  {
    static FixtureDatabase db;
    return db;
  }

  FixtureNode m_root;
};

class FixtureTreeView : public QTreeView
{
public:
  FixtureTreeView(QWidget* parent = nullptr)
      : QTreeView{parent}
  {
    setAllColumnsShowFocus(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragDropMode(QAbstractItemView::NoDragDrop);
    setAlternatingRowColors(true);
    setUniformRowHeights(true);
  }

  std::function<void(const FixtureNode&)> onSelectionChanged;
  void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
  {
    auto sel = this->selectedIndexes();
    if(!sel.empty())
    {
      auto obj = (FixtureNode*)sel.at(0).internalPointer();
      if(obj)
      {
        onSelectionChanged(*obj);
      }
    }
  }
};

class AddFixtureDialog : public QDialog
{
public:
  AddFixtureDialog(ArtnetProtocolSettingsWidget& parent)
      : QDialog{&parent}
      , m_name{this}
      , m_buttons{
            QDialogButtonBox::StandardButton::Ok
                | QDialogButtonBox::StandardButton::Cancel,
            this}
  {
    this->setLayout(&m_layout);
    m_layout.addWidget(&m_availableFixtures);
    m_layout.addLayout(&m_setupLayoutContainer);
    m_layout.setStretch(0, 3);
    m_layout.setStretch(1, 5);

    m_availableFixtures.setModel(&FixtureDatabase::instance());
    m_availableFixtures.header()->resizeSection(0, 180);
    m_availableFixtures.onSelectionChanged = [&](const FixtureNode& newFixt) {
      // Manufacturer, do nothing
      if(newFixt.childCount() > 0)
        return;

      updateParameters(newFixt);
    };

    m_setupLayoutContainer.addLayout(&m_setupLayout);
    m_setupLayout.addRow(tr("Name"), &m_name);
    m_setupLayout.addRow(tr("Address"), &m_address);
    m_setupLayout.addRow(tr("Mode"), &m_mode);
    m_setupLayout.addRow(tr("Channels"), &m_content);
    m_setupLayoutContainer.addStretch(0);
    m_setupLayoutContainer.addWidget(&m_buttons);
    connect(&m_buttons, &QDialogButtonBox::accepted, this, &AddFixtureDialog::accept);
    connect(&m_buttons, &QDialogButtonBox::rejected, this, &AddFixtureDialog::reject);

    connect(
        &m_mode, qOverload<int>(&QComboBox::currentIndexChanged), this,
        &AddFixtureDialog::setMode);
  }

  void updateParameters(const FixtureNode& fixt)
  {
    m_name.setText(fixt.name);

    m_mode.clear();
    for(auto& mode : fixt.modes)
      m_mode.addItem(mode.name);
    m_mode.setCurrentIndex(0);

    m_currentFixture = &fixt;

    setMode(0);
  }

  void setMode(int mode_index)
  {
    if(!m_currentFixture)
      return;
    if(!ossia::valid_index(mode_index, m_currentFixture->modes))
      return;

    const FixtureMode& mode = m_currentFixture->modes[mode_index];
    int numChannels = mode.channels.size();
    m_address.setRange(0, 512 - numChannels);

    m_content.setText(mode.content());
  }

  QSize sizeHint() const override { return QSize{800, 600}; }

  Artnet::Fixture fixture() const noexcept
  {
    Artnet::Fixture f;
    if(!m_currentFixture)
      return f;

    int mode_index = m_mode.currentIndex();
    if(!ossia::valid_index(mode_index, m_currentFixture->modes))
      return f;

    auto& mode = m_currentFixture->modes[mode_index];

    f.fixtureName = m_currentFixture->name;
    f.modeName = m_mode.currentText();
    f.mode.channelNames = mode.allChannels;
    f.address = m_address.value();
    f.controls = mode.channels;

    return f;
  }

private:
  QHBoxLayout m_layout;
  FixtureTreeView m_availableFixtures;

  QVBoxLayout m_setupLayoutContainer;
  QFormLayout m_setupLayout;
  State::AddressFragmentLineEdit m_name;
  QSpinBox m_address;
  QComboBox m_mode;
  QLabel m_content;
  QDialogButtonBox m_buttons;

  const FixtureData* m_currentFixture{};
};

ArtnetProtocolSettingsWidget::ArtnetProtocolSettingsWidget(QWidget* parent)
    : Device::ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  m_deviceNameEdit->setText("Artnet");

  m_host = new QComboBox{this};
  m_host->setEditable(true);
  m_host->addItems(score::list_ipv4());

  m_rate = new QSpinBox{this};
  m_rate->setRange(0, 44);
  m_rate->setValue(20);

  m_universe = new QSpinBox{this};
  m_universe->setRange(1, 65539);

  m_transport = new QComboBox{this};
  m_transport->addItems({"ArtNet", "E1.31 (sACN)", "DMX USB PRO"});

  connect(
      m_transport, qOverload<int>(&QComboBox::currentIndexChanged), this, [=](int idx) {
        m_host->clear();
        switch(idx)
        {
          case 0:
          case 1:
            m_host->addItems(score::list_ipv4());
            break;
          case 2: {
            for(auto port : QSerialPortInfo::availablePorts())
              m_host->addItem(port.portName());
            break;
          }
        }
      });

  auto layout = new QFormLayout;
  layout->addRow(tr("Name"), m_deviceNameEdit);
  layout->addRow(tr("Rate (Hz)"), m_rate);
  layout->addRow(tr("Universe"), m_universe);
  layout->addRow(tr("Transport"), m_transport);
  layout->addRow(tr("Interface"), m_host);

  m_fixturesWidget = new QTableWidget;
  layout->addRow(m_fixturesWidget);

  m_fixturesWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_fixturesWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_fixturesWidget->insertColumn(0);
  m_fixturesWidget->insertColumn(1);
  m_fixturesWidget->insertColumn(2);
  m_fixturesWidget->insertColumn(3);
  m_fixturesWidget->setHorizontalHeaderLabels(
      {tr("Name"), tr("Mode"), tr("Address"), tr("Channels used")});

  auto btns = new QHBoxLayout;
  m_addFixture = new QPushButton{"Add a fixture"};
  m_rmFixture = new QPushButton{"Remove selected fixture"};
  btns->addWidget(m_addFixture);
  btns->addWidget(m_rmFixture);
  layout->addRow(btns);

  connect(m_addFixture, &QPushButton::clicked, this, [=] {
    auto dial = new AddFixtureDialog{*this};
    if(dial->exec() == QDialog::Accepted)
    {
      auto fixt = dial->fixture();
      if(!fixt.fixtureName.isEmpty() && !fixt.controls.empty())
      {
        m_fixtures.push_back(fixt);
        updateTable();
      }
    }
  });
  connect(m_rmFixture, &QPushButton::clicked, this, [=] {
    ossia::flat_set<int> rows_to_remove;
    for(auto item : m_fixturesWidget->selectedItems())
    {
      rows_to_remove.insert(item->row());
    }

    for(auto it = rows_to_remove.rbegin(); it != rows_to_remove.rend(); ++it)
    {
      m_fixtures.erase(m_fixtures.begin() + *it);
    }

    updateTable();
  });

  setLayout(layout);
}

void ArtnetProtocolSettingsWidget::updateTable()
{
  while(m_fixturesWidget->rowCount() > 0)
    m_fixturesWidget->removeRow(int(m_fixturesWidget->rowCount()) - 1);

  int row = 0;
  for(auto& fixt : m_fixtures)
  {
    auto name_item = new QTableWidgetItem{fixt.fixtureName};
    auto mode_item = new QTableWidgetItem{fixt.modeName};
    auto address = new QTableWidgetItem{QString::number(fixt.address)};
    auto controls = new QTableWidgetItem{QString::number(fixt.controls.size())};
    name_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    mode_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    address->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    controls->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    m_fixturesWidget->insertRow(row);
    m_fixturesWidget->setItem(row, 0, name_item);
    m_fixturesWidget->setItem(row, 1, mode_item);
    m_fixturesWidget->setItem(row, 2, address);
    m_fixturesWidget->setItem(row, 3, controls);
    row++;
  }
}
ArtnetProtocolSettingsWidget::~ArtnetProtocolSettingsWidget() { }

Device::DeviceSettings ArtnetProtocolSettingsWidget::getSettings() const
{
  // TODO should be = m_settings to follow the other patterns.
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = ArtnetProtocolFactory::static_concreteKey();

  ArtnetSpecificSettings settings{};
  settings.fixtures = this->m_fixtures;
  settings.host = this->m_host->currentText();
  switch(this->m_transport->currentIndex())
  {
    case 0:
      settings.transport = ArtnetSpecificSettings::ArtNet;
      break;
    case 1:
      settings.transport = ArtnetSpecificSettings::E131;
      break;
    case 2:
      settings.transport = ArtnetSpecificSettings::DMXUSBPRO;
      break;
  }

  settings.rate = this->m_rate->value();
  settings.universe = this->m_universe->value();
  s.deviceSpecificSettings = QVariant::fromValue(settings);

  return s;
}

void ArtnetProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
  const auto& specif = settings.deviceSpecificSettings.value<ArtnetSpecificSettings>();
  m_fixtures = specif.fixtures;
  m_rate->setValue(specif.rate);
  updateTable();
}
}
#endif
