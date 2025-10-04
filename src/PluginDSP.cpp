/*
 * Re:Nooice
 * Copyright (C) 2025 Filipe Coelho <falktx@falktx.com>
 * SPDX-License-Identifier: ISC
 */

#include "DistrhoPlugin.hpp"
#include "extra/RingBuffer.hpp"
#include "extra/ValueSmoother.hpp"

#include "rnnoise.h"

START_NAMESPACE_DISTRHO

// --------------------------------------------------------------------------------------------------------------------

class ReNooicePlugin : public Plugin
{
    static constexpr const uint32_t kDenoiseScaling = std::numeric_limits<short>::max();
    static constexpr const float kDenoiseScalingInv = 1.f / kDenoiseScaling;

    const uint32_t denoiseFrameSize = static_cast<uint32_t>(rnnoise_get_frame_size());
    const uint32_t denoiseFrameSizeF = denoiseFrameSize * sizeof(float);

    DenoiseState* const denoise = rnnoise_create(nullptr);

    float* bufferIn;
    float* bufferOut;

    HeapRingBuffer ringBufferDry;
    HeapRingBuffer ringBufferOut;

    uint32_t gracePeriodInFrames = 0;
    uint32_t numFramesUntilGracePeriodOver = 0;

    uint32_t bufferInPos;
    bool processing;

    // smooth bypass
    LinearValueSmoother dryValue;

    // smooth mute/unmute
    LinearValueSmoother muteValue;

    enum Parameters {
        kParamBypass,
        kParamThreshold,
        kParamGracePeriod,
        kParamEnableStats,
        kParamCurrentVAD,
        kParamAverageVAD,
        kParamMinimumVAD,
        kParamMaximumVAD,
        kParamCount,
    };
    float parameters[kParamCount] = {};

    struct {
        float vads[128];
        float avg, min, max;
        int pos;
        bool enabled = false;
        bool running;

        void store(const float vad)
        {
            vads[pos++] = vad;

            if (pos == ARRAY_SIZE(vads))
            {
                pos = 0;
                running = true;
            }

            if (running)
            {
                avg = 0.f;
                min = 1.f;
                max = 0.f;

                for (uint32_t i = 0; i < ARRAY_SIZE(vads); ++i)
                {
                    if (vads[i] < min)
                        min = vads[i];
                    if (vads[i] > max)
                        max = vads[i];

                    avg += vads[i];
                }

                avg /= ARRAY_SIZE(vads);
            }
        }

        void reset()
        {
            avg = 0.f;
            min = 1.f;
            max = 0.f;
            pos = 0;
            running = false;
        }
    } stats;

public:
   /**
      Plugin class constructor.
      You must set all parameter values to their defaults, matching ParameterRanges::def.
    */
    ReNooicePlugin()
        : Plugin(kParamCount, 0, 0) // parameters, programs, states
    {
        dryValue.setTimeConstant(0.02f);
        dryValue.setTargetValue(0.f);

        muteValue.setTimeConstant(0.02f);
        muteValue.setTargetValue(0.f);

        parameters[kParamThreshold] = 60.f;
        parameters[kParamMinimumVAD] = 100.f;

        // initial sample rate setup
        sampleRateChanged(getSampleRate());
    }

    ~ReNooicePlugin()
    {
        rnnoise_destroy(denoise);
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

    void initAudioPort(bool input, uint32_t index, AudioPort& port) override
    {
        port.groupId = kPortGroupMono;

        Plugin::initAudioPort(input, index, port);
    }

    void initParameter(uint32_t index, Parameter& parameter) override
    {
        parameter.hints = kParameterIsAutomatable;

        switch (index)
        {
        case kParamBypass:
            parameter.initDesignation(kParameterDesignationBypass);
            break;
        case kParamThreshold:
            parameter.hints |= kParameterIsInteger;
            parameter.name   = "Threshold";
            parameter.symbol = "threshold";
            parameter.unit   = "%";
            parameter.ranges.def = 60.f;
            parameter.ranges.min = 0.f;
            parameter.ranges.max = 100.f;
            break;
        case kParamGracePeriod:
            parameter.hints |= kParameterIsInteger;
            parameter.name   = "Grace Period";
            parameter.symbol = "grace_period";
            parameter.unit   = "ms";
            parameter.ranges.def = 0.f;
            parameter.ranges.min = 0.f;
            parameter.ranges.max = 1000.f;
            break;
        case kParamEnableStats:
            parameter.hints |= kParameterIsBoolean | kParameterIsInteger;
            parameter.name   = "Enable Stats";
            parameter.symbol = "stats";
            parameter.ranges.def = 0.f;
            parameter.ranges.min = 0.f;
            parameter.ranges.max = 1.f;
            break;
        case kParamCurrentVAD:
            parameter.hints |= kParameterIsOutput;
            parameter.name   = "Current VAD";
            parameter.symbol = "cur_vad";
            parameter.unit   = "%";
            parameter.ranges.def = 0.f;
            parameter.ranges.min = 0.f;
            parameter.ranges.max = 100.f;
            break;
        case kParamAverageVAD:
            parameter.hints |= kParameterIsOutput;
            parameter.name   = "Average VAD";
            parameter.symbol = "avg_vad";
            parameter.unit   = "%";
            parameter.ranges.def = 0.f;
            parameter.ranges.min = 0.f;
            parameter.ranges.max = 100.f;
            break;
        case kParamMinimumVAD:
            parameter.hints |= kParameterIsOutput;
            parameter.name   = "Minimum VAD";
            parameter.symbol = "min_vad";
            parameter.unit   = "%";
            parameter.ranges.def = 100.f;
            parameter.ranges.min = 0.f;
            parameter.ranges.max = 100.f;
            break;
        case kParamMaximumVAD:
            parameter.hints |= kParameterIsOutput;
            parameter.name   = "Maximum VAD";
            parameter.symbol = "max_vad";
            parameter.unit   = "%";
            parameter.ranges.def = 0.f;
            parameter.ranges.min = 0.f;
            parameter.ranges.max = 100.f;
            break;
        }
    }

    float getParameterValue(uint32_t index) const override
    {
        return parameters[index];
    }

    void setParameterValue(uint32_t index, float value) override
    {
        parameters[index] = value;

        switch (index)
        {
        case kParamBypass:
            dryValue.setTargetValue(value);
            break;
        case kParamGracePeriod:
            // 48 is 1ms (48000 kHz [1s] / 1000)
            gracePeriodInFrames = d_roundToUnsignedInt(value * 48.f);
            break;
        }
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Audio/MIDI Processing

    void activate() override
    {
        const uint32_t ringBufferSize = denoiseFrameSizeF * 2;
        ringBufferDry.createBuffer(ringBufferSize);
        ringBufferOut.createBuffer(ringBufferSize);

        bufferIn = new float[denoiseFrameSize];
        bufferOut = new float[denoiseFrameSize];

        parameters[kParamCurrentVAD] = 0.f;
        parameters[kParamAverageVAD] = 0.f;
        parameters[kParamMinimumVAD] = 100.f;
        parameters[kParamMaximumVAD] = 0.f;

        dryValue.clearToTargetValue();

        muteValue.setTargetValue(0.f);
        muteValue.clearToTargetValue();

        processing = false;
        bufferInPos = 0;
        stats.reset();
    }

    void deactivate() override
    {
        delete[] bufferIn;
        delete[] bufferOut;

        ringBufferDry.deleteBuffer();
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

        // reset stats if enabled status changed
        const bool statsEnabled = parameters[kParamEnableStats] > 0.5f;
        if (stats.enabled != statsEnabled)
        {
            stats.reset();
            stats.enabled = statsEnabled;
        }

        // pass this threshold to unmute
        const float threshold = parameters[kParamThreshold] * 0.01f;

        // process audio a few frames at a time, so it always fits nicely into denoise blocks
        for (uint32_t offset = 0; offset != frames;)
        {
            const uint32_t framesCycle = std::min(denoiseFrameSize - bufferInPos, frames - offset);
            const uint32_t framesCycleF = framesCycle * sizeof(float);

            // copy input data into buffer
            std::memcpy(bufferIn + bufferInPos, input, framesCycleF);

            // run denoise once input buffer is full
            if ((bufferInPos += framesCycle) == denoiseFrameSize)
            {
                bufferInPos = 0;

                // keep hold of dry signal so we can do smooth bypass
                ringBufferDry.writeCustomData(bufferIn, denoiseFrameSizeF);
                ringBufferDry.commitWrite();

                // scale audio input for denoise
                for (uint32_t i = 0; i < denoiseFrameSize; ++i)
                    bufferIn[i] *= kDenoiseScaling;

                // run denoise
                const float vad = rnnoise_process_frame(denoise, bufferOut, bufferIn);

                // unmute according to threshold
                if (vad >= threshold)
                {
                    muteValue.setTargetValue(1.f);
                    numFramesUntilGracePeriodOver = gracePeriodInFrames;
                }
                else if (gracePeriodInFrames == 0)
                {
                    muteValue.setTargetValue(0.f);
                }

                // scale back down to regular audio level, also apply mute as needed
                for (uint32_t i = 0; i < denoiseFrameSize; ++i)
                {
                    if (numFramesUntilGracePeriodOver != 0 && --numFramesUntilGracePeriodOver == 0)
                        muteValue.setTargetValue(0.f);

                    bufferOut[i] *= kDenoiseScalingInv;
                    bufferOut[i] *= muteValue.next();
                }

                // stats are a bit expensive, so they are optional
                if (stats.enabled)
                {
                    stats.store(vad);
                    parameters[kParamCurrentVAD] = vad * 100.f;
                    parameters[kParamAverageVAD] = stats.avg * 100.f;
                    parameters[kParamMinimumVAD] = stats.min * 100.f;
                    parameters[kParamMaximumVAD] = stats.max * 100.f;
                }

                // write denoise output into ringbuffer
                ringBufferOut.writeCustomData(bufferOut, denoiseFrameSizeF);
                ringBufferOut.commitWrite();
            }

            // we have enough audio frames in the ring buffer, can give back audio to host
            if (processing)
            {
                // apply smooth bypass
                if (d_isNotEqual(dryValue.getCurrentValue(), dryValue.getTargetValue()))
                {
                    // copy processed buffer directly into output
                    ringBufferOut.readCustomData(output, framesCycleF);

                    // retrieve dry buffer
                    ringBufferDry.readCustomData(bufferOut, framesCycleF);

                    for (uint32_t i = 0; i < framesCycle; ++i)
                    {
                        const float dry = dryValue.next();
                        const float wet = 1.f - dry;
                        output[i] = output[i] * wet + bufferOut[i] * dry;
                    }
                }
                else
                {
                    // enabled (bypass off)
                    if (d_isZero(dryValue.getTargetValue()))
                    {
                        // copy processed buffer directly into output
                        ringBufferOut.readCustomData(output, framesCycleF);

                        // retrieve dry buffer (doing nothing with it)
                        ringBufferDry.readCustomData(bufferOut, framesCycleF);
                    }
                    // disable (bypass on)
                    else
                    {
                        // copy dry buffer directly into output
                        ringBufferDry.readCustomData(output, framesCycleF);

                        // retrieve processed buffer (doing nothing with it)
                        ringBufferOut.readCustomData(bufferOut, framesCycleF);
                    }
                }
            }
            // capture more audio frames until it fits 1 denoise block
            else
            {
                // mute output while still capturing audio frames
                std::memset(output, 0, framesCycleF);

                if (ringBufferOut.getReadableDataSize() >= denoiseFrameSizeF)
                    processing = true;
            }

            offset += framesCycle;
            input += framesCycle;
            output += framesCycle;
        }
    }

    void sampleRateChanged(const double sampleRate) override
    {
        dryValue.setSampleRate(sampleRate);
        muteValue.setSampleRate(sampleRate);
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
