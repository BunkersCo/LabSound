// License: BSD 2 Clause
// Copyright (C) 2015+, The LabSound Authors. All rights reserved.

#include "LabSound/core/AudioNodeOutput.h"
#include "LabSound/core/AudioNodeInput.h"
#include "LabSound/core/AudioBus.h"

#include "LabSound/extended/FunctionNode.h"
#include "LabSound/extended/AudioContextLock.h"

using namespace std;
using namespace lab;

namespace lab
{
    
    FunctionNode::FunctionNode(size_t channels) : AudioScheduledSourceNode()
    {
        addOutput(std::unique_ptr<AudioNodeOutput>(new AudioNodeOutput(this, channels)));
        initialize();
    }
    
    FunctionNode::~FunctionNode()
    {
        uninitialize();
    }
    
    void FunctionNode::process(ContextRenderLock & r)
    {
        AudioBus * outputBus = output(0)->bus(r);

        if (!isInitialized() || !outputBus->numberOfChannels() || !_function)
        {
            outputBus->zero();
            return;
        }
        
        uint32_t quantumFrameOffset;
        uint32_t nonSilentFramesToProcess;

        updateSchedulingInfo(r, outputBus, quantumFrameOffset, nonSilentFramesToProcess);

        if (!nonSilentFramesToProcess) 
        {
            outputBus->zero();
            return;
        }

        for (size_t i = 0; i < outputBus->numberOfChannels(); ++i)
        {
            float * destP = outputBus->channel(i)->mutableData();
            
            // Start rendering at the correct offset.
            destP += quantumFrameOffset;
            _function(r, this, static_cast<int>(i), destP, nonSilentFramesToProcess);
        }

        _now += double(r.context()->currentFrames()) / r.context()->sampleRate();
        outputBus->clearSilentFlag();
    }
    
    void FunctionNode::reset(ContextRenderLock & r)
    {
        // No-op
    }
    
    bool FunctionNode::propagatesSilence(ContextRenderLock & r) const
    {
        return !isPlayingOrScheduled() || hasFinished();
    }
    
} // namespace lab
