////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2017 The Authors of ANT(http://ant.sh). All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
////////////////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "apis/archive.h"
#include "apis/enumerate.h"
#include "apis/basic_util.h"

#include <array>
#include "guids.h"

#include <InitGuid.h>
#include "7zip/Archive/IArchive.h"

#include "streaming.h"

#if defined(COMPILER_MSVC)
// We usually use the _CrtDumpMemoryLeaks() with the DEBUGER and CRT library to
// check a memory leak.
#if defined(_DEBUG) && _MSC_VER > 1000 // VC++ DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#define  DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#if defined(DEBUG_NEW)
#define new DEBUG_NEW
#endif // DEBUG_NEW
#endif // VC++ DEBUG
#endif // defined(COMPILER_MSVC)

namespace juice {

const GUID *FormatGUID(const juice::Format& format) {
    static const std::array<const GUID*, 11> guid = {
        &CLSID_CFormat7z,
        &CLSID_CFormatZip,
        &CLSID_CFormatGZip,
        &CLSID_CFormatBZip2,
        &CLSID_CFormatRar,
        &CLSID_CFormatTar,
        &CLSID_CFormatIso,
        &CLSID_CFormatCab,
        &CLSID_CFormatLzma,
        &CLSID_CFormatLzma86,
        &CLSID_CFormat7z,
    };
    size_t formats = enumerate_cast(format);
    auto id = (std::min)(formats, guid.size());
    id = (std::max)(size_t(0), id);
    return guid[id];
}

const std::wstring FormatExtension(const juice::Format& format) {
    static const std::array<const wchar_t*, 11> extension = {
        L".7z", L".zip", L".gz", L".bz", L".rar", L".tar", L".iso", L".cab", L".lzma", L".lzma86",
        L".zip",
    };
    size_t formats = enumerate_cast(format);
    auto id = (std::min)(formats, extension.size());
    id = (std::max)(size_t(0), id);
    return extension[id];
}

static ScopedComObject<IInArchive> LoadReader(Archive *archive, const juice::Format &format) {
    ScopedComObject<IInArchive> obj;
    auto result = archive->GetClassObject(FormatGUID(format), IID_IInArchive, obj);
    return obj;
}

static ScopedComObject<IOutArchive> LoadEditor(Archive* archive, const juice::Format& format) {
    ScopedComObject<IOutArchive> obj;
    auto result = archive->GetClassObject(FormatGUID(format), IID_IOutArchive, obj);
    return obj;
}

Archive::Archive(const std::wstring& path) : Archive(std::make_shared<x::DynamicLibrary>(path)) {
}

Archive::Archive(const std::shared_ptr<x::DynamicLibrary>& library) : CreateObject("CreateObject"){
    CreateObject.Reset(library);
}

Archive::~Archive() {}

bool Archive::Open(const std::wstring& path, const juice::Format& format, const OpenCallback& callback) {
    if (path.empty()) return false;
    auto file = x::Open(path, true);
    if (!file) return false;
    auto archive = LoadReader(this, format);
    if (!archive) return false;

    ScopedComObject<juice::ReadFileStreamming> streamming(new juice::ReadFileStreamming(file));
    ScopedComObject<juice::ArchiveOpenning> openning(new juice::ArchiveOpenning);
    auto result = archive->Open(streamming, 0, openning);
    if (FAILED(result)) return false;
    {
        ScopedPropVariant prop;
        archive->GetArchiveProperty(kpidPath, prop.Receive());
        if (prop.get().vt == VT_BSTR) {
            callback(prop.get().bstrVal, -1);
        }
    }

    UInt32 num = 0;
    archive->GetNumberOfItems(&num);
    for (UInt32 i = 0; i < num; i++) {
        ScopedPropVariant prop;
        archive->GetProperty(i, kpidSize, prop.Receive());
        int size = prop.get().intVal;
        if (prop.get().vt == VT_BSTR) {
            callback(prop.get().bstrVal, size);
        }
    }
    archive->Close();
    return true;
}

bool Archive::Extract(const std::wstring& path, const juice::Format& format, const std::wstring& root, Progress* callback) {
    if (path.empty()) return false;

    auto file = x::Open(path, true);
    if (!file) return false;

    auto archive = LoadReader(this, format);
    if (!archive) return false;

    ScopedComObject<juice::ReadFileStreamming> streamming(new juice::ReadFileStreamming(file));
    ScopedComObject<juice::ArchiveOpenning> openning(new juice::ArchiveOpenning);
    auto result = archive->Open(streamming, 0, openning);
    if (FAILED(result)) return false;

    ScopedComObject<ArchiveExtractting> extractting(new ArchiveExtractting(archive, root, callback));
    result = archive->Extract(nullptr, -1, FALSE, extractting);
    if (FAILED(result)) return false;

    archive->Close();
    return true;
}

bool Archive::Compress(const std::wstring& path, const juice::Format& format, const std::vector<x::PlatformFileInfo>& file_list, Progress* callback) {
    if (path.empty() || file_list.empty()) return false;
    auto archive = LoadEditor(this, format);
    if (!archive) return false;

    auto file = x::Open(path, false);
    if (!file) return false;

    ScopedComObject<juice::WriteFileStreamming> streamming(new juice::WriteFileStreamming(file));
    ScopedComObject<ArchiveCompressing> compressing(new ArchiveCompressing(file_list, path, callback));

    auto result = archive->UpdateItems(streamming, static_cast<UInt32>(file_list.size()), compressing);
    if (FAILED(result)) return false;
    return true;
}








}
