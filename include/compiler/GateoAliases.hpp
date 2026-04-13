/**
 * File: GateoAliases.hpp
 * Purpose: Short names for gateo-cpp view types used by the compiler (single place to
 * retarget on a schema / view namespace bump).
 */
#pragma once

#include "gateo/v2/view.hpp"

namespace gate {

using GateObject = gateo::v2::view::GateObject;
using Node = gateo::v2::view::Node;
using GateType = gateo::v2::view::GateType;
using GateVersion = gateo::v2::view::Version;
using ComponentInstance = gateo::v2::view::ComponentInstance;

} // namespace gate
