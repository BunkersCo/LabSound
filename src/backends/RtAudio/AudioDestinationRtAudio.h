// License: BSD 3 Clause
// Copyright (C) 2010, Google Inc. All rights reserved.
// Copyright (C) 2015+, The LabSound Authors. All rights reserved.

#ifndef AudioDestinationRtAudio_h
#define AudioDestinationRtAudio_h

#include "LabSound/core/AudioNode.h"
#include "LabSound/core/AudioBus.h"

#include "internal/AudioDestination.h"

#include "rtaudio/RtAudio.h"
#include <iostream>
#include <memory>
#include <cstdlib>

namespace lab {

class AudioDestinationRtAudio : public AudioDestination
{

public:

    AudioDestinationRtAudio(AudioIOCallback &,
                            uint32_t numOutputChannels, uint32_t numInputChannels,
                            float sampleRate);
    virtual ~AudioDestinationRtAudio();

    virtual void start() override;
    virtual void stop() override;

    float sampleRate() const override { return m_sampleRate; }

    void render(int numberOfFrames, void * outputBuffer, void * inputBuffer);

private:

    void configure();

    AudioIOCallback & m_callback;
    std::unique_ptr<AudioBus> m_renderBus;
    std::unique_ptr<AudioBus> m_inputBus;
    uint32_t m_numChannels;
    uint32_t m_numInputChannels;
    float m_sampleRate;
    RtAudio dac;
};

int outputCallback(void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void *userData );

} // namespace lab

#endif // AudioDestinationRtAudio_h

