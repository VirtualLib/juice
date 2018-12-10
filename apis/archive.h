///////////////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2017 The Authors of ANT(http:://ant.sh) . All Rights Reserved. 
// Use of this source code is governed by a BSD-style license that can be 
// found in the LICENSE file. 
//
///////////////////////////////////////////////////////////////////////////////////////////

#ifndef VIRTUAL_JUICE_ARCHIVE_INCLUDE_H_
#define VIRTUAL_JUICE_ARCHIVE_INCLUDE_H_

#include <memory>
#include <vector>
#include <Windows.h>

#include "apis/juice.h"
#include "apis/basictypes.h"
#include "apis/dynamic_library.h"
#include "apis/dynamic_library_interface.h"
#include "apis/scoped_object.h"

namespace juice {

typedef enum class __Level {
    FAST    = 0,
    NORMAL  = 1
} Level;

typedef enum class __Format : std::size_t {
    SEVENZ  = 0,
    ZIP     = 1,
    GZIP    = 2,
    BZIP2   = 3,
    RAR     = 4,
    TAR     = 5,
    ISO     = 6,
    CAB     = 7,
    LZMA    = 8,
    LZMA86  = 9,
    LAST,
} Format;

class Progress {
public:
  virtual ~Progress() {}
  virtual void StartProgress(const std::wstring &path, const ULONGLONG &bytes) = 0;
  virtual void Progressed(const std::wstring &path, const ULONGLONG &bytes) = 0;
};


class JUICE_API Archive {
public:
    explicit Archive(const std::wstring& path = L"7z.dll");
    explicit Archive(const std::shared_ptr<x::DynamicLibrary>& library);
    virtual ~Archive();

    template<typename Interface>
    bool GetClassObject(const GUID* guid, const IID& id, ScopedComObject<Interface>& obj) {
        if (guid == nullptr) return false;
        uint result = CreateObject(std::move(guid), std::move(&id), obj.ReceiveVoid());
        return SUCCEEDED(result);
    }

    using OpenCallback = std::function<void(const std::wstring& path, const ULONGLONG& bytes)>;
    bool Open(const std::wstring& path, const juice::Format& format, const OpenCallback& callback);

    bool Extract(const std::wstring& path, const juice::Format& format, const std::wstring& root, Progress* callback);

    bool Compress(const std::wstring& path, const juice::Format& format, const std::vector<x::PlatformFileInfo>& file_list, Progress* callback);

protected:
    x::Function<uint, const GUID*, const GUID*, void**> CreateObject;

};

}


#endif // !VIRTUAL_JUICE_ARCHIVE_INCLUDE_H_