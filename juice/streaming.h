///////////////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2017 The Authors of ANT(http:://ant.sh) . All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
///////////////////////////////////////////////////////////////////////////////////////////

#ifndef JUICE_ARCHIVE_STREAMING_INCLUDE_H_
#define JUICE_ARCHIVE_STREAMING_INCLUDE_H_

#include <vector>

#include "7zip/Archive/IArchive.h"
#include "7zip/ICoder.h"
#include "7zip/IStream.h"
#include "7zip/IPassword.h"

#include "apis/archive.h"
#include "apis/scoped_object.h"
#include "apis/basic_util.h"

namespace juice {

class ReadFileStreamming
    : public IInStream
    , public IStreamGetSize
    , public RefCounted<ReadFileStreamming> {
public:
    explicit ReadFileStreamming(const ScopedComObject<IStream>& streaming)
        : streaming_(streaming) {}
    virtual ~ReadFileStreamming() {}

    STDMETHOD(QueryInterface)(REFIID iid, void** obj) {
        if (Query<IUnknown>(this, iid, obj)) return S_OK;
        if (Query<ISequentialInStream>(this, IID_ISequentialInStream, iid, obj)) return S_OK;
        if (Query<IInStream>(this, IID_IInStream, iid, obj)) return S_OK;
        if (Query<IStreamGetSize>(this, IID_IStreamGetSize, iid, obj)) return S_OK;
        return E_NOINTERFACE;
    }

    STDMETHOD_(ULONG, AddRef)() {
        return static_cast<ULONG>(RefCounted::AddRef());
    }

    STDMETHOD_(ULONG, Release)() {
        return static_cast<ULONG>(RefCounted::Release());
    }

    STDMETHOD(Read)(void* data, UInt32 size, UInt32* processedSize) {
        ULONG sized = 0;
        auto result = streaming_->Read(data, size, &sized);
        if (processedSize != nullptr) *processedSize = sized;
        return SUCCEEDED(result) ? S_OK : result;
    }

    STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64* newPosition) {
        LARGE_INTEGER move;
        ULARGE_INTEGER newPos;
        move.QuadPart = offset;
        auto result = streaming_->Seek(move, seekOrigin, &newPos);
        if (newPosition != nullptr) {
            *newPosition = newPos.QuadPart;
        }
        return result;
    }

    STDMETHOD(GetSize)(UInt64* size) {
        STATSTG info;
        HRESULT hr = streaming_->Stat(&info, STATFLAG_NONAME);
        if (SUCCEEDED(hr)) {
            *size = info.cbSize.QuadPart;
        }
        return hr;
    }

private:
    ScopedComObject<IStream> streaming_;
};

class WriteFileStreamming
    : public IOutStream
    , public RefCounted<WriteFileStreamming> {
public:
    explicit WriteFileStreamming(const ScopedComObject<IStream>& streaming)
        : streaming_(streaming) {}
    virtual ~WriteFileStreamming() {}

    STDMETHOD(QueryInterface)(REFIID iid, void** obj) {
        if (Query<IUnknown>(this, iid, obj)) return S_OK;
        if (Query<ISequentialOutStream>(this, IID_ISequentialOutStream, iid, obj)) return S_OK;
        if (Query<IOutStream>(this, IID_IOutStream, iid, obj)) return S_OK;
        return E_NOINTERFACE;
    }

    STDMETHOD_(ULONG, AddRef)() {
        return static_cast<ULONG>(RefCounted::AddRef());
    }

    STDMETHOD_(ULONG, Release)() {
        return static_cast<ULONG>(RefCounted::Release());
    }

    STDMETHOD(Write)(const void* data, UInt32 size, UInt32* processedSize) {
        ULONG sized = 0;
        auto result = streaming_->Write(data, size, &sized);
        if (processedSize != nullptr) *processedSize = sized;
        return SUCCEEDED(result) ? S_OK : result;
    }

    STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64* newPosition) {
        LARGE_INTEGER move;
        ULARGE_INTEGER newPos;
        move.QuadPart = offset;
        auto result = streaming_->Seek(move, seekOrigin, &newPos);
        if (newPosition != nullptr) {
            *newPosition = newPos.QuadPart;
        }
        return result;
    }

    STDMETHOD(SetSize)(UInt64 newSize) {
        ULARGE_INTEGER size;
        size.QuadPart = newSize;
        return streaming_->SetSize(size);
    }

private:
    ScopedComObject<IStream> streaming_;
};

class ArchiveOpenning
    : public IArchiveOpenCallback
    , public ICryptoGetTextPassword
    , public RefCounted<ArchiveOpenning> {
public:
    ArchiveOpenning() {}
    virtual ~ArchiveOpenning() {}

    STDMETHOD(QueryInterface)(REFIID iid, void** obj) override {
        if (Query<IUnknown>(this, iid, obj)) return S_OK;
        if (Query<IArchiveOpenCallback>(this, IID_IArchiveOpenCallback, iid, obj)) return S_OK;
        if (Query<ICryptoGetTextPassword>(this, IID_ICryptoGetTextPassword, iid, obj)) return S_OK;
        return E_NOINTERFACE;
    }

    STDMETHOD_(ULONG, AddRef)() {
        return static_cast<ULONG>(RefCounted::AddRef());
    }

    STDMETHOD_(ULONG, Release)() {
        return static_cast<ULONG>(RefCounted::Release());
    }

    STDMETHOD(SetTotal)(const UInt64* files, const UInt64* bytes) { return S_OK; }
    STDMETHOD(SetCompleted)(const UInt64* files, const UInt64* bytes) { return S_OK; }
    STDMETHOD(CryptoGetTextPassword)(BSTR* password) { return E_ABORT; }
};

class ArchiveExtractting
    : public IArchiveExtractCallback
    , public ICryptoGetTextPassword
    , public RefCounted<ArchiveExtractting> {
public:
    ArchiveExtractting(const ScopedComObject<IInArchive>& archive, const std::wstring& root, juice::Progress* callback)
        : archive_(archive), callback_(callback), root_(root){}
    virtual ~ArchiveExtractting() {}

    STDMETHOD(QueryInterface)(REFIID iid, void** obj) override {
        if (Query<IUnknown>(this, iid, obj)) return S_OK;
        if (Query<IArchiveExtractCallback>(this, IID_IArchiveExtractCallback, iid, obj)) return S_OK;
        if (Query<ICryptoGetTextPassword>(this, IID_ICryptoGetTextPassword, iid, obj)) return S_OK;
        return E_NOINTERFACE;
    }

    STDMETHOD_(ULONG, AddRef)() {
        return static_cast<ULONG>(RefCounted::AddRef());
    }

    STDMETHOD_(ULONG, Release)() {
        return static_cast<ULONG>(RefCounted::Release());
    }

    STDMETHOD(SetTotal)(UInt64 size) {
        if (callback_) callback_->StartProgress(file_.path, size);
        return S_OK;
    }

    STDMETHOD(SetCompleted)(const UInt64* completeValue) { return S_OK; }

    STDMETHOD(GetStream)(UInt32 index, ISequentialOutStream** outStream, Int32 askExtractMode) {
        if (askExtractMode != NArchive::NExtract::NAskMode::kExtract) return S_OK;
        ScopedPropVariant prop;
        HRESULT reslut = archive_->GetProperty(index, kpidPath, prop.Receive());
        if (SUCCEEDED(reslut)) {
            if (prop.get().vt == VT_EMPTY) {
                file_.filename = L"[Content]";
            } else if (prop.get().vt == VT_BSTR) {
                file_.filename = prop.get().bstrVal;
            }
        }
        prop.Reset();

        reslut = archive_->GetProperty(index, kpidAttrib, prop.Receive());
        if (SUCCEEDED(reslut)) {
            if (prop.get().vt == VT_EMPTY) {
                file_.attributes = 0;
            } else if (prop.get().vt == VT_UI4) {
                file_.attributes = prop.get().ulVal;
            }
        }
        prop.Reset();

        reslut = archive_->GetProperty(index, kpidIsDir, prop.Receive());
        if (SUCCEEDED(reslut)) {
            if (prop.get().vt == VT_EMPTY) {
                file_.directory = false;
            } else if (prop.get().vt == VT_BOOL) {
                file_.filename = prop.get().boolVal != VARIANT_FALSE;
            }
        }
        prop.Reset();

        reslut = archive_->GetProperty(index, kpidMTime, prop.Receive());
        if (SUCCEEDED(reslut)) {
            if (prop.get().vt == VT_EMPTY) {
                file_.last_modified.dwHighDateTime = 0;
                file_.last_modified.dwLowDateTime = 0;
            } else if (prop.get().vt == VT_FILETIME) {
                file_.last_modified = prop.get().filetime;
            }
        }
        prop.Reset();

        reslut = archive_->GetProperty(index, kpidSize, prop.Receive());
        if (SUCCEEDED(reslut)) {
            if (prop.get().vt == VT_EMPTY) {
                file_.size = 0;
            } else if (prop.get().vt == VT_UI1) {
                file_.size = prop.get().bVal;
            } else if (prop.get().vt == VT_UI2) {
                file_.size = prop.get().uiVal;
            } else if (prop.get().vt == VT_UI4) {
                file_.size = prop.get().ulVal;
            } else if (prop.get().vt == VT_UI8) {
                file_.size = prop.get().uhVal.QuadPart;
            } else {
                file_.size = 0;
            }
        }
        prop.Reset();

        file_.path = x::Append(root_, file_.filename);
        if (file_.directory) {
            x::CreatePathTree(file_.path);
            *outStream = nullptr;
            return S_OK;
        }

        auto directory = x::GetParent(file_.path);
        x::CreatePathTree(directory);

        auto file = x::Open(file_.path, false);
        if (!file) {
            file_.path = L"";
            return HRESULT_FROM_WIN32(::GetLastError());
        }

        ScopedComObject<WriteFileStreamming> streamming(new WriteFileStreamming(file));
        *outStream = streamming.Detach();
        return S_OK;
    }

    STDMETHOD(PrepareOperation)(Int32 askExtractMode) { return S_OK; }

    STDMETHOD(SetOperationResult)(Int32 resultEOperationResult) {
        if (!callback_) S_OK;
        if (file_.path.empty()) {
            callback_->Progressed(root_, 0);
            return S_OK;
        }
        callback_->Progressed(file_.path, file_.size);
        return S_OK;
    }

    // ICryptoGetTextPassword
    STDMETHOD(CryptoGetTextPassword)(BSTR* password) { return E_ABORT; }

private:
    x::PlatformFileInfo file_;
    std::wstring root_;
    ScopedComObject<IInArchive> archive_;
    juice::Progress* callback_ = nullptr;
};

class ArchiveCompressing
    : public IArchiveUpdateCallback
    , public ICryptoGetTextPassword2
    , public ICompressProgressInfo
    , public RefCounted<ArchiveCompressing> {
public:
    ArchiveCompressing(const std::vector<x::PlatformFileInfo>& files, const std::wstring& path, juice::Progress* callback)
        : callback_(callback), path_(path), file_list_(files) {}
    virtual ~ArchiveCompressing() {}

    STDMETHOD(QueryInterface)(REFIID iid, void** obj) override {
        if (Query<IUnknown>(this, iid, obj)) return S_OK;
        if (Query<IArchiveUpdateCallback>(this, IID_IArchiveUpdateCallback, iid, obj)) return S_OK;
        if (Query<ICryptoGetTextPassword2>(this, IID_ICryptoGetTextPassword2, iid, obj)) return S_OK;
        if (Query<ICompressProgressInfo>(this, IID_ICompressProgressInfo, iid, obj)) return S_OK;
        return E_NOINTERFACE;
    }

    STDMETHOD_(ULONG, AddRef)() {
        return static_cast<ULONG>(RefCounted::AddRef());
    }

    STDMETHOD_(ULONG, Release)() {
        return static_cast<ULONG>(RefCounted::Release());
    }

    STDMETHOD(SetTotal)(UInt64 size) {
        if (callback_ != nullptr) callback_->StartProgress(path_, size);
        return S_OK;
    }

    STDMETHOD(SetCompleted)(const UInt64* completeValue) {
        if (callback_ != nullptr) callback_->Progressed(path_, *completeValue);
        return S_OK;
    }

    STDMETHOD(GetUpdateItemInfo)(UInt32 index, Int32* newData, Int32* newProperties, UInt32* indexInArchive) {
        if (newData != nullptr) *newData = 1;
        if (newProperties != nullptr) *newProperties = 1;
        if (indexInArchive != nullptr) *indexInArchive = (std::numeric_limits<UInt32>::max)();
        return S_OK;
    }

    STDMETHOD(GetProperty)(UInt32 index, PROPID propID, PROPVARIANT* value) {
        ScopedVariant var;
        if (propID == kpidIsAnti) {
            var.Set(false);
            var.Release(value);
            return S_OK;
        }
        if (index > file_list_.size()) return E_INVALIDARG;
        const x::PlatformFileInfo& info = file_list_.at(index);
        switch (propID) {
        case kpidPath:		var.Set(info.path.c_str()); break;
        case kpidIsDir:		var.Set(info.directory); break;
        case kpidSize:		var.Set(info.size); break;
        //case kpidAttrib:	var.Set(info.attributes); break;
        //case kpidCTime:		var.Set(info.creation_time); break;
        //case kpidATime:		var.Set(info.last_accessed); break;
        //case kpidMTime:		var.Set(info.last_modified); break;
        default:
            return E_INVALIDARG;
            break;
        }
        var.Release(value);
        return S_OK;
    }

    STDMETHOD(GetStream)(UInt32 index, ISequentialInStream** inStream) {
        if (index > file_list_.size()) return E_INVALIDARG;
        const x::PlatformFileInfo& info = file_list_.at(index);
        if (info.directory) return S_OK;
        ScopedComObject<IStream> file = x::Open(info.path, true);
        if (!file) return HRESULT_FROM_WIN32(::GetLastError());

        ScopedComObject<ReadFileStreamming> streamming(new ReadFileStreamming(file));
        *inStream = streamming.Detach();
        return S_OK;
    }

    STDMETHOD(SetOperationResult)(Int32 operationResult) { return S_OK; }

    STDMETHOD(CryptoGetTextPassword2)(Int32* passwordIsDefined, BSTR* password) {
        *passwordIsDefined = 0;
        *password = ::SysAllocString(L"");
        return S_OK;
    }

    STDMETHOD(SetRatioInfo)(const UInt64* inSize, const UInt64* outSize) { return S_OK; }

private:
    std::vector<x::PlatformFileInfo> file_list_;
    std::wstring path_;
    juice::Progress* callback_ = nullptr;
};

} // namespace juice 

#endif  // !JUICE_ARCHIVE_STREAMING_INCLUDE_H_