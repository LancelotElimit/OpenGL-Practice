#include "EditorWorkspace.h"
#include "EditorLocale.h"
#include "AssetFormats.h"
#include <imgui.h>
#include <algorithm>
#ifdef _WIN32
#define NOMINMAX
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#endif

void AssetPreviewPanel::load(EditorWorkspace &ui, const std::filesystem::path &path) {
    auto &p = ui.preview_;
    p.media.close();
    p.image.destroy();
    p.width = p.height = 0;
    p.error.clear();
    p.source = path.string();
    p.open = true;
    const auto category = AssetFormats::category(path);
    if (category == AssetCategory::Image) {
        if (!p.image.loadRGBA(path, false)) {
            p.error = "Image decoding failed.";
            return;
        }
        glBindTexture(GL_TEXTURE_2D, p.image.id());
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &p.width);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &p.height);
    } else if (category == AssetCategory::Audio || category == AssetCategory::Video) {
        void *owner = nullptr;
#ifdef _WIN32
        owner = glfwGetWin32Window(ui.window_);
#endif
        if (!p.media.open(path, owner))
            p.error = p.media.status();
    } else
        p.error = "This file has no built-in preview.";
}
void AssetPreviewPanel::draw(EditorWorkspace &ui) {
    auto &p = ui.preview_;
    if (!p.open) {
        p.media.close();
        p.image.destroy();
        p.source.clear();
        return;
    }
    if (ImGui::Begin(EditorLocale::label("Asset Preview"), &p.open)) {
        ImGui::TextWrapped("%s", p.source.c_str());
        ImGui::Separator();
        if (!p.error.empty())
            ImGui::TextWrapped("%s", EditorLocale::text(p.error.c_str()));
        else if (p.image.id()) {
            ImGui::Text("%d x %d", p.width, p.height);
            ImGui::TextDisabled(EditorLocale::text("Image preview (GIF uses its first frame)."));
            const auto available = ImGui::GetContentRegionAvail();
            const float imageHeight =
                std::max(1.f, available.y - ImGui::GetFrameHeightWithSpacing());
            const float scale =
                std::max(.01f, std::min(available.x / p.width, imageHeight / p.height));
            ImGui::Image(ImTextureRef(static_cast<ImTextureID>(p.image.id())),
                         ImVec2(p.width * scale, p.height * scale));
        } else if (!p.source.empty()) {
            ImGui::TextWrapped("%s", EditorLocale::text(p.media.status().c_str()));
            ImGui::TextWrapped(EditorLocale::text(
                "Playback uses Windows codecs. Video opens in a preview window."));
            ImGui::BeginDisabled(!p.media.ready());
            if (ImGui::Button(EditorLocale::label(p.media.playing() ? "Pause" : "Play"))) {
                if (p.media.playing())
                    p.media.pause();
                else {
                    p.media.volume(p.volume);
                    p.media.play();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(EditorLocale::label("Stop")))
                p.media.stop();
            float seconds = static_cast<float>(p.media.position());
            if (ImGui::SliderFloat(EditorLocale::label("Playback position"), &seconds, 0,
                                   static_cast<float>(p.media.duration()), "%.1f s"))
                p.media.seek(seconds);
            if (ImGui::SliderFloat(EditorLocale::label("Volume"), &p.volume, 0, 1))
                p.media.volume(p.volume);
            ImGui::EndDisabled();
        }
        if (ImGui::Button(EditorLocale::label("Close preview")))
            p.open = false;
    }
    ImGui::End();
    if (!p.open) {
        p.media.close();
        // ImGui still references this frame's image until the draw data is rendered.
        // Release the texture at the start of the next frame instead.
    }
}
