#include "MtpcDetectorElement.hpp"

#include "Acts/Geometry/GeometryContext.hpp"
#include "Acts/Surfaces/CylinderBounds.hpp"
#include "Acts/Surfaces/Surface.hpp"

#include <Acts/Surfaces/CylinderSurface.hpp>

#include "Acts/Definitions/Algebra.hpp"

namespace tdis::tracking {
    // Implementation

    inline MtpcDetectorElement::MtpcDetectorElement(
        uint32_t planeId,
        std::shared_ptr<const Acts::Transform3> transform,
        std::shared_ptr<const Acts::CylinderBounds> cBounds,
        double thickness,
        std::shared_ptr<const Acts::ISurfaceMaterial> material)
        : m_elementTransform(std::move(transform)),
          m_elementThickness(thickness),
          m_elementCylinderBounds(std::move(cBounds)),
          m_id(planeId)
    {
        // Create the cylinder surface
        m_elementSurface = Acts::Surface::makeShared<Acts::CylinderSurface>(
            this, // Detector element pointer
            *m_elementTransform,
            *m_elementCylinderBounds);

        if (material) {
            m_elementSurface->assignSurfaceMaterial(material);
        }
    }

    inline const Acts::Surface& MtpcDetectorElement::surface() const { return *m_elementSurface; }

    inline Acts::Surface& MtpcDetectorElement::surface() { return *m_elementSurface; }

    inline double MtpcDetectorElement::thickness() const { return m_elementThickness; }

    inline const Acts::Transform3& MtpcDetectorElement::transform(const Acts::GeometryContext& gctx) const
    {
        // Check if a different transform than the nominal exists
        if (!m_alignedTransforms.empty()) {
            // Cast into the right context object
            auto alignContext = gctx.get<ContextType>();
            return (*m_alignedTransforms[alignContext.iov].get());
        }
        // Return the standard transform if not found
        return nominalTransform(gctx);
    }

    inline const Acts::Transform3& MtpcDetectorElement::nominalTransform(const Acts::GeometryContext& /*gctx*/) const
    {
        return *m_elementTransform;
    }

    inline void MtpcDetectorElement::addAlignedTransform(std::unique_ptr<Acts::Transform3> alignedTransform, unsigned int iov)
    {
        // Ensure the vector is large enough
        auto size = m_alignedTransforms.size();
        if (iov >= size) {
            m_alignedTransforms.resize(iov + 1);
        }
        m_alignedTransforms[iov] = std::move(alignedTransform);
    }

    inline const std::vector<std::unique_ptr<Acts::Transform3>>&
    MtpcDetectorElement::alignedTransforms() const {
        return m_alignedTransforms;
    }
}