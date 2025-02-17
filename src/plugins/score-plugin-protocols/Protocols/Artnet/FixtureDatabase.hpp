#pragma once
#include <Library/LibrarySettings.hpp>
#include <Protocols/Artnet/ArtnetSpecificSettings.hpp>

#include <score/model/tree/TreeNodeItemModel.hpp>
#include <score/tools/FindStringInFile.hpp>

#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/math.hpp>
#include <ossia/detail/string_algorithms.hpp>

#include <QDirIterator>

#include <re2/re2.h>

#include <ctre.hpp>
namespace Protocols
{

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

static std::vector<QString> fixturesLibraryPaths()
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

static ossia::flat_map<QString, QString>
readManufacturers(const rapidjson::Document& doc)
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
      str += QString::number(k + 1);
      str += ": \t";
      str += chan.isEmpty() ? "<No function>" : chan;
      str += "\n";
      k++;
    }
    return str;
  }
};

struct WheelSlot
{
  QString type;
  QString name;
};

struct Wheel
{
  QString name;
  std::vector<WheelSlot> wheelslots;
};
class FixtureData
{
public:
  using ChannelMap = ossia::hash_map<QString, Artnet::Channel>;
  QString name{};
  QStringList tags{};
  QIcon icon{};

  PixelMatrix matrix{};

  std::vector<Wheel> wheels{};
  std::vector<FixtureMode> modes{};

  static std::vector<Wheel> loadWheels(const rapidjson::Value& value) noexcept
  {
    std::vector<Wheel> wheels{};
    if(!value.IsObject())
      return wheels;

    for(auto w_it = value.MemberBegin(); w_it != value.MemberEnd(); ++w_it)
    {
      if(!w_it->value.IsObject())
        continue;

      Wheel wh;
      wh.name = w_it->name.GetString();

      const auto& w = w_it->value.GetObject();
      if(auto s_it = w.FindMember("slots"); s_it != w.MemberEnd())
      {
        if(!s_it->value.IsArray())
          continue;

        for(const auto& s : s_it->value.GetArray())
        {
          if(!s.IsObject())
            continue;

          WheelSlot ws;
          if(auto nm_it = s.FindMember("name"); nm_it != s.MemberEnd())
          {
            if(nm_it->value.IsString())
              ws.name = nm_it->value.GetString();
          }
          if(auto t_it = s.FindMember("type"); t_it != s.MemberEnd())
          {
            if(t_it->value.IsString())
              ws.type = t_it->value.GetString();
          }
          wh.wheelslots.push_back(std::move(ws));
        }
      }

      wheels.push_back(std::move(wh));
    }

    return wheels;
  }

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

    return [rexp = std::make_shared<RE2>(re2::StringPiece(cst.data(), cst.size()))](
               const QString& p) { return RE2::FullMatch(p.toStdString(), *rexp); };
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
        if((filtered = !func(pixels[i])))
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
            for(std::size_t i = 0; i < matrix.pixels.size(); i++)
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

  QString getWheelName(QString channelName, int wheelSlot) const noexcept
  {
    // It's 1-indexed...
    wheelSlot--;
    if(wheelSlot < 0)
      return {};
    for(auto& wh : this->wheels)
    {
      if(wh.name == channelName)
      {
        if(wheelSlot < std::ssize(wh.wheelslots))
        {
          const auto& w = wh.wheelslots[wheelSlot];
          if(!w.name.isEmpty())
            return w.name;
          else if(!w.type.isEmpty())
            return w.type;
        }
        break;
      }
    }

    return {};
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
            if(!capa.HasMember("type"))
              continue;

            QString type = capa["type"].GetString();
            if(type != "NoFunction")
            {
              Artnet::RangeCapability cap;
              if(auto effectname_it = capa.FindMember("effectName");
                 effectname_it != capa.MemberEnd())
              {
                cap.effectName = effectname_it->value.GetString();
              }
              else if(type.startsWith("Wheel"))
              {
                QString whName = chan.name;
                if(auto wh_it = capa.FindMember("wheel"); wh_it != capa.MemberEnd())
                {
                  if(wh_it->value.IsString())
                    whName = wh_it->value.GetString();
                }

                if(auto idx_it = capa.FindMember("slotNumber");
                   idx_it != capa.MemberEnd())
                {
                  if(idx_it->value.IsInt())
                  {
                    cap.effectName = getWheelName(whName, idx_it->value.GetInt());
                  }
                  else if(idx_it->value.IsDouble())
                  {
                    double v = idx_it->value.GetDouble();
                    double lo = std::floor(v);
                    double hi = std::ceil(v);
                    cap.effectName = QString("Split %1 %2")
                                         .arg(getWheelName(whName, lo))
                                         .arg(getWheelName(whName, hi));
                  }
                }
              }

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
        if(rf.size() != 3)
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
    wheels.clear();

    if(auto it = doc.FindMember("wheels"); it != doc.MemberEnd())
      if(it->value.IsObject())
        wheels = loadWheels(it->value);

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
                if(auto matched_channel_it = channels.find(channel.GetString());
                   matched_channel_it != channels.end())
                {
                  m.channels.push_back(matched_channel_it->second);
                }
                else
                {
                  // Channel is hardcoded with an existing template name
                  // FIXME maybe this happens with pixels too?
                  QString ch = channel.GetString();
                  for(const auto& tc : templateChannels)
                  {
                    for(const auto& g : this->matrix.groups)
                    {
                      QString repl = QString(tc.first).replace("$pixelKey", g.name);
                      if(repl == ch)
                      {
                        m.channels.push_back(tc.second);
                        m.channels.back().name = ch;
                        goto could_match_channel;
                      }
                    }
                  }
                }
              could_match_channel:
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
        QFile f{fixtures_dir + "/manufacturers.json"};
        if(!f.open(QIODevice::ReadOnly))
          continue;
        auto data = f.map(0, f.size());

        rapidjson::Document doc;
        doc.Parse(reinterpret_cast<const char*>(data), f.size());
        if(doc.HasParseError())
        {
          qDebug() << "Invalid manufacturers.json !";
          continue;
        }

        loadManufacturer(doc, fixtures_dir);

        f.unmap(data);
      }
    }
  }

  void loadManufacturer(rapidjson::Document& doc, const QString& fixtures_dir)
  {
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
        nextFixture(
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
                       it->second, other_manufacturer_it->name, Qt::CaseInsensitive)
                       >= 0)
          {
            other_manufacturer_it++;
            newRowPosition++;
          }

          beginInsertRows(rootIndex, newRowPosition, newRowPosition);
          auto& child
              = m_root.emplace(other_manufacturer_it, FixtureData{it->second}, &m_root);
          endInsertRows();

          QModelIndex manufacturerIndex = createIndex(newRowPosition, 0, &child);
          nextFixture(
              std::make_shared<Scan>(fixtures_dir + "/" + it->first, child),
              manufacturerIndex);
        }
        else
        {
          int distance = std::abs(std::distance(manufacturer_node_it, m_root.begin()));
          QModelIndex manufacturerIndex
              = createIndex(distance, 0, &*manufacturer_node_it);
          nextFixture(
              std::make_shared<Scan>(
                  fixtures_dir + "/" + it->first, *manufacturer_node_it),
              manufacturerIndex);
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
  void nextFixture(std::shared_ptr<Scan> scan, QModelIndex manufacturerIndex)
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

      m_inFlight++;
      QTimer::singleShot(
          1, this, [this, scan = std::move(scan), idx = manufacturerIndex]() mutable {
        nextFixture(std::move(scan), idx);
        m_inFlight--;
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

  QModelIndex modelIndexFromNode(const FixtureNode& n) const
  {
    node_type* parent = n.parent();
    SCORE_ASSERT(parent);
    SCORE_ASSERT(parent != &rootNode());

    return createIndex(parent->indexOfChild(&n), 0, &n);
  }

  static FixtureDatabase& instance()
  {
    static FixtureDatabase db;
    return db;
  }

  void onPopulated(auto func)
  {
    if(m_inFlight == 0)
    {
      func();
    }
    else
    {
      QTimer::singleShot(8, this, [this, func = std::move(func)]() mutable {
        onPopulated(std::move(func));
      });
    }
  }

  FixtureNode m_root;

  int m_inFlight{};
};

}
