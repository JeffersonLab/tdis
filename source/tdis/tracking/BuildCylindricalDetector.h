// This file is part of the Acts project.
//
// Copyright (C) 2020-2021 CERN for the benefit of the Acts project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <memory>
#include <spdlog/logger.h>
#include "MtpcDetectorElement.hpp"

namespace Acts {
class TrackingGeometry;
}  // namespace Acts

namespace tdis::tracking {


/** This geometry uses cylindrical surfaces with:
 *    - R (radius) of each cylinder - corresponding to each of mTPC ring centers
 *    - The length of each cylinder in z direction equal to the whole detector length
 *  This way the finding a point on a plane for ACTS Kalman filtering is straightforward in terms of Z coordinate
 *  Then each cylinder corresponds to MtpcDetectorElement in detectorStore
 */
std::unique_ptr<const Acts::TrackingGeometry> buildCylindricalDetector(
        const std::shared_ptr<spdlog::logger>& log,
        const typename MtpcDetectorElement::ContextType& gctx,
        std::unordered_map<uint32_t, std::shared_ptr<MtpcDetectorElement>>& surfaceStore
    );

}  // namespace ActsExamples::Telescope
