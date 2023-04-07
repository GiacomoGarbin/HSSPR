#include "MaterialManager.h"

std::unordered_map<std::string, std::size_t> MaterialManager::lookup;
std::vector<Material> MaterialManager::materials;