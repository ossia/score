#pragma once

#include <ossia/network/value/value.hpp>

#include <boost/multi_array.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <algorithm>

namespace avnd_tools
{

struct Table2D
{
  halp_meta(name, "Table (2D)")
  halp_meta(author, "ossia team")
  halp_meta(category, "Control/Data processing")
  halp_meta(description, "Store arbitrary data in a 2-dimensional table")
  halp_meta(c_name, "avnd_table_2d")
  halp_meta(uuid, "b8c4d2e3-5f60-7890-bcde-f01234567892")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/table.html")

  using value_type = ossia::value;
  using array_type = boost::multi_array<value_type, 2>;

  struct
  {
    struct : halp::val_port<"Read", std::optional<std::pair<int64_t, int64_t>>>
    {
      void update(Table2D& t)
      {
        if(value)
        {
          if(t.read(value->first, value->second))
            return;
        }
        t.outputs.output.value = ossia::value{};
      }
    } read;

    struct : halp::val_port<"Read row", std::optional<int64_t>>
    {
      void update(Table2D& t)
      {
        if(value)
          t.outputs.row_output.value = t.get_row(*value);
      }
    } read_row;

    struct : halp::val_port<"Read column", std::optional<int64_t>>
    {
      void update(Table2D& t)
      {
        if(value)
          t.outputs.column_output.value = t.get_column(*value);
      }
    } read_column;

    struct : halp::val_port<"Set cell", std::vector<ossia::value>>
    {
      void update(Table2D& t)
      {
        // Expects [row, col, value]
        if(value.size() != 3)
          return;
        const int64_t row = ossia::convert<int>(value[0]);
        const int64_t col = ossia::convert<int>(value[1]);
        t.set(row, col, value[2]);
      }
    } set;

    struct : halp::val_port<"Set row", std::pair<int64_t, ossia::value>>
    {
      void update(Table2D& t) { t.set_row(value.first, value.second); }
    } set_row;

    struct : halp::val_port<"Set column", std::pair<int64_t, ossia::value>>
    {
      void update(Table2D& t) { t.set_column(value.first, value.second); }
    } set_column;

    struct : halp::val_port<"Clear cell", std::pair<int64_t, int64_t>>
    {
      void update(Table2D& t) { t.clear_cell(value.first, value.second); }
    } clear_cell;

    struct : halp::val_port<"Clear row", int64_t>
    {
      void update(Table2D& t) { t.clear_row(value); }
    } clear_row;

    struct : halp::val_port<"Clear column", int64_t>
    {
      void update(Table2D& t) { t.clear_column(value); }
    } clear_column;

    struct : halp::val_port<"Insert row", std::pair<int64_t, ossia::value>>
    {
      void update(Table2D& t) { t.insert_row(value.first, value.second); }
    } insert_row;

    struct : halp::val_port<"Insert column", std::pair<int64_t, ossia::value>>
    {
      void update(Table2D& t) { t.insert_column(value.first, value.second); }
    } insert_column;

    struct : halp::val_port<"Erase row", int64_t>
    {
      void update(Table2D& t) { t.erase_row(value); }
    } erase_row;

    struct : halp::val_port<"Erase column", int64_t>
    {
      void update(Table2D& t) { t.erase_column(value); }
    } erase_column;

    struct : halp::val_port<"Append row", ossia::value>
    {
      void update(Table2D& t) { t.append_row(value); }
    } append_row;

    struct : halp::val_port<"Append column", ossia::value>
    {
      void update(Table2D& t) { t.append_column(value); }
    } append_column;

    struct : halp::val_port<"Resize", std::pair<int64_t, int64_t>>
    {
      void update(Table2D& t) { t.resize(value.first, value.second); }
    } resize;

    struct : halp::val_port<"Fill", ossia::value>
    {
      void update(Table2D& t) { t.fill(value); }
    } fill_port;

    struct : halp::val_port<"Transpose", bool>
    {
      void update(Table2D& t)
      {
        if(value)
          t.transpose();
      }
    } transpose;

    halp::maintained_button<"Clear"> clear;
    halp::maintained_button<"Lock"> lock;
    struct : halp::impulse_button<"Dump">
    {
      void update(Table2D& t) { t.outputs.output.value = t.dump(); }
    } dump;
    halp::toggle<"Preserve"> preserve;
  } inputs;

  struct
  {
    halp::val_port<"Output", ossia::value> output;
    halp::val_port<"Row", std::vector<ossia::value>> row_output;
    halp::val_port<"Column", std::vector<ossia::value>> column_output;
    halp::val_port<"Rows", int64_t> rows;
    halp::val_port<"Columns", int64_t> columns;
    halp::val_port<"Size", int64_t> size;
  } outputs;

  array_type buffer;

  std::size_t num_rows() const { return buffer.shape()[0]; }
  std::size_t num_cols() const { return buffer.shape()[1]; }

  bool in_bounds(int64_t row, int64_t col) const
  {
    if(row < 0 || col < 0)
      return false;
    return static_cast<std::size_t>(row) < num_rows()
           && static_cast<std::size_t>(col) < num_cols();
  }

  bool read(int64_t row, int64_t col)
  {
    if(!in_bounds(row, col))
      return false;

    outputs.output.value = buffer[row][col];
    return true;
  }

  void set(int64_t row, int64_t col, const value_type& value)
  {
    if(row < 0 || col < 0 || row >= INT_MAX || col >= INT_MAX)
      return;

    const std::size_t r = static_cast<std::size_t>(row);
    const std::size_t c = static_cast<std::size_t>(col);

    const std::size_t cur_rows = num_rows();
    const std::size_t cur_cols = num_cols();

    if(r >= cur_rows || c >= cur_cols)
    {
      const std::size_t new_rows = std::max(cur_rows, r + 1);
      const std::size_t new_cols = std::max(cur_cols, c + 1);
      buffer.resize(boost::extents[new_rows][new_cols]);
    }

    buffer[r][c] = value;
  }

  std::vector<ossia::value> get_row(int64_t row) const
  {
    if(row < 0 || static_cast<std::size_t>(row) >= num_rows())
      return {};

    std::vector<ossia::value> result;
    result.reserve(num_cols());
    for(std::size_t c = 0; c < num_cols(); ++c)
      result.push_back(buffer[row][c]);
    return result;
  }

  std::vector<ossia::value> get_column(int64_t col) const
  {
    if(col < 0 || static_cast<std::size_t>(col) >= num_cols())
      return {};

    std::vector<ossia::value> result;
    result.reserve(num_rows());
    for(std::size_t r = 0; r < num_rows(); ++r)
      result.push_back(buffer[r][col]);
    return result;
  }

  void set_row(int64_t row, const value_type& v)
  {
    if(row < 0 || static_cast<std::size_t>(row) >= num_rows())
      return;

    if(auto* vec = v.target<std::vector<ossia::value>>())
    {
      const std::size_t count = std::min(vec->size(), num_cols());
      for(std::size_t c = 0; c < count; ++c)
        buffer[row][c] = (*vec)[c];
    }
    else
    {
      for(std::size_t c = 0; c < num_cols(); ++c)
        buffer[row][c] = v;
    }
  }

  void set_column(int64_t col, const value_type& v)
  {
    if(col < 0 || static_cast<std::size_t>(col) >= num_cols())
      return;

    if(auto* vec = v.target<std::vector<ossia::value>>())
    {
      const std::size_t count = std::min(vec->size(), num_rows());
      for(std::size_t r = 0; r < count; ++r)
        buffer[r][col] = (*vec)[r];
    }
    else
    {
      for(std::size_t r = 0; r < num_rows(); ++r)
        buffer[r][col] = v;
    }
  }

  void clear_cell(int64_t row, int64_t col)
  {
    if(!in_bounds(row, col))
      return;
    buffer[row][col] = value_type{};
  }

  void clear_row(int64_t row)
  {
    if(row < 0 || static_cast<std::size_t>(row) >= num_rows())
      return;
    for(std::size_t c = 0; c < num_cols(); ++c)
      buffer[row][c] = value_type{};
  }

  void clear_column(int64_t col)
  {
    if(col < 0 || static_cast<std::size_t>(col) >= num_cols())
      return;
    for(std::size_t r = 0; r < num_rows(); ++r)
      buffer[r][col] = value_type{};
  }

  void insert_row(int64_t index, const value_type& v)
  {
    if(index < 0)
      return;

    const std::size_t idx = static_cast<std::size_t>(index);
    const std::size_t rows = num_rows();
    const std::size_t cols = num_cols();

    if(idx > rows)
      return;

    buffer.resize(boost::extents[rows + 1][std::max(cols, std::size_t{1})]);

    // Shift rows down
    for(std::size_t r = rows; r > idx; --r)
      for(std::size_t c = 0; c < buffer.shape()[1]; ++c)
        buffer[r][c] = std::move(buffer[r - 1][c]);

    // Fill new row
    if(auto* vec = v.target<std::vector<ossia::value>>())
    {
      const std::size_t count = std::min(vec->size(), buffer.shape()[1]);
      for(std::size_t c = 0; c < count; ++c)
        buffer[idx][c] = (*vec)[c];
    }
    else
    {
      for(std::size_t c = 0; c < buffer.shape()[1]; ++c)
        buffer[idx][c] = v;
    }
  }

  void insert_column(int64_t index, const value_type& v)
  {
    if(index < 0)
      return;

    const std::size_t idx = static_cast<std::size_t>(index);
    const std::size_t rows = num_rows();
    const std::size_t cols = num_cols();

    if(idx > cols)
      return;

    // Resize to add one column
    buffer.resize(boost::extents[std::max(rows, std::size_t{1})][cols + 1]);

    // Shift columns right
    for(std::size_t r = 0; r < buffer.shape()[0]; ++r)
      for(std::size_t c = cols; c > idx; --c)
        buffer[r][c] = std::move(buffer[r][c - 1]);

    // Fill new column
    if(auto* vec = v.target<std::vector<ossia::value>>())
    {
      const std::size_t count = std::min(vec->size(), buffer.shape()[0]);
      for(std::size_t r = 0; r < count; ++r)
        buffer[r][idx] = (*vec)[r];
    }
    else
    {
      for(std::size_t r = 0; r < buffer.shape()[0]; ++r)
        buffer[r][idx] = v;
    }
  }

  void erase_row(int64_t row)
  {
    if(row < 0 || static_cast<std::size_t>(row) >= num_rows())
      return;

    const std::size_t rows = num_rows();
    const std::size_t cols = num_cols();

    // Shift rows up
    for(std::size_t r = row; r + 1 < rows; ++r)
      for(std::size_t c = 0; c < cols; ++c)
        buffer[r][c] = std::move(buffer[r + 1][c]);

    buffer.resize(boost::extents[rows - 1][cols]);
  }

  void erase_column(int64_t col)
  {
    if(col < 0 || static_cast<std::size_t>(col) >= num_cols())
      return;

    const std::size_t rows = num_rows();
    const std::size_t cols = num_cols();

    for(std::size_t r = 0; r < rows; ++r)
      for(std::size_t c = col; c + 1 < cols; ++c)
        buffer[r][c] = std::move(buffer[r][c + 1]);

    buffer.resize(boost::extents[rows][cols - 1]);
  }

  void append_row(const value_type& v)
  {
    insert_row(static_cast<int64_t>(num_rows()), v);
  }

  void append_column(const value_type& v)
  {
    insert_column(static_cast<int64_t>(num_cols()), v);
  }

  void resize(int64_t rows, int64_t cols)
  {
    if(rows < 0 || cols < 0)
      return;
    buffer.resize(
        boost::extents[static_cast<std::size_t>(rows)][static_cast<std::size_t>(cols)]);
  }

  void fill(const value_type& value)
  {
    std::fill(buffer.data(), buffer.data() + buffer.num_elements(), value);
  }

  void transpose()
  {
    const std::size_t rows = num_rows();
    const std::size_t cols = num_cols();

    if(rows == 0 || cols == 0)
      return;

    array_type new_buffer(boost::extents[cols][rows]);

    for(std::size_t r = 0; r < rows; ++r)
      for(std::size_t c = 0; c < cols; ++c)
        new_buffer[c][r] = std::move(buffer[r][c]);

    buffer = std::move(new_buffer);
  }

  void do_clear() { buffer.resize(boost::extents[0][0]); }

  ossia::value dump() const
  {
    std::vector<ossia::value> result;
    const std::size_t rows = num_rows();
    const std::size_t cols = num_cols();

    result.reserve(rows);
    for(std::size_t r = 0; r < rows; ++r)
    {
      std::vector<ossia::value> row;
      row.reserve(cols);
      for(std::size_t c = 0; c < cols; ++c)
        row.push_back(buffer[r][c]);
      result.push_back(std::move(row));
    }
    return result;
  }

  void operator()()
  {
    if(inputs.clear)
    {
      do_clear();
    }

    outputs.rows.value = static_cast<int64_t>(num_rows());
    outputs.columns.value = static_cast<int64_t>(num_cols());
    outputs.size.value = static_cast<int64_t>(buffer.num_elements());
  }
};

}
