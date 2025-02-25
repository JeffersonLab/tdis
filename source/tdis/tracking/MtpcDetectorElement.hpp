#pragma once

#include <memory>
#include <vector>

#include "Acts/Geometry/DetectorElementBase.hpp"


namespace Acts {
    class Surface;
    class CylinderBounds;
    class CylinderSurface;
    class ISurfaceMaterial;
}

namespace tdis::tracking {
    /// @class MtpcDetectorElement
    ///
    /// This is a lightweight detector element for cylindrical surfaces.
    class MtpcDetectorElement : public Acts::DetectorElementBase {
    public:
        /// @class ContextType
        /// Convention: nested to the Detector element
        struct ContextType {
            /// The current interval of validity
            unsigned int iov = 0;
        };

        /// Constructor for detector element bound to a Cylinder Surface
        ///
        /// @param ringId is the unique identifier for the detector element
        /// @param transform is the transform that places the element in 3D space
        /// @param cBounds are the cylinder bounds for the detector element
        /// @param thickness is the module thickness
        /// @param material is the (optional) Surface material associated with it
        MtpcDetectorElement(
            uint32_t ringId,
            std::shared_ptr<const Acts::Transform3> transform,
            std::shared_ptr<const Acts::CylinderBounds> cBounds,
            double thickness,
            std::shared_ptr<const Acts::ISurfaceMaterial> material = nullptr);

        /// Destructor
        ~MtpcDetectorElement() override = default;

        /// Return the surface associated with this detector element
        const Acts::Surface &surface() const final;

        /// Non-const access to the surface associated with this detector element
        Acts::Surface &surface() final;

        /// The maximal thickness of the detector element with respect to the normal axis
        double thickness() const final;

        /// Return local to global transform associated with this identifier
        const Acts::Transform3 &transform(const Acts::GeometryContext &gctx) const final;

        /// Return the nominal local to global transform
        const Acts::Transform3 &nominalTransform(const Acts::GeometryContext &gctx) const;

        /// Add an aligned transform for a specific interval of validity
        void addAlignedTransform(std::unique_ptr<Acts::Transform3> alignedTransform, unsigned int iov);

        /// Return the set of alignment transforms in flight
        const std::vector<std::unique_ptr<Acts::Transform3> > &alignedTransforms() const;

    private:
        /// The transform for positioning in 3D space
        std::shared_ptr<const Acts::Transform3> m_elementTransform = nullptr;
        /// The aligned transforms
        std::vector<std::unique_ptr<Acts::Transform3> > m_alignedTransforms = {};
        /// The surface represented by it
        std::shared_ptr<Acts::CylinderSurface> m_elementSurface = nullptr;
        /// The element thickness
        double m_elementThickness = 0.;
        /// The cylinder bounds
        std::shared_ptr<const Acts::CylinderBounds> m_elementCylinderBounds = nullptr;
        /// Unique identifier
        uint32_t m_id;
    };
} // namespace tdis::tracking
