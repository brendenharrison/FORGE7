#include "MidiHardwareMapping.h"

namespace forge7
{

DevelopmentMidiMapping::DevelopmentMidiMapping()
{
    normalizeEncoderCcList();
}

void DevelopmentMidiMapping::normalizeEncoderCcList()
{
    if (! encoderRelativeCcNumbers.contains(ccEncoderPrimary))
        encoderRelativeCcNumbers.add(ccEncoderPrimary);
}

bool DevelopmentMidiMapping::operator==(const DevelopmentMidiMapping& o) const noexcept
{
    if (ccKnobs != o.ccKnobs)
        return false;

    if (noteAssignButton1 != o.noteAssignButton1 || noteAssignButton2 != o.noteAssignButton2
        || noteChainPrevious != o.noteChainPrevious || noteChainNext != o.noteChainNext)
        return false;

    if (ccEncoderPrimary != o.ccEncoderPrimary || noteEncoderPress != o.noteEncoderPress
        || noteEncoderLongPress != o.noteEncoderLongPress)
        return false;

    return encoderRelativeCcNumbers == o.encoderRelativeCcNumbers;
}

} // namespace forge7
