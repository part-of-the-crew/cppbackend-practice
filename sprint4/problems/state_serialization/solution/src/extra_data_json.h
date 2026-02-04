#pragma once

#include <boost/json.hpp>

#include "extra_data.h"

namespace extra_data_json {

namespace json = boost::json;

extra_data::ExtraData ExtractExtraData(const json::value& root);

}  // namespace extra_data_json