#pragma once

#include <juce_core/juce_core.h>

class CuarzoProcessor;

namespace cuarzo::presets
{
int count();
juce::String name (int index);
void apply (int index, CuarzoProcessor& processor);
}
