#include "MediaPreview.h"
#include "AssetFormats.h"
#include <algorithm>
#include <atomic>
#include <iomanip>
#include <sstream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <mfapi.h>
#include <mfplay.h>
#include <propvarutil.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
namespace {
struct PlaybackState {
    std::atomic<bool> ready{false};
    std::atomic<HRESULT> error{S_OK};
};
class PlaybackCallback final : public IMFPMediaPlayerCallback {
    std::atomic<ULONG> refs_{1};
    std::shared_ptr<PlaybackState> state_;

  public:
    explicit PlaybackCallback(std::shared_ptr<PlaybackState> state) : state_(std::move(state)) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **output) override {
        if (!output)
            return E_POINTER;
        *output = nullptr;
        if (id != __uuidof(IUnknown) && id != __uuidof(IMFPMediaPlayerCallback))
            return E_NOINTERFACE;
        *output = static_cast<IMFPMediaPlayerCallback *>(this);
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override {
        const auto refs = --refs_;
        if (!refs)
            delete this;
        return refs;
    }
    void STDMETHODCALLTYPE OnMediaPlayerEvent(MFP_EVENT_HEADER *event) override {
        if (FAILED(event->hrEvent)) {
            state_->error = event->hrEvent;
            state_->ready = false;
        } else if (event->eEventType == MFP_EVENT_TYPE_MEDIAITEM_SET)
            state_->ready = true;
    }
};
} // namespace
struct MediaPreview::Impl {
    ComPtr<IMFPMediaPlayer> player;
    ComPtr<IMFPMediaPlayerCallback> callback;
    std::shared_ptr<PlaybackState> state = std::make_shared<PlaybackState>();
    HWND video = nullptr;
    bool com = false, foundation = false;
    double seconds = 0;
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
        auto *self = reinterpret_cast<Impl *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_CLOSE) {
            if (self && self->player)
                self->player->Pause();
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        if (message == WM_PAINT) {
            PAINTSTRUCT paint;
            BeginPaint(hwnd, &paint);
            if (self && self->player)
                self->player->UpdateVideo();
            EndPaint(hwnd, &paint);
            return 0;
        }
        if (message == WM_SIZE && self && self->player)
            self->player->UpdateVideo();
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
    bool result(HRESULT hr) {
        if (FAILED(hr))
            state->error = hr;
        return SUCCEEDED(hr);
    }
    ~Impl() {
        if (player)
            player->Shutdown();
        player.Reset();
        callback.Reset();
        if (video) {
            SetWindowLongPtrW(video, GWLP_USERDATA, 0);
            DestroyWindow(video);
        }
        if (foundation)
            MFShutdown();
        if (com)
            CoUninitialize();
    }
};
#else
struct MediaPreview::Impl {};
#endif
MediaPreview::MediaPreview() = default;
MediaPreview::~MediaPreview() = default;
void MediaPreview::close() { impl_.reset(); }
bool MediaPreview::open(const std::filesystem::path &path, void *owner) {
    close();
    impl_ = std::make_unique<Impl>();
#ifdef _WIN32
    auto &p = *impl_;
    auto hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    p.com = SUCCEEDED(hr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
        return p.result(hr);
    if (!p.result(MFStartup(MF_VERSION)))
        return false;
    p.foundation = true;
    std::error_code fileError;
    if (!std::filesystem::is_regular_file(path,fileError))
        return p.result(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
    if (AssetFormats::category(path) == AssetCategory::Video) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = Impl::windowProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"LancelotMediaPreview";
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        RegisterClassW(&wc);
        p.video = CreateWindowExW(0, wc.lpszClassName, path.filename().wstring().c_str(),
                                  WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 720, 440,
                                  static_cast<HWND>(owner), nullptr, wc.hInstance, nullptr);
        if (!p.video)
            return p.result(HRESULT_FROM_WIN32(GetLastError()));
        SetWindowLongPtrW(p.video, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&p));
    }
    p.callback.Attach(new PlaybackCallback(p.state));
    if (!p.result(MFPCreateMediaPlayer(nullptr, FALSE, MFP_OPTION_FREE_THREADED_CALLBACK,
                                       p.callback.Get(), p.video, p.player.GetAddressOf())))
        return false;
    ComPtr<IMFPMediaItem> item;
    if (!p.result(p.player->CreateMediaItemFromURL(
            std::filesystem::absolute(path).wstring().c_str(), TRUE, 0, item.GetAddressOf())))
        return false;
    PROPVARIANT value;
    PropVariantInit(&value);
    ULONGLONG ticks = 0;
    if (SUCCEEDED(item->GetDuration(MFP_POSITIONTYPE_100NS, &value)) &&
        SUCCEEDED(PropVariantToUInt64(value, &ticks)))
        p.seconds = ticks / 10000000.0;
    PropVariantClear(&value);
    return p.result(p.player->SetMediaItem(item.Get()));
#else
    return false;
#endif
}
bool MediaPreview::ready() const {
#ifdef _WIN32
    return impl_ && impl_->state->ready && SUCCEEDED(impl_->state->error);
#else
    return false;
#endif
}
bool MediaPreview::playing() const {
#ifdef _WIN32
    MFP_MEDIAPLAYER_STATE value{};
    return ready() && SUCCEEDED(impl_->player->GetState(&value)) &&
           value == MFP_MEDIAPLAYER_STATE_PLAYING;
#else
    return false;
#endif
}
bool MediaPreview::play() {
#ifdef _WIN32
    if (!ready())
        return false;
    if (impl_->video)
        ShowWindow(impl_->video, SW_SHOWNOACTIVATE);
    return impl_->result(impl_->player->Play());
#else
    return false;
#endif
}
bool MediaPreview::pause() {
#ifdef _WIN32
    return ready() && impl_->result(impl_->player->Pause());
#else
    return false;
#endif
}
bool MediaPreview::stop() {
#ifdef _WIN32
    return ready() && impl_->result(impl_->player->Stop());
#else
    return false;
#endif
}
bool MediaPreview::seek(double seconds) {
#ifdef _WIN32
    if (!ready())
        return false;
    PROPVARIANT value;
    PropVariantInit(&value);
    value.vt = VT_I8;
    value.hVal.QuadPart = static_cast<LONGLONG>(std::clamp(seconds, 0.0, duration()) * 10000000.0);
    return impl_->result(impl_->player->SetPosition(MFP_POSITIONTYPE_100NS, &value));
#else
    return false;
#endif
}
void MediaPreview::volume(float value) {
#ifdef _WIN32
    if (ready())
        impl_->result(impl_->player->SetVolume(std::clamp(value, 0.f, 1.f)));
#endif
}
double MediaPreview::duration() const {
#ifdef _WIN32
    return impl_ ? impl_->seconds : 0;
#else
    return 0;
#endif
}
double MediaPreview::position() const {
#ifdef _WIN32
    if (!ready())
        return 0;
    PROPVARIANT value;
    PropVariantInit(&value);
    double seconds = 0;
    ULONGLONG ticks = 0;
    if (SUCCEEDED(impl_->player->GetPosition(MFP_POSITIONTYPE_100NS, &value)) &&
        SUCCEEDED(PropVariantToUInt64(value, &ticks)))
        seconds = ticks / 10000000.0;
    PropVariantClear(&value);
    return seconds;
#else
    return 0;
#endif
}
std::string MediaPreview::status() const {
#ifdef _WIN32
    if (!impl_)
        return "Select an audio or video file.";
    const auto hr = impl_->state->error.load();
    if (FAILED(hr)) {
        std::ostringstream text;
        text << "Media decoder error 0x" << std::hex << static_cast<unsigned long>(hr)
             << ". Check the file and installed Windows codecs.";
        return text.str();
    }
    return ready() ? "Media ready." : "Loading media...";
#else
    return "Media playback requires Windows.";
#endif
}
