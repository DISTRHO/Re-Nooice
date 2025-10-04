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
    static constexpr const uint32_t kDenoiseScaling = std::numeric_limits<short>::max();

    const uint32_t denoiseFrameSize = static_cast<uint32_t>(rnnoise_get_frame_size());

    DenoiseState* const denoise = rnnoise_create(nullptr);

    float* const bufferIn = new float[denoiseFrameSize];
    float* const bufferOut = new float[denoiseFrameSize];

    HeapRingBuffer ringBufferOut;

    uint32_t bufferInPos = 0;
    bool processing = false;

    enum Parameters {
        kParamCurVAD,
        kParamMaxVAD,
        kParamCount,
    };
    float parameters[kParamCount] = {};

public:
   /**
      Plugin class constructor.
      You must set all parameter values to their defaults, matching ParameterRanges::def.
    */
    ReNooicePlugin()
        : Plugin(kParamCount, 0, 0) // parameters, programs, states
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

    void initAudioPort(bool input, uint32_t index, AudioPort& port)
    {
        port.groupId = kPortGroupMono;

        Plugin::initAudioPort(input, index, port);
    }

    void initParameter(uint32_t index, Parameter& parameter)
    {
        parameter.hints      = kParameterIsAutomatable;
        parameter.ranges.min = 0.0f;
        parameter.ranges.max = 1.0f;

        switch (index)
        {
        case kParamCurVAD:
            parameter.hints     |= kParameterIsOutput;
            parameter.name       = "Current VAD";
            parameter.symbol     = "cur_vad";
            parameter.ranges.def = 0.f;
            break;
        case kParamMaxVAD:
            parameter.hints     |= kParameterIsOutput;
            parameter.name       = "Maximum VAD";
            parameter.symbol     = "max_vad";
            parameter.ranges.def = 0.f;
            break;
        }
    }

    float getParameterValue(uint32_t index) const
    {
        return parameters[index];
    }

    void setParameterValue(uint32_t index, float value)
    {
        parameters[index] = value;
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Audio/MIDI Processing

    void activate() override
    {
        const uint32_t ringBufferSize = denoiseFrameSize * 2 * sizeof(float);
        ringBufferOut.createBuffer(ringBufferSize);
        parameters[kParamCurVAD] = parameters[kParamMaxVAD] = 0.f;
        processing = false;
        bufferInPos = 0;
    }

    void deactivate() override
    {
        ringBufferOut.deleteBuffer();
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
            const uint32_t framesCycle = std::min(denoiseFrameSize - bufferInPos, frames - offset);

            // copy input data into buffer
            std::memcpy(bufferIn + bufferInPos, input + offset, framesCycle * sizeof(float));

            // run denoise once input buffer is full
            if ((bufferInPos += framesCycle) == denoiseFrameSize)
            {
                bufferInPos = 0;

                for (uint32_t i = 0; i < denoiseFrameSize; ++i)
                    bufferIn[i] *= kDenoiseScaling;

                parameters[kParamCurVAD] = rnnoise_process_frame(denoise, bufferOut, bufferIn);

                if (parameters[kParamCurVAD] > parameters[kParamMaxVAD])
                    parameters[kParamMaxVAD] = parameters[kParamCurVAD];

                // write denoise output into ringbuffer
                ringBufferOut.writeCustomData(bufferOut, denoiseFrameSize * sizeof(float));
                ringBufferOut.commitWrite();
            }

            if (processing)
            {
                ringBufferOut.readCustomData(output + offset, framesCycle * sizeof(float));

                for (uint32_t i = 0; i < framesCycle; ++i)
                    output[i + offset] /= kDenoiseScaling;
            }
            else
            {
                std::memset(output + offset, 0, framesCycle * sizeof(float));

                if (ringBufferOut.getReadableDataSize() / sizeof(float) >= denoiseFrameSize)
                    processing = true;
            }

            offset += framesCycle;
        }
    }

    void sampleRateChanged(double sampleRate) override
    {
        setLatency(d_roundToUnsignedInt(sampleRate / 48000.0 * denoiseFrameSize));
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
