//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2018
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "td/telegram/files/FileLoaderUtils.h"

#include "td/telegram/files/FileLocation.h"
#include "td/telegram/Global.h"

#include "td/utils/common.h"
#include "td/utils/filesystem.h"
#include "td/utils/format.h"
#include "td/utils/logging.h"
#include "td/utils/misc.h"
#include "td/utils/PathView.h"
#include "td/utils/port/FileFd.h"
#include "td/utils/port/path.h"
#include "td/utils/Random.h"
#include "td/utils/StringBuilder.h"

#include <tuple>

namespace td {

namespace {
Result<std::pair<FileFd, string>> try_create_new_file(Result<CSlice> result_name) {
  TRY_RESULT(name, std::move(result_name));
  TRY_RESULT(fd, FileFd::open(name, FileFd::Read | FileFd::Write | FileFd::CreateNew, 0640));
  return std::make_pair(std::move(fd), name.str());
}
Result<std::pair<FileFd, string>> try_open_file(Result<CSlice> result_name) {
  TRY_RESULT(name, std::move(result_name));
  TRY_RESULT(fd, FileFd::open(name, FileFd::Read, 0640));
  return std::make_pair(std::move(fd), name.str());
}

struct RandSuff {
  int len;
};
StringBuilder &operator<<(StringBuilder &sb, const RandSuff &) {
  for (int i = 0; i < 6; i++) {
    sb << format::hex_digit(Random::fast(0, 15));
  }
  return sb;
}
struct Ext {
  Slice ext;
};
StringBuilder &operator<<(StringBuilder &sb, const Ext &ext) {
  if (ext.ext.empty()) {
    return sb;
  }
  return sb << "." << ext.ext;
}
}  // namespace

Result<std::pair<FileFd, string>> open_temp_file(const FileType &file_type) {
  auto pmc = G()->td_db()->get_binlog_pmc();
  // TODO: CAS?
  int32 file_id = to_integer<int32>(pmc->get("tmp_file_id"));
  pmc->set("tmp_file_id", to_string(file_id + 1));

  auto temp_dir = get_files_temp_dir(file_type);
  auto res = try_create_new_file(PSLICE_SAFE() << temp_dir << file_id);
  if (res.is_error()) {
    res = try_create_new_file(PSLICE_SAFE() << temp_dir << file_id << "_" << RandSuff{6});
  }
  return res;
}

template <class F>
bool for_suggested_file_name(CSlice name, bool use_pmc, bool use_random, F &&callback) {
  auto try_callback = [&](Result<CSlice> r_path) {
    if (r_path.is_error()) {
      return true;
    }
    return callback(r_path.move_as_ok());
  };
  auto cleaned_name = clean_filename(name);
  PathView path_view(cleaned_name);
  auto stem = path_view.file_stem();
  auto ext = path_view.extension();
  bool active = true;
  if (!stem.empty() && !G()->parameters().ignore_file_names) {
    active = try_callback(PSLICE_SAFE() << stem << Ext{ext});
    for (int i = 0; active && i < 10; i++) {
      active = try_callback(PSLICE_SAFE() << stem << "_(" << i << ")" << Ext{ext});
    }
    for (int i = 2; active && i < 12 && use_random; i++) {
      active = try_callback(PSLICE_SAFE() << stem << "_(" << RandSuff{i} << ")" << Ext{ext});
    }
  } else if (use_pmc) {
    auto pmc = G()->td_db()->get_binlog_pmc();
    int32 file_id = to_integer<int32>(pmc->get("perm_file_id"));
    pmc->set("perm_file_id", to_string(file_id + 1));
    active = try_callback(PSLICE_SAFE() << "file_" << file_id << Ext{ext});
    if (active) {
      active = try_callback(PSLICE_SAFE() << "file_" << file_id << "_" << RandSuff{6} << Ext{ext});
    }
  }
  return active;
}

Result<string> create_from_temp(CSlice temp_path, CSlice dir, CSlice name) {
  LOG(INFO) << "Create file in directory " << dir << " with suggested name " << name << " from temporary file "
            << temp_path;
  Result<std::pair<FileFd, string>> res = Status::Error(500, "Can't find suitable file name");
  for_suggested_file_name(name, true, true, [&](CSlice suggested_name) {
    res = try_create_new_file(PSLICE_SAFE() << dir << suggested_name);
    return res.is_error();
  });
  TRY_RESULT(tmp, std::move(res));
  tmp.first.close();
  auto perm_path = std::move(tmp.second);
  TRY_STATUS(rename(temp_path, perm_path));
  return perm_path;
}

Result<string> search_file(CSlice dir, CSlice name, int64 expected_size) {
  Result<std::string> res = Status::Error(500, "Can't find suitable file name");
  for_suggested_file_name(name, false, false, [&](CSlice suggested_name) {
    auto r_pair = try_open_file(PSLICE_SAFE() << dir << suggested_name);
    if (r_pair.is_error()) {
      return false;
    }
    FileFd fd;
    std::string path;
    std::tie(fd, path) = r_pair.move_as_ok();
    if (fd.stat().size_ != expected_size) {
      return true;
    }
    fd.close();
    res = std::move(path);
    return false;
  });
  return res;
}

const char *file_type_name[file_type_size] = {"thumbnails", "profile_photos", "photos",     "voice",
                                              "videos",     "documents",      "secret",     "temp",
                                              "stickers",   "music",          "animations", "secret_thumbnails",
                                              "wallpapers", "video_notes"};

string get_file_base_dir(const FileDirType &file_dir_type) {
  switch (file_dir_type) {
    case FileDirType::Secure:
      return G()->get_dir().str();
    case FileDirType::Common:
      return G()->get_files_dir().str();
    default:
      UNREACHABLE();
      return "";
  }
}

string get_files_base_dir(const FileType &file_type) {
  return get_file_base_dir(get_file_dir_type(file_type));
}
string get_files_temp_dir(const FileType &file_type) {
  return get_files_base_dir(file_type) + "temp" + TD_DIR_SLASH;
}
string get_files_dir(const FileType &file_type) {
  return get_files_base_dir(file_type) + file_type_name[static_cast<int32>(file_type)] + TD_DIR_SLASH;
}

}  // namespace td
