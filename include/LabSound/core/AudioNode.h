// License: BSD 2 Clause
// Copyright (C) 2010, Google Inc. All rights reserved.
// Copyright (C) 2015+, The LabSound Authors. All rights reserved.

// @tofix - webkit change 1f083e8 and 2bd2dc2 adds support for different behaviors on mixing such as
// clamping the max number of channels, and mixing 5.1 down to mono

#ifndef AudioNode_h
#define AudioNode_h

#include "LabSound/core/Mixing.h"

#include <algorithm>
#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

namespace lab
{

// clang-format off
enum PanningMode
{
    PANNING_NONE = 0,
    EQUALPOWER   = 1,
    HRTF         = 2,
    _PanningModeCount
};

enum FilterType
{
    FILTER_NONE = 0,
    LOWPASS     = 1,
    HIGHPASS    = 2,
    BANDPASS    = 3,
    LOWSHELF    = 4,
    HIGHSHELF   = 5,
    PEAKING     = 6,
    NOTCH       = 7,
    ALLPASS     = 8,
    _FilterTypeCount
};

enum OscillatorType
{
    OSCILLATOR_NONE = 0,
    SINE            = 1,
    FAST_SINE       = 2,
    SQUARE          = 3,
    SAWTOOTH        = 4,
    TRIANGLE        = 5,
    CUSTOM          = 6,
    _OscillatorTypeCount
};
// clang-format on

class AudioContext;
class AudioNodeInput;
class AudioNodeOutput;
class AudioParam;
class AudioSetting;
class ContextGraphLock;
class ContextRenderLock;

enum class SchedulingState : int
{
    UNSCHEDULED = 0, // Initial playback state. Created, but not yet scheduled
    SCHEDULED,       // Scheduled to play (via noteOn() or noteGrainOn()), but not yet playing
    FADE_IN,         // First epoch, fade in, then play
    PLAYING,         // Generating sound
    STOPPING,        // Transitioning to finished
    RESETTING,       // Node is resetting to initial, unscheduled state
    FINISHING,       // Playing has finished
    FINISHED         // Node has finished
};


class AudioNodeScheduler
{
public:
    AudioNodeScheduler(float sampleRate);
    ~AudioNodeScheduler() = default;

    // Scheduling.
    void start(double when);
    void stop(double when);
    void finish(ContextRenderLock&);  // Called when there is no more sound to play or the noteOff/stop() time has been reached.
    void reset();

    SchedulingState playbackState() const { return _playbackState; }
    bool hasFinished() const { return _playbackState == SchedulingState::FINISHED; }

    bool update(ContextRenderLock&, int epoch_length);

    SchedulingState _playbackState = SchedulingState::UNSCHEDULED;

    // epoch is a long count at sample rate; 136 years at 48kHz
    // For use in an interstellar sound installation or radio frequency signal processing, 
    // please consider upgrading these to uint64_t or write some rollover logic.

    uint64_t _epoch = 0;        // the epoch rendered currently in the busses
    uint64_t _epochLength = 0;  // number of frames in current epoch

    uint64_t _startWhen = 0;    // requested start in epochal coordinate system
    uint64_t _stopWhen = 0;     // requested end in epochal coordinate system

    int _renderOffset = 0; // where rendering starts in the current frame
    int _renderLength = 0; // number of rendered frames in the current frame 

    float _sampleRate = 1;

    std::function<void()> _onEnded;
};


struct BangInterface
{
    virtual void bang(const double length = 0.0) = 0;
    std::function<void()> _bang;
};

// An AudioNode is the basic building block for handling audio within an AudioContext.
// It may be an audio source, an intermediate processing module, or an audio destination.
// Each AudioNode can have inputs and/or outputs.
// An AudioHardwareDeviceNode has one input and no outputs and represents the final destination to the audio hardware.
// Most processing nodes such as filters will have one input and one output, although multiple inputs and outputs are possible.
class AudioNode
{
public:
    enum : int
    {
        OSCILLATOR_NONE  = 0,
        SINE             = 1,
        SQUARE           = 2,
        SAWTOOTH         = 3,
        TRIANGLE         = 4,
        CUSTOM           = 5,
        _OscillatorCount = 6
    };

class AudioContext;
class AudioNodeInput;
class AudioNodeOutput;
class AudioParam;
class AudioSetting;
class ContextGraphLock;
class ContextRenderLock;

// An AudioNode is the basic building block for handling audio within an AudioContext.
// It may be an audio source, an intermediate processing module, or an audio destination.
// Each AudioNode can have inputs and/or outputs.
// An AudioHardwareDeviceNode has one input and no outputs and represents the final destination to the audio hardware.
// Most processing nodes such as filters will have one input and one output, although multiple inputs and outputs are possible.
class AudioNode
{
public:
    enum
    {
        ProcessingSizeInFrames = 128
    };

    AudioNode() = delete;
    virtual ~AudioNode();

    explicit AudioNode(AudioContext &);

    // LabSound: If the node included ScheduledNode in its hierarchy, this will return true.
    // This is to save the cost of a dynamic_cast when scheduling nodes.
    virtual bool isScheduledNode() const { return false; }

    virtual bool hasBang() const { return false; }

    // The AudioNodeInput(s) (if any) will already have their input data available when process() is called.
    // Subclasses will take this input data and put the results in the AudioBus(s) of its AudioNodeOutput(s) (if any).
    // Called from context's audio thread.
    virtual void process(ContextRenderLock &, int bufferSize) = 0;

    // Resets DSP processing state (clears delay lines, filter memory, etc.)
    // Called from context's audio thread.
    virtual void reset(ContextRenderLock &) = 0;

    // No significant resources should be allocated until initialize() is called.
    // Processing may not occur until a node is initialized.
    virtual void initialize();
    virtual void uninitialize();
    bool isInitialized() const { return m_isInitialized; }

    int numberOfInputs() const { return static_cast<int>(m_inputs.size()); }
    int numberOfOutputs() const { return static_cast<int>(m_outputs.size()); }

    // Called when a new connection has been made to one of our inputs or the connection number of channels has changed.
    // This potentially gives us enough information to perform a lazy initialization or, if necessary, a re-initialization.
    // Called from main thread.
    virtual void checkNumberOfChannelsForInput(ContextRenderLock &, AudioNodeInput *);

    // processIfNecessary() is called by our output(s) when the rendering graph needs this AudioNode to process.
    // This method ensures that the AudioNode will only process once per rendering time quantum even if it's called repeatedly.
    // This handles the case of "fanout" where an output is connected to multiple AudioNode inputs.
    // Called from context's audio thread.
    void processIfNecessary(ContextRenderLock & r, int bufferSize);

    // Called when a new connection has been made to one of our inputs or the connection number of channels has changed.
    // This potentially gives us enough information to perform a lazy initialization or, if necessary, a re-initialization.
    // Called from main thread.
    virtual void checkNumberOfChannelsForInput(ContextRenderLock &, AudioNodeInput *);

    // tailTime() is the length of time (not counting latency time) where non-zero output may occur after continuous silent input.
    virtual double tailTime(ContextRenderLock & r) const = 0;

    // latencyTime() is the length of time it takes for non-zero output to appear after non-zero input is provided. This only applies to
    // processing delay which is an artifact of the processing algorithm chosen and is *not* part of the intrinsic desired effect. For
    // example, a "delay" effect is expected to delay the signal, and thus would not be considered latency.
    virtual double latencyTime(ContextRenderLock & r) const = 0;

    // propagatesSilence() should return true if the node will generate silent output when given silent input. By default, AudioNode
    // will take tailTime() and latencyTime() into account when determining whether the node will propagate silence.
    virtual bool propagatesSilence(ContextRenderLock & r) const;

    void setChannelCount(ContextGraphLock & g, size_t channelCount);

    ChannelCountMode channelCountMode() const { return m_channelCountMode; }
    void setChannelCountMode(ContextGraphLock & g, ChannelCountMode mode);

        size_t channelCount();
        void setChannelCount(ContextGraphLock & g, size_t channelCount);

    ChannelCountMode channelCountMode() const { return m_channelCountMode; }
    void setChannelCountMode(ContextGraphLock & g, ChannelCountMode mode);

    ChannelInterpretation channelInterpretation() const { return m_channelInterpretation; }
    void setChannelInterpretation(ChannelInterpretation interpretation) { m_channelInterpretation = interpretation; }

    // returns a vector of parameter names
    std::vector<std::string> paramNames() const;
    std::vector<std::string> paramShortNames() const;

    // returns a vector of setting names
    std::vector<std::string> settingNames() const;
    std::vector<std::string> settingShortNames() const;

    std::shared_ptr<AudioParam> param(char const * const str);
    std::shared_ptr<AudioSetting> setting(char const * const str);

    std::vector<std::shared_ptr<AudioParam>> params() const { return m_params; }
    std::vector<std::shared_ptr<AudioSetting>> settings() const { return m_settings; }

    // USER FACING FUNCTIONS <

    // Called by processIfNecessary() to cause all parts of the rendering graph connected to us to process.
    // Each rendering quantum, the audio data for each of the AudioNode's inputs will be available after this method is called.
    // Called from context's audio thread.
    virtual void pullInputs(ContextRenderLock &, int bufferSize);

    // Force all inputs to take any channel interpretation changes into account.
    void updateChannelsForInputs(ContextGraphLock &);

    friend class AudioContext;

    AudioNodeScheduler _scheduler;

        volatile bool m_isInitialized {false};

    std::vector<std::shared_ptr<AudioParam>> m_params;
    std::vector<std::shared_ptr<AudioSetting>> m_settings;

    double m_lastProcessingTime{-1.0};
    double m_lastNonSilentTime{-1.0};

    float audibleThreshold() const { return 0.05f; }

    ChannelCountMode m_channelCountMode{ ChannelCountMode::Max };
    ChannelInterpretation m_channelInterpretation{ ChannelInterpretation::Speakers };

    // starts an immediate ramp to zero in preparation for disconnection
    void scheduleDisconnect() { _scheduler.stop(0); }

    // returns true if the disconnection ramp has reached zero.
    // This is intended to allow the AudioContext to manage popping artifacts
    bool disconnectionReady() const { return m_disconnectSchedule >= 0.f && m_disconnectSchedule <= audibleThreshold(); }

    // starts an immediate ramp to unity due to being newly connected to a graph
    void scheduleConnect()
    {
        m_disconnectSchedule = -1.f;
        m_connectSchedule = 0.f;
    }

    std::atomic<float> m_disconnectSchedule{-1.f};
    std::atomic<float> m_connectSchedule{0.f};

    std::atomic<float> m_disconnectSchedule{-1.f};
    std::atomic<float> m_connectSchedule{0.f};

    size_t m_channelCount{0};

    ChannelCountMode m_channelCountMode{ChannelCountMode::Max};
    ChannelInterpretation m_channelInterpretation{ChannelInterpretation::Speakers};
};

}  // namespace lab

#endif  // AudioNode_h

