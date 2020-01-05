
// License: BSD 3 Clause
// Copyright (C) 2020, The LabSound Authors. All rights reserved.

#ifndef AudioDestinationRtAudio_h
#define AudioDestinationRtAudio_h

#include "LabSound/core/AudioBus.h"
#include "LabSound/core/AudioNode.h"

#include "internal/AudioDestination.h"

#include "RtAudio.h"

namespace lab
{

class AudioDestinationRtAudio : public AudioDestination
{

public:
    AudioDestinationRtAudio(AudioIOCallback &,
                            uint32_t numOutputChannels, uint32_t numInputChannels,
                            float sampleRate);
    virtual ~AudioDestinationRtAudio();

    virtual void start() override;
    virtual void stop() override;

    float sampleRate() const override { return _sampleRate; }

    void render(int numberOfFrames, void * outputBuffer, void * inputBuffer);

    uint32_t outputChannelCount() const { return _numOutputChannels; }

private:
    void configure();

    AudioIOCallback & _callback;
    AudioBus * _renderBus = nullptr;
    AudioBus * _inputBus = nullptr;
    uint32_t _numOutputChannels = 0;
    uint32_t _numInputChannels = 0;
    float _sampleRate = 44100.f;
    RtAudio _dac;
};

int outputCallback(void * outputBuffer, void * inputBuffer, unsigned int nBufferFrames, double streamTime, 
                   RtAudioStreamStatus status, void * userData);

}  // namespace lab

#endif  // AudioDestinationRtAudio_h
