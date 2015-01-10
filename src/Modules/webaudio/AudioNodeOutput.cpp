/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "LabSoundConfig.h"
#include "AudioNodeOutput.h"

#include "AudioBus.h"
#include "AudioContext.h"
#include "AudioNodeInput.h"
#include "AudioParam.h"
#include <wtf/Threading.h>

namespace WebCore {

AudioNodeOutput::AudioNodeOutput(AudioNode* node, unsigned numberOfChannels)
    : m_node(node)
    , m_numberOfChannels(numberOfChannels)
    , m_desiredNumberOfChannels(numberOfChannels)
    , m_actualDestinationBus(0)
    , m_isEnabled(true)
    , m_renderingFanOutCount(0)
    , m_renderingParamFanOutCount(0)
{
    ASSERT(numberOfChannels <= AudioContext::maxNumberOfChannels());

    m_internalBus = std::unique_ptr<AudioBus>(new AudioBus(numberOfChannels, AudioNode::ProcessingSizeInFrames));
    m_actualDestinationBus = m_internalBus.get();
}

void AudioNodeOutput::setNumberOfChannels(unsigned numberOfChannels)
{
    if (m_numberOfChannels != numberOfChannels) {
        ASSERT(numberOfChannels <= AudioContext::maxNumberOfChannels());
        ASSERT(!context().expired());
        std::shared_ptr<AudioContext> ac = context().lock();
        ASSERT(ac->isGraphOwner());

        m_desiredNumberOfChannels = numberOfChannels;
    }
}

void AudioNodeOutput::updateInternalBus()
{
    if (numberOfChannels() == m_internalBus->numberOfChannels())
        return;

    m_internalBus = std::unique_ptr<AudioBus>(new AudioBus(numberOfChannels(), AudioNode::ProcessingSizeInFrames));

    // This may later be changed in pull() to point to an in-place bus with the same number of channels.
    m_actualDestinationBus = m_internalBus.get();
}

void AudioNodeOutput::updateRenderingState()
{
    updateNumberOfChannels();
    m_renderingFanOutCount = fanOutCount();
    m_renderingParamFanOutCount = paramFanOutCount();
}

void AudioNodeOutput::updateNumberOfChannels()
{
    if (m_numberOfChannels != m_desiredNumberOfChannels) {
        ASSERT(!context().expired());
        std::shared_ptr<AudioContext> ac = context().lock();
        
        ASSERT(ac->isAudioThread());
        ASSERT(ac->isGraphOwner());

        m_numberOfChannels = m_desiredNumberOfChannels;
        updateInternalBus();
        propagateChannelCount();
    }
}

void AudioNodeOutput::propagateChannelCount()
{
    if (isChannelCountKnown()) {
        ASSERT(!context().expired());
        std::shared_ptr<AudioContext> ac = context().lock();
        ASSERT(ac->isAudioThread() && ac->isGraphOwner());
        
        // Announce to any nodes we're connected to that we changed our channel count for its input.
        for (auto i = m_inputs.begin(); i != m_inputs.end(); ++i) {
            AudioNodeInput* input = (*i).get();
            AudioNode* connectionNode = input->node();
            connectionNode->checkNumberOfChannelsForInput(input);
        }
    }
}

AudioBus* AudioNodeOutput::pull(AudioBus* inPlaceBus, size_t framesToProcess)
{
    ASSERT(!context().expired());
    std::shared_ptr<AudioContext> ac = context().lock();
    if (!ac->isAudioThread()) {
        printf("foo %d\n", currentThread());
    }
    ASSERT(ac->isAudioThread());
    ASSERT(m_renderingFanOutCount > 0 || m_renderingParamFanOutCount > 0);
    
    updateNumberOfChannels();
    
    // Causes our AudioNode to process if it hasn't already for this render quantum.
    // We try to do in-place processing (using inPlaceBus) if at all possible,
    // but we can't process in-place if we're connected to more than one input (fan-out > 1).
    // In this case pull() is called multiple times per rendering quantum, and the processIfNecessary() call below will
    // cause our node to process() only the first time, caching the output in m_internalOutputBus for subsequent calls.    
    
    bool isInPlace = inPlaceBus && inPlaceBus->numberOfChannels() == numberOfChannels() && (m_renderingFanOutCount + m_renderingParamFanOutCount) == 1;

    // Setup the actual destination bus for processing when our node's process() method gets called in processIfNecessary() below.
    m_actualDestinationBus = isInPlace ? inPlaceBus : m_internalBus.get();

    node()->processIfNecessary(framesToProcess);
    return m_actualDestinationBus;
}

AudioBus* AudioNodeOutput::bus() const
{
    ASSERT(m_actualDestinationBus);
    return m_actualDestinationBus;
}

unsigned AudioNodeOutput::fanOutCount()
{
    return m_inputs.size();
}

unsigned AudioNodeOutput::paramFanOutCount()
{
    return m_params.size();
}

unsigned AudioNodeOutput::renderingFanOutCount() const
{
    return m_renderingFanOutCount;
}

unsigned AudioNodeOutput::renderingParamFanOutCount() const
{
    return m_renderingParamFanOutCount;
}

void AudioNodeOutput::addInput(std::shared_ptr<AudioNodeInput> input)
{
    ASSERT(!context().expired());
    std::shared_ptr<AudioContext> ac = context().lock();
    ASSERT(ac->isGraphOwner());

    ASSERT(input);
    if (!input)
        return;

    m_inputs.insert(input);
}

void AudioNodeOutput::removeInput(std::shared_ptr<AudioNodeInput> input)
{
    ASSERT(!context().expired());
    std::shared_ptr<AudioContext> ac = context().lock();
    ASSERT(ac->isGraphOwner());

    ASSERT(input);
    if (!input)
        return;

    auto it = m_inputs.find(input);
    if (it != m_inputs.end())
        m_inputs.erase(it);
}

void AudioNodeOutput::disconnectAllInputs(std::shared_ptr<AudioNodeOutput> self)
{
    ASSERT(!self->context().expired());
    std::shared_ptr<AudioContext> ac = self->context().lock();
    ASSERT(ac->isGraphOwner());
    
    // AudioNodeInput::disconnect() changes m_inputs by calling removeInput().
    while (self->m_inputs.size()) {
        auto input = self->m_inputs.begin();
        (*input)->disconnect(*input, self);
    }
}

    void AudioNodeOutput::addParam(std::shared_ptr<AudioParam> param)
{
    ASSERT(!context().expired());
    std::shared_ptr<AudioContext> ac = context().lock();
    ASSERT(ac->isGraphOwner());

    ASSERT(param);
    if (!param)
        return;

    m_params.insert(param);
}

void AudioNodeOutput::removeParam(std::shared_ptr<AudioParam> param)
{
    ASSERT(!context().expired());
    std::shared_ptr<AudioContext> ac = context().lock();
    ASSERT(ac->isGraphOwner());

    ASSERT(param);
    if (!param)
        return;

    auto it = m_params.find(param);
    if (it != m_params.end())
        m_params.erase(it);
}

void AudioNodeOutput::disconnectAllParams(std::shared_ptr<AudioNodeOutput> self)
{
    ASSERT(!self->context().expired());
    std::shared_ptr<AudioContext> ac = self->context().lock();
    ASSERT(ac->isGraphOwner());

    // AudioParam::disconnect() changes m_params by calling removeParam().
    while (self->m_params.size()) {
        auto param = self->m_params.begin();
        (*param)->disconnect(*param, self);
    }
}

void AudioNodeOutput::disconnectAll(std::shared_ptr<AudioNodeOutput> self)
{
    self->disconnectAllInputs(self);
    self->disconnectAllParams(self);
}

void AudioNodeOutput::disable(std::shared_ptr<AudioNodeOutput> self)
{
    ASSERT(!self->context().expired());
    std::shared_ptr<AudioContext> ac = self->context().lock();
    ASSERT(ac->isGraphOwner());

    if (self->m_isEnabled) {
        for (auto i : self->m_inputs) {
            AudioNodeInput::disable(i, self);
        }
        self->m_isEnabled = false;
    }
}

void AudioNodeOutput::enable(std::shared_ptr<AudioNodeOutput> self)
{
    if (!self->m_isEnabled) {
        ASSERT(!self->context().expired());
        std::shared_ptr<AudioContext> ac = self->context().lock();
        ASSERT(ac->isGraphOwner());
        
        for (auto i : self->m_inputs) {
            AudioNodeInput::enable(i, self);
        }
        self->m_isEnabled = true;
    }
}

} // namespace WebCore
