#include "pch.h"
#include "wic_loader.h"
#include <wincodec.h>
#include <wrl/client.h>
#include <iostream>

#pragma comment(lib, "windowscodecs.lib")

using Microsoft::WRL::ComPtr;

static bool g_wic_initialized = false;

bool WIC_LoadImage(const char* filename, pic_s& pic) {
    if (!g_wic_initialized) {
        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        if (SUCCEEDED(hr) || hr == S_FALSE) {
            g_wic_initialized = true;
        } else {
            std::cerr << "[WIC] CoInitializeEx failed: " << std::hex << hr << "\n";
            return false;
        }
    }

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return false;

    // Convert filename to WCHAR
    wchar_t wfilename[1024];
    MultiByteToWideChar(CP_UTF8, 0, filename, -1, wfilename, 1024);

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(wfilename, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr)) return false;

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) return false;

    UINT width, height;
    frame->GetSize(&width, &height);

    pic.width_ = width;
    pic.height_ = height;
    pic.pixels_ = (uint8_t*)MEM_ALLOC(width * height * 4);

    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) return false;

    hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, NULL, 0.0f, WICBitmapPaletteTypeMedianCut);
    if (FAILED(hr)) return false;

    hr = converter->CopyPixels(NULL, width * 4, width * height * 4, pic.pixels_);
    if (FAILED(hr)) return false;

    return true;
}
