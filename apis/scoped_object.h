/////////////////////////////////////////////////////////////////////////////////////////// 
// 
// Copyright (c) 2018 The Authors of ANT(http:://ant.sh) . All Rights Reserved. 
// Use of this source code is governed by a BSD-style license that can be 
// found in the LICENSE file. 
// 
/////////////////////////////////////////////////////////////////////////////////////////// 


#ifndef JUICE_SCOPED_OBJECT_INCLUDE_H_
#define JUICE_SCOPED_OBJECT_INCLUDE_H_

#include <algorithm>

#include <stdlib.h>
#include <unknwn.h>
#include <assert.h>
#include <Windows.h>
#include <OleAuto.h>

#include "apis/juice.h"
#include "apis/basictypes.h"

static const int32 kExChangedStep = 1;

template<typename T>
class JUICE_API RefCounted {
public:
    RefCounted() {}

    auto AddRef() const {
        auto atom = InterlockedExchangeAdd(
            reinterpret_cast<volatile LONG*>(&ref_count_),
            static_cast<LONG>(kExChangedStep));
        return atom;
    }

    auto Release() const {
        auto step = -kExChangedStep;
        auto atom = InterlockedExchangeAdd(
            reinterpret_cast<volatile LONG*>(&ref_count_),
            static_cast<LONG>(step));
        if (0 == atom) {
            DeleteInternal(static_cast<const T*>(this));
        }
        return atom;
    }

    bool HasOneRef() const { return 1 = *(&ref_count_); }

protected:
    virtual ~RefCounted() {}
    
private:
    static void DeleteInternal(const T* x) { delete x; }
    mutable int32 ref_count_ = 0;
    DISALLOW_COPY_AND_ASSIGN(RefCounted);
};

template<class Interface, class Containter>
bool Query(Containter* containter, REFIID iid, void** obj) {
    if (iid == __uuidof(Interface)) {
        *obj = reinterpret_cast<Interface*>(containter);
        containter->AddRef();
        return true;
    }
    return false;
}

template<class Interface, class Containter>
bool Query(Containter* containter, const IID& id, REFIID iid, void** obj) {
    if (iid == id) {
        *obj = reinterpret_cast<Interface*>(containter);
        containter->AddRef();
        return true;
    }
    return false;
}


template<class Interface>
class ScopedComObject {
public:
    class IUnknownMethods : public Interface {
    private:
        STDMETHOD(QueryInterface)(REFIID iid, void** object) = 0;
        STDMETHOD_(ULONG, AddRef)() = 0;
        STDMETHOD_(ULONG, Release)() = 0;
    };

    ScopedComObject() {}
    ScopedComObject(std::nullptr_t) {}

    explicit ScopedComObject(Interface* p) : ptr_(p) {
        if (ptr_)
            ptr_->AddRef();
    }

    ScopedComObject(const ScopedComObject<Interface>& p)
        : ScopedComObject(p.ptr_) {}

    virtual ~ScopedComObject() {
        if (ptr_)
            ptr_->Release();
    }

    ScopedComObject& operator=(std::nullptr_t) {
        Release();
        return *this;
    }

    template <typename U>
    ScopedComObject& operator=(U *p) {
        ScopedComObject(p).swap(*this);
        return *this;
    }

    ScopedComObject& operator=(const ScopedComObject &p) {
        if (ptr_ != p.ptr_) { ScopedComObject(p).swap(*this); }
        return *this;
    }

    template<class U>
    ScopedComObject& operator=(const ScopedComObject<U>& p) {
        ScopedComObject(other).swap(*this);
        return *this;
    }

    ScopedComObject<Interface>& operator=(Interface* p) {
        if (ptr_ != p) { ScopedComObject(p).swap(*this); }
        return *this;
    }

    IUnknownMethods* operator->() const {
        return reinterpret_cast<IUnknownMethods*>(ptr_);
    }

    operator Interface*() const { return ptr_; }

    Interface* get() const { return ptr_; }

    void Release() {
        if (ptr_ != NULL) {
            ptr_->Release();
            ptr_ = NULL;
        }
    }

    void Attach(Interface* p) {
        assert(!ptr_);
        ptr_ = p;
    }

    Interface* Detach() {
        Interface* p = ptr_;
        ptr_ = NULL;
        return p;
    }

    Interface** Receive() {
        assert(!ptr_);
        return &ptr_;
    }

    void** ReceiveVoid() {
        return reinterpret_cast<void**>(Receive());
    }

    Interface* const* QueryAdddress() const {
        assert(!ptr_);
        return &ptr_;
    }

    template <class Query>
    HRESULT QueryInterface(Query** p) {
        assert(p != NULL);
        assert(ptr_ != NULL);
        return ptr_->QueryInterface(p);
    }

    HRESULT QueryInterface(const IID& iid, void** obj) {
        assert(obj != NULL);
        assert(ptr_ != NULL);
        return ptr_->QueryInterface(iid, obj);
    }

    HRESULT QueryFrom(IUnknown* object) {
        assert(object != NULL);
        return object->QueryInterface(Receive());
    }

    HRESULT CreateInstance(const CLSID& clsid, IUnknown* outer = NULL,
        DWORD context = CLSCTX_ALL) {
        assert(!ptr_);
        HRESULT hr = ::CoCreateInstance(clsid, outer, context, *id,
            reinterpret_cast<void**>(&ptr_));
        return hr;
    }

    bool IsSameObject(IUnknown* other) {
        if (!other && !ptr_)
            return true;

        if (!other || !ptr_)
            return false;

        ScopedComObject<IUnknown> my_identity;
        QueryInterface(my_identity.Receive());

        ScopedComObject<IUnknown> other_identity;
        other->QueryInterface(other_identity.Receive());

        return static_cast<IUnknown*>(my_identity) ==
            static_cast<IUnknown*>(other_identity);
    }

    void swap(Interface** pp) {
        Interface* p = ptr_;
        ptr_ = *pp;
        *pp = p;
    }

    void swap(ScopedComObject<Interface>& r) {
        swap(&r.ptr_);
    }

protected:
    Interface* ptr_ = nullptr;

};

template <typename T, typename Traits>
class ScopedGeneric {
  private:
    // This must be first since it's used inline below.
    struct Data : public Traits {
        explicit Data(const T &in) : generic(in) {}
        Data(const T &in, const Traits &other) : Traits(other), generic(in) {}
        T generic;
    };

  public:
    typedef T element_type;
    typedef Traits traits_type;

    ScopedGeneric() : data_(traits_type::InvalidValue()) {}
    explicit ScopedGeneric(const element_type &value) : data_(value) {}
    ScopedGeneric(const element_type &value, const traits_type &traits)
        : data_(value, traits) {}
    ScopedGeneric(ScopedGeneric<T, Traits> &&rvalue)
        : data_(rvalue.release(), rvalue.get_traits()) {}

    ~ScopedGeneric() { FreeIfNecessary(); }
    ScopedGeneric &operator=(ScopedGeneric<T, Traits> &&rvalue) {
        reset(rvalue.release());
        return *this;
    }
    void reset(const element_type &value = traits_type::InvalidValue()) {
        if (data_.generic != traits_type::InvalidValue() && data_.generic == value)
            abort();
        FreeIfNecessary();
        data_.generic = value;
    }
    void swap(ScopedGeneric &other) {
        if (&other == this) return;
        std::swap(static_cast<Traits &>(data_), static_cast<Traits &>(other.data_));
        std::swap(data_.generic, other.data_.generic);
    }

    element_type release() WARN_UNUSED_RESULT {
        element_type old_generic = data_.generic;
        data_.generic = traits_type::InvalidValue();
        return old_generic;
    }
    class Receiver {
      public:
        explicit Receiver(ScopedGeneric &parent) : scoped_generic_(&parent) {
            scoped_generic_->receiving_ = true;
        }

        ~Receiver() {
            if (scoped_generic_)  {
                scoped_generic_->reset(value_);
                scoped_generic_->receiving_ = false;
            }
        }

        Receiver(Receiver &&move) {
            scoped_generic_ = move.scoped_generic_;
            move.scoped_generic_ = nullptr;
        }

        Receiver &operator=(Receiver &&move) {
            scoped_generic_ = move.scoped_generic_;
            move.scoped_generic_ = nullptr;
        }

        // We hand out a pointer to a field in Receiver instead of directly to
        // ScopedGeneric's internal storage in order to make it so that users can't
        // accidentally silently break ScopedGeneric's invariants. This way, an
        // incorrect use-after-scope-exit is more detectable by ASan or static
        // analysis tools, as the pointer is only valid for the lifetime of the
        // Receiver, not the ScopedGeneric.
        T *get() {
            used_ = true;
            return &value_;
        }

      private:
        T value_ = Traits::InvalidValue();
        ScopedGeneric *scoped_generic_;
        bool used_ = false;
        DISALLOW_COPY_AND_ASSIGN(Receiver);
    };
    const element_type &get() const { return data_.generic; }
    bool is_valid() const { return data_.generic != traits_type::InvalidValue(); }
    bool operator==(const element_type &value) const { return data_.generic == value; }
    bool operator!=(const element_type &value) const { return data_.generic != value; }
    Traits &get_traits() { return data_; }
    const Traits &get_traits() const { return data_; }

  private:
    void FreeIfNecessary() {
        if (data_.generic != traits_type::InvalidValue()) {
            data_.Free(data_.generic);
            data_.generic = traits_type::InvalidValue();
        }
    }

    template <typename T2, typename Traits2>
    bool operator==(
        const ScopedGeneric<T2, Traits2> &p2) const;
    template <typename T2, typename Traits2>
    bool operator!=(
        const ScopedGeneric<T2, Traits2> &p2) const;

    Data data_;
    bool receiving_ = false;

    DISALLOW_COPY_AND_ASSIGN(ScopedGeneric);
};

template <class T, class Traits>
void swap(const ScopedGeneric<T, Traits> &a, const ScopedGeneric<T, Traits> &b) {
    a.swap(b);
}

template <class T, class Traits>
bool operator==(const T &value, const ScopedGeneric<T, Traits> &scoped) {
    return value == scoped.get();
}

template <class T, class Traits>
bool operator!=(const T &value, const ScopedGeneric<T, Traits> &scoped) {
    return value != scoped.get();
}

class JUICE_API ScopedVariant {
 public:
    // Declaration of a global variant variable that's always VT_EMPTY
    static const VARIANT kEmptyVariant;

    ScopedVariant() { var_.vt = VT_EMPTY; }
    explicit ScopedVariant(const wchar_t* str) { var_.vt = VT_EMPTY; Set(str); }

    // Creates a new VT_BSTR variant of a specified length.
    ScopedVariant(const wchar_t* str, UINT length) {
        var_.vt = VT_BSTR;
        var_.bstrVal = ::SysAllocStringLen(str, length);
    }

    // Creates a new integral type variant and assigns the value to
    // VARIANT.lVal (32 bit sized field).
    explicit ScopedVariant(int value, VARTYPE vt = VT_I4) {
        var_.vt = vt;
        var_.lVal = value;
    }

    // Creates a new double-precision type variant.  |vt| must be either VT_R8
    // or VT_DATE.
    explicit ScopedVariant(double value, VARTYPE vt = VT_R8) {
        assert(vt == VT_R8 || vt == VT_DATE);
        var_.vt = vt;
        var_.dblVal = value;
    }

    // VT_DISPATCH
    explicit ScopedVariant(IDispatch* dispatch) {
        var_.vt = VT_EMPTY;
        Set(dispatch);
    }

    // VT_UNKNOWN
    explicit ScopedVariant(IUnknown* unknown) {
        var_.vt = VT_EMPTY;
        Set(unknown);
    }

    // SAFEARRAY
    explicit ScopedVariant(SAFEARRAY* safearray) {
        var_.vt = VT_EMPTY;
        Set(safearray);
    }

    // Copies the variant.
    explicit ScopedVariant(const VARIANT& var) {
        var_.vt = VT_EMPTY;
        Set(var);
    }

    virtual ~ScopedVariant() {
      ::VariantClear(&var_);
    }

    inline VARTYPE type() const { return var_.vt; }

    // Give ScopedVariant ownership over an already allocated VARIANT.
    void Reset(const VARIANT& var = kEmptyVariant) {
      if (&var_ != &var) {
        ::VariantClear(&var_);
        var_ = var;
      }
    }

    // Releases ownership of the VARIANT to the caller.
    VARIANT Release() {
        VARIANT var = var_;
        var_.vt = VT_EMPTY;
        return var;
    }

    HRESULT Release(PROPVARIANT* var) {
        if (IsLeakableVarType(var->vt)) {
            auto result = ::PropVariantClear(var);
            if (FAILED(result))  return result;
        } else {
            var->vt = VT_EMPTY;
            var->wReserved1 = 0;
        }
        std::memcpy(var, &var_, sizeof(PROPVARIANT));
        return S_OK;
    }

    // Swap two ScopedVariant's.
    void Swap(ScopedVariant& var) {
      VARIANT tmp = var_;
      var_ = var.var_;
      var.var_ = tmp;
    }

    // Returns a copy of the variant.
    VARIANT Copy() const {
      VARIANT ret = {{{VT_EMPTY}}};
      ::VariantCopy(&ret, &var_);
      return ret;  
    }

    // The return value is 0 if the variants are equal, 1 if this object is
    // greater than |var|, -1 if it is smaller.
    int Compare(const VARIANT& var, bool ignore_case = false) const {
      ULONG flags = ignore_case ? NORM_IGNORECASE : 0;
      HRESULT hr = ::VarCmp(const_cast<VARIANT*>(&var_), const_cast<VARIANT*>(&var), LOCALE_USER_DEFAULT, flags);
      int ret = 0;
      switch (hr) {
        case VARCMP_LT:
          ret = -1;
          break;
        case VARCMP_GT:
        case VARCMP_NULL:
          ret = 1;
          break;
        default:
          // Equal.
          break;
      }
      return ret;
    }

    // Retrieves the pointer address.
    // Used to receive a VARIANT as an out argument (and take ownership).
    // The function DCHECKs on the current value being empty/null.
    // Usage: GetVariant(var.receive());
    VARIANT* Receive() {
      assert(!IsLeakableVarType(var_.vt));
      return &var_;
    }

    void Set(const wchar_t* str) {
      assert(!IsLeakableVarType(var_.vt));
      var_.vt = VT_BSTR;
      var_.bstrVal = ::SysAllocString(str);
    }

    // Setters for simple types.
    void Set(int8_t i8) {
      assert(!IsLeakableVarType(var_.vt));
      var_.vt = VT_I1;
      var_.cVal = i8;
    }
    void Set(uint8_t ui8) {
      assert(!IsLeakableVarType(var_.vt));
      var_.vt = VT_UI1;
      var_.bVal = ui8;
    }
    void Set(int16_t i16) {
      assert(!IsLeakableVarType(var_.vt));
      var_.vt = VT_I2;
      var_.iVal = i16;
    }
    void Set(uint16_t ui16) {
      assert(!IsLeakableVarType(var_.vt));
      var_.vt = VT_UI2;
      var_.uiVal = ui16;
    }
    void Set(int32_t i32) {
      assert(!IsLeakableVarType(var_.vt));
      var_.vt = VT_I4;
      var_.lVal = i32;
    }
    void Set(uint32_t ui32) {
      assert(!IsLeakableVarType(var_.vt));
      var_.vt = VT_UI4;
      var_.ulVal = ui32;
    }
    void Set(int64_t i64) {
      assert(!IsLeakableVarType(var_.vt));
      var_.vt = VT_I8;
      var_.llVal = i64;
    }
    void Set(uint64_t ui64) {
      assert(!IsLeakableVarType(var_.vt));
      var_.vt = VT_UI8;
      var_.ullVal = ui64;
    }
    void Set(float r32) {
      assert(!IsLeakableVarType(var_.vt));
      var_.vt = VT_R4;
      var_.fltVal = r32;
    }
    void Set(double r64) {
      assert(!IsLeakableVarType(var_.vt));
      var_.vt = VT_R8;
      var_.dblVal = r64;
    }
    void Set(bool b) {
      assert(!IsLeakableVarType(var_.vt));
      var_.vt = VT_BOOL;
      var_.boolVal = b ? VARIANT_TRUE : VARIANT_FALSE;
    }

    // Creates a copy of |var| and assigns as this instance's value.
    // Note that this is different from the Reset() method that's used to
    // free the current value and assume ownership.
    void Set(const VARIANT& var) {
      assert(!IsLeakableVarType(var_.vt));
      if (FAILED(::VariantCopy(&var_, &var))) {
        var_.vt = VT_EMPTY;
      }
    }

    // COM object setters
    void Set(IDispatch* disp) {
      assert(!IsLeakableVarType(var_.vt));
      var_.vt = VT_DISPATCH;
      var_.pdispVal = disp;
      if (disp)
        disp->AddRef();
    }
    void Set(IUnknown* unk) {
      assert(!IsLeakableVarType(var_.vt));
      var_.vt = VT_UNKNOWN;
      var_.punkVal = unk;
      if (unk)
        unk->AddRef();
    }

    // SAFEARRAY support
    void Set(SAFEARRAY* array) {
      assert(!IsLeakableVarType(var_.vt));
      if (SUCCEEDED(::SafeArrayGetVartype(array, &var_.vt))) {
        var_.vt |= VT_ARRAY;
        var_.parray = array;
      } else {
        var_.vt = VT_EMPTY;
      }
    }

    // Special setter for DATE since DATE is a double and we already have
    // a setter for double.
    void SetDate(DATE date) {
      assert(!IsLeakableVarType(var_.vt));
      var_.vt = VT_DATE;
      var_.date = date;
    }

    // Allows const access to the contained variant without DCHECKs etc.
    // This support is necessary for the V_XYZ (e.g. V_BSTR) set of macros to
    // work properly but still doesn't allow modifications since we want control
    // over that.
    const VARIANT* ptr() const { return &var_; }

    // Like other scoped classes (e.g. scoped_refptr, ScopedBstr,
    // Microsoft::WRL::ComPtr) we support the assignment operator for the type we
    // wrap.
    ScopedVariant& operator=(const VARIANT& var) {
      if (&var != &var_) {
        VariantClear(&var_);
        Set(var);
      }
      return *this;
    }

    // A hack to pass a pointer to the variant where the accepting
    // function treats the variant as an input-only, read-only value
    // but the function prototype requires a non const variant pointer.
    // There's no DCHECK or anything here.  Callers must know what they're doing.
    //
    // The nature of this function is const, so we declare
    // it as such and cast away the constness here.
    VARIANT* AsInput() const { return const_cast<VARIANT*>(&var_); }

    // Allows the ScopedVariant instance to be passed to functions either by value
    // or by const reference.
    operator const VARIANT&() const { return var_; }

    // Used as a debug check to see if we're leaking anything.
    static bool IsLeakableVarType(VARTYPE vt) {
      bool leakable = false;
      switch (vt & VT_TYPEMASK) {
        case VT_BSTR:
        case VT_DISPATCH:
        case VT_VARIANT:
        case VT_UNKNOWN:
        case VT_SAFEARRAY:
        case VT_VOID:
        case VT_PTR:
        case VT_CARRAY:
        case VT_USERDEFINED:
        case VT_LPSTR:
        case VT_LPWSTR:
        case VT_RECORD:
        case VT_INT_PTR:
        case VT_UINT_PTR:
        case VT_FILETIME:
        case VT_BLOB:
        case VT_STREAM:
        case VT_STORAGE:
        case VT_STREAMED_OBJECT:
        case VT_STORED_OBJECT:
        case VT_BLOB_OBJECT:
        case VT_VERSIONED_STREAM:
        case VT_BSTR_BLOB:
          leakable = true;
          break;
      }

      if (!leakable && (vt & VT_ARRAY) != 0) {
        leakable = true;
      }

      return leakable;
    }

protected:
    VARIANT var_;

private:
    // Comparison operators for ScopedVariant are not supported at this point.
    // Use the Compare method instead.
    bool operator==(const ScopedVariant& var) const;
    bool operator!=(const ScopedVariant& var) const;
    DISALLOW_COPY_AND_ASSIGN(ScopedVariant);
};

X_ABSL_COMDAT const VARIANT ScopedVariant::kEmptyVariant = {{{VT_EMPTY}}};




class JUICE_API ScopedPropVariant {
public:
    ScopedPropVariant() { PropVariantInit(&pv_); }

    virtual ~ScopedPropVariant() { Reset(); }

    // Returns a pointer to the underlying PROPVARIANT for use as an out param in
    // a function call.
    PROPVARIANT* Receive() {
        assert(pv_.vt == VT_EMPTY);
        return &pv_;
    }

    // Clears the instance to prepare it for re-use (e.g., via Receive).
    void Reset() {
        if (pv_.vt != VT_EMPTY) {
            HRESULT result = PropVariantClear(&pv_);
            assert(result == S_OK);
        }
    }
    const PROPVARIANT& get() const { return pv_; }
    const PROPVARIANT* ptr() const { return &pv_; }

private:
    PROPVARIANT pv_;

    // Comparison operators for ScopedPropVariant are not supported at this point.
    bool operator==(const ScopedPropVariant&) const;
    bool operator!=(const ScopedPropVariant&) const;
    DISALLOW_COPY_AND_ASSIGN(ScopedPropVariant);
};





#endif // !#define (JUICE_SCOPED_OBJECT_INCLUDE_H_ )