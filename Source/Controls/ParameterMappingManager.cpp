#include "ParameterMappingManager.h"

#include <algorithm>
#include <utility>

#include "HardwareControlTypes.h"

#include "../PluginHost/PluginChain.h"
#include "../PluginHost/PluginHostManager.h"
#include "../PluginHost/PluginSlot.h"
#include "../Scene/Scene.h"
#include "../Scene/SceneManager.h"
#include "../Utilities/Logger.h"

namespace forge7
{
namespace
{
constexpr const char* kHardwareIdKeys[] = {
    "Knob1",
    "Knob2",
    "Knob3",
    "Knob4",
    "AssignButton1",
    "AssignButton2",
    "ChainPreviousButton",
    "ChainNextButton",
    "EncoderRotate",
    "EncoderPress",
    "EncoderLongPress",
};

bool isMappableAssignable(HardwareControlId id) noexcept
{
    switch (id)
    {
        case HardwareControlId::Knob1:
        case HardwareControlId::Knob2:
        case HardwareControlId::Knob3:
        case HardwareControlId::Knob4:
        case HardwareControlId::AssignButton1:
        case HardwareControlId::AssignButton2:
            return true;
        default:
            return false;
    }
}
} // namespace

ParameterMappingManager::ParameterMappingManager(SceneManager& scenes, PluginHostManager& hosts)
    : sceneManager(scenes)
    , pluginHostManager(hosts)
{
}

ParameterMappingManager::~ParameterMappingManager() = default;

void ParameterMappingManager::clampDescriptor(ParameterMappingDescriptor& d)
{
    d.minValue = juce::jlimit(0.0f, 1.0f, d.minValue);
    d.maxValue = juce::jlimit(0.0f, 1.0f, d.maxValue);

    if (d.minValue > d.maxValue)
        std::swap(d.minValue, d.maxValue);

    if (d.toggleMode && d.momentaryMode)
        d.momentaryMode = false;
}

juce::String ParameterMappingManager::hardwareIdToKey(const HardwareControlId id)
{
    const int i = static_cast<int>(id);

    if (i >= 0 && i < static_cast<int>(sizeof(kHardwareIdKeys) / sizeof(kHardwareIdKeys[0])))
        return kHardwareIdKeys[i];

    return "Knob1";
}

bool ParameterMappingManager::keyToHardwareId(const juce::String& key, HardwareControlId& out)
{
    for (int i = 0; i < static_cast<int>(sizeof(kHardwareIdKeys) / sizeof(kHardwareIdKeys[0])); ++i)
    {
        if (key == kHardwareIdKeys[i])
        {
            out = static_cast<HardwareControlId>(i);
            return true;
        }
    }

    return false;
}

const ParameterMappingDescriptor* ParameterMappingManager::findActiveMapping(const HardwareControlEvent& event,
                                                                             const juce::String& sceneId,
                                                                             const juce::String& variationId) const
{
    if (! isMappableAssignable(event.id))
        return nullptr;

    for (const auto& m : mappings)
    {
        if (m.hardwareControlId != event.id)
            continue;

        if (m.sceneId != sceneId || m.chainVariationId != variationId)
            continue;

        return &m;
    }

    return nullptr;
}

juce::AudioProcessorParameter* ParameterMappingManager::resolveParameter(juce::AudioProcessor& processor,
                                                                        const ParameterMappingDescriptor& row) const
{
    if (row.pluginParameterId.isNotEmpty())
    {
        for (auto* p : processor.getParameters())
        {
            if (p == nullptr)
                continue;

            if (auto* hp = dynamic_cast<juce::HostedAudioProcessorParameter*>(p))
                if (hp->getParameterID() == row.pluginParameterId)
                    return p;
        }
    }

    if (row.pluginParameterIndex >= 0)
    {
        const auto params = processor.getParameters();

        if (row.pluginParameterIndex < params.size())
            return params[static_cast<size_t>(row.pluginParameterIndex)];
    }

    return nullptr;
}

void ParameterMappingManager::applyNormalizedToParameter(juce::AudioProcessorParameter& param,
                                                       const ParameterMappingDescriptor& row,
                                                       const float hardwareNormalized01) const
{
    float hw = juce::jlimit(0.0f, 1.0f, hardwareNormalized01);

    if (row.invert)
        hw = 1.0f - hw;

    const float span = row.maxValue - row.minValue;
    const float pluginNorm = row.minValue + hw * span;

    param.beginChangeGesture();
    param.setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, pluginNorm));
    param.endChangeGesture();
}

void ParameterMappingManager::applyButtonToParameter(juce::AudioProcessorParameter& param,
                                                     const ParameterMappingDescriptor& row,
                                                     const bool pressed) const
{
    if (row.toggleMode)
    {
        if (! pressed)
            return;

        const float cur = param.getValue();
        const float mid = (row.minValue + row.maxValue) * 0.5f;
        const float next = (cur < mid) ? row.maxValue : row.minValue;

        param.beginChangeGesture();
        param.setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, next));
        param.endChangeGesture();
        return;
    }

    param.beginChangeGesture();

    if (pressed)
        param.setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, row.maxValue));
    else
        param.setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, row.minValue));

    param.endChangeGesture();
}

void ParameterMappingManager::processHardwareEvent(const HardwareControlEvent& event)
{
#if JUCE_DEBUG
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr)
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
#endif

    auto* scene = sceneManager.getActiveScene();

    if (scene == nullptr)
        return;

    scene->clampActiveVariationIndex();

    const int vidx = scene->getActiveChainVariationIndex();
    const auto& vars = scene->getVariations();

    if (! juce::isPositiveAndBelow(vidx, static_cast<int>(vars.size())))
        return;

    auto* variation = vars[static_cast<size_t>(vidx)].get();

    if (variation == nullptr)
        return;

    const juce::String sceneId = scene->getSceneId();
    const juce::String variationId = variation->getVariationId();

    /** Assign Mode (fullscreen editor): first K1–K4 movement after prepareKnobAssignment… binds hardware,
        then subsequent moves use normal mapping — audio stays on message thread only. */
    if (event.type == HardwareControlType::AbsoluteNormalized && isKnobId(event.id))
    {
        juce::String learnParamId;
        int learnParamIdx = -1;
        int learnSlotIdx = -1;
        juce::String learnDisplay;
        bool consumeLearn = false;

        {
            const juce::ScopedLock lock(assignmentLearnLock);

            if (knobAssignmentLearn.awaitingKnobTwist)
            {
                learnParamId = knobAssignmentLearn.pluginParameterId;
                learnParamIdx = knobAssignmentLearn.pluginParameterIndex;
                learnSlotIdx = knobAssignmentLearn.pluginSlotIndex;
                learnDisplay = knobAssignmentLearn.displayNameForUi;
                knobAssignmentLearn.awaitingKnobTwist = false;
                consumeLearn = true;
            }
        }

        if (consumeLearn)
        {
            assignParameterToHardwareInActiveVariation(event.id,
                                                       learnSlotIdx,
                                                       learnParamId,
                                                       learnParamIdx,
                                                       learnDisplay);
            return;
        }
    }

    ParameterMappingDescriptor mapping {};
    bool foundMapping = false;

    {
        const juce::ScopedLock lock(mappingLock);

        if (const auto* row = findActiveMapping(event, sceneId, variationId))
        {
            mapping = *row;
            foundMapping = true;
        }
    }

    if (! foundMapping)
        return;

    auto* chain = pluginHostManager.getPluginChain();

    if (chain == nullptr)
        return;

    auto* slot = chain->getSlot(static_cast<size_t>(mapping.pluginSlotIndex));

    if (slot == nullptr)
        return;

    auto* instance = slot->getHostedInstance();

    if (instance == nullptr)
        return;

    auto* param = resolveParameter(*instance, mapping);

    if (param == nullptr)
    {
        Logger::warn("FORGE7: mapping targets missing parameter — slot "
                     + juce::String(mapping.pluginSlotIndex));
        return;
    }

    if (! param->isAutomatable())
        return;

    if (event.type == HardwareControlType::AbsoluteNormalized)
    {
        applyNormalizedToParameter(*param, mapping, event.value);
        return;
    }

    if (event.type == HardwareControlType::ButtonPressed || event.type == HardwareControlType::ButtonReleased)
    {
        const bool pressed = (event.type == HardwareControlType::ButtonPressed);
        applyButtonToParameter(*param, mapping, pressed);
    }
}

void ParameterMappingManager::upsertMapping(ParameterMappingDescriptor mapping)
{
    clampDescriptor(mapping);

    const juce::ScopedLock lock(mappingLock);

    for (auto& existing : mappings)
    {
        if (existing.hardwareControlId == mapping.hardwareControlId && existing.sceneId == mapping.sceneId
            && existing.chainVariationId == mapping.chainVariationId)
        {
            existing = std::move(mapping);
            return;
        }
    }

    mappings.push_back(std::move(mapping));
}

void ParameterMappingManager::removeMapping(const ParameterMappingDescriptor& keyMatch)
{
    const juce::ScopedLock lock(mappingLock);

    mappings.erase(std::remove_if(mappings.begin(),
                                  mappings.end(),
                                  [&](const ParameterMappingDescriptor& m)
                                  {
                                      return m.hardwareControlId == keyMatch.hardwareControlId
                                             && m.sceneId == keyMatch.sceneId
                                             && m.chainVariationId == keyMatch.chainVariationId;
                                  }),
                   mappings.end());
}

juce::Array<ParameterMappingDescriptor> ParameterMappingManager::getAllMappings() const
{
    const juce::ScopedLock lock(mappingLock);

    juce::Array<ParameterMappingDescriptor> out;

    for (const auto& m : mappings)
        out.add(m);

    return out;
}

bool ParameterMappingManager::upsertMappingForActiveSceneVariation(ParameterMappingDescriptor fields)
{
    auto* scene = sceneManager.getActiveScene();

    if (scene == nullptr)
        return false;

    scene->clampActiveVariationIndex();

    const int vidx = scene->getActiveChainVariationIndex();
    const auto& vars = scene->getVariations();

    if (! juce::isPositiveAndBelow(vidx, static_cast<int>(vars.size())))
        return false;

    auto* variation = vars[static_cast<size_t>(vidx)].get();

    if (variation == nullptr)
        return false;

    fields.sceneId = scene->getSceneId();
    fields.chainVariationId = variation->getVariationId();

    upsertMapping(std::move(fields));
    return true;
}

bool ParameterMappingManager::assignParameterToHardwareInActiveVariation(const HardwareControlId hardwareId,
                                                                        const int pluginSlotIndex,
                                                                        const juce::String& pluginParameterId,
                                                                        const int pluginParameterIndexFallback,
                                                                        const juce::String& displayNameForUi,
                                                                        const float minNorm,
                                                                        const float maxNorm,
                                                                        const bool invertMapping,
                                                                        const bool toggleForButton,
                                                                        const bool momentaryForButton)
{
    if (! isMappableAssignable(hardwareId))
        return false;

    ParameterMappingDescriptor d {};
    d.hardwareControlId = hardwareId;
    d.pluginSlotIndex = pluginSlotIndex;
    d.pluginParameterId = pluginParameterId;
    d.pluginParameterIndex = pluginParameterIndexFallback;
    d.displayName = displayNameForUi;
    d.minValue = minNorm;
    d.maxValue = maxNorm;
    d.invert = invertMapping;

    const bool isButton = isAssignButtonId(hardwareId);

    if (isButton)
    {
        d.toggleMode = toggleForButton;
        d.momentaryMode = momentaryForButton;

        if (toggleForButton)
            d.momentaryMode = false;
    }
    else
    {
        d.toggleMode = false;
        d.momentaryMode = true;
    }

    return upsertMappingForActiveSceneVariation(std::move(d));
}

juce::Array<AutomatableParameterSummary> ParameterMappingManager::getAutomatableParametersForSlot(const int pluginSlotIndex) const
{
    juce::Array<AutomatableParameterSummary> result;

    auto* chain = pluginHostManager.getPluginChain();

    if (chain == nullptr || ! juce::isPositiveAndBelow(pluginSlotIndex, PluginChain::getMaxSlots()))
        return result;

    auto* slot = chain->getSlot(static_cast<size_t>(pluginSlotIndex));

    if (slot == nullptr)
        return result;

    auto* instance = slot->getHostedInstance();

    if (instance == nullptr)
        return result;

    auto& processor = *instance;

    int index = 0;

    for (auto* p : processor.getParameters())
    {
        if (p == nullptr || ! p->isAutomatable())
        {
            ++index;
            continue;
        }

        AutomatableParameterSummary row;

        if (auto* hp = dynamic_cast<juce::HostedAudioProcessorParameter*>(p))
            row.parameterId = hp->getParameterID();
        row.parameterIndex = index;
        row.name = p->getName(256);
        row.isAutomatable = true;
        result.add(std::move(row));
        ++index;
    }

    return result;
}

juce::var ParameterMappingManager::exportMappingsToVar() const
{
    juce::Array<juce::var> rows;

    const juce::ScopedLock lock(mappingLock);

    for (const auto& m : mappings)
    {
        auto* o = new juce::DynamicObject();

        o->setProperty("hardwareControlId", hardwareIdToKey(m.hardwareControlId));
        o->setProperty("sceneId", m.sceneId);
        o->setProperty("chainVariationId", m.chainVariationId);
        o->setProperty("pluginSlotIndex", m.pluginSlotIndex);
        o->setProperty("pluginParameterId", m.pluginParameterId);
        o->setProperty("pluginParameterIndex", m.pluginParameterIndex);
        o->setProperty("displayName", m.displayName);
        o->setProperty("minValue", static_cast<double>(m.minValue));
        o->setProperty("maxValue", static_cast<double>(m.maxValue));
        o->setProperty("invert", m.invert);
        o->setProperty("toggleMode", m.toggleMode);
        o->setProperty("momentaryMode", m.momentaryMode);

        rows.add(juce::var(o));
    }

    return juce::var(rows);
}

void ParameterMappingManager::importMappingsFromVar(const juce::var& data)
{
    const juce::ScopedLock lock(mappingLock);
    mappings.clear();

    if (! data.isArray())
        return;

    const auto* arr = data.getArray();

    if (arr == nullptr)
        return;

    for (const auto& item : *arr)
    {
        auto* o = item.getDynamicObject();

        if (o == nullptr)
            continue;

        if (o->hasProperty("slotIndex") && o->hasProperty("parameterIndex") && ! o->hasProperty("hardwareControlId"))
            continue;

        if (! o->hasProperty("hardwareControlId"))
            continue;

        HardwareControlId hid {};

        if (! keyToHardwareId(o->getProperty("hardwareControlId").toString(), hid))
            continue;

        ParameterMappingDescriptor m {};
        m.hardwareControlId = hid;
        m.sceneId = o->getProperty("sceneId").toString();
        m.chainVariationId = o->getProperty("chainVariationId").toString();
        m.pluginSlotIndex = static_cast<int>(o->getProperty("pluginSlotIndex"));
        m.pluginParameterId = o->getProperty("pluginParameterId").toString();
        m.pluginParameterIndex = o->hasProperty("pluginParameterIndex")
                                       ? static_cast<int>(o->getProperty("pluginParameterIndex"))
                                       : -1;
        m.displayName = o->getProperty("displayName").toString();
        m.minValue = o->hasProperty("minValue")
                          ? static_cast<float>(static_cast<double>(o->getProperty("minValue")))
                          : 0.0f;
        m.maxValue = o->hasProperty("maxValue")
                          ? static_cast<float>(static_cast<double>(o->getProperty("maxValue")))
                          : 1.0f;
        m.invert = o->hasProperty("invert") ? static_cast<bool>(o->getProperty("invert")) : false;
        m.toggleMode = o->hasProperty("toggleMode") ? static_cast<bool>(o->getProperty("toggleMode")) : false;
        m.momentaryMode =
            o->hasProperty("momentaryMode") ? static_cast<bool>(o->getProperty("momentaryMode")) : true;

        clampDescriptor(m);
        mappings.push_back(std::move(m));
    }
}

void ParameterMappingManager::prepareKnobAssignmentToNextHardwareMove(const int pluginSlotIndex,
                                                                        const juce::String& pluginParameterId,
                                                                        const int pluginParameterIndexFallback,
                                                                        const juce::String& displayNameForUi)
{
    const juce::ScopedLock lock(assignmentLearnLock);
    knobAssignmentLearn.awaitingKnobTwist = true;
    knobAssignmentLearn.pluginSlotIndex = pluginSlotIndex;
    knobAssignmentLearn.pluginParameterId = pluginParameterId;
    knobAssignmentLearn.pluginParameterIndex = pluginParameterIndexFallback;
    knobAssignmentLearn.displayNameForUi = displayNameForUi;
}

void ParameterMappingManager::cancelKnobAssignmentLearn() noexcept
{
    const juce::ScopedLock lock(assignmentLearnLock);
    knobAssignmentLearn.awaitingKnobTwist = false;
}

bool ParameterMappingManager::isAwaitingKnobAssignmentHardwareMove() const noexcept
{
    const juce::ScopedLock lock(assignmentLearnLock);
    return knobAssignmentLearn.awaitingKnobTwist;
}

void ParameterMappingManager::armLearnTargetForHardware(const HardwareControlId hardwareId)
{
    /** Future: host a modal learn session; tie into `AudioProcessorEditor` param touch — not implemented. */
    learnHardwareTarget = hardwareId;
    learnArmed = true;
}

void ParameterMappingManager::cancelLearn()
{
    learnArmed = false;
}

bool ParameterMappingManager::isLearning() const noexcept
{
    return learnArmed;
}

} // namespace forge7
