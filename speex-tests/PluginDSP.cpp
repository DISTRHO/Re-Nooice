/*
 * Re:Speex
 * Copyright (C) 2025 Filipe Coelho <falktx@falktx.com>
 * SPDX-License-Identifier: ISC
 */

#include "DistrhoPlugin.hpp"
#include "extra/RingBuffer.hpp"
#include "extra/ValueSmoother.hpp"

#include "speex/speex_echo.h"
#include "speex/speex_preprocess.h"

START_NAMESPACE_DISTRHO

// --------------------------------------------------------------------------------------------------------------------

static constexpr inline
int16_t float16(const float s)
{
    return s <= -1.f ? -32767 :
           s >= 1.f ? 32767 :
           std::lrintf(s * 32767.f);
}

// FIXME
static constexpr inline
uint32_t rnnoise_get_frame_size()
{
    return 480;
}

// --------------------------------------------------------------------------------------------------------------------

class ReSpeexPlugin : public Plugin
{
    // scaling used for denoise processing
    static constexpr const uint32_t kDenoiseScaling = std::numeric_limits<short>::max();
    static constexpr const float kDenoiseScalingInv = 1.f / kDenoiseScaling;

    // denoise block size
    const uint32_t denoiseFrameSize = static_cast<uint32_t>(rnnoise_get_frame_size());

    // echo canceller block size
    static constexpr const uint32_t echoFrameSize = 32;
    const uint32_t echoFilterLength = denoiseFrameSize * 10;

    // echo canceller handle, keep it const so we never modify it
    SpeexEchoState* const echo = speex_echo_state_init(echoFrameSize, echoFilterLength);
    SpeexPreprocessState* const preproc = speex_preprocess_state_init(echoFrameSize, 48000);

    // buffers for latent processing
    spx_int16_t* bufferInDry;
    spx_int16_t* bufferInWet;
    spx_int16_t* bufferOut;
    float* bufferOutFloat;
    // HeapRingBuffer ringBufferDry;
    HeapRingBuffer ringBufferOut;
    uint32_t bufferInPos;

    // whether we received enough latent audio frames
    bool latent;
    bool processing;

public:
   /**
      Plugin class constructor.
      You must set all parameter values to their defaults, matching ParameterRanges::def.
    */
    ReSpeexPlugin()
        : Plugin(kParamCount, 0, 0) // parameters, programs, states
    {
        int sampleRate = 48000;
        speex_echo_ctl(echo, SPEEX_ECHO_SET_SAMPLING_RATE, &sampleRate);
        speex_preprocess_ctl(preproc, SPEEX_PREPROCESS_SET_ECHO_STATE, echo);

        spx_int32_t off = 0;
        speex_preprocess_ctl(preproc, SPEEX_PREPROCESS_SET_DENOISE, &off);

        // initial sample rate setup
        sampleRateChanged(getSampleRate());
    }

   /**
      Destructor.
    */
    ~ReSpeexPlugin()
    {
        speex_echo_state_destroy(echo);
        speex_preprocess_state_destroy(preproc);
    }

protected:
    // ----------------------------------------------------------------------------------------------------------------
    // Information

   /**
      Get the plugin label.
      This label is a short restricted name consisting of only _, a-z, A-Z and 0-9 characters.
    */
    const char* getLabel() const noexcept override
    {
        return "ReSpeex";
    }

   /**
      Get the plugin author/maker.
    */
    const char* getMaker() const noexcept override
    {
        return "DISTRHO";
    }

   /**
      Get the plugin license (a single line of text or a URL).
      For commercial plugins this should return some short copyright information.
    */
    const char* getLicense() const noexcept override
    {
        return "ISC";
    }

   /**
      Get the plugin version, in hexadecimal.
    */
    uint32_t getVersion() const noexcept override
    {
        return d_version(1, 0, 0);
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Audio/MIDI Processing

   /**
      Activate this plugin.
    */
    void activate() override
    {
        // ringBufferDry.createBuffer((denoiseFrameSize + echoFrameSize) * sizeof(int16_t) * 2);
        ringBufferOut.createBuffer(denoiseFrameSize * sizeof(float) * 2);

        bufferInDry = new spx_int16_t[echoFrameSize];
        bufferInWet = new spx_int16_t[echoFrameSize];
        bufferOut = new spx_int16_t[echoFrameSize];
        bufferOutFloat = new float[echoFrameSize];
        bufferInPos = 0;
        latent = false;
        processing = false;
    }

   /**
      Deactivate this plugin.
    */
    void deactivate() override
    {
        delete[] bufferInDry;
        delete[] bufferInWet;
        delete[] bufferOut;
        delete[] bufferOutFloat;

        // ringBufferDry.deleteBuffer();
        ringBufferOut.deleteBuffer();
    }

   /**
      Run/process function for plugins without MIDI input.
      @note Some parameters might be null if there are no audio inputs or outputs.
    */
    void run(const float** const inputs, float** const outputs, const uint32_t frames) override
    {
        const float* inDry = inputs[0];
        const float* inWet = inputs[1];
        /* */ float* output = outputs[0];

        uint32_t offset = 0;

#if 0
        // capture enough frames in dry buffer (compensating Re:Nooice latency)
        if (! latent)
        {
            const uint32_t framesCycle = std::min<uint32_t>(denoiseFrameSize - ringBufferDry.getReadableDataSize() / sizeof(int16_t), frames);

            for (uint32_t i = 0; i < framesCycle; ++i)
                ringBufferDry.writeShort(float16(inDry[offset]));

            ringBufferDry.commitWrite();

            if (ringBufferDry.getReadableDataSize() / sizeof(int16_t) == denoiseFrameSize)
                latent = true;

            offset += framesCycle;
            inDry += framesCycle;
        }
#endif

        // process audio a few frames at a time, so it always fits nicely into speex blocks
        for (; offset != frames;)
        {
            const uint32_t framesCycle = std::min(echoFrameSize - bufferInPos, frames - offset);

            // copy input data into buffers
            for (uint32_t i = 0; i < framesCycle; ++i)
            {
                // ringBufferDry.writeShort(float16(inDry[i]));
                bufferInDry[bufferInPos + i] = float16(inDry[i]);
                bufferInWet[bufferInPos + i] = float16(inWet[i]);
            }

            // ringBufferDry.commitWrite();

            // run denoise once input buffer is full
            if ((bufferInPos += framesCycle) == echoFrameSize)
            {
                bufferInPos = 0;

                // ringBufferDry.readCustomData(bufferInDry, framesCycle * sizeof(int16_t));

                // run denoise
                speex_echo_cancellation(echo, bufferInDry, bufferInWet, bufferOut);
                speex_preprocess_run(preproc, bufferOut);

                // scale back down to regular audio level
                for (uint32_t i = 0; i < echoFrameSize; ++i)
                    bufferOutFloat[i] = static_cast<float>(bufferOut[i]) * (1.f / 32767.f);

                // write denoise output into ringbuffer
                ringBufferOut.writeCustomData(bufferOutFloat, echoFrameSize * sizeof(float));
                ringBufferOut.commitWrite();
            }

            // we have enough audio frames in the ring buffer, can give back audio to host
            if (processing)
            {
                // copy processed buffer directly into output
                ringBufferOut.readCustomData(output, framesCycle * sizeof(float));
            }
            // capture more audio frames until it fits 1 denoise block
            else
            {
                // mute output while still capturing audio frames
                std::memset(output, 0, framesCycle * sizeof(float));

                if (ringBufferOut.getReadableDataSize() >= echoFrameSize * sizeof(float))
                    processing = true;
            }

            offset += framesCycle;
            inDry += framesCycle;
            inWet += framesCycle;
            output += framesCycle;
        }
    }

   /**
      Optional callback to inform the plugin about a sample rate change.
      This function will only be called when the plugin is deactivated.
    */
    void sampleRateChanged(const double sampleRate) override
    {
        setLatency(d_roundToUnsignedInt(sampleRate / 48000.0 * (denoiseFrameSize + echoFrameSize)));
    }

    // ----------------------------------------------------------------------------------------------------------------

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReSpeexPlugin)
};

// --------------------------------------------------------------------------------------------------------------------

/**
   Create an instance of the Plugin class.
   This is the entry point for DPF plugins.
   DPF will call this to either create an instance of your plugin for the host
   or to fetch some initial information for internal caching.
 */
Plugin* createPlugin()
{
    return new ReSpeexPlugin();
}

// --------------------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
