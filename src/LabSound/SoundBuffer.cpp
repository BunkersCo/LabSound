// Copyright (c) 2003-2013 Nick Porcino, All rights reserved.
// License is MIT: http://opensource.org/licenses/MIT

#include "SoundBuffer.h"

#include <stdio.h>
#include <iostream>

namespace LabSound {
    
    using namespace WebCore;
    
    SoundBuffer::SoundBuffer(std::shared_ptr<AudioContext> context, const char* path)
    : context(context)
    {
        FILE* f = fopen(path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            int l = ftell(f);
            fseek(f, 0, SEEK_SET);
            uint8_t* data = new uint8_t[l];
            fread(data, 1, l, f);
            fclose(f);
            
            bool mixToMono = true;
            
            // create an audio buffer from the file data. The file data will be
            // parsed, and does not need to be retained.
            audioBuffer = AudioBuffer::createFromAudioFileData(data, l, mixToMono, context->sampleRate());
            delete [] data;
        }
        else
            std::cerr << "File not found " << path << std::endl;
    }
    
    SoundBuffer::~SoundBuffer()
    {
    }
  
    std::shared_ptr<AudioBufferSourceNode> SoundBuffer::create()
    {
        if (audioBuffer) {
            ASSERT(!context.expired());
            std::shared_ptr<AudioContext> ac = context.lock();
            std::shared_ptr<AudioBufferSourceNode> sourceBuffer(new AudioBufferSourceNode(ac, ac->destination()->sampleRate()));
            
            // Connect the source node to the parsed audio data for playback
            sourceBuffer->setBuffer(audioBuffer);
            return sourceBuffer;
        }
        return 0;
    }
    
	// Output to the default context output 
	std::shared_ptr<AudioBufferSourceNode> SoundBuffer::play(float when)
	{
		if (audioBuffer) {
            ASSERT(!context.expired());
            std::shared_ptr<AudioContext> ac = context.lock();
			return play(ac->destination(), when);
		}
		return 0;
	}

	// Output to a specific note 
    std::shared_ptr<AudioBufferSourceNode> SoundBuffer::play(std::shared_ptr<AudioNode> outputNode, float when)
    {
        if (audioBuffer) {
            ASSERT(!context.expired());
            std::shared_ptr<AudioContext> ac = context.lock();
            std::shared_ptr<AudioBufferSourceNode> sourceBufferNode(new AudioBufferSourceNode(ac, ac->destination()->sampleRate()));
            sourceBufferNode->setBuffer(audioBuffer);
            
            // bus the sound to the output node
            WebCore::ExceptionCode ec;
            sourceBufferNode->connect(outputNode.get(), 0, 0, ec);
            if (ec != NO_ERR) {
                // @dp add this - audio context should be responsible for clean up, not the sound buffer itself ac->manageLifespan(sourceBufferNode);
                sourceBufferNode->start(when);
                return sourceBufferNode;
            }
        }
        return 0;
    }
    
    // This variant starts a sound at a given offset relative to the beginning of the
    // sample, ends it an offfset (relative to the beginning), and optional delays
    // the start. If 0 is passed as end, then the sound will play to the end.
    std::shared_ptr<AudioBufferSourceNode> SoundBuffer::play(float start, float end, float when)
    {
        if (audioBuffer) {

            if (end == 0)
                end = audioBuffer->duration();
            
            ASSERT(!context.expired());
            std::shared_ptr<AudioContext> ac = context.lock();
            std::shared_ptr<AudioBufferSourceNode> sourceBuffer(new AudioBufferSourceNode(ac, ac->destination()->sampleRate()));
            
            // Connect the source node to the parsed audio data for playback
            sourceBuffer->setBuffer(audioBuffer);
            
            // bus the sound to the mixer.
            WebCore::ExceptionCode ec;

            sourceBuffer->connect(ac->destination().get(), 0, 0, ec);
            sourceBuffer->startGrain(when, start, end - start, ec);

            return sourceBuffer;
        }

        return 0;

    }

} // LabSound
