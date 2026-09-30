#include "MediaPreview.h"
#include "VideoFixture.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <thread>
void check(bool ok, const char *message) {
    if (!ok) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
template <class T> void write(std::ofstream &stream, T value) {
    stream.write(reinterpret_cast<const char *>(&value), sizeof(value));
}
template <class F> bool until(F predicate) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < end) {
        glfwPollEvents();
        if (predicate())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}
int main() {
#ifdef _WIN32
    check(glfwInit() == GLFW_TRUE, "Windows message loop");
    const auto path =
        std::filesystem::temp_directory_path() /
        ("lancelot-media-test-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".wav");
    {
        std::ofstream file(path, std::ios::binary);
        file.write("RIFF", 4);
        write<std::uint32_t>(file, 36 + 16000);
        file.write("WAVEfmt ", 8);
        write<std::uint32_t>(file, 16);
        write<std::uint16_t>(file, 1);
        write<std::uint16_t>(file, 1);
        write<std::uint32_t>(file, 8000);
        write<std::uint32_t>(file, 16000);
        write<std::uint16_t>(file, 2);
        write<std::uint16_t>(file, 16);
        file.write("data", 4);
        write<std::uint32_t>(file, 16000);
        for (int i = 0; i < 8000; ++i)
            write<std::int16_t>(file, 0);
    }
    MediaPreview player;
    check(player.open(path), player.status().c_str());
    check(until([&] { return player.ready(); }), player.status().c_str());
    check(std::abs(player.duration() - 1) < .05, "WAV duration");
    player.volume(0);
    check(player.play() && until([&] { return player.playing(); }), "play");
    check(player.pause() && until([&] { return !player.playing(); }), "pause");
    check(player.seek(.5) && until([&] { return player.position() > .4; }), "seek");
    check(player.stop(), "stop");
    player.close();
    check(!player.ready(), "close releases playback");
    check(!player.open(path.parent_path() / "lancelot-missing-media.wav"), "missing media error");
    player.close();
    std::filesystem::remove(path);
    auto video = path;
    video.replace_extension(".mp4");
    check(SUCCEEDED(writeVideoFixture(video)), "generate H.264 MP4 fixture");
    check(player.open(video), player.status().c_str());
    check(until([&] { return player.ready(); }), player.status().c_str());
    check(player.duration() > .8, "video duration");
    check(player.play() && until([&] { return player.playing(); }), "video playback");
    check(until([&] { return player.position() > .15 && player.ready(); }), "video decoding advances");
    check(player.pause() && until([&] { return !player.playing(); }), "video pause");
    player.close();
    std::filesystem::remove(video);
    glfwTerminate();
#endif
    std::cout << "Media preview tests passed\n";
}
