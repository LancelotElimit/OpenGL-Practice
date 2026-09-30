#pragma once
#include <filesystem>
#include <memory>
#include <string>

// Editor audition/preview; deliberately independent of the scene simulation clock.
class MediaPreview {
  public:
    MediaPreview();
    ~MediaPreview();
    MediaPreview(const MediaPreview &) = delete;
    MediaPreview &operator=(const MediaPreview &) = delete;
    bool open(const std::filesystem::path &path, void *owner = nullptr);
    void close();
    bool ready() const;
    bool playing() const;
    bool play();
    bool pause();
    bool stop();
    bool seek(double seconds);
    void volume(float value);
    double duration() const;
    double position() const;
    std::string status() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
