#include "file_management.h"

#include <unordered_set>
#include <vector>

#include "common/log/log.h"
#include "common/util/json_util.h"

#include "third-party/fmt/core.h"

std::vector<OfflineTestSourceFile> find_source_files(const std::string& game_name,
                                                     const std::vector<std::string>& dgos,
                                                     const std::string& single_file) {
  std::vector<OfflineTestSourceFile> result;

  auto base_dir =
      file_util::get_jak_project_dir() / "test" / "decompiler" / "reference" / game_name;
  auto ref_file_paths = file_util::find_files_recursively(base_dir, std::regex(".*_REF\\..*"));
  std::unordered_map<std::string, fs::path> ref_file_names = {};
  for (const auto& path : ref_file_paths) {
    auto ref_name = path.filename().replace_extension().string();
    ref_name.erase(ref_name.begin() + ref_name.find("_REF"), ref_name.end());
    if (single_file.empty() || ref_name == single_file) {
      ref_file_names[ref_name] = path;
    }
  }

  lg::info("Found {} reference files", ref_file_paths.size());

  // use the all_objs.json file to place them in the correct build order
  auto obj_json = parse_commented_json(
      file_util::read_text_file(
          (file_util::get_jak_project_dir() / "goal_src" / game_name / "build" / "all_objs.json")
              .string()),
      "all_objs.json");

  std::unordered_set<std::string> matched_files;
  for (auto& x : obj_json) {
    auto unique_name = x[0].get<std::string>();

    std::vector<std::string> dgoList = x[3].get<std::vector<std::string>>();
    auto it = ref_file_names.find(unique_name);
    if (it != ref_file_names.end()) {
      // Check to see if we've included atleast one of the DGO/CGOs in our hardcoded list
      // If not BLOW UP
      std::optional<std::string> containing_dgo = {};
      for (int i = 0; i < (int)dgoList.size(); i++) {
        std::string& dgo = dgoList.at(i);
        // can either be in the DGO or CGO folder, and can either end with .CGO or .DGO
        if (std::find(dgos.begin(), dgos.end(), fmt::format("DGO/{}.DGO", dgo)) != dgos.end() ||
            std::find(dgos.begin(), dgos.end(), fmt::format("DGO/{}.CGO", dgo)) != dgos.end() ||
            std::find(dgos.begin(), dgos.end(), fmt::format("CGO/{}.DGO", dgo)) != dgos.end() ||
            std::find(dgos.begin(), dgos.end(), fmt::format("CGO/{}.CGO", dgo)) != dgos.end()) {
          containing_dgo = dgo;
          break;
        }
      }
      if (!containing_dgo) {
        lg::error(
            "File [{}] is in the following DGOs [{}], and not one of these is in our list! Add "
            "it!",
            unique_name, fmt::join(dgoList, ", "));
        exit(1);
      }

      OfflineTestSourceFile file(it->second, containing_dgo.value(), x[1], it->first);
      result.push_back(file);
      matched_files.insert(unique_name);
    }
  }

  if (matched_files.size() != ref_file_names.size()) {
    lg::error("Some REF files were not matched to files in all_objs.json:");
    for (const auto& [path, flag] : ref_file_names) {
      if (matched_files.count(path) == 0) {
        lg::error("- '{}'", path);
      }
    }
    exit(1);
  }

  return result;
}

std::vector<OfflineTestArtFile> find_art_files(const std::string& game_name,
                                               const std::vector<std::string>& dgos) {
  // TODO - Jak 2
  if (game_name != "jak1") {
    return {};
  }

  std::vector<OfflineTestArtFile> result;

  // use the all_objs.json file to place them in the correct build order
  auto obj_json = parse_commented_json(
      file_util::read_text_file(
          (file_util::get_jak_project_dir() / "goal_src" / game_name / "build" / "all_objs.json")
              .string()),
      "all_objs.json");

  for (const auto& x : obj_json) {
    auto unique_name = x[0].get<std::string>();
    auto version = x[2].get<int>();

    std::vector<std::string> dgoList = x[3].get<std::vector<std::string>>();
    if (version == 4) {
      bool skip_file = false;

      // Check to see if we've included atleast one of the DGO/CGOs in our hardcoded list
      // If not BLOW UP
      std::optional<std::string> containing_dgo = {};
      for (int i = 0; i < (int)dgoList.size(); i++) {
        std::string& dgo = dgoList.at(i);
        if (dgo == "NO-XGO") {
          skip_file = true;
          break;
        }
        // can either be in the DGO or CGO folder, and can either end with .CGO or .DGO
        // TODO - Jak 2 folder may structure will be different!
        if (std::find(dgos.begin(), dgos.end(), fmt::format("DGO/{}.DGO", dgo)) != dgos.end() ||
            std::find(dgos.begin(), dgos.end(), fmt::format("DGO/{}.CGO", dgo)) != dgos.end() ||
            std::find(dgos.begin(), dgos.end(), fmt::format("CGO/{}.DGO", dgo)) != dgos.end() ||
            std::find(dgos.begin(), dgos.end(), fmt::format("CGO/{}.CGO", dgo)) != dgos.end()) {
          containing_dgo = dgo;
          break;
        }
      }
      if (skip_file) {
        continue;
      }
      if (!containing_dgo) {
        lg::error(
            "File [{}] is in the following DGOs [{}], and not one of these is in our list! Add "
            "it!",
            unique_name, fmt::join(dgoList, ", "));
        exit(1);
      }

      OfflineTestArtFile file(containing_dgo.value(), x[1], unique_name);
      result.push_back(file);
    }
  }

  return result;
}
