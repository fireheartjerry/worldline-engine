#pragma once

#include "app/UniverseModel.hpp"

#include <filesystem>
#include <vector>

namespace Storage {

std::filesystem::path data_root();
std::filesystem::path projects_root();
std::filesystem::path settings_path();
void ensure_storage_dirs();

bool save_project(const UniverseProject& project);
bool load_project(const std::string& id, UniverseProject& project);
CatalogIndex load_catalog();
std::vector<std::size_t> query_catalog(const CatalogIndex& catalog, const CatalogQuery& query);

PersistentAppSettings load_settings();
void save_settings(const PersistentAppSettings& settings);

std::filesystem::path cosmos_root();
bool save_cosmos_bookmark(const CosmosBookmark& bookmark);
std::vector<CosmosBookmark> load_cosmos_bookmarks(); // newest first

std::string make_project_id(const std::string& seed);
std::string now_timestamp();

} // namespace Storage
