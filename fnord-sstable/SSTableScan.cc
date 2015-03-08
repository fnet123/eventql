/**
 * This file is part of the "FnordMetric" project
 *   Copyright (c) 2014 Paul Asmuth, Google Inc.
 *
 * FnordMetric is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License v3.0. You should have received a
 * copy of the GNU General Public License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 */
#include <fnord-base/inspect.h>
#include <fnord-sstable/SSTableScan.h>
#include <fnord-sstable/SSTableColumnReader.h>

namespace fnord {
namespace sstable {

SSTableScan::SSTableScan(
    SSTableColumnSchema* schema) :
    schema_(schema),
    limit_(-1),
    offset_(0),
    has_order_by_(false) {
  select_list_.emplace_back(0);

  auto col_ids = schema->columnIDs();
  for (const auto& c : col_ids) {
    select_list_.emplace_back(c);
  }
}

void SSTableScan::setLimit(long int limit) {
  limit_ = limit;
}

void SSTableScan::setOffset(long unsigned int offset) {
  offset_ = offset;
}

Vector<String> SSTableScan::columnNames() const {
  Vector<String> cols;

  for (const auto& s : select_list_) {
    cols.emplace_back(s == 0 ? "_key" : schema_->columnName(s));
  }

  return cols;
}

void SSTableScan::execute(
    Cursor* cursor,
    Function<void (const Vector<String> row)> fn) {
  Vector<Vector<String>> rows;
  size_t limit_ctr = 0;
  size_t offset_ctr = 0;

  for (; cursor->valid(); cursor->next()) {
    // filter key...

    auto val = cursor->getDataBuffer();
    sstable::SSTableColumnReader cols(schema_, val);

    Vector<String> row;
    for (const auto& s : select_list_) {
      switch (s) {
        case 0:
          row.emplace_back(cursor->getKeyString());
          break;

        default:
          row.emplace_back(cols.getStringColumn(s));
          break;
      }
    }

    // filter cols...

    if (!has_order_by_ && offset_ctr++ < offset_) {
      continue;
    }

    if (has_order_by_) {
      rows.emplace_back(row);
    } else {
      fn(row);

      if (limit_ > 0 && ++limit_ctr >= limit_) {
        break;
      }
    }
  }

  if (has_order_by_) {
    // sort

    auto limit = rows.size();
    if (limit_ > 0) {
      limit = offset_ + limit_;

      if (limit > rows.size()) {
        limit = rows.size();
      }
    }

    for (int i = offset_; i < limit; ++i) {
      fn(rows[i]);
    }
  }
}

} // namespace sstable
} // namespace fnord
