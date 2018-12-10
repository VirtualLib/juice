///////////////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2017 The Authors of ANT(http:://ant.sh) . All Rights Reserved. 
// Use of this source code is governed by a BSD-style license that can be 
// found in the LICENSE file. 
//
///////////////////////////////////////////////////////////////////////////////////////////

#ifndef JUICE_BASIC_UTIL_INCLUDE_H_
#define JUICE_BASIC_UTIL_INCLUDE_H_

#include <memory>
#include <stack>
#include <Windows.h>
#include <Shlwapi.h>

#include "apis/juice.h"
#include "apis/basictypes.h"
#include "apis/scoped_object.h"

namespace x {

const wchar_t kSeparators[] = L"\\/";
const size_t kSeparatorsLength = arraysize(kSeparators);
const wchar_t kCurrentDirectory[] = L".";
const wchar_t kParentDirectory[] = L"..";
const wchar_t kExtensionSeparator[] = L".";
const wchar_t kStringTerminator = L'\0';
const wchar_t kSearchAll = L'*';

struct JUICE_API PlatformFileInfo {
    PlatformFileInfo() {}
    virtual ~PlatformFileInfo() {}

    ULONGLONG size = 0;
    DWORD attributes = 0;
    bool directory = false;
    FILETIME creation_time;
    FILETIME last_modified;
    FILETIME last_accessed;
    std::wstring filename;
    std::wstring path;
};

static bool IsSeparator(wchar_t character) {
    for (size_t i = 0; i < kSeparatorsLength - 1; ++i) {
        if (character == kSeparators[i]) {
            return true;
        }
    }
    return false;
}

static std::wstring::size_type FindDriveLetter(const std::wstring& path) {
    // This is dependent on an ASCII-based character set, but that's a
    // reasonable assumption.  iswalpha can be too inclusive here.
    if (path.length() >= 2 && path[1] == L':' &&
        ((path[0] >= L'A' && path[0] <= L'Z') ||
        (path[0] >= L'a' && path[0] <= L'z'))) {
        return 1;
    }
    return std::wstring::npos;
}

static std::wstring StripTrailingSeparators(std::wstring path) {
    auto start = FindDriveLetter(path) + 2;
    std::wstring::size_type last_stripped = std::wstring::npos;
    for (std::wstring::size_type pos = path.length(); pos > start && IsSeparator(path[pos - 1]); --pos) {
        // If the string only has two separators and they're at the beginning,
        // don't strip them, unless the string began with more than two separators.
        if (pos != start + 1 || last_stripped == start + 2 || !IsSeparator(path[start - 1])) {
            path.resize(pos - 1);
            last_stripped = pos;
        }
    }
    return path;
}

bool IsPathAbsolute(const std::wstring path) {
    std::wstring::size_type letter = FindDriveLetter(path);
    if (letter != std::wstring::npos) {
        // Look for a separator right after the drive specification.
        return path.length() > letter + 1 && IsSeparator(path[letter + 1]);
    }
    // Look for a pair of leading separators.
    return path.length() > 1 && IsSeparator(path[0]) && IsSeparator(path[1]);
}

static std::wstring GetParent(std::wstring path) {
    path = StripTrailingSeparators(path);

    // The drive letter, if any, always needs to remain in the output.  If there
    // is no drive letter, as will always be the case on platforms which do not
    // support drive letters, letter will be npos, or -1, so the comparisons and
    // resizes below using letter will still be valid.
    auto letter = FindDriveLetter(path);

    auto last_separator = path.find_last_of(kSeparators, std::wstring::npos, kSeparatorsLength - 1);
    if (last_separator == std::wstring::npos) {
        // path_ is in the current directory.
        path.resize(letter + 1);
    } else if (last_separator == letter + 1) {
        // path_ is in the root directory.
        path.resize(letter + 2);
    } else if (last_separator == letter + 2 && IsSeparator(path[letter + 1])) {
        // path_ is in "//" (possibly with a drive letter); leave the double
        // separator intact indicating alternate root.
        path.resize(letter + 3);
    } else if (last_separator != 0) {
        // path_ is somewhere else, trim the basename.
        path.resize(last_separator);
    }

    path = StripTrailingSeparators(path);
    if (!path.length()) path = kCurrentDirectory;

    return path;
}

static std::wstring GetFileName(std::wstring path) {
    path = StripTrailingSeparators(path);
    auto letter = FindDriveLetter(path);
    if (letter != std::wstring::npos) {
        path.erase(0, letter + 1);
    }
    auto last_separator = path.find_last_of(kSeparators, std::wstring::npos, kSeparatorsLength - 1);
    if (last_separator != std::wstring::npos && last_separator < path.length() - 1) {
        path.erase(0, last_separator + 1);
    }
    return path;
}

static std::wstring Append(std::wstring path, const std::wstring& component) WARN_UNUSED_RESULT {
    const std::wstring* appended = &component;
    std::wstring without_nuls;
    auto nul_pos = component.find(kStringTerminator);
    if (nul_pos != std::wstring::npos) {
        without_nuls = component.substr(0, nul_pos);
        appended = &without_nuls;
    }

    if (path.length() <= 0) return *appended;
    if (appended->length() <= 0) return path;

    if (path.compare(kCurrentDirectory) == 0) {
        return *appended;
    }
    if (IsPathAbsolute(*appended)) return L"";

    path = StripTrailingSeparators(path);
    if (!IsSeparator(path[path.length() - 1])) {
        // Don't append a separator if the path is just a drive letter.
        if (FindDriveLetter(path) + 1 != path.length()) {
            path.append(1, kSeparators[0]);
        }
    }
    return path.append(*appended);
}


bool DirectoryExists(const std::wstring& path) {
    DWORD fileattr = GetFileAttributes(path.c_str());
    if (fileattr != INVALID_FILE_ATTRIBUTES)
        return (fileattr & FILE_ATTRIBUTE_DIRECTORY) != 0;
    return false;
}

bool CreatePathTree(const std::wstring& path) {
    auto result = CreateDirectoryEx(nullptr, path.c_str(), nullptr);
    if (result == ERROR_SUCCESS) return true;
    DWORD fileattr = ::GetFileAttributes(path.c_str());
    if (fileattr != INVALID_FILE_ATTRIBUTES) {
        if ((fileattr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            return true;
        }
        return false;
    }

    auto parent_path = GetParent(path);
    if (path == parent_path) return false;
    if (!CreatePathTree(parent_path)) return false;

    if (!::CreateDirectory(path.c_str(), NULL)) {
        DWORD error_code = ::GetLastError();
        if (error_code == ERROR_ALREADY_EXISTS && DirectoryExists(path)) {
            return true;
        }
        return false;
    }
    return true;
}

bool GetFileInfo(const std::wstring& path, PlatformFileInfo* results) {
    WIN32_FILE_ATTRIBUTE_DATA attr;
    if (!GetFileAttributesEx(path.c_str(), GetFileExInfoStandard, &attr)) {
        return false;
    }

    ULARGE_INTEGER size;
    size.HighPart = attr.nFileSizeHigh;
    size.LowPart = attr.nFileSizeLow;
    results->size = size.QuadPart;

    results->attributes = attr.dwFileAttributes;
    results->directory = (attr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    results->last_modified = attr.ftLastWriteTime;
    results->last_accessed = attr.ftLastAccessTime;
    results->creation_time = attr.ftCreationTime;
    return true;
}

int64 GetFileSize(const std::wstring& path) {
    PlatformFileInfo info;
    if (!GetFileInfo(path, &info)) return -1;
    return info.size;
}

int64_t GetFileSize(const WIN32_FILE_ATTRIBUTE_DATA &find_data) {
    ULARGE_INTEGER size;
    size.HighPart = find_data.nFileSizeHigh;
    size.LowPart = find_data.nFileSizeLow;
    if (size.QuadPart > static_cast<ULONGLONG>((std::numeric_limits<int64_t>::max)())) {
        return (std::numeric_limits<int64_t>::max)();
    }
    return static_cast<int64_t>(size.QuadPart);
}

int64_t GetFileSize(const WIN32_FIND_DATA& find_data) {
  ULARGE_INTEGER size;
  size.HighPart = find_data.nFileSizeHigh;
  size.LowPart = find_data.nFileSizeLow;
  if (size.QuadPart >
      static_cast<ULONGLONG>((std::numeric_limits<int64_t>::max)())) {
    return (std::numeric_limits<int64_t>::max)();
  }
  return static_cast<int64_t>(size.QuadPart);
}

namespace internal {

struct ScopedHANDLECloseTraits {
    static HANDLE InvalidValue() { return nullptr; }
    static void Free(HANDLE handle) { ::CloseHandle(handle); }
};

struct ScopedSearchHANDLECloseTraits {
    static HANDLE InvalidValue() { return INVALID_HANDLE_VALUE; }
    static void Free(HANDLE handle) { ::FindClose(handle); }
};

} // namespace internal

// TODO: This should be terminate for |FILE| when |CloseHandle| failed.
using ScopedHANDLE = ScopedGeneric<HANDLE, internal::ScopedHANDLECloseTraits>;
using ScopedSearchHANDLE = ScopedGeneric<HANDLE, internal::ScopedSearchHANDLECloseTraits>;

bool IsSymbolicLink(const std::wstring& path) {
    WIN32_FIND_DATA find_data;
    ScopedSearchHANDLE handle(::FindFirstFileEx(path.c_str(), FindExInfoBasic, &find_data, FindExSearchNameMatch, nullptr, 0));
    if (!handle.is_valid()) return false;
    return (find_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 &&
        find_data.dwReserved0 == IO_REPARSE_TAG_SYMLINK;
}

bool IsRegularFile(const std::wstring& path) {
    auto fileattr = ::GetFileAttributes(path.c_str());
    if (fileattr == INVALID_FILE_ATTRIBUTES) return false;
    if ((fileattr & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
        (fileattr & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        return false;
    }
    return true;
}

bool IsDirectory(const DWORD& attributes, bool allow_symlinks) {
    if (attributes == INVALID_FILE_ATTRIBUTES) return false;
    if (!allow_symlinks && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) return false;
    return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool IsDirectory(const std::wstring& path, bool allow_symlinks) {
    return IsDirectory(::GetFileAttributes(path.c_str()), allow_symlinks);
}

// A class for enumerating the files in a provided path. The order of the
// results is not guaranteed. This is blocking. Do not use on critical threads.
// Example:
//   base::FileEnumerator enum(my_dir, false, base::FileEnumerator::FILES, L"*.txt");
//   for (auto name = enum.Next(); !name.empty(); name = enum.Next())
//     ...
class JUICE_API FileEnumerator {
public:
    // Note: copy & assign supported.
    class JUICE_API FileInfo {
    public:
        explicit FileInfo() { }
        virtual ~FileInfo() {}
        std::wstring GetName() const { return find_data_.cFileName; } // The name of the file. This will not include any path information.
        int64_t GetSize() const { return GetFileSize(find_data_); }
        auto GetLastModifiedTime() const { return find_data_.ftLastWriteTime; }
        bool IsDirectory() const { return x::IsDirectory(find_data_.dwFileAttributes, true); }
        const WIN32_FIND_DATA& find_data() const { return find_data_; }
    private:
        friend class FileEnumerator;
        WIN32_FIND_DATA find_data_ = { 0 };
    };

    enum FileType {
        FILES = 1 << 0,
        DIRECTORIES = 1 << 1,
        INCLUDE_DOT_DOT = 1 << 2,
    };

    explicit FileEnumerator(const std::wstring& root_path, bool recursive, int file_type)
        : FileEnumerator(root_path, recursive, file_type, L"") {}

    explicit FileEnumerator(const std::wstring& root_path, bool recursive, int file_type, const std::wstring& pattern)
        : recursive_(recursive), file_type_(file_type), pattern_(pattern.empty() ? std::wstring(1, kSearchAll) : pattern) {
        assert(!(recursive && (INCLUDE_DOT_DOT & file_type_)));
        pending_paths_.push(root_path);
    }
    virtual ~FileEnumerator() {}

    std::wstring Next() {
        while (has_find_data_ || !pending_paths_.empty()) {
            if (!has_find_data_) {
                // The last find FindFirstFile operation is done, prepare a new one.
                root_path_ = pending_paths_.top();
                pending_paths_.pop();

                // Start a new find operation.
                auto path = Append(root_path_, pattern_);
                find_handle_.reset(FindFirstFileEx(path.c_str(), FindExInfoBasic, &find_data_, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH));
                has_find_data_ = true;
            } else if (find_handle_.is_valid() && !FindNextFile(find_handle_.get(), &find_data_)) { // Search for the next file/directory.
                find_handle_.reset();
            }

            if (!find_handle_.is_valid()) {
                has_find_data_ = false;
                pattern_ = kSearchAll;
                continue;
            }

            if (ShouldSkip(find_data_.cFileName))
                continue;

            auto cur_file = Append(root_path_, find_data_.cFileName);
            if (IsDirectory(find_data_.dwFileAttributes, true)) {
                if (recursive_ && x::IsDirectory(cur_file.c_str(), true)) {
                    pending_paths_.push(cur_file);
                }
                if (file_type_ & FileEnumerator::DIRECTORIES) return cur_file;
            } else if (file_type_ & FileEnumerator::FILES) {
                return cur_file;
            }
        }
        return L"";
    }

    FileInfo GetInfo() const {
        if (!has_find_data_) return FileInfo();
        FileInfo ret;
        std::memcpy(&ret.find_data_, &find_data_, sizeof(find_data_));
        return ret;
    }

    PlatformFileInfo GetPlatformFileInfo() const {
        PlatformFileInfo results;
        if (!has_find_data_) return results;
        auto data = GetInfo();
        results.size = data.GetSize();
        results.attributes = data.find_data().dwFileAttributes;
        results.directory = data.IsDirectory();
        results.last_modified = data.find_data().ftLastWriteTime;
        results.last_accessed = data.find_data().ftLastAccessTime;
        results.creation_time = data.find_data().ftCreationTime;
        return results;
    }

private:
    // Returns true if the given path should be skipped in enumeration.
    bool ShouldSkip(const std::wstring& path) {
        auto basename = GetFileName(path);
        return basename == L"." || (basename == L".." && !(INCLUDE_DOT_DOT & file_type_));
    }

    // True when find_data_ is valid.
    bool has_find_data_ = false;
    bool recursive_ = false;
    int file_type_ = 0;

    ScopedSearchHANDLE find_handle_;
    WIN32_FIND_DATA find_data_ = { 0 };

    std::wstring root_path_; 
    std::wstring pattern_;  // Empty when we want to find everything.
    std::stack<std::wstring> pending_paths_;
    DISALLOW_COPY_AND_ASSIGN(FileEnumerator);
};

bool IsDirectoryEmpty(const std::wstring& path) {
    FileEnumerator files(path, false, FileEnumerator::FILES | FileEnumerator::DIRECTORIES);
    if (files.Next().empty()) return true;
    return false;
}

ScopedComObject<IStream> Open(const std::wstring& path, bool read) {
    ScopedComObject<IStream> file_stream;
    auto mode = read ? STGM_READ : (STGM_CREATE | STGM_WRITE);
    auto result = ::SHCreateStreamOnFileEx(path.c_str(), mode, FILE_ATTRIBUTE_NORMAL, read ? FALSE : TRUE, nullptr, file_stream.Receive());
    return file_stream;
}

static bool GetCurrentDirectory(std::wstring* dir) {
    wchar_t system_buffer[MAX_PATH] = { 0 };
    auto len = ::GetCurrentDirectory(MAX_PATH, system_buffer);
    if (len == 0 || len > MAX_PATH) return false;
    std::wstring dir_str(system_buffer);
    *dir = StripTrailingSeparators(dir_str);
    return true;
}

static bool SetCurrentDirectory(const std::wstring& directory) {
    BOOL ret = ::SetCurrentDirectory(directory.c_str());
    return ret != 0;
}

namespace internal {

typedef HMODULE(WINAPI* LoadLibraryFunction)(const wchar_t* file_name);

// LoadLibrary() opens the file off disk.
HMODULE LoadNativeLibraryHelper(const std::wstring& library_path, LoadLibraryFunction load_library_api) {
    // Switch the current directory to the library directory as the library
    // may have dependencies on DLLs in this directory.
    bool restore_directory = false;
    std::wstring current_directory;
    if (x::GetCurrentDirectory(&current_directory)) {
        auto plugin_path = GetParent(library_path);
        if (!plugin_path.empty()) {
            x::SetCurrentDirectory(plugin_path);
            restore_directory = true;
        }
    }
    HMODULE module = (*load_library_api)(library_path.c_str());
    if (restore_directory)
    x::SetCurrentDirectory(current_directory);

    return module;
}

} // namespace internal

JUICE_API HMODULE LoadLibrary(const std::wstring& path, std::string* error) {
    return internal::LoadNativeLibraryHelper(path, ::LoadLibraryW);
}

JUICE_API HMODULE LoadLibraryDynamically(const std::wstring& path) {
    typedef HMODULE(WINAPI* LoadLibraryFunction)(const wchar_t* file_name);

    LoadLibraryFunction load_library = reinterpret_cast<LoadLibraryFunction>(
        ::GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryW"));

    return internal::LoadNativeLibraryHelper(path, load_library);
}

JUICE_API void UnloadNativeLibrary(HMODULE library) {
    if (library == nullptr) return;
    ::FreeLibrary(library);
}

JUICE_API void* GetFunctionPointerFromNativeLibrary(HMODULE library, const char* name) {
    if (name == nullptr) return nullptr;
    return ::GetProcAddress(library, name);
}

JUICE_API void* GetFunctionPointerFromNativeLibrary(const std::wstring& library_name, const char* name) {
    if (name == nullptr) return nullptr;
    HMODULE wellknown_handler = ::GetModuleHandle(library_name.c_str());
    if (nullptr == wellknown_handler) return nullptr;
    return GetFunctionPointerFromNativeLibrary(wellknown_handler, name);
}

// Returns the result whether |library_name| had been loaded.
// It will be true if |library_name| is empty.
JUICE_API bool WellKnownLibrary(const std::wstring& library_name) {
    if (library_name.empty()) return false;
    HMODULE wellknown_handler = ::GetModuleHandle(library_name.c_str());
    return nullptr != wellknown_handler;
}



} // namespace x


#endif // !JUICE_BASIC_UTIL_INCLUDE_H_