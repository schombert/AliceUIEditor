#include "filesystem.hpp"
#include <shobjidl.h> 
#include <memory>

namespace fs {

std::wstring pick_existing_file(std::wstring extension) {
        // CREATE FileOpenDialog OBJECT
        IFileOpenDialog* f_FileSystem;
        auto f_SysHr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&f_FileSystem));
        if(FAILED(f_SysHr)) {
                return L"";
        }

        if(extension.size() > 0) {
                f_FileSystem->SetDefaultExtension(extension.c_str());
                std::wstring w_filter = L"*." + extension;
                _COMDLG_FILTERSPEC spec;
                spec.pszName = w_filter.c_str();
                spec.pszSpec = w_filter.c_str();
                f_FileSystem->SetFileTypes(1, &spec);

                f_FileSystem->SetOptions(FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST | FOS_NOCHANGEDIR | FOS_FORCEFILESYSTEM);
        }

        //  SHOW OPEN FILE DIALOG WINDOW
        f_SysHr = f_FileSystem->Show(NULL);
        if(FAILED(f_SysHr)) {
                f_FileSystem->Release();
                return L"";
        }

        //  RETRIEVE FILE NAME FROM THE SELECTED ITEM
        IShellItem* f_Files;
        f_SysHr = f_FileSystem->GetResult(&f_Files);
        if(FAILED(f_SysHr)) {
                f_FileSystem->Release();
                return L"";
        }

        //  STORE AND CONVERT THE FILE NAME
        PWSTR f_Path;
        f_SysHr = f_Files->GetDisplayName(SIGDN_FILESYSPATH, &f_Path);
        if(FAILED(f_SysHr)) {
                f_Files->Release();
                f_FileSystem->Release();
                return L"";
        }

        //  FORMAT AND STORE THE FILE PATH
        std::wstring path(f_Path);

        //  SUCCESS, CLEAN UP
        CoTaskMemFree(f_Path);
        f_Files->Release();
        f_FileSystem->Release();
        return path;
}

std::wstring pick_new_file(std::wstring extension) {
        //  CREATE FILE OBJECT INSTANCE

        IFileSaveDialog* f_FileSystem;
        auto f_SysHr = CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_ALL, IID_IFileSaveDialog, reinterpret_cast<void**>(&f_FileSystem));
        if(FAILED(f_SysHr)) {
                return L"";
        }

        if(extension.size() > 0) {
                f_FileSystem->SetDefaultExtension(extension.c_str());
                std::wstring w_filter = L"*." + extension;
                _COMDLG_FILTERSPEC spec;
                spec.pszName = w_filter.c_str();
                spec.pszSpec = w_filter.c_str();
                f_FileSystem->SetFileTypes(1, &spec);

                f_FileSystem->SetOptions(FOS_OVERWRITEPROMPT | FOS_NOREADONLYRETURN | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR | FOS_STRICTFILETYPES | FOS_FORCEFILESYSTEM);
        }

        //  SHOW OPEN FILE DIALOG WINDOW
        f_SysHr = f_FileSystem->Show(NULL);
        if(FAILED(f_SysHr)) {
                f_FileSystem->Release();
                return L"";
        }

        //  RETRIEVE FILE NAME FROM THE SELECTED ITEM
        IShellItem* f_Files;
        f_SysHr = f_FileSystem->GetResult(&f_Files);
        if(FAILED(f_SysHr)) {
                f_FileSystem->Release();
                return L"";
        }

        //  STORE AND CONVERT THE FILE NAME
        PWSTR f_Path;
        f_SysHr = f_Files->GetDisplayName(SIGDN_FILESYSPATH, &f_Path);
        if(FAILED(f_SysHr)) {
                f_Files->Release();
                f_FileSystem->Release();
                return L"";
        }

        //  FORMAT AND STORE THE FILE PATH
        std::wstring path(f_Path);

        //  SUCCESS, CLEAN UP
        CoTaskMemFree(f_Path);
        f_Files->Release();
        f_FileSystem->Release();
        return path;
}

std::wstring pick_directory(std::wstring const& default_folder) {
        // CREATE FileOpenDialog OBJECT
        IFileOpenDialog* f_FileSystem;
        auto f_SysHr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&f_FileSystem));
        if(FAILED(f_SysHr)) {
                return L"";
        }

        f_FileSystem->SetOptions(FOS_PATHMUSTEXIST | FOS_PICKFOLDERS | FOS_NOCHANGEDIR);
        if(default_folder.empty() == false) {
                IShellItem* pCurFolder = nullptr;
                SHCreateItemFromParsingName(default_folder.c_str(), nullptr, IID_PPV_ARGS(&pCurFolder));
                if(pCurFolder) {
                        f_FileSystem->SetFolder(pCurFolder);
                        pCurFolder->Release();
                }
        }

        //  SHOW OPEN FILE DIALOG WINDOW
        f_SysHr = f_FileSystem->Show(nullptr);
        if(FAILED(f_SysHr)) {
                f_FileSystem->Release();
                return L"";
        }

        //  RETRIEVE FILE NAME FROM THE SELECTED ITEM
        IShellItem* f_Files;
        f_SysHr = f_FileSystem->GetResult(&f_Files);
        if(FAILED(f_SysHr)) {
                f_FileSystem->Release();
                return L"";
        }

        //  STORE AND CONVERT THE FILE NAME
        PWSTR f_Path;
        f_SysHr = f_Files->GetDisplayName(SIGDN_FILESYSPATH, &f_Path);
        if(FAILED(f_SysHr)) {
                f_Files->Release();
                f_FileSystem->Release();
                return L"";
        }

        //  FORMAT AND STORE THE FILE PATH
        std::wstring path(f_Path);

        //  SUCCESS, CLEAN UP
        CoTaskMemFree(f_Path);
        f_Files->Release();
        f_FileSystem->Release();
        return path;
}

file::~file() {
        if(mapping_handle) {
                if(contents.data)
                        UnmapViewOfFile(contents.data);
                CloseHandle(mapping_handle);
        }
        if(file_handle != INVALID_HANDLE_VALUE) {
                CloseHandle(file_handle);
        }
}

file::file(file&& other) noexcept  {
        mapping_handle = other.mapping_handle;
        file_handle = other.file_handle;
        other.mapping_handle = nullptr;
        other.file_handle = INVALID_HANDLE_VALUE;
        contents = other.contents;
        other.contents.data = nullptr;
        other.contents.file_size = 0;
}
void file::operator=(file&& other) noexcept {
        mapping_handle = other.mapping_handle;
        file_handle = other.file_handle;
        other.mapping_handle = nullptr;
        other.file_handle = INVALID_HANDLE_VALUE;
        contents = other.contents;
        other.contents.data = nullptr;
        other.contents.file_size = 0;
}

file::file(std::wstring const& full_path) {
        file_handle = CreateFileW(full_path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if(file_handle != INVALID_HANDLE_VALUE) {
                mapping_handle = CreateFileMappingW(file_handle, nullptr, PAGE_READONLY, 0, 0, nullptr);
                if(mapping_handle) {
                        contents.data = (char const*)MapViewOfFile(mapping_handle, FILE_MAP_READ, 0, 0, 0);
                        if(contents.data) {
                                _LARGE_INTEGER pvalue;
                                GetFileSizeEx(file_handle, &pvalue);
                                contents.file_size = uint32_t(pvalue.QuadPart);
                        }
                }
        }
}

void write_file(std::wstring const& full_path, char const* file_data, uint32_t file_size) {
        HANDLE file_handle = CreateFileW(full_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if(file_handle != INVALID_HANDLE_VALUE) {
                DWORD written_bytes = 0;
                WriteFile(file_handle, file_data, DWORD(file_size), &written_bytes, nullptr);
                (void)written_bytes;
                SetEndOfFile(file_handle);
                CloseHandle(file_handle);
        }
}

std::wstring utf8_to_native(std::string_view str) {
        if(str.size() > 0) {
                auto buffer = std::unique_ptr<WCHAR[]>(new WCHAR[str.length() * 2]);
                auto chars_written = MultiByteToWideChar(CP_UTF8, 0, str.data(), int32_t(str.length()), buffer.get(), int32_t(str.length() * 2));
                return std::wstring(buffer.get(), size_t(chars_written));
        }
        return std::wstring{ };
}

std::string native_to_utf8(std::wstring_view str) {
        if(str.size() > 0) {
                auto buffer = std::unique_ptr<char[]>(new char[str.length() * 4]);
                auto chars_written = WideCharToMultiByte(CP_UTF8, 0, str.data(), int32_t(str.length()), buffer.get(), int32_t(str.length() * 4), NULL, NULL);
                return std::string(buffer.get(), size_t(chars_written));
        }
        return std::string{ };
}

}
