#include "ParameterMappingDescriptor.h"

namespace forge7
{

bool ParameterMappingDescriptor::operator==(const ParameterMappingDescriptor& o) const noexcept
{
    return hardwareControlId == o.hardwareControlId && sceneId == o.sceneId
           && chainVariationId == o.chainVariationId && pluginSlotIndex == o.pluginSlotIndex
           && pluginParameterId == o.pluginParameterId && pluginParameterIndex == o.pluginParameterIndex
           && displayName == o.displayName && minValue == o.minValue && maxValue == o.maxValue
           && invert == o.invert && toggleMode == o.toggleMode && momentaryMode == o.momentaryMode;
}

} // namespace forge7
