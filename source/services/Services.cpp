#include "Services.h"

// Static initialization of services map
std::unordered_map<const std::type_info*, std::shared_ptr<parus::Service>> parus::Services::services;