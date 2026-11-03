#pragma once

#include "compat/win_types.h"

#include <cstdarg>
#include <cstdint>
#include <type_traits>
#include <utility>

typedef const char* LPCTSTR;
typedef char* LPTSTR;

struct CStringData {
    int32_t nRefs;
    int32_t nDataLength;
    int32_t nAllocLength;
};

class CString;

namespace compat {
namespace detail {

    // A CString becomes const char*; everything else is passed through after decay.
    template <class T>
    auto FormatArg(T&& value);

} // namespace detail
} // namespace compat

class CString {
public:
    CString();
    CString(const CString& other);
    CString(const char* psz);
    // MFC's overload for unsigned text, such as the engine's RESREF arrays.
    CString(const unsigned char* psz)
        : CString(reinterpret_cast<const char*>(psz))
    {
    }
    CString(const char* pch, int len);
    CString(char ch, int repeat = 1);
    ~CString();

    CString& operator=(const CString& other);
    CString& operator=(const char* psz);
    CString& operator=(const unsigned char* psz) { return *this = reinterpret_cast<const char*>(psz); }
    CString& operator=(char ch);

    operator const char*() const { return m_pchData; }
    // The later MFC spelling of the same conversion.
    const char* GetString() const { return m_pchData; }

    int GetLength() const { return Header()->nDataLength; }
    // MFC: allocated size minus the null terminator.
    int GetAllocLength() const { return Header()->nAllocLength; }
    BOOL IsEmpty() const { return Header()->nDataLength == 0; }
    void Empty();

    char GetAt(int index) const { return m_pchData[index]; }
    char operator[](int index) const { return m_pchData[index]; }
    void SetAt(int index, char ch) { m_pchData[index] = ch; }

    // Loads a string from the engine string table (compat/mfc_resource.h).
    // Returns FALSE and leaves the string unchanged for an unknown id.
    BOOL LoadStringA(UINT id);

    int Compare(const char* psz) const;
    int CompareNoCase(const char* psz) const;

    CString Mid(int first) const;
    CString Mid(int first, int count) const;
    CString Left(int count) const;
    CString Right(int count) const;

    int Find(char ch) const;
    int Find(const char* psz) const;
    int Find(char ch, int start) const;
    int Find(const char* psz, int start) const;
    int FindOneOf(const char* set) const;
    int ReverseFind(char ch) const;

    void TrimLeft();
    void TrimLeft(char ch);
    void TrimLeft(const char* set);
    void TrimRight();
    void TrimRight(char ch);
    void TrimRight(const char* set);

    void MakeUpper();
    void MakeLower();
    void MakeReverse();

    char* GetBuffer(int minLen);
    // The later MFC overload: the buffer at the current length.
    char* GetBuffer() { return GetBuffer(0); }
    void ReleaseBuffer(int newLen = -1);
    char* GetBufferSetLength(int newLen);

    CString SpanIncluding(const char* set) const;
    CString SpanExcluding(const char* set) const;

    int Remove(char ch);
    int Replace(char from, char to);
    int Replace(const char* from, const char* to);
    int Insert(int index, char ch);
    int Insert(int index, const char* psz);
    int Delete(int index, int count = 1);

    void FormatV(const char* fmt, va_list args);

    template <class... Args>
    void Format(const char* fmt, Args&&... args)
    {
        FormatPrintf(fmt, compat::detail::FormatArg(std::forward<Args>(args))...);
    }

    CString& operator+=(const CString& other);
    CString& operator+=(const char* psz);
    CString& operator+=(char ch);

private:
    void FormatPrintf(const char* fmt, ...);

    CStringData* Header() const { return reinterpret_cast<CStringData*>(m_pchData) - 1; }
    bool IsShared() const;
    void Release();
    void Init();
    void Assign(const char* pch, int len);
    void Append(const char* pch, int len);
    void AllocateExact(int len);
    void EnsureCapacity(int len);
    void ChopTo(int len);

    char* m_pchData;
};

static_assert(sizeof(CString) == sizeof(void*), "CString must hold exactly one pointer");

namespace compat {
namespace detail {

    template <class T>
    auto FormatArg(T&& value)
    {
        if constexpr (std::is_same<std::decay_t<T>, CString>::value)
            return static_cast<const char*>(value);
        else
            return static_cast<std::decay_t<T>>(std::forward<T>(value));
    }

} // namespace detail
} // namespace compat

CString operator+(const CString& a, const CString& b);
CString operator+(const CString& a, const char* b);
CString operator+(const char* a, const CString& b);
CString operator+(const CString& a, char b);
CString operator+(char a, const CString& b);

int CStringCompare(const char* a, const char* b);

inline bool operator==(const CString& a, const CString& b) { return CStringCompare(a, b) == 0; }
inline bool operator==(const CString& a, const char* b) { return CStringCompare(a, b) == 0; }
inline bool operator==(const char* a, const CString& b) { return CStringCompare(a, b) == 0; }
inline bool operator!=(const CString& a, const CString& b) { return CStringCompare(a, b) != 0; }
inline bool operator!=(const CString& a, const char* b) { return CStringCompare(a, b) != 0; }
inline bool operator!=(const char* a, const CString& b) { return CStringCompare(a, b) != 0; }
inline bool operator<(const CString& a, const CString& b) { return CStringCompare(a, b) < 0; }
inline bool operator<(const CString& a, const char* b) { return CStringCompare(a, b) < 0; }
inline bool operator<(const char* a, const CString& b) { return CStringCompare(a, b) < 0; }
inline bool operator>(const CString& a, const CString& b) { return CStringCompare(a, b) > 0; }
inline bool operator>(const CString& a, const char* b) { return CStringCompare(a, b) > 0; }
inline bool operator>(const char* a, const CString& b) { return CStringCompare(a, b) > 0; }
inline bool operator<=(const CString& a, const CString& b) { return CStringCompare(a, b) <= 0; }
inline bool operator<=(const CString& a, const char* b) { return CStringCompare(a, b) <= 0; }
inline bool operator<=(const char* a, const CString& b) { return CStringCompare(a, b) <= 0; }
inline bool operator>=(const CString& a, const CString& b) { return CStringCompare(a, b) >= 0; }
inline bool operator>=(const CString& a, const char* b) { return CStringCompare(a, b) >= 0; }
inline bool operator>=(const char* a, const CString& b) { return CStringCompare(a, b) >= 0; }
