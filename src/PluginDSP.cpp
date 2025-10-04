/*
 * Re:Nooice
 * Copyright (C) 2025 Filipe Coelho <falktx@falktx.com>
 * SPDX-License-Identifier: ISC
 */

#include "DistrhoPlugin.hpp"
#include "extra/RingBuffer.hpp"

#include "rnnoise.h"

START_NAMESPACE_DISTRHO

// --------------------------------------------------------------------------------------------------------------------

class ReNooicePlugin : public Plugin
{
    const uint32_t denoiseFrameSize = static_cast<uint32_t>(rnnoise_get_frame_size());

    DenoiseState* const denoise = rnnoise_create(nullptr);

    float* const bufferIn = new float[denoiseFrameSize];
    float* const bufferOut = new float[denoiseFrameSize];

    HeapRingBuffer ringBufferIn;
    HeapRingBuffer ringBufferOut;

    bool processing = false;

public:
   /**
      Plugin class constructor.
      You must set all parameter values to their defaults, matching ParameterRanges::def.
    */
    ReNooicePlugin()
        : Plugin(0, 0, 0) // parameters, programs, states
    {
        // initial sample rate setup
        sampleRateChanged(getSampleRate());
    }

    ~ReNooicePlugin()
    {
        rnnoise_destroy(denoise);

        delete[] bufferIn;
        delete[] bufferOut;
    }

protected:
    // ----------------------------------------------------------------------------------------------------------------
    // Information

   /**
      Get the plugin label.@n
      This label is a short restricted name consisting of only _, a-z, A-Z and 0-9 characters.
    */
    const char* getLabel() const noexcept override
    {
        return "ReNooice";
    }

   /**
      Get the plugin author/maker.
    */
    const char* getMaker() const noexcept override
    {
        return "DISTRHO";
    }

   /**
      Get the plugin license (a single line of text or a URL).@n
      For commercial plugins this should return some short copyright information.
    */
    const char* getLicense() const noexcept override
    {
        return "ISC";
    }

   /**
      Get the plugin version, in hexadecimal.
      @see d_version()
    */
    uint32_t getVersion() const noexcept override
    {
        return d_version(1, 0, 0);
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Init

    // ----------------------------------------------------------------------------------------------------------------
    // Audio/MIDI Processing

    void activate() override
    {
        const uint32_t ringBufferSize = denoiseFrameSize * 3 * sizeof(float);
        ringBufferIn.createBuffer(ringBufferSize);
        ringBufferOut.createBuffer(ringBufferSize);
        processing = false;
    }

    void deactivate() override
    {
        ringBufferIn.deleteBuffer();
        ringBufferOut.deleteBuffer();
    }

    void runDenoise()
    {
        // extract input data from ringbuffer
        ringBufferIn.readCustomData(bufferIn, denoiseFrameSize * sizeof(float));

        // run denoise
        rnnoise_process_frame(denoise, bufferOut, bufferIn);

        // write denoise output into ringbuffer
        ringBufferOut.writeCustomData(bufferOut, denoiseFrameSize * sizeof(float));
        ringBufferOut.commitWrite();
    }

   /**
      Run/process function for plugins without MIDI input.
      @note Some parameters might be null if there are no audio inputs or outputs.
    */
    void run(const float** const inputs, float** const outputs, const uint32_t frames) override
    {
        const float* input = inputs[0];
        /* */ float* output = outputs[0];

        for (uint32_t offset = 0; offset != frames;)
        {
            const uint32_t framesCycle = std::min(denoiseFrameSize, frames);

            // write input data into ringbuffer
            ringBufferIn.writeCustomData(input, sizeof(float) * framesCycle);
            ringBufferIn.commitWrite();

            if (ringBufferIn.getReadableDataSize() / sizeof(float) >= denoiseFrameSize)
                runDenoise();

            if (processing)
            {
                ringBufferOut.readCustomData(output + offset, framesCycle * sizeof(float));
            }
            else
            {
                std::memset(output + offset, 0, sizeof(float) * framesCycle);

                if (ringBufferOut.getReadableDataSize() / sizeof(float) >= denoiseFrameSize * 2)
                    processing = true;
            }

            offset += framesCycle;
        }
    }

    void sampleRateChanged(double sampleRate) override
    {
        setLatency(d_roundToUnsignedInt(sampleRate / 48000.0 * denoiseFrameSize * 2));
    }

    // ----------------------------------------------------------------------------------------------------------------

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReNooicePlugin)
};

// --------------------------------------------------------------------------------------------------------------------

Plugin* createPlugin()
{
    return new ReNooicePlugin();
}

// --------------------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
