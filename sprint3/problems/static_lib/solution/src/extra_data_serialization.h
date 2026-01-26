#pragma once

#include <boost/json.hpp>  // Dependency is contained here

#include "extra_data.h"

namespace extra_data_ser {

namespace json = boost::json;

extra_data::ExtraData ExtractExtraData(const json::value& root);

}  // namespace extra_data_ser