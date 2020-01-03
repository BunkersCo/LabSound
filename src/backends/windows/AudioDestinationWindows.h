// License: BSD 3 Clause
// Copyright (C) 2010, Google Inc. All rights reserved.
// Copyright (C) 2015+, The LabSound Authors. All rights reserved.

#ifndef AudioDestinationWin_h
#define AudioDestinationWin_h

#include "LabSound/core/AudioNode.h"
#include "LabSound/core/AudioBus.h"

#include "internal/AudioDestination.h"

#include "rtaudio/RtAudio.h"
#include <iostream>
#include <memory>
#include <cstdlib>

namespace lab {

class AudioDestinationWin : public AudioDestination
{

public:

    AudioDestinationWin(AudioIOCallback &, uint32_t numChannels, float sampleRate);
    virtual ~AudioDestinationWin();

    virtual void start() override;
    virtual void stop() override;

    float sampleRate() const override { return m_sampleRate; }

    void render(int numberOfFrames, void * outputBuffer, void * inputBuffer);

private:
    void configure();

    AudioIOCallback & m_callback;
    std::unique_ptr<AudioBus> m_renderBus;
    std::unique_ptr<AudioBus> m_inputBus;
    size_t m_numChannels;
    float m_sampleRate;
    RtAudio dac;
};

int outputCallback(void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void *userData );

} // namespace lab

#endif // AudioDestinationWin_h

