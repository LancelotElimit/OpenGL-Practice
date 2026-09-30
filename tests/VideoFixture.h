#pragma once
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <filesystem>

// Generate a short H.264 MP4 locally; no downloaded media or external encoder.
inline HRESULT writeVideoFixture(const std::filesystem::path &path) {
    using Microsoft::WRL::ComPtr;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return hr;
    struct Cleanup {
        bool mf = false;
        ~Cleanup() {
            if (mf)
                MFShutdown();
            CoUninitialize();
        }
    } cleanup;
    hr = MFStartup(MF_VERSION);
    if (FAILED(hr))
        return hr;
    cleanup.mf = true;
    ComPtr<IMFSinkWriter> writer;
    hr = MFCreateSinkWriterFromURL(path.wstring().c_str(), nullptr, nullptr, writer.GetAddressOf());
    if (FAILED(hr))
        return hr;
    ComPtr<IMFMediaType> output, input;
    if (FAILED(hr = MFCreateMediaType(output.GetAddressOf())))
        return hr;
    output->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    output->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    output->SetUINT32(MF_MT_AVG_BITRATE, 200000);
    output->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize(output.Get(), MF_MT_FRAME_SIZE, 64, 64);
    MFSetAttributeRatio(output.Get(), MF_MT_FRAME_RATE, 10, 1);
    MFSetAttributeRatio(output.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    DWORD stream = 0;
    if (FAILED(hr = writer->AddStream(output.Get(), &stream)))
        return hr;
    if (FAILED(hr = MFCreateMediaType(input.GetAddressOf())))
        return hr;
    input->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    input->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    input->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize(input.Get(), MF_MT_FRAME_SIZE, 64, 64);
    MFSetAttributeRatio(input.Get(), MF_MT_FRAME_RATE, 10, 1);
    MFSetAttributeRatio(input.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    if (FAILED(hr = writer->SetInputMediaType(stream, input.Get(), nullptr)))
        return hr;
    if (FAILED(hr = writer->BeginWriting()))
        return hr;
    for (int frame = 0; frame < 10; ++frame) {
        ComPtr<IMFMediaBuffer> buffer;
        if (FAILED(hr = MFCreateMemoryBuffer(64 * 64 * 4, buffer.GetAddressOf())))
            return hr;
        BYTE *pixels = nullptr;
        if (FAILED(hr = buffer->Lock(&pixels, nullptr, nullptr)))
            return hr;
        for (int i = 0; i < 64 * 64; ++i) {
            pixels[i * 4] = 32;
            pixels[i * 4 + 1] = static_cast<BYTE>(100 + frame * 10);
            pixels[i * 4 + 2] = 230;
            pixels[i * 4 + 3] = 255;
        }
        buffer->Unlock();
        buffer->SetCurrentLength(64 * 64 * 4);
        ComPtr<IMFSample> sample;
        if (FAILED(hr = MFCreateSample(sample.GetAddressOf())))
            return hr;
        sample->AddBuffer(buffer.Get());
        sample->SetSampleTime(frame * 1000000LL);
        sample->SetSampleDuration(1000000);
        if (FAILED(hr = writer->WriteSample(stream, sample.Get())))
            return hr;
    }
    return writer->Finalize();
}
#endif
