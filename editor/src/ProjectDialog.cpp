#include "ProjectDialog.h"
#define NOMINMAX
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <shobjidl.h>
#include <wrl/client.h>

ProjectDialogResult chooseProjectPath(GLFWwindow *owner, const std::filesystem::path &initial,
                                      bool folder, bool chinese) {
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    // The host may already have initialized COM. Balance only our successful call.
    struct Apartment {
        bool owned;
        ~Apartment() {
            if (owned)
                CoUninitialize();
        }
    } apartment{SUCCEEDED(initialized)};
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
        return {{}, "Unable to initialize project dialog."};
    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
    HRESULT result = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&dialog));
    if (SUCCEEDED(result)) {
        DWORD options = 0;
        result = dialog->GetOptions(&options);
        if (SUCCEEDED(result))
            result = dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST |
                                        FOS_NOCHANGEDIR |
                                        (folder ? FOS_PICKFOLDERS : FOS_FILEMUSTEXIST));
    }
    if (SUCCEEDED(result))
        result =
            dialog->SetTitle(folder ? (chinese ? L"选择新项目的父目录" : L"Choose project location")
                                    : (chinese ? L"打开 Lancelot 项目" : L"Open Lancelot project"));
    if (SUCCEEDED(result) && !folder) {
        const COMDLG_FILTERSPEC filter[] = {{L"Lancelot project (*.lancelot)", L"*.lancelot"}};
        result = dialog->SetFileTypes(1, filter);
    }
    if (SUCCEEDED(result) && !initial.empty()) {
        Microsoft::WRL::ComPtr<IShellItem> start;
        if (SUCCEEDED(SHCreateItemFromParsingName(initial.c_str(), nullptr, IID_PPV_ARGS(&start))))
            dialog->SetFolder(start.Get());
    }
    if (SUCCEEDED(result))
        result = dialog->Show(owner ? glfwGetWin32Window(owner) : nullptr);
    if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED))
        return {};
    Microsoft::WRL::ComPtr<IShellItem> selection;
    if (SUCCEEDED(result))
        result = dialog->GetResult(&selection);
    PWSTR name = nullptr;
    if (SUCCEEDED(result))
        result = selection->GetDisplayName(SIGDN_FILESYSPATH, &name);
    ProjectDialogResult answer;
    if (SUCCEEDED(result))
        answer.path = name;
    else
        answer.error = "Unable to open project dialog (HRESULT " + std::to_string(result) + ").";
    CoTaskMemFree(name);
    return answer;
}
