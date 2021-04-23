#include "LbapSpatAlgorithm.hpp"

//==============================================================================
void LbapSpatAlgorithm::init(SpeakersData const & speakers)
{
    mData = lbap_field_setup(speakers);
}

//==============================================================================
void LbapSpatAlgorithm::computeSpeakerGains(SourceData const & source, SpeakersSpatGains & gains) const noexcept
{
    lbap_field_compute(source, mData);
}

//==============================================================================
juce::Array<Triplet> LbapSpatAlgorithm::getTriplets() const noexcept
{
    jassertfalse;
    return juce::Array<Triplet>{};
}