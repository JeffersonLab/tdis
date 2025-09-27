/**
* The `PodioSurfaceIdConverter` acts as a **bridge between ACTS tracking objects and Podio data format**.
* Its main role is to:
* 1. **Surface Mapping**: Converts between ACTS `Surface` objects (geometric detector surfaces)
*    and Podio `Identifier` numbers, maintaining a bidirectional lookup table
* 2. **SourceLink Management**: Handles conversion between ACTS `SourceLink` objects (which connect
*    measurements to detector surfaces) and Podio identifiers by storing them in a vector
* 3. **Container Support**: Enables ACTS track containers (`MutablePodioTrackStateContainer` and
*    `MutablePodioTrackContainer`) to properly serialize/deserialize tracking data in Podio format
* Essentially, it's an adapter that allows the ACTS Kalman fitter to work with Podio-based
* persistent storage, ensuring that geometric references (surfaces) and measurement links remain
* valid when tracks are saved to or loaded from Podio collections.

Created by Dmitry Romanov on 9/27/2025.
Subject to the terms in the LICENSE file found in the top-level directory.
*/


# pragma once


struct PodioSurfaceIdConverter : public Acts::PodioUtil::ConversionHelper {

    virtual ~PodioSurfaceIdConverter() = default;
    std::optional<Acts::PodioUtil::Identifier> surfaceToIdentifier(const Acts::Surface& surface) const override {
        for (auto&& [id, srf] : surfaces) {
            if (srf == &surface) {
                return id;
            }
        }
        return {};
    }

    const Acts::Surface* identifierToSurface(Acts::PodioUtil::Identifier id) const override {
        auto it = surfaces.find(id);
        if (it == surfaces.end()) {
            return nullptr;
        }

        return it->second;
    }

    Acts::PodioUtil::Identifier sourceLinkToIdentifier(const Acts::SourceLink& sl) override {
        sourceLinks.push_back(sl);
        return sourceLinks.size() - 1;
    }

    Acts::SourceLink identifierToSourceLink(Acts::PodioUtil::Identifier id) const override {
        return sourceLinks.at(id);
    }

    std::unordered_map<Acts::PodioUtil::Identifier, const Acts::Surface*> surfaces;
    std::vector<Acts::SourceLink> sourceLinks;
};

struct Factory {
    using trajectory_t = Acts::MutablePodioTrackStateContainer;
    using const_trajectory_t = Acts::ConstPodioTrackStateContainer;

    PodioSurfaceIdConverter m_helper;

    Acts::MutablePodioTrackStateContainer create() {
        return Acts::MutablePodioTrackStateContainer{m_helper};
    }
};