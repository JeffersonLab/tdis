// Constants (define outside of the component)
export const numRings = 21;
export const numPadsPerRing = 122;
export const firstRingInnerRadius = 5.0;
export const maxRadius = 15.0;
export const totalRadius = maxRadius - firstRingInnerRadius;
export const ringWidth = totalRadius / numRings;
export const deltaTheta = (2 * Math.PI) / numPadsPerRing;

export const planePositions = [

]


export function getPadCenter(ring: number, pad: number): [number, number] {
    if (!(ring >= 0 && ring < numRings)) {
        throw new Error(`Ring index must be between 0 and ${numRings - 1}`);
    }
    if (!(pad >= 0 && pad < numPadsPerRing)) {
        throw new Error(`Pad index must be between 0 and ${numPadsPerRing - 1}`);
    }
    const rCenter = ringWidth * (ring + 0.5) + firstRingInnerRadius;
    const thetaOffset = ring % 2 === 0 ? 0 : deltaTheta / 2;
    const thetaCenter = deltaTheta / 2 + pad * deltaTheta + thetaOffset;
    const x = rCenter * Math.cos(thetaCenter);
    const y = rCenter * Math.sin(thetaCenter);
    return [x, y];
}
