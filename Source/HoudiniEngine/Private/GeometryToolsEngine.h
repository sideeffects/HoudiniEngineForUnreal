
#pragma once

#include "CoreMinimal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

// ----------------------------------------------
// ProgressCancel.h
// ----------------------------------------------

// Copyright Epic Games, Inc. All Rights Reserved.

// Port of geometry3cpp ProgressCancel

/**
 * FHEProgressCancel is an obejct that is intended to be passed to long-running
 * computes to do two things:
 * 1) provide progress info back to caller (not implemented yet)
 * 2) allow caller to cancel the computation
 */
class FHEProgressCancel
{
private:
	bool WasCancelled = false;  // will be set to true if CancelF() ever returns true

public:
	TFunction<bool()> CancelF = []() { return false; };

	/**
	 * @return true if client would like to cancel operation
	 */
	bool Cancelled()
	{
		if (WasCancelled)
		{
			return true;
		}
		WasCancelled = CancelF();
		return WasCancelled;
	}


public:

	enum class EMessageLevel
	{
		// Note: Corresponds to EToolMessageLevel in InteractiveToolsFramework/ToolContextInterfaces.h

		/** Development message goes into development log */
		Internal = 0,
		/** User message should appear in user-facing log */
		UserMessage = 1,
		/** Notification message should be shown in a non-modal notification window */
		UserNotification = 2,
		/** Warning message should be shown in a non-modal notification window with panache */
		UserWarning = 3,
		/** Error message should be shown in a modal notification window */
		UserError = 4
	};

	struct FMessageInfo
	{
		FText MessageText;
		EMessageLevel MessageLevel;
		FDateTime Timestamp;
	};

	void AddWarning(const FText& MessageText, EMessageLevel MessageLevel)
	{
		Warnings.Add(FMessageInfo{ MessageText , MessageLevel, FDateTime::Now() });
	}

	TArray<FMessageInfo> Warnings;
};

// ----------------------------------------------
// GTEngineDEF.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

//----------------------------------------------------------------------------
// The platform specification.
//
// __MSWINDOWS__            :  Microsoft Windows (WIN32 or WIN64)
// __APPLE__                :  Macintosh OS X
// __LINUX__                :  Linux or Cygwin
//----------------------------------------------------------------------------

#if !defined(__LINUX__) && (defined(WIN32) || defined(_WIN64))
#define __MSWINDOWS__

#if !defined(_MSC_VER)
#error Microsoft Visual Studio 2013 or later is required.
#endif

//  MSVC  6   is version 12.0
//  MSVC  7.0 is version 13.0 (MSVS 2002)
//  MSVC  7.1 is version 13.1 (MSVS 2003)
//  MSVC  8.0 is version 14.0 (MSVS 2005)
//  MSVC  9.0 is version 15.0 (MSVS 2008)
//  MSVC 10.0 is version 16.0 (MSVS 2010)
//  MSVC 11.0 is version 17.0 (MSVS 2012)
//  MSVC 12.0 is version 18.0 (MSVS 2013)
//  MSVC 14.0 is version 19.0 (MSVS 2015)
//  Currently, projects are provided only for MSVC 12.0 and 14.0.
#if _MSC_VER < 1800
#error Microsoft Visual Studio 2013 or later is required.
#endif

// Debug build values (choose_your_value is 0, 1, or 2)
// 0:  Disables checked iterators and disables iterator debugging.
// 1:  Enables checked iterators and disables iterator debugging.
// 2:  (default) Enables iterator debugging; checked iterators are not relevant.
//
// Release build values (choose_your_value is 0 or 1)
// 0:  (default) Disables checked iterators.
// 1:  Enables checked iterators; iterator debugging is not relevant.
//
// #define _ITERATOR_DEBUG_LEVEL choose_your_value

#endif  // WIN32 or _WIN64

// TODO: Windows DLL configurations have not yet been added to the project,
// but these defines are required to support them (when we do add them).
//
// Add GTE_EXPORT to project preprocessor options for dynamic library
// configurations to export their symbols.
#if defined(GTE_EXPORT)
    // For the dynamic library configurations.
    #define GTE_IMPEXP __declspec(dllexport)
#else
    // For a client of the dynamic library or for the static library
    // configurations.
    #define GTE_IMPEXP
#endif

// Expose exactly one of these.
#define GTE_USE_ROW_MAJOR
//#define GTE_USE_COL_MAJOR

// Expose exactly one of these.
#define GTE_USE_MAT_VEC
//#define GTE_USE_VEC_MAT

#if (defined(GTE_USE_ROW_MAJOR) && defined(GTE_USE_COL_MAJOR)) || (!defined(GTE_USE_ROW_MAJOR) && !defined(GTE_USE_COL_MAJOR))
#error Exactly one storage order must be specified.
#endif

#if (defined(GTE_USE_MAT_VEC) && defined(GTE_USE_VEC_MAT)) || (!defined(GTE_USE_MAT_VEC) && !defined(GTE_USE_VEC_MAT))
#error Exactly one multiplication convention must be specified.
#endif

// ----------------------------------------------
// GteLogger.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

// Uncomment this to turn off the logging system.  The macros LogAssert,
// LogError, LogWarning, and LogInformation expand to nothing.  (Do this for
// optimal performance.)
//#define GTE_NO_LOGGER

namespace gte
{

class GTE_IMPEXP Logger
{
public:
    // Construction.  The Logger object is designed to exist only for a
    // single-line call.  A string is generated from the input parameters and
    // is used for reporting.
    Logger(char const* file, char const* function, int line,
        std::string const& message);

    // Notify current listeners about the logged information.
    void Assertion();
    void Error();
    void Warning();
    void Information();

    // Listeners subscribe to Logger to receive message strings.
    class Listener
    {
    public:
        enum
        {
            LISTEN_FOR_NOTHING      = 0x00000000,
            LISTEN_FOR_ASSERTION    = 0x00000001,
            LISTEN_FOR_ERROR        = 0x00000002,
            LISTEN_FOR_WARNING      = 0x00000004,
            LISTEN_FOR_INFORMATION  = 0x00000008,
            LISTEN_FOR_ALL          = 0xFFFFFFFF
        };

        // Construction and destruction.
        virtual ~Listener();
        Listener(int flags = LISTEN_FOR_NOTHING);

        // What the listener wants to hear.
        int GetFlags() const;

        // Handlers for the messages received from the logger.
        void Assertion(std::string const& message);
        void Error(std::string const& message);
        void Warning(std::string const& message);
        void Information(std::string const& message);

    private:
        virtual void Report(std::string const& message);

        int mFlags;
    };

    static void Subscribe(Listener* listener);
    static void Unsubscribe(Listener* listener);

private:
    std::string mMessage;

    static std::mutex msMutex;
    static std::set<Listener*> msListeners;
};

}


#if !defined(GTE_NO_LOGGER)

#define LogAssert(condition, message) \
    if (!(condition)) \
    { \
        gte::Logger(__FILE__, __FUNCTION__, __LINE__, message).Assertion(); \
    }

#define LogError(message) \
    gte::Logger(__FILE__, __FUNCTION__, __LINE__, message).Error()

#define LogWarning(message) \
    gte::Logger(__FILE__, __FUNCTION__, __LINE__, message).Warning()

#define LogInformation(message) \
    gte::Logger(__FILE__, __FUNCTION__, __LINE__, message).Information()

#else

// No logging of assertions, warnings, errors, or information.
#define LogAssert(condition, message)
#define LogError(message)
#define LogWarning(message)
#define LogInformation(message)

#endif



// ----------------------------------------------
// GteVector.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.3 (2018/10/05)

namespace gte
{

template <int N, typename Real>
class Vector
{
public:
    // The tuple is uninitialized.
    Vector();

    // The tuple is fully initialized by the inputs.
    Vector(std::array<Real, N> const& values);

    // At most N elements are copied from the initializer list, setting any
    // remaining elements to zero.  Create the zero vector using the syntax
    //   Vector<N,Real> zero{(Real)0};
    // WARNING:  The C++ 11 specification states that
    //   Vector<N,Real> zero{};
    // will lead to a call of the default constructor, not the initializer
    // constructor!
    Vector(std::initializer_list<Real> values);

    // For 0 <= d < N, element d is 1 and all others are 0.  If d is invalid,
    // the zero vector is created.  This is a convenience for creating the
    // standard Euclidean basis vectors; see also MakeUnit(int) and Unit(int).
    Vector(int d);

    // The copy constructor, destructor, and assignment operator are generated
    // by the compiler.

    // Member access.  The first operator[] returns a const reference rather
    // than a Real value.  This supports writing via standard file operations
    // that require a const pointer to data.
    inline int GetSize() const;  // returns N
    inline Real const& operator[](int i) const;
    inline Real& operator[](int i);

    // Comparisons for sorted containers and geometric ordering.
    inline bool operator==(Vector const& vec) const;
    inline bool operator!=(Vector const& vec) const;
    inline bool operator< (Vector const& vec) const;
    inline bool operator<=(Vector const& vec) const;
    inline bool operator> (Vector const& vec) const;
    inline bool operator>=(Vector const& vec) const;

    // Special vectors.
    void MakeZero();  // All components are 0.
    void MakeUnit(int d);  // Component d is 1, all others are zero.
    static Vector Zero();
    static Vector Unit(int d);

protected:
    // This data structure takes advantage of the built-in operator[],
    // range checking, and visualizers in MSVS.
    std::array<Real, N> mTuple;
};

// Unary operations.
template <int N, typename Real>
Vector<N, Real> operator+(Vector<N, Real> const& v);

template <int N, typename Real>
Vector<N, Real> operator-(Vector<N, Real> const& v);

// Linear-algebraic operations.
template <int N, typename Real>
Vector<N, Real>
operator+(Vector<N, Real> const& v0, Vector<N, Real> const& v1);

template <int N, typename Real>
Vector<N, Real>
operator-(Vector<N, Real> const& v0, Vector<N, Real> const& v1);

template <int N, typename Real>
Vector<N, Real> operator*(Vector<N, Real> const& v, Real scalar);

template <int N, typename Real>
Vector<N, Real> operator*(Real scalar, Vector<N, Real> const& v);

template <int N, typename Real>
Vector<N, Real> operator/(Vector<N, Real> const& v, Real scalar);

template <int N, typename Real>
Vector<N, Real>& operator+=(Vector<N, Real>& v0, Vector<N, Real> const& v1);

template <int N, typename Real>
Vector<N, Real>& operator-=(Vector<N, Real>& v0, Vector<N, Real> const& v1);

template <int N, typename Real>
Vector<N, Real>& operator*=(Vector<N, Real>& v, Real scalar);

template <int N, typename Real>
Vector<N, Real>& operator/=(Vector<N, Real>& v, Real scalar);

// Componentwise algebraic operations.
template <int N, typename Real>
Vector<N, Real>
operator*(Vector<N, Real> const& v0, Vector<N, Real> const& v1);

template <int N, typename Real>
Vector<N, Real>
operator/(Vector<N, Real> const& v0, Vector<N, Real> const& v1);

template <int N, typename Real>
Vector<N, Real>& operator*=(Vector<N, Real>& v0, Vector<N, Real> const& v1);

template <int N, typename Real>
Vector<N, Real>& operator/=(Vector<N, Real>& v0, Vector<N, Real> const& v1);

// Geometric operations.  The functions with 'robust' set to 'false' use the
// standard algorithm for normalizing a vector by computing the length as a
// square root of the squared length and dividing by it.  The results can be
// infinite (or NaN) if the length is zero.  When 'robust' is set to 'true',
// the algorithm is designed to avoid floating-point overflow and sets the
// normalized vector to zero when the length is zero.
template <int N, typename Real>
Real Dot(Vector<N, Real> const& v0, Vector<N, Real> const& v1);

template <int N, typename Real>
Real Length(Vector<N, Real> const& v, bool robust = false);

template <int N, typename Real>
Real Normalize(Vector<N, Real>& v, bool robust = false);

// Gram-Schmidt orthonormalization to generate orthonormal vectors from the
// linearly independent inputs.  The function returns the smallest length of
// the unnormalized vectors computed during the process.  If this value is
// nearly zero, it is possible that the inputs are linearly dependent (within
// numerical round-off errors).  On input, 1 <= numElements <= N and v[0]
// through v[numElements-1] must be initialized.  On output, the vectors
// v[0] through v[numElements-1] form an orthonormal set.
template <int N, typename Real>
Real Orthonormalize(int numElements, Vector<N, Real>* v, bool robust = false);

// Construct a single vector orthogonal to the nonzero input vector.  If the
// maximum absolute component occurs at index i, then the orthogonal vector
// U has u[i] = v[i+1], u[i+1] = -v[i], and all other components zero.  The
// index addition i+1 is computed modulo N.
template <int N, typename Real>
Vector<N, Real> GetOrthogonal(Vector<N, Real> const& v, bool unitLength);

// Compute the axis-aligned bounding box of the vectors.  The return value is
// 'true' iff the inputs are valid, in which case vmin and vmax have valid
// values.
template <int N, typename Real>
bool ComputeExtremes(int numVectors, Vector<N, Real> const* v,
    Vector<N, Real>& vmin, Vector<N, Real>& vmax);

// Lift n-tuple v to homogeneous (n+1)-tuple (v,last).
template <int N, typename Real>
Vector<N + 1, Real> HLift(Vector<N, Real> const& v, Real last);

// Project homogeneous n-tuple v = (u,v[n-1]) to (n-1)-tuple u.
template <int N, typename Real>
Vector<N - 1, Real> HProject(Vector<N, Real> const& v);

// Lift n-tuple v = (w0,w1) to (n+1)-tuple u = (w0,u[inject],w1).  By
// inference, w0 is a (inject)-tuple [nonexistent when inject=0] and w1 is a
// (n-inject)-tuple [nonexistent when inject=n].
template <int N, typename Real>
Vector<N + 1, Real> Lift(Vector<N, Real> const& v, int inject, Real value);

// Project n-tuple v = (w0,v[reject],w1) to (n-1)-tuple u = (w0,w1).  By
// inference, w0 is a (reject)-tuple [nonexistent when reject=0] and w1 is a
// (n-1-reject)-tuple [nonexistent when reject=n-1].
template <int N, typename Real>
Vector<N - 1, Real> Project(Vector<N, Real> const& v, int reject);


template <int N, typename Real>
Vector<N, Real>::Vector()
{
    // Uninitialized.
}

template <int N, typename Real>
Vector<N, Real>::Vector(std::array<Real, N> const& values)
    :
    mTuple(values)
{
}
template <int N, typename Real>
Vector<N, Real>::Vector(std::initializer_list<Real> values)
{
    int const numValues = static_cast<int>(values.size());
    if (N == numValues)
    {
        std::copy(values.begin(), values.end(), mTuple.begin());
    }
    else if (N > numValues)
    {
        std::copy(values.begin(), values.end(), mTuple.begin());
        std::fill(mTuple.begin() + numValues, mTuple.end(), (Real)0);
    }
    else // N < numValues
    {
        std::copy(values.begin(), values.begin() + N, mTuple.begin());
    }
}

template <int N, typename Real>
Vector<N, Real>::Vector(int d)
{
    MakeUnit(d);
}

template <int N, typename Real> inline
int Vector<N, Real>::GetSize() const
{
    return N;
}

template <int N, typename Real> inline
Real const& Vector<N, Real>::operator[](int i) const
{
    return mTuple[i];
}

template <int N, typename Real> inline
Real& Vector<N, Real>::operator[](int i)
{
    return mTuple[i];
}

template <int N, typename Real> inline
bool Vector<N, Real>::operator==(Vector const& vec) const
{
    return mTuple == vec.mTuple;
}

template <int N, typename Real> inline
bool Vector<N, Real>::operator!=(Vector const& vec) const
{
    return mTuple != vec.mTuple;
}

template <int N, typename Real> inline
bool Vector<N, Real>::operator<(Vector const& vec) const
{
    return mTuple < vec.mTuple;
}

template <int N, typename Real> inline
bool Vector<N, Real>::operator<=(Vector const& vec) const
{
    return mTuple <= vec.mTuple;
}

template <int N, typename Real> inline
bool Vector<N, Real>::operator>(Vector const& vec) const
{
    return mTuple > vec.mTuple;
}

template <int N, typename Real> inline
bool Vector<N, Real>::operator>=(Vector const& vec) const
{
    return mTuple >= vec.mTuple;
}

template <int N, typename Real>
void Vector<N, Real>::MakeZero()
{
    std::fill(mTuple.begin(), mTuple.end(), (Real)0);
}

template <int N, typename Real>
void Vector<N, Real>::MakeUnit(int d)
{
    std::fill(mTuple.begin(), mTuple.end(), (Real)0);
    if (0 <= d && d < N)
    {
        mTuple[d] = (Real)1;
    }
}

template <int N, typename Real>
Vector<N, Real> Vector<N, Real>::Zero()
{
    Vector<N, Real> v;
    v.MakeZero();
    return v;
}

template <int N, typename Real>
Vector<N, Real> Vector<N, Real>::Unit(int d)
{
    Vector<N, Real> v;
    v.MakeUnit(d);
    return v;
}



template <int N, typename Real>
Vector<N, Real> operator+(Vector<N, Real> const& v)
{
    return v;
}

template <int N, typename Real>
Vector<N, Real> operator-(Vector<N, Real> const& v)
{
    Vector<N, Real> result;
    for (int i = 0; i < N; ++i)
    {
        result[i] = -v[i];
    }
    return result;
}

template <int N, typename Real>
Vector<N, Real> operator+(Vector<N, Real> const& v0,
    Vector<N, Real> const& v1)
{
    Vector<N, Real> result = v0;
    return result += v1;
}

template <int N, typename Real>
Vector<N, Real> operator-(Vector<N, Real> const& v0,
    Vector<N, Real> const& v1)
{
    Vector<N, Real> result = v0;
    return result -= v1;
}

template <int N, typename Real>
Vector<N, Real> operator*(Vector<N, Real> const& v, Real scalar)
{
    Vector<N, Real> result = v;
    return result *= scalar;
}

template <int N, typename Real>
Vector<N, Real> operator*(Real scalar, Vector<N, Real> const& v)
{
    Vector<N, Real> result = v;
    return result *= scalar;
}

template <int N, typename Real>
Vector<N, Real> operator/(Vector<N, Real> const& v, Real scalar)
{
    Vector<N, Real> result = v;
    return result /= scalar;
}

template <int N, typename Real>
Vector<N, Real>& operator+=(Vector<N, Real>& v0, Vector<N, Real> const& v1)
{
    for (int i = 0; i < N; ++i)
    {
        v0[i] += v1[i];
    }
    return v0;
}

template <int N, typename Real>
Vector<N, Real>& operator-=(Vector<N, Real>& v0, Vector<N, Real> const& v1)
{
    for (int i = 0; i < N; ++i)
    {
        v0[i] -= v1[i];
    }
    return v0;
}

template <int N, typename Real>
Vector<N, Real>& operator*=(Vector<N, Real>& v, Real scalar)
{
    for (int i = 0; i < N; ++i)
    {
        v[i] *= scalar;
    }
    return v;
}

template <int N, typename Real>
Vector<N, Real>& operator/=(Vector<N, Real>& v, Real scalar)
{
    if (scalar != (Real)0)
    {
        Real invScalar = ((Real)1) / scalar;
        for (int i = 0; i < N; ++i)
        {
            v[i] *= invScalar;
        }
    }
    else
    {
        for (int i = 0; i < N; ++i)
        {
            v[i] = (Real)0;
        }
    }
    return v;
}

template <int N, typename Real>
Vector<N, Real> operator*(Vector<N, Real> const& v0,
    Vector<N, Real> const& v1)
{
    Vector<N, Real> result = v0;
    return result *= v1;
}

template <int N, typename Real>
Vector<N, Real> operator/(Vector<N, Real> const& v0,
    Vector<N, Real> const& v1)
{
    Vector<N, Real> result = v0;
    return result /= v1;
}

template <int N, typename Real>
Vector<N, Real>& operator*=(Vector<N, Real>& v0, Vector<N, Real> const& v1)
{
    for (int i = 0; i < N; ++i)
    {
        v0[i] *= v1[i];
    }
    return v0;
}

template <int N, typename Real>
Vector<N, Real>& operator/=(Vector<N, Real>& v0, Vector<N, Real> const& v1)
{
    for (int i = 0; i < N; ++i)
    {
        v0[i] /= v1[i];
    }
    return v0;
}

template <int N, typename Real>
Real Dot(Vector<N, Real> const& v0, Vector<N, Real> const& v1)
{
    Real dot = v0[0] * v1[0];
    for (int i = 1; i < N; ++i)
    {
        dot += v0[i] * v1[i];
    }
    return dot;
}

template <int N, typename Real>
Real Length(Vector<N, Real> const& v, bool robust)
{
    if (robust)
    {
        Real maxAbsComp = std::abs(v[0]);
        for (int i = 1; i < N; ++i)
        {
            Real absComp = std::abs(v[i]);
            if (absComp > maxAbsComp)
            {
                maxAbsComp = absComp;
            }
        }

        Real length;
        if (maxAbsComp > (Real)0)
        {
            Vector<N, Real> scaled = v / maxAbsComp;
            length = maxAbsComp * std::sqrt(Dot(scaled, scaled));
        }
        else
        {
            length = (Real)0;
        }
        return length;
    }
    else
    {
        return std::sqrt(Dot(v, v));
    }
}

template <int N, typename Real>
Real Normalize(Vector<N, Real>& v, bool robust)
{
    if (robust)
    {
        Real maxAbsComp = std::abs(v[0]);
        for (int i = 1; i < N; ++i)
        {
            Real absComp = std::abs(v[i]);
            if (absComp > maxAbsComp)
            {
                maxAbsComp = absComp;
            }
        }

        Real length;
        if (maxAbsComp > (Real)0)
        {
            v /= maxAbsComp;
            length = std::sqrt(Dot(v, v));
            v /= length;
            length *= maxAbsComp;
        }
        else
        {
            length = (Real)0;
            for (int i = 0; i < N; ++i)
            {
                v[i] = (Real)0;
            }
        }
        return length;
    }
    else
    {
        Real length = std::sqrt(Dot(v, v));
        if (length > (Real)0)
        {
            v /= length;
        }
        else
        {
            for (int i = 0; i < N; ++i)
            {
                v[i] = (Real)0;
            }
        }
        return length;
    }
}

template <int N, typename Real>
Real Orthonormalize(int numInputs, Vector<N, Real>* v, bool robust)
{
    if (v && 1 <= numInputs && numInputs <= N)
    {
        Real minLength = Normalize(v[0], robust);
        for (int i = 1; i < numInputs; ++i)
        {
            for (int j = 0; j < i; ++j)
            {
                Real dot = Dot(v[i], v[j]);
                v[i] -= v[j] * dot;
            }
            Real length = Normalize(v[i], robust);
            if (length < minLength)
            {
                minLength = length;
            }
        }
        return minLength;
    }

    return (Real)0;
}

template <int N, typename Real>
Vector<N, Real> GetOrthogonal(Vector<N, Real> const& v, bool unitLength)
{
    Real cmax = std::abs(v[0]);
    int imax = 0;
    for (int i = 1; i < N; ++i)
    {
        Real c = std::abs(v[i]);
        if (c > cmax)
        {
            cmax = c;
            imax = i;
        }
    }

    Vector<N, Real> result;
    result.MakeZero();
    int inext = imax + 1;
    if (inext == N)
    {
        inext = 0;
    }
    result[imax] = v[inext];
    result[inext] = -v[imax];
    if (unitLength)
    {
        Real sqrDistance = result[imax] * result[imax] + result[inext] * result[inext];
        Real invLength = ((Real)1) / std::sqrt(sqrDistance);
        result[imax] *= invLength;
        result[inext] *= invLength;
    }
    return result;
}

template <int N, typename Real>
bool ComputeExtremes(int numVectors, Vector<N, Real> const* v,
    Vector<N, Real>& vmin, Vector<N, Real>& vmax)
{
    if (v && numVectors > 0)
    {
        vmin = v[0];
        vmax = vmin;
        for (int j = 1; j < numVectors; ++j)
        {
            Vector<N, Real> const& vec = v[j];
            for (int i = 0; i < N; ++i)
            {
                if (vec[i] < vmin[i])
                {
                    vmin[i] = vec[i];
                }
                else if (vec[i] > vmax[i])
                {
                    vmax[i] = vec[i];
                }
            }
        }
        return true;
    }

    return false;
}

template <int N, typename Real>
Vector<N + 1, Real> HLift(Vector<N, Real> const& v, Real last)
{
    Vector<N + 1, Real> result;
    for (int i = 0; i < N; ++i)
    {
        result[i] = v[i];
    }
    result[N] = last;
    return result;
}

template <int N, typename Real>
Vector<N - 1, Real> HProject(Vector<N, Real> const& v)
{
    static_assert(N >= 2, "Invalid dimension.");
    Vector<N - 1, Real> result;
    for (int i = 0; i < N - 1; ++i)
    {
        result[i] = v[i];
    }
    return result;
}

template <int N, typename Real>
Vector<N + 1, Real> Lift(Vector<N, Real> const& v, int inject, Real value)
{
    Vector<N + 1, Real> result;
    int i;
    for (i = 0; i < inject; ++i)
    {
        result[i] = v[i];
    }
    result[i] = value;
    int j = i;
    for (++j; i < N; ++i, ++j)
    {
        result[j] = v[i];
    }
    return result;
}

template <int N, typename Real>
Vector<N - 1, Real> Project(Vector<N, Real> const& v, int reject)
{
    static_assert(N >= 2, "Invalid dimension.");
    Vector<N - 1, Real> result;
    for (int i = 0, j = 0; i < N - 1; ++i, ++j)
    {
        if (j == reject)
        {
            ++j;
        }
        result[i] = v[j];
    }
    return result;
}

}

// ----------------------------------------------
// GteVector3.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.2 (2018/10/04)

namespace gte
{

// Template alias for convenience.
template <typename Real>
using Vector3 = Vector<3, Real>;

// Cross, UnitCross, and DotCross have a template parameter N that should
// be 3 or 4.  The latter case supports affine vectors in 4D (last component
// w = 0) when you want to use 4-tuples and 4x4 matrices for affine algebra.

// Compute the cross product using the formal determinant:
//   cross = det{{e0,e1,e2},{x0,x1,x2},{y0,y1,y2}}
//         = (x1*y2-x2*y1, x2*y0-x0*y2, x0*y1-x1*y0)
// where e0 = (1,0,0), e1 = (0,1,0), e2 = (0,0,1), v0 = (x0,x1,x2), and
// v1 = (y0,y1,y2).
template <int N, typename Real>
Vector<N,Real> Cross(Vector<N,Real> const& v0, Vector<N,Real> const& v1);

// Compute the normalized cross product.
template <int N, typename Real>
Vector<N, Real> UnitCross(Vector<N,Real> const& v0, Vector<N,Real> const& v1,
    bool robust = false);

// Compute Dot((x0,x1,x2),Cross((y0,y1,y2),(z0,z1,z2)), the triple scalar
// product of three vectors, where v0 = (x0,x1,x2), v1 = (y0,y1,y2), and
// v2 is (z0,z1,z2).
template <int N, typename Real>
Real DotCross(Vector<N,Real> const& v0, Vector<N,Real> const& v1,
    Vector<N,Real> const& v2);

// Compute a right-handed orthonormal basis for the orthogonal complement
// of the input vectors.  The function returns the smallest length of the
// unnormalized vectors computed during the process.  If this value is nearly
// zero, it is possible that the inputs are linearly dependent (within
// numerical round-off errors).  On input, numInputs must be 1 or 2 and
// v[0] through v[numInputs-1] must be initialized.  On output, the
// vectors v[0] through v[2] form an orthonormal set.
template <typename Real>
Real ComputeOrthogonalComplement(int numInputs, Vector3<Real>* v, bool robust = false);

// Compute the barycentric coordinates of the point P with respect to the
// tetrahedron <V0,V1,V2,V3>, P = b0*V0 + b1*V1 + b2*V2 + b3*V3, where
// b0 + b1 + b2 + b3 = 1.  The return value is 'true' iff {V0,V1,V2,V3} is
// a linearly independent set.  Numerically, this is measured by
// |det[V0 V1 V2 V3]| <= epsilon.  The values bary[] are valid only when the
// return value is 'true' but set to zero when the return value is 'false'.
template <typename Real>
bool ComputeBarycentrics(Vector3<Real> const& p, Vector3<Real> const& v0,
    Vector3<Real> const& v1, Vector3<Real> const& v2, Vector3<Real> const& v3,
    Real bary[4], Real epsilon = (Real)0);

// Get intrinsic information about the input array of vectors.  The return
// value is 'true' iff the inputs are valid (numVectors > 0, v is not null,
// and epsilon >= 0), in which case the class members are valid.
template <typename Real>
class IntrinsicsVector3
{
public:
    // The constructor sets the class members based on the input set.
    IntrinsicsVector3(int numVectors, Vector3<Real> const* v, Real inEpsilon);

    // A nonnegative tolerance that is used to determine the intrinsic
    // dimension of the set.
    Real epsilon;

    // The intrinsic dimension of the input set, computed based on the
    // nonnegative tolerance mEpsilon.
    int dimension;

    // Axis-aligned bounding box of the input set.  The maximum range is
    // the larger of max[0]-min[0], max[1]-min[1], and max[2]-min[2].
    Real min[3], max[3];
    Real maxRange;

    // Coordinate system.  The origin is valid for any dimension d.  The
    // unit-length direction vector is valid only for 0 <= i < d.  The
    // extreme index is relative to the array of input points, and is also
    // valid only for 0 <= i < d.  If d = 0, all points are effectively
    // the same, but the use of an epsilon may lead to an extreme index
    // that is not zero.  If d = 1, all points effectively lie on a line
    // segment.  If d = 2, all points effectively line on a plane.  If
    // d = 3, the points are not coplanar.
    Vector3<Real> origin;
    Vector3<Real> direction[3];

    // The indices that define the maximum dimensional extents.  The
    // values extreme[0] and extreme[1] are the indices for the points
    // that define the largest extent in one of the coordinate axis
    // directions.  If the dimension is 2, then extreme[2] is the index
    // for the point that generates the largest extent in the direction
    // perpendicular to the line through the points corresponding to
    // extreme[0] and extreme[1].  Furthermore, if the dimension is 3,
    // then extreme[3] is the index for the point that generates the
    // largest extent in the direction perpendicular to the triangle
    // defined by the other extreme points.  The tetrahedron formed by the
    // points V[extreme[0]], V[extreme[1]], V[extreme[2]], and
    // V[extreme[3]] is clockwise or counterclockwise, the condition
    // stored in extremeCCW.
    int extreme[4];
    bool extremeCCW;
};


template <int N, typename Real>
Vector<N, Real> Cross(Vector<N, Real> const& v0, Vector<N, Real> const& v1)
{
    static_assert(N == 3 || N == 4, "Dimension must be 3 or 4.");

    Vector<N, Real> result;
    result.MakeZero();
    result[0] = v0[1] * v1[2] - v0[2] * v1[1];
    result[1] = v0[2] * v1[0] - v0[0] * v1[2];
    result[2] = v0[0] * v1[1] - v0[1] * v1[0];
    return result;
}

template <int N, typename Real>
Vector<N, Real> UnitCross(Vector<N, Real> const& v0, Vector<N, Real> const& v1, bool robust)
{
    static_assert(N == 3 || N == 4, "Dimension must be 3 or 4.");

    Vector<N, Real> unitCross = Cross(v0, v1);
    Normalize(unitCross, robust);
    return unitCross;
}

template <int N, typename Real>
Real DotCross(Vector<N, Real> const& v0, Vector<N, Real> const& v1,
    Vector<N, Real> const& v2)
{
    static_assert(N == 3 || N == 4, "Dimension must be 3 or 4.");

    return Dot(v0, Cross(v1, v2));
}

template <typename Real>
Real ComputeOrthogonalComplement(int numInputs, Vector3<Real>* v, bool robust)
{
    if (numInputs == 1)
    {
        if (std::abs(v[0][0]) > std::abs(v[0][1]))
        {
            v[1] = { -v[0][2], (Real)0, +v[0][0] };
        }
        else
        {
            v[1] = { (Real)0, +v[0][2], -v[0][1] };
        }
        numInputs = 2;
    }

    if (numInputs == 2)
    {
        v[2] = Cross(v[0], v[1]);
        return Orthonormalize<3, Real>(3, v, robust);
    }

    return (Real)0;
}

template <typename Real>
bool ComputeBarycentrics(Vector3<Real> const& p, Vector3<Real> const& v0,
    Vector3<Real> const& v1, Vector3<Real> const& v2, Vector3<Real> const& v3,
    Real bary[4], Real epsilon)
{
    // Compute the vectors relative to V3 of the tetrahedron.
    Vector3<Real> diff[4] = { v0 - v3, v1 - v3, v2 - v3, p - v3 };

    Real det = DotCross(diff[0], diff[1], diff[2]);
    if (det < -epsilon || det > epsilon)
    {
        Real invDet = ((Real)1) / det;
        bary[0] = DotCross(diff[3], diff[1], diff[2]) * invDet;
        bary[1] = DotCross(diff[3], diff[2], diff[0]) * invDet;
        bary[2] = DotCross(diff[3], diff[0], diff[1]) * invDet;
        bary[3] = (Real)1 - bary[0] - bary[1] - bary[2];
        return true;
    }

    for (int i = 0; i < 4; ++i)
    {
        bary[i] = (Real)0;
    }
    return false;
}

template <typename Real>
IntrinsicsVector3<Real>::IntrinsicsVector3(int numVectors,
    Vector3<Real> const* v, Real inEpsilon)
    :
    epsilon(inEpsilon),
    dimension(0),
    maxRange((Real)0),
    origin({ (Real)0, (Real)0, (Real)0 }),
    extremeCCW(false)
{
    min[0] = (Real)0;
    min[1] = (Real)0;
    min[2] = (Real)0;
    direction[0] = { (Real)0, (Real)0, (Real)0 };
    direction[1] = { (Real)0, (Real)0, (Real)0 };
    direction[2] = { (Real)0, (Real)0, (Real)0 };
    extreme[0] = 0;
    extreme[1] = 0;
    extreme[2] = 0;
    extreme[3] = 0;

    if (numVectors > 0 && v && epsilon >= (Real)0)
    {
        // Compute the axis-aligned bounding box for the input vectors.  Keep
        // track of the indices into 'vectors' for the current min and max.
        int j, indexMin[3], indexMax[3];
        for (j = 0; j < 3; ++j)
        {
            min[j] = v[0][j];
            max[j] = min[j];
            indexMin[j] = 0;
            indexMax[j] = 0;
        }

        int i;
        for (i = 1; i < numVectors; ++i)
        {
            for (j = 0; j < 3; ++j)
            {
                if (v[i][j] < min[j])
                {
                    min[j] = v[i][j];
                    indexMin[j] = i;
                }
                else if (v[i][j] > max[j])
                {
                    max[j] = v[i][j];
                    indexMax[j] = i;
                }
            }
        }

        // Determine the maximum range for the bounding box.
        maxRange = max[0] - min[0];
        extreme[0] = indexMin[0];
        extreme[1] = indexMax[0];
        Real range = max[1] - min[1];
        if (range > maxRange)
        {
            maxRange = range;
            extreme[0] = indexMin[1];
            extreme[1] = indexMax[1];
        }
        range = max[2] - min[2];
        if (range > maxRange)
        {
            maxRange = range;
            extreme[0] = indexMin[2];
            extreme[1] = indexMax[2];
        }

        // The origin is either the vector of minimum x0-value, vector of
        // minimum x1-value, or vector of minimum x2-value.
        origin = v[extreme[0]];

        // Test whether the vector set is (nearly) a vector.
        if (maxRange <= epsilon)
        {
            dimension = 0;
            for (j = 0; j < 3; ++j)
            {
                extreme[j + 1] = extreme[0];
            }
            return;
        }

        // Test whether the vector set is (nearly) a line segment.  We need
        // {direction[2],direction[3]} to span the orthogonal complement of
        // direction[0].
        direction[0] = v[extreme[1]] - origin;
        Normalize(direction[0], false);
        if (std::abs(direction[0][0]) > std::abs(direction[0][1]))
        {
            direction[1][0] = -direction[0][2];
            direction[1][1] = (Real)0;
            direction[1][2] = +direction[0][0];
        }
        else
        {
            direction[1][0] = (Real)0;
            direction[1][1] = +direction[0][2];
            direction[1][2] = -direction[0][1];
        }
        Normalize(direction[1], false);
        direction[2] = Cross(direction[0], direction[1]);

        // Compute the maximum distance of the points from the line
        // origin+t*direction[0].
        Real maxDistance = (Real)0;
        Real distance, dot;
        extreme[2] = extreme[0];
        for (i = 0; i < numVectors; ++i)
        {
            Vector3<Real> diff = v[i] - origin;
            dot = Dot(direction[0], diff);
            Vector3<Real> proj = diff - dot * direction[0];
            distance = Length(proj, false);
            if (distance > maxDistance)
            {
                maxDistance = distance;
                extreme[2] = i;
            }
        }

        if (maxDistance <= epsilon*maxRange)
        {
            // The points are (nearly) on the line origin+t*direction[0].
            dimension = 1;
            extreme[2] = extreme[1];
            extreme[3] = extreme[1];
            return;
        }

        // Test whether the vector set is (nearly) a planar polygon.  The
        // point v[extreme[2]] is farthest from the line: origin + 
        // t*direction[0].  The vector v[extreme[2]]-origin is not
        // necessarily perpendicular to direction[0], so project out the
        // direction[0] component so that the result is perpendicular to
        // direction[0].
        direction[1] = v[extreme[2]] - origin;
        dot = Dot(direction[0], direction[1]);
        direction[1] -= dot * direction[0];
        Normalize(direction[1], false);

        // We need direction[2] to span the orthogonal complement of
        // {direction[0],direction[1]}.
        direction[2] = Cross(direction[0], direction[1]);

        // Compute the maximum distance of the points from the plane
        // origin+t0*direction[0]+t1*direction[1].
        maxDistance = (Real)0;
        Real maxSign = (Real)0;
        extreme[3] = extreme[0];
        for (i = 0; i < numVectors; ++i)
        {
            Vector3<Real> diff = v[i] - origin;
            distance = Dot(direction[2], diff);
            Real sign = (distance >(Real)0 ? (Real)1 :
                (distance < (Real)0 ? (Real)-1 : (Real)0));
            distance = std::abs(distance);
            if (distance > maxDistance)
            {
                maxDistance = distance;
                maxSign = sign;
                extreme[3] = i;
            }
        }

        if (maxDistance <= epsilon * maxRange)
        {
            // The points are (nearly) on the plane origin+t0*direction[0]
            // +t1*direction[1].
            dimension = 2;
            extreme[3] = extreme[2];
            return;
        }

        dimension = 3;
        extremeCCW = (maxSign > (Real)0);
        return;
    }
}

}

// ----------------------------------------------
// GteHypersphere.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

// The hypersphere is represented as |X-C| = R where C is the center and R is
// the radius.  The hypersphere is a circle for dimension 2 or a sphere for
// dimension 3.

namespace gte
{

template <int N, typename Real>
class Hypersphere
{
public:
    // Construction and destruction.  The default constructor sets the center
    // to (0,...,0) and the radius to 1.
    Hypersphere();
    Hypersphere(Vector<N, Real> const& inCenter, Real inRadius);

    // Public member access.
    Vector<N, Real> center;
    Real radius;

public:
    // Comparisons to support sorted containers.
    bool operator==(Hypersphere const& hypersphere) const;
    bool operator!=(Hypersphere const& hypersphere) const;
    bool operator< (Hypersphere const& hypersphere) const;
    bool operator<=(Hypersphere const& hypersphere) const;
    bool operator> (Hypersphere const& hypersphere) const;
    bool operator>=(Hypersphere const& hypersphere) const;
};

// Template aliases for convenience.
template <typename Real>
using Circle2 = Hypersphere<2, Real>;

template <typename Real>
using Sphere3 = Hypersphere<3, Real>;


template <int N, typename Real>
Hypersphere<N, Real>::Hypersphere()
    :
    radius((Real)1)
{
    center.MakeZero();
}

template <int N, typename Real>
Hypersphere<N, Real>::Hypersphere(Vector<N, Real> const& inCenter,
    Real inRadius)
    :
    center(inCenter),
    radius(inRadius)
{
}

template <int N, typename Real>
bool Hypersphere<N, Real>::operator==(Hypersphere const& hypersphere) const
{
    return center == hypersphere.center && radius == hypersphere.radius;
}

template <int N, typename Real>
bool Hypersphere<N, Real>::operator!=(Hypersphere const& hypersphere) const
{
    return !operator==(hypersphere);
}

template <int N, typename Real>
bool Hypersphere<N, Real>::operator<(Hypersphere const& hypersphere) const
{
    if (center < hypersphere.center)
    {
        return true;
    }

    if (center > hypersphere.center)
    {
        return false;
    }

    return radius < hypersphere.radius;
}

template <int N, typename Real>
bool Hypersphere<N, Real>::operator<=(Hypersphere const& hypersphere) const
{
    return operator<(hypersphere) || operator==(hypersphere);
}

template <int N, typename Real>
bool Hypersphere<N, Real>::operator>(Hypersphere const& hypersphere) const
{
    return !operator<=(hypersphere);
}

template <int N, typename Real>
bool Hypersphere<N, Real>::operator>=(Hypersphere const& hypersphere) const
{
    return !operator<(hypersphere);
}


}


// ----------------------------------------------
// GteOrientedBox.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

// A box has center C, axis directions U[i], and extents e[i].  The set
// {U[0],...,U[N-1]} is orthonormal, which means the vectors are
// unit-length and mutually perpendicular.  The extents are nonnegative;
// zero is allowed, meaning the box is degenerate in the corresponding
// direction.  A point X is represented in box coordinates by
// X = C + y[0]*U[0] + y[1]*U[1].  This point is inside or on the
// box whenever |y[i]| <= e[i] for all i.

namespace gte
{

template <int N, typename Real>
class OrientedBox
{
public:
    // Construction and destruction.  The default constructor sets the center
    // to (0,...,0), axis d to Vector<N,Real>::Unit(d), and extent d to +1.
    OrientedBox();
    OrientedBox(Vector<N, Real> const& inCenter,
        std::array<Vector<N, Real>, N> const& inAxis,
        Vector<N, Real> const& inExtent);

    // Compute the vertices of the box.  If index i has the bit pattern
    // i = b[N-1]...b[0], then
    //    vertex[i] = center + sum_{d=0}^{N-1} sign[d] * extent[d] * axis[d]
    // where sign[d] = 2*b[d] - 1.
    void GetVertices(std::array<Vector<N, Real>, (1 << N)>& vertex) const;

    // Public member access.  It is required that extent[i] >= 0.
    Vector<N, Real> center;
    std::array<Vector<N, Real>, N> axis;
    Vector<N, Real> extent;

public:
    // Comparisons to support sorted containers.
    bool operator==(OrientedBox const& box) const;
    bool operator!=(OrientedBox const& box) const;
    bool operator< (OrientedBox const& box) const;
    bool operator<=(OrientedBox const& box) const;
    bool operator> (OrientedBox const& box) const;
    bool operator>=(OrientedBox const& box) const;
};

// Template aliases for convenience.
template <typename Real>
using OrientedBox2 = OrientedBox<2, Real>;

template <typename Real>
using OrientedBox3 = OrientedBox<3, Real>;


template <int N, typename Real>
OrientedBox<N, Real>::OrientedBox()
{
    center.MakeZero();
    for (int i = 0; i < N; ++i)
    {
        axis[i].MakeUnit(i);
        extent[i] = (Real)1;
    }
}

template <int N, typename Real>
OrientedBox<N, Real>::OrientedBox(Vector<N, Real> const& inCenter,
    std::array<Vector<N, Real>, N> const& inAxis,
    Vector<N, Real> const& inExtent)
    :
    center(inCenter),
    axis(inAxis),
    extent(inExtent)
{
}

template <int N, typename Real>
void OrientedBox<N, Real>::GetVertices(
    std::array<Vector<N, Real>, (1 << N)>& vertex) const
{
    unsigned int const dsup = static_cast<unsigned int>(N);
    std::array<Vector<N, Real>, N> product;
    for (unsigned int d = 0; d < dsup; ++d)
    {
        product[d] = extent[d] * axis[d];
    }

    int const imax = (1 << N);
    for (int i = 0; i < imax; ++i)
    {
        vertex[i] = center;
        for (unsigned int d = 0, mask = 1; d < dsup; ++d, mask <<= 1)
        {
            Real sign = (i & mask ? (Real)1 : (Real)-1);
            vertex[i] += sign * product[d];
        }
    }
}

template <int N, typename Real>
bool OrientedBox<N, Real>::operator==(OrientedBox const& box) const
{
    return center == box.center && axis == box.axis && extent == box.extent;
}

template <int N, typename Real>
bool OrientedBox<N, Real>::operator!=(OrientedBox const& box) const
{
    return !operator==(box);
}

template <int N, typename Real>
bool OrientedBox<N, Real>::operator<(OrientedBox const& box) const
{
    if (center < box.center)
    {
        return true;
    }

    if (center > box.center)
    {
        return false;
    }

    if (axis < box.axis)
    {
        return true;
    }

    if (axis > box.axis)
    {
        return false;
    }

    return extent < box.extent;
}

template <int N, typename Real>
bool OrientedBox<N, Real>::operator<=(OrientedBox const& box) const
{
    return operator<(box) || operator==(box);
}

template <int N, typename Real>
bool OrientedBox<N, Real>::operator>(OrientedBox const& box) const
{
    return !operator<=(box);
}

template <int N, typename Real>
bool OrientedBox<N, Real>::operator>=(OrientedBox const& box) const
{
    return !operator<(box);
}


}

// ----------------------------------------------
// GteFeatureKey.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

namespace gte
{

template <int N, bool Ordered>
class FeatureKey
{
protected:
    // Abstract base class with V[] uninitialized.  The derived classes must
    // set the V[] values accordingly.
    //
    // An ordered feature key has V[0] = min(V[]) with (V[0],V[1],...,V[N-1]) a
    // permutation of N inputs with an even number of transpositions.
    //
    // An unordered feature key has V[0] < V[1] < ... < V[N-1].
    //
    // Note that the word 'order' is about the geometry of the feature, not
    // the comparison order for any sorting.
    FeatureKey();

public:
    bool operator<(FeatureKey const& key) const;
    bool operator==(FeatureKey const& key) const;

    int V[N];
};


template <int N, bool Ordered>
FeatureKey<N, Ordered>::FeatureKey()
{
}

template <int N, bool Ordered>
bool FeatureKey<N, Ordered>::operator<(FeatureKey const& key) const
{
    for (int i = N - 1; i >= 0; --i)
    {
        if (V[i] < key.V[i])
        {
            return true;
        }

        if (V[i] > key.V[i])
        {
            return false;
        }
    }
    return false;
}

template <int N, bool Ordered>
bool FeatureKey<N, Ordered>::operator==(FeatureKey const& key) const
{
    for (int i = 0; i < N; ++i)
    {
        if (V[i] != key.V[i])
        {
            return false;
        }
    }
    return true;
}


}


// ----------------------------------------------
// GteEdgeKey.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2018/05/22)

namespace gte
{
    template <bool Ordered>
    class EdgeKey : public FeatureKey<2, Ordered>
    {
    public:
        // An ordered edge has (V[0],V[1]) = (v0,v1).  An unordered edge has
        // (V[0],V[1]) = (min(V[0],V[1]),max(V[0],V[1])).
        EdgeKey();  // creates key (-1,-1)
        explicit EdgeKey(int v0, int v1);
    };

	template<>
	inline
	EdgeKey<true>::EdgeKey()
	{
		V[0] = -1;
		V[1] = -1;
	}

	template<>
	inline
	EdgeKey<true>::EdgeKey(int v0, int v1)
	{
		V[0] = v0;
		V[1] = v1;
	}

	template<>
	inline
	EdgeKey<false>::EdgeKey()
	{
		V[0] = -1;
		V[1] = -1;
	}

	template<>
	inline
	EdgeKey<false>::EdgeKey(int v0, int v1)
	{
		if (v0 < v1)
		{
			// v0 is minimum
			V[0] = v0;
			V[1] = v1;
		}
		else
		{
			// v1 is minimum
			V[0] = v1;
			V[1] = v0;
		}
	}
}


// ----------------------------------------------
// GteTriangleKey.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2018/05/22)

namespace gte
{
    template <bool Ordered>
    class TriangleKey : public FeatureKey<3, Ordered>
    {
    public:
        // An ordered triangle has V[0] = min(v0,v1,v2).  Choose
        // (V[0],V[1],V[2]) to be a permutation of (v0,v1,v2) so that the
        // final storage is one of
        //   (v0,v1,v2), (v1,v2,v0), (v2,v0,v1)
        // The idea is that if v0 corresponds to (1,0,0), v1 corresponds to
        // (0,1,0), and v2 corresponds to (0,0,1), the ordering (v0,v1,v2)
        // corresponds to the 3x3 identity matrix I; the rows are the
        // specified 3-tuples.  The permutation (V[0],V[1],V[2]) induces a
        // permutation of the rows of the identity matrix to form a
        // permutation matrix P with det(P) = 1 = det(I).
        //
        // An unordered triangle stores a permutation of (v0,v1,v2) so that
        // V[0] < V[1] < V[2].
        TriangleKey();  // creates key (-1,-1,-1)
        explicit TriangleKey(int v0, int v1, int v2);
    };

	template<>
	inline
	TriangleKey<true>::TriangleKey()
	{
		V[0] = -1;
		V[1] = -1;
		V[2] = -1;
	}

	template<>
	inline
	TriangleKey<true>::TriangleKey(int v0, int v1, int v2)
	{
		if (v0 < v1)
		{
			if (v0 < v2)
			{
				// v0 is minimum
				V[0] = v0;
				V[1] = v1;
				V[2] = v2;
			}
			else
			{
				// v2 is minimum
				V[0] = v2;
				V[1] = v0;
				V[2] = v1;
			}
		}
		else
		{
			if (v1 < v2)
			{
				// v1 is minimum
				V[0] = v1;
				V[1] = v2;
				V[2] = v0;
			}
			else
			{
				// v2 is minimum
				V[0] = v2;
				V[1] = v0;
				V[2] = v1;
			}
		}
	}

	template<>
	inline
	TriangleKey<false>::TriangleKey()
	{
		V[0] = -1;
		V[1] = -1;
		V[2] = -1;
	}

	template<>
	inline
	TriangleKey<false>::TriangleKey(int v0, int v1, int v2)
	{
		if (v0 < v1)
		{
			if (v0 < v2)
			{
				// v0 is minimum
				V[0] = v0;
				V[1] = std::min(v1, v2);
				V[2] = std::max(v1, v2);
			}
			else
			{
				// v2 is minimum
				V[0] = v2;
				V[1] = std::min(v0, v1);
				V[2] = std::max(v0, v1);
			}
		}
		else
		{
			if (v1 < v2)
			{
				// v1 is minimum
				V[0] = v1;
				V[1] = std::min(v2, v0);
				V[2] = std::max(v2, v0);
			}
			else
			{
				// v2 is minimum
				V[0] = v2;
				V[1] = std::min(v0, v1);
				V[2] = std::max(v0, v1);
			}
		}
	}
}


// ----------------------------------------------
// GteETManifoldMesh.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2017/01/02)

namespace gte
{

class GTE_IMPEXP ETManifoldMesh
{
public:
    // Edge data types.
    class Edge;
    typedef std::shared_ptr<Edge> (*ECreator)(int, int);
    typedef std::map<EdgeKey<false>, std::shared_ptr<Edge>> EMap;

    // Triangle data types.
    class Triangle;
    typedef std::shared_ptr<Triangle> (*TCreator)(int, int, int);
    typedef std::map<TriangleKey<true>, std::shared_ptr<Triangle>> TMap;

    // Edge object.
    class GTE_IMPEXP Edge
    {
    public:
        virtual ~Edge();
        Edge(int v0, int v1);

        // Vertices of the edge.
        int V[2];

        // Triangles sharing the edge.
        std::weak_ptr<Triangle> T[2];
    };

    // Triangle object.
    class GTE_IMPEXP Triangle
    {
    public:
        virtual ~Triangle();
        Triangle(int v0, int v1, int v2);

        // Vertices, listed in counterclockwise order (V[0],V[1],V[2]).
        int V[3];

        // Adjacent edges.  E[i] points to edge (V[i],V[(i+1)%3]).
        std::weak_ptr<Edge> E[3];

        // Adjacent triangles.  T[i] points to the adjacent triangle
        // sharing edge E[i].
        std::weak_ptr<Triangle> T[3];
    };


    // Construction and destruction.
    virtual ~ETManifoldMesh();
    ETManifoldMesh(ECreator eCreator = nullptr, TCreator tCreator = nullptr);

    // Support for a deep copy of the mesh.  The mEMap and mTMap objects have
    // dynamically allocated memory for edges and triangles.  A shallow copy
    // of the pointers to this memory is problematic.  Allowing sharing, say,
    // via std::shared_ptr, is an option but not really the intent of copying
    // the mesh graph.
    ETManifoldMesh(ETManifoldMesh const& mesh);
    ETManifoldMesh& operator=(ETManifoldMesh const& mesh);

    // Member access.
    EMap const& GetEdges() const;
    TMap const& GetTriangles() const;

    // If the insertion of a triangle fails because the mesh would become
    // nonmanifold, the default behavior is to trigger a LogError message.
    // You can disable this behavior in situations where you want the Logger
    // system on but you want to continue gracefully without an assertion.
    // The return value is the previous value of the internal state
    // mAssertOnNonmanifoldInsertion.
    bool AssertOnNonmanifoldInsertion(bool doAssert);

    // If <v0,v1,v2> is not in the mesh, a Triangle object is created and
    // returned; otherwise, <v0,v1,v2> is in the mesh and nullptr is returned.
    // If the insertion leads to a nonmanifold mesh, the call fails with a
    // nullptr returned.
    virtual std::shared_ptr<Triangle> Insert(int v0, int v1, int v2);

    // If <v0,v1,v2> is in the mesh, it is removed and 'true' is returned;
    // otherwise, <v0,v1,v2> is not in the mesh and 'false' is returned.
    virtual bool Remove(int v0, int v1, int v2);

    // Destroy the edges and triangles to obtain an empty mesh.
    virtual void Clear();

    // A manifold mesh is closed if each edge is shared twice.  A closed
    // mesh is not necessarily oriented.  For example, you could have a
    // mesh with spherical topology.  The upper hemisphere has outer-facing
    // normals and the lower hemisphere has inner-facing normals.  The
    // discontinuity in orientation occurs on the circle shared by the
    // hemispheres.
    bool IsClosed() const;

    // Test whether all triangles in the mesh are oriented consistently and
    // that no two triangles are coincident.  The latter means that you
    // cannot have both triangles <v0,v1,v2> and <v0,v2,v1> in the mesh to
    // be considered oriented.
    bool IsOriented() const;

    // Compute the connected components of the edge-triangle graph that the
    // mesh represents.  The first function returns pointers into 'this'
    // object's containers, so you must consume the components before
    // clearing or destroying 'this'.  The second function returns triangle
    // keys, which requires three times as much storage as the pointers but
    // allows you to clear or destroy 'this' before consuming the components.
    void GetComponents(std::vector<std::vector<std::shared_ptr<Triangle>>>& components) const;
    void GetComponents(std::vector<std::vector<TriangleKey<true>>>& components) const;

protected:
    // The edge data and default edge creation.
    static std::shared_ptr<Edge> CreateEdge(int v0, int v1);
    ECreator mECreator;
    EMap mEMap;

    // The triangle data and default triangle creation.
    static std::shared_ptr<Triangle> CreateTriangle(int v0, int v1, int v2);
    TCreator mTCreator;
    TMap mTMap;
    bool mAssertOnNonmanifoldInsertion;  // default: true

    // Support for computing connected components.  This is a straightforward
    // depth-first search of the graph but uses a preallocated stack rather
    // than a recursive function that could possibly overflow the call stack.
    void DepthFirstSearch(std::shared_ptr<Triangle> const& tInitial,
        std::map<std::shared_ptr<Triangle>, int>& visited,
        std::vector<std::shared_ptr<Triangle>>& component) const;
};

}

// ----------------------------------------------
// GteLine.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

// The line is represented by P+t*D, where P is an origin point, D is a
// unit-length direction vector, and t is any real number.  The user must
// ensure that D is unit length.

namespace gte
{

template <int N, typename Real>
class Line
{
public:
    // Construction and destruction.  The default constructor sets the origin
    // to (0,...,0) and the line direction to (1,0,...,0).
    Line();
    Line(Vector<N, Real> const& inOrigin, Vector<N, Real> const& inDirection);

    // Public member access.  The direction must be unit length.
    Vector<N, Real> origin, direction;

public:
    // Comparisons to support sorted containers.
    bool operator==(Line const& line) const;
    bool operator!=(Line const& line) const;
    bool operator< (Line const& line) const;
    bool operator<=(Line const& line) const;
    bool operator> (Line const& line) const;
    bool operator>=(Line const& line) const;
};

// Template aliases for convenience.
template <typename Real>
using Line2 = Line<2, Real>;

template <typename Real>
using Line3 = Line<3, Real>;


template <int N, typename Real>
Line<N, Real>::Line()
{
    origin.MakeZero();
    direction.MakeUnit(0);
}

template <int N, typename Real>
Line<N, Real>::Line(Vector<N, Real> const& inOrigin,
    Vector<N, Real> const& inDirection)
    :
    origin(inOrigin),
    direction(inDirection)
{
}

template <int N, typename Real>
bool Line<N, Real>::operator==(Line const& line) const
{
    return origin == line.origin && direction == line.direction;
}

template <int N, typename Real>
bool Line<N, Real>::operator!=(Line const& line) const
{
    return !operator==(line);
}

template <int N, typename Real>
bool Line<N, Real>::operator<(Line const& line) const
{
    if (origin < line.origin)
    {
        return true;
    }

    if (origin > line.origin)
    {
        return false;
    }

    return direction < line.direction;
}

template <int N, typename Real>
bool Line<N, Real>::operator<=(Line const& line) const
{
    return operator<(line) || operator==(line);
}

template <int N, typename Real>
bool Line<N, Real>::operator>(Line const& line) const
{
    return !operator<=(line);
}

template <int N, typename Real>
bool Line<N, Real>::operator>=(Line const& line) const
{
    return !operator<(line);
}


}


// ----------------------------------------------
// GtePrimalQuery3.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

// Queries about the relation of a point to various geometric objects.  The
// choices for N when using UIntegerFP32<N> for either BSNumber of BSRational
// are determined in GeometricTools/GTEngine/Tools/PrecisionCalculator.  These
// N-values are worst case scenarios. Your specific input data might require
// much smaller N, in which case you can modify PrecisionCalculator to use the
// BSPrecision(int32_t,int32_t,int32_t,bool) constructors.

namespace gte
{

template <typename Real>
class PrimalQuery3
{
public:
    // The caller is responsible for ensuring that the array is not empty
    // before calling queries and that the indices passed to the queries are
    // valid.  The class does no range checking.
    PrimalQuery3();
    PrimalQuery3(int numVertices, Vector3<Real> const* vertices);

    // Member access.
    inline void Set(int numVertices, Vector3<Real> const* vertices);
    inline int GetNumVertices() const;
    inline Vector3<Real> const* GetVertices() const;

    // In the following, point P refers to vertices[i] or 'test' and Vi refers
    // to vertices[vi].

    // For a plane with origin V0 and normal N = Cross(V1-V0,V2-V0), ToPlane
    // returns
    //   +1, P on positive side of plane (side to which N points)
    //   -1, P on negative side of plane (side to which -N points)
    //    0, P on the plane
    //
    // Choice of N for UIntegerFP32<N>.
    //    input type | compute type | N
    //    -----------+--------------+------
    //    float      | BSNumber     |    27
    //    double     | BSNumber     |   197
    //    float      | BSRational   |  2882
    //    double     | BSRational   | 21688
    int ToPlane(int i, int v0, int v1, int v2) const;
    int ToPlane(Vector3<Real> const& test, int v0, int v1, int v2) const;

    // For a tetrahedron with vertices ordered as described in the file
    // GteTetrahedronKey.h, the function returns
    //   +1, P outside tetrahedron
    //   -1, P inside tetrahedron
    //    0, P on tetrahedron
    //
    // Choice of N for UIntegerFP32<N>.
    //    input type | compute type | N
    //    -----------+--------------+----
    //    float      | BSNumber     |    27
    //    double     | BSNumber     |   197
    //    float      | BSRational   |  2882
    //    double     | BSRational   | 21688
    // The query involves four calls of ToPlane, so the numbers match those
    // of ToPlane.
    int ToTetrahedron(int i, int v0, int v1, int v2, int v3) const;
    int ToTetrahedron(Vector3<Real> const& test, int v0, int v1, int v2, int v3) const;

    // For a tetrahedron with vertices ordered as described in the file
    // GteTetrahedronKey.h, the function returns
    //   +1, P outside circumsphere of tetrahedron
    //   -1, P inside circumsphere of tetrahedron
    //    0, P on circumsphere of tetrahedron
    //
    // Choice of N for UIntegerFP32<N>.
    //    input type | compute type | N
    //    -----------+--------------+--------
    //    float      | BSNumber     |      44
    //    float      | BSRational   |     329
    //    double     | BSNumber     |  298037
    //    double     | BSRational   | 2254442
    int ToCircumsphere(int i, int v0, int v1, int v2, int v3) const;
    int ToCircumsphere(Vector3<Real> const& test, int v0, int v1, int v2, int v3) const;

private:
    int mNumVertices;
    Vector3<Real> const* mVertices;
};


template <typename Real>
PrimalQuery3<Real>::PrimalQuery3()
    :
    mNumVertices(0),
    mVertices(nullptr)
{
}

template <typename Real>
PrimalQuery3<Real>::PrimalQuery3(int numVertices,
    Vector3<Real> const* vertices)
    :
    mNumVertices(numVertices),
    mVertices(vertices)
{
}

template <typename Real> inline
void PrimalQuery3<Real>::Set(int numVertices, Vector3<Real> const* vertices)
{
    mNumVertices = numVertices;
    mVertices = vertices;
}

template <typename Real> inline
int PrimalQuery3<Real>::GetNumVertices() const
{
    return mNumVertices;
}

template <typename Real> inline
Vector3<Real> const* PrimalQuery3<Real>::GetVertices() const
{
    return mVertices;
}

template <typename Real>
int PrimalQuery3<Real>::ToPlane(int i, int v0, int v1, int v2) const
{
    return ToPlane(mVertices[i], v0, v1, v2);
}

template <typename Real>
int PrimalQuery3<Real>::ToPlane(Vector3<Real> const& test, int v0, int v1,
    int v2) const
{
    Vector3<Real> const& vec0 = mVertices[v0];
    Vector3<Real> const& vec1 = mVertices[v1];
    Vector3<Real> const& vec2 = mVertices[v2];

    Real x0 = test[0] - vec0[0];
    Real y0 = test[1] - vec0[1];
    Real z0 = test[2] - vec0[2];
    Real x1 = vec1[0] - vec0[0];
    Real y1 = vec1[1] - vec0[1];
    Real z1 = vec1[2] - vec0[2];
    Real x2 = vec2[0] - vec0[0];
    Real y2 = vec2[1] - vec0[1];
    Real z2 = vec2[2] - vec0[2];
    Real y1z2 = y1*z2;
    Real y2z1 = y2*z1;
    Real y2z0 = y2*z0;
    Real y0z2 = y0*z2;
    Real y0z1 = y0*z1;
    Real y1z0 = y1*z0;
    Real c0 = y1z2 - y2z1;
    Real c1 = y2z0 - y0z2;
    Real c2 = y0z1 - y1z0;
    Real x0c0 = x0*c0;
    Real x1c1 = x1*c1;
    Real x2c2 = x2*c2;
    Real term = x0c0 + x1c1;
    Real det = term + x2c2;
    Real const zero(0);

    return (det > zero ? +1 : (det < zero ? -1 : 0));
}

template <typename Real>
int PrimalQuery3<Real>::ToTetrahedron(int i, int v0, int v1, int v2, int v3)
    const
{
    return ToTetrahedron(mVertices[i], v0, v1, v2, v3);
}

template <typename Real>
int PrimalQuery3<Real>::ToTetrahedron(Vector3<Real> const& test, int v0,
    int v1, int v2, int v3) const
{
    int sign0 = ToPlane(test, v1, v2, v3);
    if (sign0 > 0)
    {
        return +1;
    }

    int sign1 = ToPlane(test, v0, v2, v3);
    if (sign1 < 0)
    {
        return +1;
    }

    int sign2 = ToPlane(test, v0, v1, v3);
    if (sign2 > 0)
    {
        return +1;
    }

    int sign3 = ToPlane(test, v0, v1, v2);
    if (sign3 < 0)
    {
        return +1;
    }

    return ((sign0 && sign1 && sign2 && sign3) ? -1 : 0);
}

template <typename Real>
int PrimalQuery3<Real>::ToCircumsphere(int i, int v0, int v1, int v2, int v3)
const
{
    return ToCircumsphere(mVertices[i], v0, v1, v2, v3);
}

template <typename Real>
int PrimalQuery3<Real>::ToCircumsphere(Vector3<Real> const& test, int v0,
    int v1, int v2, int v3) const
{
    Vector3<Real> const& vec0 = mVertices[v0];
    Vector3<Real> const& vec1 = mVertices[v1];
    Vector3<Real> const& vec2 = mVertices[v2];
    Vector3<Real> const& vec3 = mVertices[v3];

    Real x0 = vec0[0] - test[0];
    Real y0 = vec0[1] - test[1];
    Real z0 = vec0[2] - test[2];
    Real s00 = vec0[0] + test[0];
    Real s01 = vec0[1] + test[1];
    Real s02 = vec0[2] + test[2];
    Real t00 = s00*x0;
    Real t01 = s01*y0;
    Real t02 = s02*z0;
    Real t00pt01 = t00 + t01;
    Real w0 = t00pt01 + t02;

    Real x1 = vec1[0] - test[0];
    Real y1 = vec1[1] - test[1];
    Real z1 = vec1[2] - test[2];
    Real s10 = vec1[0] + test[0];
    Real s11 = vec1[1] + test[1];
    Real s12 = vec1[2] + test[2];
    Real t10 = s10*x1;
    Real t11 = s11*y1;
    Real t12 = s12*z1;
    Real t10pt11 = t10 + t11;
    Real w1 = t10pt11 + t12;

    Real x2 = vec2[0] - test[0];
    Real y2 = vec2[1] - test[1];
    Real z2 = vec2[2] - test[2];
    Real s20 = vec2[0] + test[0];
    Real s21 = vec2[1] + test[1];
    Real s22 = vec2[2] + test[2];
    Real t20 = s20*x2;
    Real t21 = s21*y2;
    Real t22 = s22*z2;
    Real t20pt21 = t20 + t21;
    Real w2 = t20pt21 + t22;

    Real x3 = vec3[0] - test[0];
    Real y3 = vec3[1] - test[1];
    Real z3 = vec3[2] - test[2];
    Real s30 = vec3[0] + test[0];
    Real s31 = vec3[1] + test[1];
    Real s32 = vec3[2] + test[2];
    Real t30 = s30*x3;
    Real t31 = s31*y3;
    Real t32 = s32*z3;
    Real t30pt31 = t30 + t31;
    Real w3 = t30pt31 + t32;

    Real x0y1 = x0*y1;
    Real x0y2 = x0*y2;
    Real x0y3 = x0*y3;
    Real x1y0 = x1*y0;
    Real x1y2 = x1*y2;
    Real x1y3 = x1*y3;
    Real x2y0 = x2*y0;
    Real x2y1 = x2*y1;
    Real x2y3 = x2*y3;
    Real x3y0 = x3*y0;
    Real x3y1 = x3*y1;
    Real x3y2 = x3*y2;
    Real a0 = x0y1 - x1y0;
    Real a1 = x0y2 - x2y0;
    Real a2 = x0y3 - x3y0;
    Real a3 = x1y2 - x2y1;
    Real a4 = x1y3 - x3y1;
    Real a5 = x2y3 - x3y2;

    Real z0w1 = z0*w1;
    Real z0w2 = z0*w2;
    Real z0w3 = z0*w3;
    Real z1w0 = z1*w0;
    Real z1w2 = z1*w2;
    Real z1w3 = z1*w3;
    Real z2w0 = z2*w0;
    Real z2w1 = z2*w1;
    Real z2w3 = z2*w3;
    Real z3w0 = z3*w0;
    Real z3w1 = z3*w1;
    Real z3w2 = z3*w2;
    Real b0 = z0w1 - z1w0;
    Real b1 = z0w2 - z2w0;
    Real b2 = z0w3 - z3w0;
    Real b3 = z1w2 - z2w1;
    Real b4 = z1w3 - z3w1;
    Real b5 = z2w3 - z3w2;
    Real a0b5 = a0*b5;
    Real a1b4 = a1*b4;
    Real a2b3 = a2*b3;
    Real a3b2 = a3*b2;
    Real a4b1 = a4*b1;
    Real a5b0 = a5*b0;
    Real term0 = a0b5 - a1b4;
    Real term1 = term0 + a2b3;
    Real term2 = term1 + a3b2;
    Real term3 = term2 - a4b1;
    Real det = term3 + a5b0;
    Real const zero(0);

    return (det > zero ? 1 : (det < zero ? -1 : 0));
}


}


// ----------------------------------------------
// GteLexicoArray2.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

namespace gte
{

// A template class to provide 2D array access that conforms to row-major
// order (RowMajor = true) or column-major order (RowMajor = false).  The
template <bool RowMajor, typename Real, int... Dimensions>
class LexicoArray2 {};

// The array dimensions are known only at run time.
template <typename Real>
class LexicoArray2<true, Real>
{
public:
    inline LexicoArray2(int numRows, int numCols, Real* matrix);

    inline int GetNumRows() const;
    inline int GetNumCols() const;
    inline Real& operator()(int r, int c);
    inline Real const& operator()(int r, int c) const;

private:
    int mNumRows, mNumCols;
    Real* mMatrix;
};

template <typename Real>
class LexicoArray2<false, Real>
{
public:
    inline LexicoArray2(int numRows, int numCols, Real* matrix);

    inline int GetNumRows() const;
    inline int GetNumCols() const;
    inline Real& operator()(int r, int c);
    inline Real const& operator()(int r, int c) const;

private:
    int mNumRows, mNumCols;
    Real* mMatrix;
};

// The array dimensions are known at compile time.
template <typename Real, int NumRows, int NumCols>
class LexicoArray2<true, Real, NumRows, NumCols>
{
public:
    inline LexicoArray2(Real* matrix);

    inline int GetNumRows() const;
    inline int GetNumCols() const;
    inline Real& operator()(int r, int c);
    inline Real const& operator()(int r, int c) const;

private:
    Real* mMatrix;
};

template <typename Real, int NumRows, int NumCols>
class LexicoArray2<false, Real, NumRows, NumCols>
{
public:
    inline LexicoArray2(Real* matrix);

    inline int GetNumRows() const;
    inline int GetNumCols() const;
    inline Real& operator()(int r, int c);
    inline Real const& operator()(int r, int c) const;

private:
    Real* mMatrix;
};


template <typename Real> inline
LexicoArray2<true, Real>::LexicoArray2(int numRows, int numCols, Real* matrix)
    :
    mNumRows(numRows),
    mNumCols(numCols),
    mMatrix(matrix)
{
}

template <typename Real> inline
int LexicoArray2<true, Real>::GetNumRows() const
{
    return mNumRows;
}

template <typename Real> inline
int LexicoArray2<true, Real>::GetNumCols() const
{
    return mNumCols;
}

template <typename Real> inline
Real& LexicoArray2<true, Real>::operator()(int r, int c)
{
    return mMatrix[c + mNumCols*r];
}

template <typename Real> inline
Real const& LexicoArray2<true, Real>::operator()(int r, int c) const
{
    return mMatrix[c + mNumCols*r];
}



template <typename Real> inline
LexicoArray2<false, Real>::LexicoArray2(int numRows, int numCols, Real* matrix)
    :
    mNumRows(numRows),
    mNumCols(numCols),
    mMatrix(matrix)
{
}

template <typename Real> inline
int LexicoArray2<false, Real>::GetNumRows() const
{
    return mNumRows;
}

template <typename Real> inline
int LexicoArray2<false, Real>::GetNumCols() const
{
    return mNumCols;
}

template <typename Real> inline
Real& LexicoArray2<false, Real>::operator()(int r, int c)
{
    return mMatrix[r + mNumRows*c];
}

template <typename Real> inline
Real const& LexicoArray2<false, Real>::operator()(int r, int c) const
{
    return mMatrix[r + mNumRows*c];
}



template <typename Real, int NumRows, int NumCols> inline
LexicoArray2<true, Real, NumRows, NumCols>::LexicoArray2(Real* matrix)
    :
    mMatrix(matrix)
{
}

template <typename Real, int NumRows, int NumCols> inline
int LexicoArray2<true, Real, NumRows, NumCols>::GetNumRows() const
{
    return NumRows;
}

template <typename Real, int NumRows, int NumCols> inline
int LexicoArray2<true, Real, NumRows, NumCols>::GetNumCols() const
{
    return NumCols;
}

template <typename Real, int NumRows, int NumCols> inline
Real& LexicoArray2<true, Real, NumRows, NumCols>::operator()(int r, int c)
{
    return mMatrix[c + NumCols*r];
}

template <typename Real, int NumRows, int NumCols> inline
Real const& LexicoArray2<true, Real, NumRows, NumCols>::operator()(int r, int c) const
{
    return mMatrix[c + NumCols*r];
}



template <typename Real, int NumRows, int NumCols> inline
LexicoArray2<false, Real, NumRows, NumCols>::LexicoArray2(Real* matrix)
    :
    mMatrix(matrix)
{
}

template <typename Real, int NumRows, int NumCols> inline
int LexicoArray2<false, Real, NumRows, NumCols>::GetNumRows() const
{
    return NumRows;
}

template <typename Real, int NumRows, int NumCols> inline
int LexicoArray2<false, Real, NumRows, NumCols>::GetNumCols() const
{
    return NumCols;
}

template <typename Real, int NumRows, int NumCols> inline
Real& LexicoArray2<false, Real, NumRows, NumCols>::operator()(int r, int c)
{
    return mMatrix[r + NumRows*c];
}

template <typename Real, int NumRows, int NumCols> inline
Real const& LexicoArray2<false, Real, NumRows, NumCols>::operator()(int r, int c) const
{
    return mMatrix[r + NumRows*c];
}

}


// ----------------------------------------------
// GteGaussianElimination.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

// The input matrix M must be NxN.  The storage convention for element lookup
// is determined by GTE_USE_ROW_MAJOR or GTE_USE_COL_MAJOR, whichever is
// active.  If you want the inverse of M, pass a nonnull pointer inverseM;
// this matrix must also be NxN and use the same storage convention as M.  If
// you do not want the inverse of M, pass a nullptr for inverseM.  If you want
// to solve M*X = B for X, where X and B are Nx1, pass nonnull pointers for B
// and X.  If you want to solve M*Y = C for Y, where X and C are NxK, pass
// nonnull pointers for C and Y and pass K to numCols.  In all cases, pass
// N to numRows.

namespace gte
{

template <typename Real>
class GaussianElimination
{
public:
    bool operator()(int numRows,
        Real const* M, Real* inverseM, Real& determinant,
        Real const* B, Real* X,
        Real const* C, int numCols, Real* Y) const;

private:
    // Support for copying source to target or to set target to zero.  If
    // source is nullptr, then target is set to zero; otherwise source is
    // copied to target.  This function hides the type traits used to
    // determine whether Real is native floating-point or otherwise (such
    // as BSNumber or BSRational).
    void Set(int numElements, Real const* source, Real* target) const;
};


template <typename Real>
bool GaussianElimination<Real>::operator()(int numRows, Real const* M,
    Real* inverseM, Real& determinant, Real const* B, Real* X, Real const* C,
    int numCols, Real* Y) const
{
    if (numRows <= 0 || !M
        || ((B != nullptr) != (X != nullptr))
        || ((C != nullptr) != (Y != nullptr))
        || (C != nullptr && numCols < 1))
    {
        LogError("Invalid input.");
        return false;
    }

    int numElements = numRows * numRows;
    bool wantInverse = (inverseM != nullptr);
    std::vector<Real> localInverseM;
    if (!wantInverse)
    {
        localInverseM.resize(numElements);
        inverseM = localInverseM.data();
    }
    Set(numElements, M, inverseM);

    if (B)
    {
        Set(numRows, B, X);
    }

    if (C)
    {
        Set(numRows * numCols, C, Y);
    }

#if defined(GTE_USE_ROW_MAJOR)
    LexicoArray2<true, Real> matInvM(numRows, numRows, inverseM);
    LexicoArray2<true, Real> matY(numRows, numCols, Y);
#else
    LexicoArray2<false, Real> matInvM(numRows, numRows, inverseM);
    LexicoArray2<false, Real> matY(numRows, numCols, Y);
#endif

    std::vector<int> colIndex(numRows), rowIndex(numRows), pivoted(numRows);
    std::fill(pivoted.begin(), pivoted.end(), 0);

    Real const zero = (Real)0;
    Real const one = (Real)1;
    bool odd = false;
    determinant = one;

    // Elimination by full pivoting.
    int i1, i2, row = 0, col = 0;
    for (int i0 = 0; i0 < numRows; ++i0)
    {
        // Search matrix (excluding pivoted rows) for maximum absolute entry.
        Real maxValue = zero;
        for (i1 = 0; i1 < numRows; ++i1)
        {
            if (!pivoted[i1])
            {
                for (i2 = 0; i2 < numRows; ++i2)
                {
                    if (!pivoted[i2])
                    {
                        Real absValue = std::abs(matInvM(i1, i2));
                        if (absValue > maxValue)
                        {
                            maxValue = absValue;
                            row = i1;
                            col = i2;
                        }
                    }
                }
            }
        }

        if (maxValue == zero)
        {
            // The matrix is not invertible.
            if (wantInverse)
            {
                Set(numElements, nullptr, inverseM);
            }
            determinant = zero;

            if (B)
            {
                Set(numRows, nullptr, X);
            }

            if (C)
            {
                Set(numRows * numCols, nullptr, Y);
            }
            return false;
        }

        pivoted[col] = true;

        // Swap rows so that the pivot entry is in row 'col'.
        if (row != col)
        {
            odd = !odd;
            for (int i = 0; i < numRows; ++i)
            {
                std::swap(matInvM(row, i), matInvM(col, i));
            }

            if (B)
            {
                std::swap(X[row], X[col]);
            }

            if (C)
            {
                for (int i = 0; i < numCols; ++i)
                {
                    std::swap(matY(row, i), matY(col, i));
                }
            }
        }

        // Keep track of the permutations of the rows.
        rowIndex[i0] = row;
        colIndex[i0] = col;

        // Scale the row so that the pivot entry is 1.
        Real diagonal = matInvM(col, col);
        determinant *= diagonal;
        Real inv = one / diagonal;
        matInvM(col, col) = one;
        for (i2 = 0; i2 < numRows; ++i2)
        {
            matInvM(col, i2) *= inv;
        }

        if (B)
        {
            X[col] *= inv;
        }

        if (C)
        {
            for (i2 = 0; i2 < numCols; ++i2)
            {
                matY(col, i2) *= inv;
            }
        }

        // Zero out the pivot column locations in the other rows.
        for (i1 = 0; i1 < numRows; ++i1)
        {
            if (i1 != col)
            {
                Real save = matInvM(i1, col);
                matInvM(i1, col) = zero;
                for (i2 = 0; i2 < numRows; ++i2)
                {
                    matInvM(i1, i2) -= matInvM(col, i2) * save;
                }

                if (B)
                {
                    X[i1] -= X[col] * save;
                }

                if (C)
                {
                    for (i2 = 0; i2 < numCols; ++i2)
                    {
                        matY(i1, i2) -= matY(col, i2) * save;
                    }
                }
            }
        }
    }

    if (wantInverse)
    {
        // Reorder rows to undo any permutations in Gaussian elimination.
        for (i1 = numRows - 1; i1 >= 0; --i1)
        {
            if (rowIndex[i1] != colIndex[i1])
            {
                for (i2 = 0; i2 < numRows; ++i2)
                {
                    std::swap(matInvM(i2, rowIndex[i1]),
                        matInvM(i2, colIndex[i1]));
                }
            }
        }
    }

    if (odd)
    {
        determinant = -determinant;
    }

    return true;
}

template <typename Real>
void GaussianElimination<Real>::Set(int numElements, Real const* source,
    Real* target) const
{
    if (std::is_floating_point<Real>() == std::true_type())
    {
        // Fast set/copy for native floating-point.
        size_t numBytes = numElements * sizeof(Real);
        if (source)
        {
            Memcpy(target, source, numBytes);
        }
        else
        {
            memset(target, 0, numBytes);
        }
    }
    else
    {
        // The inputs are not std containers, so ensure assignment works
        // correctly.
        if (source)
        {
            for (int i = 0; i < numElements; ++i)
            {
                target[i] = source[i];
            }
        }
        else
        {
            Real const zero = (Real)0;
            for (int i = 0; i < numElements; ++i)
            {
                target[i] = zero;
            }
        }
    }
}


}



// ----------------------------------------------
// GteMatrix.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.2 (2018/10/05)

namespace gte
{

template <int NumRows, int NumCols, typename Real>
class Matrix
{
public:
    // The table is initialized to zero.
    Matrix();

    // The table is fully initialized by the inputs.  The 'values' must be
    // specified in row-major order, regardless of the active storage scheme
    // (GTE_USE_ROW_MAJOR or GTE_USE_COL_MAJOR).
    Matrix(std::array<Real, NumRows*NumCols> const& values);

    // At most NumRows*NumCols are copied from the initializer list, setting
    // any remaining elements to zero.  The 'values' must be specified in
    // row-major order, regardless of the active storage scheme
    // (GTE_USE_ROW_MAJOR or GTE_USE_COL_MAJOR).  Create the zero matrix using
    // the syntax
    //   Matrix<NumRows,NumCols,Real> zero{(Real)0};
    // WARNING: The C++ 11 specification states that
    //   Matrix<NumRows,NumCols,Real> zero{};
    // will lead to a call of the default constructor, not the initializer
    // constructor!
    Matrix(std::initializer_list<Real> values);

    // For 0 <= r < NumRows and 0 <= c < NumCols, element (r,c) is 1 and all
    // others are 0.  If either of r or c is invalid, the zero matrix is
    // created.  This is a convenience for creating the standard Euclidean
    // basis matrices; see also MakeUnit(int,int) and Unit(int,int).
    Matrix(int r, int c);

    // The copy constructor, destructor, and assignment operator are generated
    // by the compiler.

    // Member access for which the storage representation is transparent.  The
    // matrix entry in row r and column c is A(r,c).  The first operator()
    // returns a const reference rather than a Real value.  This supports
    // writing via standard file operations that require a const pointer to
    // data.
    inline Real const& operator()(int r, int c) const;
    inline Real& operator()(int r, int c);

    // Member access by rows or by columns.
    void SetRow(int r, Vector<NumCols,Real> const& vec);
    void SetCol(int c, Vector<NumRows,Real> const& vec);
    Vector<NumCols,Real> GetRow(int r) const;
    Vector<NumRows,Real> GetCol(int c) const;

    // Member access by 1-dimensional index.  NOTE: These accessors are
    // useful for the manipulation of matrix entries when it does not
    // matter whether storage is row-major or column-major.  Do not use
    // constructs such as M[c+NumCols*r] or M[r+NumRows*c] that expose the
    // storage convention.
    inline Real const& operator[](int i) const;
    inline Real& operator[](int i);

    // Comparisons for sorted containers and geometric ordering.
    inline bool operator==(Matrix const& mat) const;
    inline bool operator!=(Matrix const& mat) const;
    inline bool operator< (Matrix const& mat) const;
    inline bool operator<=(Matrix const& mat) const;
    inline bool operator> (Matrix const& mat) const;
    inline bool operator>=(Matrix const& mat) const;

    // Special matrices.
    void MakeZero();  // All components are 0.
    void MakeUnit(int r, int c);  // Component (r,c) is 1, all others zero.
    void MakeIdentity();  // Diagonal entries 1, others 0, even when nonsquare
    static Matrix Zero();
    static Matrix Unit(int r, int c);
    static Matrix Identity();

protected:
    class Table
    {
    public:
        // Storage-order-independent element access as 2D array.
        inline Real const& operator()(int r, int c) const;
        inline Real& operator()(int r, int c);

        // Element access as 1D array.  Use this internally only when
        // the 2D storage order is not relevant.
        inline Real const& operator[](int i) const;
        inline Real& operator[](int i);

#if defined(GTE_USE_ROW_MAJOR)
        std::array<std::array<Real,NumCols>,NumRows> mStorage;
#else
        std::array<std::array<Real,NumRows>,NumCols> mStorage;
#endif
    };

    Table mTable;
};

// Unary operations.
template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>
operator+(Matrix<NumRows,NumCols,Real> const& M);

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>
operator-(Matrix<NumRows,NumCols,Real> const& M);

// Linear-algebraic operations.
template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>
operator+(
    Matrix<NumRows,NumCols,Real> const& M0,
    Matrix<NumRows,NumCols,Real> const& M1);

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>
operator-(
    Matrix<NumRows,NumCols,Real> const& M0,
    Matrix<NumRows,NumCols,Real> const& M1);

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>
operator*(Matrix<NumRows,NumCols,Real> const& M, Real scalar);

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>
operator*(Real scalar, Matrix<NumRows,NumCols,Real> const& M);

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>
operator/(Matrix<NumRows,NumCols,Real> const& M, Real scalar);

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>&
operator+=(
    Matrix<NumRows,NumCols,Real>& M0,
    Matrix<NumRows,NumCols,Real> const& M1);

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>&
operator-=(
    Matrix<NumRows,NumCols,Real>& M0,
    Matrix<NumRows,NumCols,Real> const& M1);

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>&
operator*=(Matrix<NumRows,NumCols,Real>& M, Real scalar);

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>&
operator/=(Matrix<NumRows,NumCols,Real>& M, Real scalar);

// Geometric operations.
template <int NumRows, int NumCols, typename Real>
Real L1Norm(Matrix<NumRows,NumCols,Real> const& M);

template <int NumRows, int NumCols, typename Real>
Real L2Norm(Matrix<NumRows,NumCols,Real> const& M);

template <int NumRows, int NumCols, typename Real>
Real LInfinityNorm(Matrix<NumRows,NumCols,Real> const& M);

template <int N, typename Real>
Matrix<N,N,Real> Inverse(Matrix<N,N,Real> const& M,
    bool* reportInvertibility = nullptr);

template <int N, typename Real>
Real Determinant(Matrix<N, N, Real> const& M);

// M^T
template <int NumRows, int NumCols, typename Real>
Matrix<NumCols,NumRows,Real>
Transpose(Matrix<NumRows,NumCols,Real> const& M);

// M*V
template <int NumRows, int NumCols, typename Real>
Vector<NumRows,Real>
operator*(
    Matrix<NumRows,NumCols,Real> const& M,
    Vector<NumCols,Real> const& V);

// V^T*M
template <int NumRows, int NumCols, typename Real>
Vector<NumCols,Real>
operator*(
    Vector<NumRows,Real> const& V,
    Matrix<NumRows,NumCols,Real> const& M);

// A*B
template <int NumRows, int NumCols, int NumCommon, typename Real>
Matrix<NumRows,NumCols,Real>
operator*(
    Matrix<NumRows,NumCommon,Real> const& A,
    Matrix<NumCommon,NumCols,Real> const& B);

template <int NumRows, int NumCols, int NumCommon, typename Real>
Matrix<NumRows,NumCols,Real>
MultiplyAB(
    Matrix<NumRows,NumCommon,Real> const& A,
    Matrix<NumCommon,NumCols,Real> const& B);

// A*B^T
template <int NumRows, int NumCols, int NumCommon, typename Real>
Matrix<NumRows,NumCols,Real>
MultiplyABT(
    Matrix<NumRows,NumCommon,Real> const& A,
    Matrix<NumCols,NumCommon,Real> const& B);

// A^T*B
template <int NumRows, int NumCols, int NumCommon, typename Real>
Matrix<NumRows,NumCols,Real>
MultiplyATB(
    Matrix<NumCommon,NumRows,Real> const& A,
    Matrix<NumCommon,NumCols,Real> const& B);

// A^T*B^T
template <int NumRows, int NumCols, int NumCommon, typename Real>
Matrix<NumRows,NumCols,Real>
MultiplyATBT(
    Matrix<NumCommon,NumRows,Real> const& A,
    Matrix<NumCols,NumCommon,Real> const& B);

// M*D, D is diagonal NumCols-by-NumCols
template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>
MultiplyMD(
    Matrix<NumRows,NumCols,Real> const& M,
    Vector<NumCols,Real> const& D);

// D*M, D is diagonal NumRows-by-NumRows
template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>
MultiplyDM(
    Vector<NumRows,Real> const& D,
    Matrix<NumRows,NumCols,Real> const& M);

// U*V^T, U is NumRows-by-1, V is Num-Cols-by-1, result is NumRows-by-NumCols.
template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>
OuterProduct(Vector<NumRows, Real> const& U, Vector<NumCols, Real> const& V);

// Initialization to a diagonal matrix whose diagonal entries are the
// components of D.
template <int N, typename Real>
void MakeDiagonal(Vector<N, Real> const& D, Matrix<N, N, Real>& M);

// Create an (N+1)-by-(N+1) matrix H by setting the upper N-by-N block to the
// input N-by-N matrix and all other entries to 0 except for the last row
// and last column entry which is set to 1.
template <int N, typename Real>
Matrix<N + 1, N+1, Real> HLift(Matrix<N, N, Real> const& M);

// Extract the upper (N-1)-by-(N-1) block of the input N-by-N matrix.
template <int N, typename Real>
Matrix<N - 1, N - 1, Real> HProject(Matrix<N, N, Real> const& M);


template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>::Matrix()
{
    MakeZero();
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>::Matrix(
    std::array<Real, NumRows*NumCols> const& values)
{
    for (int r = 0, i = 0; r < NumRows; ++r)
    {
        for (int c = 0; c < NumCols; ++c, ++i)
        {
            mTable(r, c) = values[i];
        }
    }
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>::Matrix(std::initializer_list<Real> values)
{
    int const numValues = static_cast<int>(values.size());
    auto iter = values.begin();
    int r, c, i;
    for (r = 0, i = 0; r < NumRows; ++r)
    {
        for (c = 0; c < NumCols; ++c, ++i)
        {
            if (i < numValues)
            {
                mTable(r, c) = *iter++;
            }
            else
            {
                break;
            }
        }

        if (c < NumCols)
        {
            // Fill in the remaining columns of the current row with zeros.
            for (/**/; c < NumCols; ++c)
            {
                mTable(r, c) = (Real)0;
            }
            ++r;
            break;
        }
    }

    if (r < NumRows)
    {
        // Fill in the remain rows with zeros.
        for (/**/; r < NumRows; ++r)
        {
            for (c = 0; c < NumCols; ++c)
            {
                mTable(r, c) = (Real)0;
            }
        }
    }
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>::Matrix(int r, int c)
{
    MakeUnit(r, c);
}

template <int NumRows, int NumCols, typename Real> inline
Real const& Matrix<NumRows, NumCols, Real>::operator()(int r, int c) const
{
    return mTable(r, c);
}

template <int NumRows, int NumCols, typename Real> inline
Real& Matrix<NumRows, NumCols, Real>::operator()(int r, int c)
{
    return mTable(r, c);
}

template <int NumRows, int NumCols, typename Real>
void Matrix<NumRows, NumCols, Real>::SetRow(int r,
    Vector<NumCols, Real> const& vec)
{
    for (int c = 0; c < NumCols; ++c)
    {
        mTable(r, c) = vec[c];
    }
}

template <int NumRows, int NumCols, typename Real>
void Matrix<NumRows, NumCols, Real>::SetCol(int c,
    Vector<NumRows, Real> const& vec)
{
    for (int r = 0; r < NumRows; ++r)
    {
        mTable(r, c) = vec[r];
    }
}

template <int NumRows, int NumCols, typename Real>
Vector<NumCols, Real> Matrix<NumRows, NumCols, Real>::GetRow(int r) const
{
    Vector<NumCols, Real> vec;
    for (int c = 0; c < NumCols; ++c)
    {
        vec[c] = mTable(r, c);
    }
    return vec;
}

template <int NumRows, int NumCols, typename Real>
Vector<NumRows, Real> Matrix<NumRows, NumCols, Real>::GetCol(int c) const
{
    Vector<NumRows, Real> vec;
    for (int r = 0; r < NumRows; ++r)
    {
        vec[r] = mTable(r, c);
    }
    return vec;
}

template <int NumRows, int NumCols, typename Real> inline
Real const& Matrix<NumRows, NumCols, Real>::operator[](int i) const
{
    return mTable[i];
}

template <int NumRows, int NumCols, typename Real> inline
Real& Matrix<NumRows, NumCols, Real>::operator[](int i)
{
    return mTable[i];
}

template <int NumRows, int NumCols, typename Real> inline
bool Matrix<NumRows, NumCols, Real>::operator==(Matrix const& mat) const
{
    return mTable.mStorage == mat.mTable.mStorage;
}

template <int NumRows, int NumCols, typename Real> inline
bool Matrix<NumRows, NumCols, Real>::operator!=(Matrix const& mat) const
{
    return mTable.mStorage != mat.mTable.mStorage;
}

template <int NumRows, int NumCols, typename Real> inline
bool Matrix<NumRows, NumCols, Real>::operator<(Matrix const& mat) const
{
    return mTable.mStorage < mat.mTable.mStorage;
}

template <int NumRows, int NumCols, typename Real> inline
bool Matrix<NumRows, NumCols, Real>::operator<=(Matrix const& mat) const
{
    return mTable.mStorage <= mat.mTable.mStorage;
}

template <int NumRows, int NumCols, typename Real> inline
bool Matrix<NumRows, NumCols, Real>::operator>(Matrix const& mat) const
{
    return mTable.mStorage > mat.mTable.mStorage;
}

template <int NumRows, int NumCols, typename Real> inline
bool Matrix<NumRows, NumCols, Real>::operator>=(Matrix const& mat) const
{
    return mTable.mStorage >= mat.mTable.mStorage;
}

template <int NumRows, int NumCols, typename Real>
void Matrix<NumRows, NumCols, Real>::MakeZero()
{
    Real const zero = (Real)0;
    for (int i = 0; i < NumRows * NumCols; ++i)
    {
        mTable[i] = zero;
    }
}

template <int NumRows, int NumCols, typename Real>
void Matrix<NumRows, NumCols, Real>::MakeUnit(int r, int c)
{
    MakeZero();
    if (0 <= r && r < NumRows && 0 <= c && c < NumCols)
    {
        mTable(r, c) = (Real)1;
    }
}

template <int NumRows, int NumCols, typename Real>
void Matrix<NumRows, NumCols, Real>::MakeIdentity()
{
    MakeZero();
    int const numDiagonal = (NumRows <= NumCols ? NumRows : NumCols);
    for (int i = 0; i < numDiagonal; ++i)
    {
        mTable(i, i) = (Real)1;
    }
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real> Matrix<NumRows, NumCols, Real>::Zero()
{
    Matrix M;
    M.MakeZero();
    return M;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real> Matrix<NumRows, NumCols, Real>::Unit(int r,
    int c)
{
    Matrix M;
    M.MakeUnit(r, c);
    return M;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real> Matrix<NumRows, NumCols, Real>::Identity()
{
    Matrix M;
    M.MakeIdentity();
    return M;
}



template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>
operator+(Matrix<NumRows, NumCols, Real> const& M)
{
    return M;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>
operator-(Matrix<NumRows, NumCols, Real> const& M)
{
    Matrix<NumRows, NumCols, Real> result;
    for (int i = 0; i < NumRows*NumCols; ++i)
    {
        result[i] = -M[i];
    }
    return result;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>
operator+(
    Matrix<NumRows, NumCols, Real> const& M0,
    Matrix<NumRows, NumCols, Real> const& M1)
{
    Matrix<NumRows, NumCols, Real> result = M0;
    return result += M1;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>
operator-(
    Matrix<NumRows, NumCols, Real> const& M0,
    Matrix<NumRows, NumCols, Real> const& M1)
{
    Matrix<NumRows, NumCols, Real> result = M0;
    return result -= M1;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>
operator*(Matrix<NumRows, NumCols, Real> const& M, Real scalar)
{
    Matrix<NumRows, NumCols, Real> result = M;
    return result *= scalar;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>
operator*(Real scalar, Matrix<NumRows, NumCols, Real> const& M)
{
    Matrix<NumRows, NumCols, Real> result = M;
    return result *= scalar;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>
operator/(Matrix<NumRows, NumCols, Real> const& M, Real scalar)
{
    Matrix<NumRows, NumCols, Real> result = M;
    return result /= scalar;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>&
operator+=(
    Matrix<NumRows, NumCols, Real>& M0,
    Matrix<NumRows, NumCols, Real> const& M1)
{
    for (int i = 0; i < NumRows*NumCols; ++i)
    {
        M0[i] += M1[i];
    }
    return M0;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>&
operator-=(
    Matrix<NumRows, NumCols, Real>& M0,
    Matrix<NumRows, NumCols, Real> const& M1)
{
    for (int i = 0; i < NumRows*NumCols; ++i)
    {
        M0[i] -= M1[i];
    }
    return M0;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>&
operator*=(Matrix<NumRows, NumCols, Real>& M, Real scalar)
{
    for (int i = 0; i < NumRows*NumCols; ++i)
    {
        M[i] *= scalar;
    }
    return M;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>&
operator/=(Matrix<NumRows, NumCols, Real>& M, Real scalar)
{
    if (scalar != (Real)0)
    {
        Real invScalar = ((Real)1) / scalar;
        for (int i = 0; i < NumRows*NumCols; ++i)
        {
            M[i] *= invScalar;
        }
    }
    else
    {
        for (int i = 0; i < NumRows*NumCols; ++i)
        {
            M[i] = (Real)0;
        }
    }
    return M;
}

template <int NumRows, int NumCols, typename Real>
Real L1Norm(Matrix<NumRows, NumCols, Real> const& M)
{
    Real sum = std::abs(M[0]);
    for (int i = 1; i < NumRows*NumCols; ++i)
    {
        sum += std::abs(M[i]);
    }
    return sum;
}

template <int NumRows, int NumCols, typename Real>
Real L2Norm(Matrix<NumRows, NumCols, Real> const& M)
{
    Real sum = M[0] * M[0];
    for (int i = 1; i < NumRows*NumCols; ++i)
    {
        sum += M[i] * M[i];
    }
    return std::sqrt(sum);
}

template <int NumRows, int NumCols, typename Real>
Real LInfinityNorm(Matrix<NumRows, NumCols, Real> const& M)
{
    Real maxAbsElement = M[0];
    for (int i = 1; i < NumRows*NumCols; ++i)
    {
        Real absElement = std::abs(M[i]);
        if (absElement > maxAbsElement)
        {
            maxAbsElement = absElement;
        }
    }
    return maxAbsElement;
}

template <int N, typename Real>
Matrix<N, N, Real> Inverse(Matrix<N, N, Real> const& M,
    bool* reportInvertibility)
{
    Matrix<N, N, Real> invM;
    Real determinant;
    bool invertible = GaussianElimination<Real>()(N, &M[0], &invM[0],
        determinant, nullptr, nullptr, nullptr, 0, nullptr);
    if (reportInvertibility)
    {
        *reportInvertibility = invertible;
    }
    return invM;
}

template <int N, typename Real>
Real Determinant(Matrix<N, N, Real> const& M)
{
    Real determinant;
    GaussianElimination<Real>()(N, &M[0], nullptr, determinant, nullptr,
        nullptr, nullptr, 0, nullptr);
    return determinant;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumCols, NumRows, Real>
Transpose(Matrix<NumRows, NumCols, Real> const& M)
{
    Matrix<NumCols, NumRows, Real> result;
    for (int r = 0; r < NumRows; ++r)
    {
        for (int c = 0; c < NumCols; ++c)
        {
            result(c, r) = M(r, c);
        }
    }
    return result;
}

template <int NumRows, int NumCols, typename Real>
Vector<NumRows, Real>
operator*(
    Matrix<NumRows, NumCols, Real> const& M,
    Vector<NumCols, Real> const& V)
{
    Vector<NumRows, Real> result;
    for (int r = 0; r < NumRows; ++r)
    {
        result[r] = (Real)0;
        for (int c = 0; c < NumCols; ++c)
        {
            result[r] += M(r, c) * V[c];
        }
    }
    return result;
}

template <int NumRows, int NumCols, typename Real>
Vector<NumCols, Real> operator*(Vector<NumRows, Real> const& V,
    Matrix<NumRows, NumCols, Real> const& M)
{
    Vector<NumCols, Real> result;
    for (int c = 0; c < NumCols; ++c)
    {
        result[c] = (Real)0;
        for (int r = 0; r < NumRows; ++r)
        {
            result[c] += V[r] * M(r, c);
        }
    }
    return result;
}

template <int NumRows, int NumCols, int NumCommon, typename Real>
Matrix<NumRows, NumCols, Real>
operator*(
    Matrix<NumRows, NumCommon, Real> const& A,
    Matrix<NumCommon, NumCols, Real> const& B)
{
    return MultiplyAB(A, B);
}

template <int NumRows, int NumCols, int NumCommon, typename Real>
Matrix<NumRows, NumCols, Real>
MultiplyAB(
    Matrix<NumRows, NumCommon, Real> const& A,
    Matrix<NumCommon, NumCols, Real> const& B)
{
    Matrix<NumRows, NumCols, Real> result;
    for (int r = 0; r < NumRows; ++r)
    {
        for (int c = 0; c < NumCols; ++c)
        {
            result(r, c) = (Real)0;
            for (int i = 0; i < NumCommon; ++i)
            {
                result(r, c) += A(r, i) * B(i, c);
            }
        }
    }
    return result;
}

template <int NumRows, int NumCols, int NumCommon, typename Real>
Matrix<NumRows, NumCols, Real>
MultiplyABT(
    Matrix<NumRows, NumCommon, Real> const& A,
    Matrix<NumCols, NumCommon, Real> const& B)
{
    Matrix<NumRows, NumCols, Real> result;
    for (int r = 0; r < NumRows; ++r)
    {
        for (int c = 0; c < NumCols; ++c)
        {
            result(r, c) = (Real)0;
            for (int i = 0; i < NumCommon; ++i)
            {
                result(r, c) += A(r, i) * B(c, i);
            }
        }
    }
    return result;
}

template <int NumRows, int NumCols, int NumCommon, typename Real>
Matrix<NumRows, NumCols, Real>
MultiplyATB(
    Matrix<NumCommon, NumRows, Real> const& A,
    Matrix<NumCommon, NumCols, Real> const& B)
{
    Matrix<NumRows, NumCols, Real> result;
    for (int r = 0; r < NumRows; ++r)
    {
        for (int c = 0; c < NumCols; ++c)
        {
            result(r, c) = (Real)0;
            for (int i = 0; i < NumCommon; ++i)
            {
                result(r, c) += A(i, r) * B(i, c);
            }
        }
    }
    return result;
}

template <int NumRows, int NumCols, int NumCommon, typename Real>
Matrix<NumRows, NumCols, Real>
MultiplyATBT(
    Matrix<NumCommon, NumRows, Real> const& A,
    Matrix<NumCols, NumCommon, Real> const& B)
{
    Matrix<NumRows, NumCols, Real> result;
    for (int r = 0; r < NumRows; ++r)
    {
        for (int c = 0; c < NumCols; ++c)
        {
            result(r, c) = (Real)0;
            for (int i = 0; i < NumCommon; ++i)
            {
                result(r, c) += A(i, r) * B(c, i);
            }
        }
    }
    return result;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>
MultiplyMD(
    Matrix<NumRows, NumCols, Real> const& M,
    Vector<NumCols, Real> const& D)
{
    Matrix<NumRows, NumCols, Real> result;
    for (int r = 0; r < NumRows; ++r)
    {
        for (int c = 0; c < NumCols; ++c)
        {
            result(r, c) = M(r, c) * D[c];
        }
    }
    return result;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>
MultiplyDM(
    Vector<NumRows, Real> const& D,
    Matrix<NumRows, NumCols, Real> const& M)
{
    Matrix<NumRows, NumCols, Real> result;
    for (int r = 0; r < NumRows; ++r)
    {
        for (int c = 0; c < NumCols; ++c)
        {
            result(r, c) = D[r] * M(r, c);
        }
    }
    return result;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>
OuterProduct(Vector<NumRows, Real> const& U, Vector<NumCols, Real> const& V)
{
    Matrix<NumRows, NumCols, Real> result;
    for (int r = 0; r < NumRows; ++r)
    {
        for (int c = 0; c < NumCols; ++c)
        {
            result(r, c) = U[r] * V[c];
        }
    }
    return result;
}

template <int N, typename Real>
void MakeDiagonal(Vector<N, Real> const& D, Matrix<N, N, Real>& M)
{
    for (int i = 0; i < N*N; ++i)
    {
        M[i] = (Real)0;
    }

    for (int i = 0; i < N; ++i)
    {
        M(i, i) = D[i];
    }
}

template <int N, typename Real>
Matrix<N + 1, N + 1, Real> HLift(Matrix<N, N, Real> const& M)
{
    Matrix<N + 1, N + 1, Real> result;
    result.MakeIdentity();
    for (int r = 0; r < N; ++r)
    {
        for (int c = 0; c < N; ++c)
        {
            result(r, c) = M(r, c);
        }
    }
    return result;
}

// Extract the upper (N-1)-by-(N-1) block of the input N-by-N matrix.
template <int N, typename Real>
Matrix<N - 1, N - 1, Real> HProject(Matrix<N, N, Real> const& M)
{
    static_assert(N >= 2, "Invalid matrix dimension.");
    Matrix<N - 1, N - 1, Real> result;
    for (int r = 0; r < N - 1; ++r)
    {
        for (int c = 0; c < N - 1; ++c)
        {
            result(r, c) = M(r, c);
        }
    }
    return result;
}


// Matrix<N,C,Real>::Table

template <int NumRows, int NumCols, typename Real> inline
Real const& Matrix<NumRows, NumCols, Real>::Table::operator()(int r, int c)
    const
{
#if defined(GTE_USE_ROW_MAJOR)
    return mStorage[r][c];
#else
    return mStorage[c][r];
#endif
}

template <int NumRows, int NumCols, typename Real> inline
Real& Matrix<NumRows, NumCols, Real>::Table::operator()(int r, int c)
{
#if defined(GTE_USE_ROW_MAJOR)
    return mStorage[r][c];
#else
    return mStorage[c][r];
#endif
}

template <int NumRows, int NumCols, typename Real> inline
Real const& Matrix<NumRows, NumCols, Real>::Table::operator[](int i) const
{
    Real const* elements = &mStorage[0][0];
    return elements[i];
}

template <int NumRows, int NumCols, typename Real> inline
Real& Matrix<NumRows, NumCols, Real>::Table::operator[](int i)
{
    Real* elements = &mStorage[0][0];
    return elements[i];
}


}


// ----------------------------------------------
// GteRangeIteration.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

// For information on range-based for-loops, see
// http://en.cppreference.com/w/cpp/language/range-for

namespace gte
{

// The function gte::reverse supports reverse iteration in range-based
// for-loops using the auto keyword.  For example,
//
//   std::vector<int> numbers(4);
//   int i = 0;
//   for (auto& number : numbers)
//   {
//       number = i++;
//       std::cout << number << ' ';
//   }
//   // Output:  0 1 2 3
//
//   for (auto& number : gte::reverse(numbers))
//   {
//       std::cout << number << ' ';
//   }
//   // Output:  3 2 1 0

template <typename Iterator>
class ReversalObject
{
public:
    ReversalObject(Iterator begin, Iterator end)
        :
        mBegin(begin),
        mEnd(end)
    {
    }

    Iterator begin() const { return mBegin; }
    Iterator end() const { return mEnd; }

private:
    Iterator mBegin, mEnd;
};

template
<
    typename Iterable,
    typename Iterator = decltype(std::begin(std::declval<Iterable>())),
    typename ReverseIterator = std::reverse_iterator<Iterator>
>
ReversalObject<ReverseIterator> reverse(Iterable&& range)
{
    return ReversalObject<ReverseIterator>(
        ReverseIterator(std::end(range)),
        ReverseIterator(std::begin(range)));
}

}


// ----------------------------------------------
// GteWrapper.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

// Wrappers around platform-specific low-level library calls.

namespace gte
{

void Memcpy(void* target, void const* source, size_t count);
void Memcpy(wchar_t* target, wchar_t const* source, size_t count);

}

// ----------------------------------------------
// GteSingularValueDecomposition.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2018/10/05)

// The SingularValueDecomposition class is an implementation of Algorithm
// 8.3.2 (The SVD Algorithm) described in "Matrix Computations, 2nd
// edition" by G. H. Golub and Charles F. Van Loan, The Johns Hopkins
// Press, Baltimore MD, Fourth Printing 1993.  Algorithm 5.4.2 (Householder
// Bidiagonalization) is used to reduce A to bidiagonal B.  Algorithm 8.3.1
// (Golub-Kahan SVD Step) is used for the iterative reduction from bidiagonal
// to diagonal.  If A is the original matrix, S is the matrix whose diagonal
// entries are the singular values, and U and V are corresponding matrices,
// then theoretically U^T*A*V = S.  Numerically, we have errors
// E = U^T*A*V - S.  Algorithm 8.3.2 mentions that one expects |E| is
// approximately u*|A|, where |M| denotes the Frobenius norm of M and where
// u is the unit roundoff for the floating-point arithmetic: 2^{-23} for
// 'float', which is FLT_EPSILON = 1.192092896e-7f, and 2^{-52} for'double',
// which is DBL_EPSILON = 2.2204460492503131e-16.
//
// The condition |a(i,i+1)| <= epsilon*(|a(i,i) + a(i+1,i+1)|) used to
// determine when the reduction decouples to smaller problems is implemented
// as:  sum = |a(i,i)| + |a(i+1,i+1)|; sum + |a(i,i+1)| == sum.  The idea is
// that the superdiagonal term is small relative to its diagonal neighbors,
// and so it is effectively zero.  The unit tests have shown that this
// interpretation of decoupling is effective.
//
// The condition |a(i,i)| <= epsilon*|B| used to determine when the
// reduction decouples (with a zero singular value) is implemented using
// the Frobenius norm of B and epsilon = multiplier*u, where for now the
// multiplier is hard-coded in Solve(...) as 8.
//
// The authors suggest that once you have the bidiagonal matrix, a practical
// implementation will store the diagonal and superdiagonal entries in linear
// arrays, ignoring the theoretically zero values not in the 2-band.  This is
// good for cache coherence, and we have used the suggestion.  The essential
// parts of the Householder u-vectors are stored in the lower-triangular
// portion of the matrix and the essential parts of the Householder v-vectors
// are stored in the upper-triangular portion of the matrix.  To avoid having
// to recompute 2/Dot(u,u) and 2/Dot(v,v) when constructing orthogonal U and
// V, we store these quantities in additional memory during bidiagonalization.
//
// For matrices with randomly generated values in [0,1], the unit tests
// produce the following information for N-by-N matrices.
//
// N  |A|     |E|        |E|/|A|    iterations
// -------------------------------------------
//  2  1.4831 4.1540e-16 2.8007e-16  1
//  3  2.1065 3.5024e-16 1.6626e-16  4
//  4  2.4979 7.4605e-16 2.9867e-16  6
//  5  3.6591 1.8305e-15 5.0025e-16  9
//  6  4.0572 2.0571e-15 5.0702e-16 10
//  7  4.7745 2.9057e-15 6.0859e-16 12
//  8  5.1964 2.7958e-15 5.3803e-16 13
//  9  5.7599 3.3128e-15 5.7514e-16 16
// 10  6.2700 3.7209e-15 5.9344e-16 16
// 11  6.8220 5.0580e-15 7.4142e-16 18
// 12  7.4540 5.2493e-15 7.0422e-16 21
// 13  8.1225 5.6043e-15 6.8997e-16 24
// 14  8.5883 5.8553e-15 6.8177e-16 26
// 15  9.1337 6.9663e-15 7.6270e-16 27
// 16  9.7884 9.1358e-15 9.3333e-16 29
// 17 10.2407 8.2715e-15 8.0771e-16 34
// 18 10.7147 8.9748e-15 8.3761e-16 33
// 19 11.1887 1.0094e-14 9.0220e-16 32
// 20 11.7739 9.7000e-15 8.2386e-16 35
// 21 12.2822 1.1217e-14 9.1329e-16 36
// 22 12.7649 1.1071e-14 8.6732e-16 37
// 23 13.3366 1.1271e-14 8.4513e-16 41
// 24 13.8505 1.2806e-14 9.2460e-16 43
// 25 14.4332 1.3081e-14 9.0637e-16 43
// 26 14.9301 1.4882e-14 9.9680e-16 46
// 27 15.5214 1.5571e-14 1.0032e-15 48
// 28 16.1029 1.7553e-14 1.0900e-15 49
// 29 16.6407 1.6219e-14 9.7467e-16 53
// 30 17.1891 1.8560e-14 1.0797e-15 55
// 31 17.7773 1.8522e-14 1.0419e-15 56
//
// The singularvalues and |E|/|A| values were compared to those generated by
// Mathematica Version 9.0; Wolfram Research, Inc., Champaign IL, 2012.
// The results were all comparable with singular values agreeing to a large
// number of decimal places.
//
// Timing on an Intel (R) Core (TM) i7-3930K CPU @ 3.20 GHZ (in seconds)
// for NxN matrices:
//
// N    |E|/|A|    iters bidiag  QR     U-and-V    comperr
// -------------------------------------------------------
//  512 3.8632e-15  848   0.341  0.016    1.844      2.203
// 1024 5.6456e-15 1654   4.279  0.032   18.765     20.844 
// 2048 7.5499e-15 3250  40.421  0.141  186.607    213.216
//
// where iters is the number of QR steps taken, bidiag is the computation
// of the Householder reflection vectors, U-and-V is the composition of
// Householder reflections and Givens rotations to obtain the orthogonal
// matrices of the decomposigion, and comperr is the computation E =
// U^T*A*V - S.

namespace gte
{

template <typename Real>
class SingularValueDecomposition
{
public:
    // The solver processes MxN symmetric matrices, where M >= N > 1
    // ('numRows' is M and 'numCols' is N) and the matrix is stored in
    // row-major order.  The maximum number of iterations ('maxIterations')
    // must be specified for the reduction of a bidiagonal matrix to a
    // diagonal matrix.  The goal is to compute MxM orthogonal U, NxN
    // orthogonal V, and MxN matrix S for which U^T*A*V = S.  The only
    // nonzero entries of S are on the diagonal; the diagonal entries are
    // the singular values of the original matrix.
    SingularValueDecomposition(int numRows, int numCols,
        unsigned int maxIterations);

    // A copy of the MxN input is made internally.  The order of the singular
    // values is specified by sortType: -1 (decreasing), 0 (no sorting), or +1
    // (increasing).  When sorted, the columns of the orthogonal matrices
    // are ordered accordingly.  The return value is the number of iterations
    // consumed when convergence occurred, 0xFFFFFFFF when convergence did not
    // occur or 0 when N <= 1 or M < N was passed to the constructor.
    unsigned int Solve(Real const* input, int sortType);

    // Get the singular values of the matrix passed to Solve(...).  The input
    // 'singularValues' must have N elements.
    void GetSingularValues(Real* singularValues) const;

    // Accumulate the Householder reflections, the Givens rotations, and the
    // diagonal fix-up matrix to compute the orthogonal matrices U and V for
    // which U^T*A*V = S.  The input uMatrix must be MxM and the input vMatrix
    // must be NxN, both stored in row-major order.
    void GetU(Real* uMatrix) const;
    void GetV(Real* vMatrix) const;

    // Compute a single column of U or V.  The reflections and rotations are
    // applied incrementally.  This is useful when you want only a small
    // number of the singular values or vectors.
    void GetUColumn(int index, Real* uColumn) const;
    void GetVColumn(int index, Real* vColumn) const;
    Real GetSingularValue(int index) const;

private:
    // Bidiagonalize using Householder reflections.  On input, mMatrix is a
    // copy of the input matrix and has one extra row.  On output, the
    // diagonal and superdiagonal contain the bidiagonalized results.  The
    // lower-triangular portion stores the essential parts of the Householder
    // u vectors (the elements of u after the leading 1-valued component) and
    // the upper-triangular portion stores the essential parts of the
    // Householder v vectors.  To avoid recomputing 2/Dot(u,u) and 2/Dot(v,v),
    // these quantities are stored in mTwoInvUTU and mTwoInvVTV.
    void Bidiagonalize();

    // A helper for generating Givens rotation sine and cosine robustly.
    void GetSinCos(Real u, Real v, Real& cs, Real& sn);

    // Test for (effectively) zero-valued diagonal entries (through all but
    // the last).  For each such entry, the B matrix decouples.  Perform
    // that decoupling.  If there are no zero-valued entries, then the
    // Golub-Kahan step must be performed.
    bool DiagonalEntriesNonzero(int imin, int imax, Real threshold);

    // This is Algorithm 8.3.1 in "Matrix Computations, 2nd edition" by
    // G. H. Golub and C. F. Van Loan.
    void DoGolubKahanStep(int imin, int imax);

    // The diagonal entries are not guaranteed to be nonnegative during the
    // construction.  After convergence to a diagonal matrix S, test for
    // negative entries and build a diagonal matrix that reverses the sign
    // on the S-entry.
    void EnsureNonnegativeDiagonal();

    // Sort the singular values and compute the corresponding permutation of
    // the indices of the array storing the singular values.  The permutation
    // is used for reordering the singular values and the corresponding
    // columns of the orthogonal matrix in the calls to GetSingularValues(...)
    // and GetOrthogonalMatrices(...).
    void ComputePermutation(int sortType);

    // The number rows and columns of the matrices to be processed.
    int mNumRows, mNumCols;

    // The maximum number of iterations for reducing the bidiagonal matrix
    // to a diagonal matrix.
    unsigned int mMaxIterations;

    // The internal copy of a matrix passed to the solver.  See the comments
    // about function Bidiagonalize() about what is stored in the matrix.
    std::vector<Real> mMatrix;  // MxN elements

    // After the initial bidiagonalization by Householder reflections, we no
    // longer need the full mMatrix.  Copy the diagonal and superdiagonal
    // entries to linear arrays in order to be cache friendly.
    std::vector<Real> mDiagonal;  // N elements
    std::vector<Real> mSuperdiagonal;  // N-1 elements

    // The Givens rotations used to reduce the initial bidiagonal matrix to
    // a diagonal matrix.  A rotation is the identity with the following
    // replacement entries:  R(index0,index0) = cs, R(index0,index1) = sn,
    // R(index1,index0) = -sn, and R(index1,index1) = cs.  If N is the
    // number of matrix columns and K is the maximum number of iterations, the
    // maximum number of right or left Givens rotations is K*(N-1).  The
    // maximum amount of memory is allocated to store these.  However, we also
    // potentially need left rotations to decouple the matrix when a diagonal
    // terms are zero.  Worst case is a number of matrices quadratic in N, so
    // for now we just use std::vector<Rotation> whose initial capacity is
    // K*(N-1).
    struct GivensRotation
    {
        GivensRotation();
        GivensRotation(int inIndex0, int inIndex1, Real inCs, Real inSn);
        int index0, index1;
        Real cs, sn;
    };

    std::vector<GivensRotation> mRGivens;
    std::vector<GivensRotation> mLGivens;

    // The diagonal matrix that is used to convert S-entries to nonnegative.
    std::vector<Real> mFixupDiagonal;  // N elements

    // When sorting is requested, the permutation associated with the sort is
    // stored in mPermutation.  When sorting is not requested, mPermutation[0]
    // is set to -1.  mVisited is used for finding cycles in the permutation.
    std::vector<int> mPermutation;  // N elements
    mutable std::vector<int> mVisited;  // N elements

    // Temporary storage to compute Householder reflections and to support
    // sorting of columns of the orthogonal matrices.
    std::vector<Real> mTwoInvUTU;  // N elements
    std::vector<Real> mTwoInvVTV;  // N-2 elements
    mutable std::vector<Real> mUVector;  // M elements
    mutable std::vector<Real> mVVector;  // N elements
    mutable std::vector<Real> mWVector;  // max(M,N) elements
};


template <typename Real>
SingularValueDecomposition<Real>::SingularValueDecomposition(int numRows,
    int numCols, unsigned int maxIterations)
    :
    mNumRows(0),
    mNumCols(0),
    mMaxIterations(0)
{
    if (numCols > 1 && numRows >= numCols && maxIterations > 0)
    {
        mNumRows = numRows;
        mNumCols = numCols;
        mMaxIterations = maxIterations;
        mMatrix.resize(numRows * numCols);
        mDiagonal.resize(numCols);
        mSuperdiagonal.resize(numCols - 1);
        mRGivens.reserve(maxIterations*(numCols - 1));
        mLGivens.reserve(maxIterations*(numCols - 1));
        mFixupDiagonal.resize(numCols);
        mPermutation.resize(numCols);
        mVisited.resize(numCols);
        mTwoInvUTU.resize(numCols);
        mTwoInvVTV.resize(numCols - 2);
        mUVector.resize(numRows);
        mVVector.resize(numCols);
        mWVector.resize(numRows);
    }
}

template <typename Real>
unsigned int SingularValueDecomposition<Real>::Solve(Real const* input,
    int sortType)
{
    if (mNumRows > 0)
    {
        int numElements = mNumRows * mNumCols;
        std::copy(input, input + numElements, mMatrix.begin());
        Bidiagonalize();

        // Compute 'threshold = multiplier*epsilon*|B|' as the threshold for
        // diagonal entries effectively zero; that is, |d| <= |threshold|
        // implies that d is (effectively) zero.  TODO: Allow the caller to
        // pass 'multiplier' to the constructor.
        //
        // We will use the L2-norm |B|, which is the length of the elements
        // of B treated as an NM-tuple.  The following code avoids overflow
        // when accumulating the squares of the elements when those elements
        // are large.
        Real maxAbsComp = std::abs(input[0]);
        for (int i = 1; i < numElements; ++i)
        {
            Real absComp = std::abs(input[i]);
            if (absComp > maxAbsComp)
            {
                maxAbsComp = absComp;
            }
        }

        Real norm = (Real)0;
        if (maxAbsComp > (Real)0)
        {
            Real invMaxAbsComp = ((Real)1) / maxAbsComp;
            for (int i = 0; i < numElements; ++i)
            {
                Real ratio = input[i] * invMaxAbsComp;
                norm += ratio * ratio;
            }
            norm = maxAbsComp * std::sqrt(norm);
        }

        Real const multiplier = (Real)8;  // TODO: Expose to caller.
        Real const epsilon = std::numeric_limits<Real>::epsilon();
        Real const threshold = multiplier * epsilon * norm;

        mRGivens.clear();
        mLGivens.clear();
        for (unsigned int j = 0; j < mMaxIterations; ++j)
        {
            int imin = -1, imax = -1;
            for (int i = mNumCols - 2; i >= 0; --i)
            {
                // When a01 is much smaller than its diagonal neighbors, it is
                // effectively zero.
                Real a00 = mDiagonal[i];
                Real a01 = mSuperdiagonal[i];
                Real a11 = mDiagonal[i + 1];
                Real sum = std::abs(a00) + std::abs(a11);
                if (sum + std::abs(a01) != sum)
                {
                    if (imax == -1)
                    {
                        imax = i;
                    }
                    imin = i;
                }
                else
                {
                    // The superdiagonal term is effectively zero compared to
                    // the neighboring diagonal terms.
                    if (imin >= 0)
                    {
                        break;
                    }
                }
            }

            if (imax == -1)
            {
                // The algorithm has converged.
                EnsureNonnegativeDiagonal();
                ComputePermutation(sortType);
                return j;
            }

            // We need to test diagonal entries of B for zero.  For each zero
            // diagonal entry, zero the superdiagonal.
            if (DiagonalEntriesNonzero(imin, imax, threshold))
            {
                // Process the lower-right-most unreduced bidiagonal block.
                DoGolubKahanStep(imin, imax);
            }
        }
        return 0xFFFFFFFF;
    }
    else
    {
        return 0;
    }
}

template <typename Real>
void SingularValueDecomposition<Real>::GetSingularValues(
    Real* singularValues) const
{
    if (singularValues && mNumCols > 0)
    {
        if (mPermutation[0] >= 0)
        {
            // Sorting was requested.
            for (int i = 0; i < mNumCols; ++i)
            {
                int p = mPermutation[i];
                singularValues[i] = mDiagonal[p];
            }
        }
        else
        {
            // Sorting was not requested.
            size_t numBytes = mNumCols*sizeof(Real);
            Memcpy(singularValues, &mDiagonal[0], numBytes);
        }
    }
}

template <typename Real>
void SingularValueDecomposition<Real>::GetU(Real* uMatrix) const
{
    if (!uMatrix || mNumCols == 0)
    {
        // Invalid input or the constructor failed.
        return;
    }

    // Start with the identity matrix.
    std::fill(uMatrix, uMatrix + mNumRows*mNumRows, (Real)0);
    for (int d = 0; d < mNumRows; ++d)
    {
        uMatrix[d + mNumRows*d] = (Real)1;
    }

    // Multiply the Householder reflections using backward accumulation.
    int r, c;
    for (int i0 = mNumCols - 1, i1 = i0 + 1; i0 >= 0; --i0, --i1)
    {
        // Copy the u vector and 2/Dot(u,u) from the matrix.
        Real twoinvudu = mTwoInvUTU[i0];
        Real const* column = &mMatrix[i0];
        mUVector[i0] = (Real)1;
        for (r = i1; r < mNumRows; ++r)
        {
            mUVector[r] = column[mNumCols*r];
        }

        // Compute the w vector.
        mWVector[i0] = twoinvudu;
        for (r = i1; r < mNumRows; ++r)
        {
            mWVector[r] = (Real)0;
            for (c = i1; c < mNumRows; ++c)
            {
                mWVector[r] += mUVector[c] * uMatrix[r + mNumRows*c];
            }
            mWVector[r] *= twoinvudu;
        }

        // Update the matrix, U <- U - u*w^T.
        for (r = i0; r < mNumRows; ++r)
        {
            for (c = i0; c < mNumRows; ++c)
            {
                uMatrix[c + mNumRows*r] -= mUVector[r] * mWVector[c];
            }
        }
    }

    // Multiply the Givens rotations.
    for (auto const& givens : mLGivens)
    {
        int j0 = givens.index0;
        int j1 = givens.index1;
        for (r = 0; r < mNumRows; ++r, j0 += mNumRows, j1 += mNumRows)
        {
            Real& q0 = uMatrix[j0];
            Real& q1 = uMatrix[j1];
            Real prd0 = givens.cs * q0 - givens.sn * q1;
            Real prd1 = givens.sn * q0 + givens.cs * q1;
            q0 = prd0;
            q1 = prd1;
        }
    }

    if (mPermutation[0] >= 0)
    {
        // Sorting was requested.
        std::fill(mVisited.begin(), mVisited.end(), 0);
        for (c = 0; c < mNumCols; ++c)
        {
            if (mVisited[c] == 0 && mPermutation[c] != c)
            {
                // The item starts a cycle with 2 or more elements.
                int start = c, current = c, next;
                for (r = 0; r < mNumRows; ++r)
                {
                    mWVector[r] = uMatrix[c + mNumRows*r];
                }
                while ((next = mPermutation[current]) != start)
                {
                    mVisited[current] = 1;
                    for (r = 0; r < mNumRows; ++r)
                    {
                        uMatrix[current + mNumRows*r] =
                            uMatrix[next + mNumRows*r];
                    }
                    current = next;
                }
                mVisited[current] = 1;
                for (r = 0; r < mNumRows; ++r)
                {
                    uMatrix[current + mNumRows*r] = mWVector[r];
                }
            }
        }
    }
}

template <typename Real>
void SingularValueDecomposition<Real>::GetV(Real* vMatrix) const
{
    if (!vMatrix || mNumCols == 0)
    {
        // Invalid input or the constructor failed.
        return;
    }

    // Start with the identity matrix.
    std::fill(vMatrix, vMatrix + mNumCols*mNumCols, (Real)0);
    for (int d = 0; d < mNumCols; ++d)
    {
        vMatrix[d + mNumCols*d] = (Real)1;
    }

    // Multiply the Householder reflections using backward accumulation.
    int i0 = mNumCols - 3;
    int i1 = i0 + 1;
    int i2 = i0 + 2;
    int r, c;
    for (/**/; i0 >= 0; --i0, --i1, --i2)
    {
        // Copy the v vector and 2/Dot(v,v) from the matrix.
        Real twoinvvdv = mTwoInvVTV[i0];
        Real const* row = &mMatrix[mNumCols*i0];
        mVVector[i1] = (Real)1;
        for (r = i2; r < mNumCols; ++r)
        {
            mVVector[r] = row[r];
        }

        // Compute the w vector.
        mWVector[i1] = twoinvvdv;
        for (r = i2; r < mNumCols; ++r)
        {
            mWVector[r] = (Real)0;
            for (c = i2; c < mNumCols; ++c)
            {
                mWVector[r] += mVVector[c] * vMatrix[r + mNumCols*c];
            }
            mWVector[r] *= twoinvvdv;
        }

        // Update the matrix, V <- V - v*w^T.
        for (r = i1; r < mNumCols; ++r)
        {
            for (c = i1; c < mNumCols; ++c)
            {
                vMatrix[c + mNumCols*r] -= mVVector[r] * mWVector[c];
            }
        }
    }

    // Multiply the Givens rotations.
    for (auto const& givens : mRGivens)
    {
        int j0 = givens.index0;
        int j1 = givens.index1;
        for (c = 0; c < mNumCols; ++c, j0 += mNumCols, j1 += mNumCols)
        {
            Real& q0 = vMatrix[j0];
            Real& q1 = vMatrix[j1];
            Real prd0 = givens.cs * q0 - givens.sn * q1;
            Real prd1 = givens.sn * q0 + givens.cs * q1;
            q0 = prd0;
            q1 = prd1;
        }
    }

    // Fix-up the diagonal.
    for (r = 0; r < mNumCols; ++r)
    {
        for (c = 0; c < mNumCols; ++c)
        {
            vMatrix[c + mNumCols*r] *= mFixupDiagonal[c];
        }
    }

    if (mPermutation[0] >= 0)
    {
        // Sorting was requested.
        std::fill(mVisited.begin(), mVisited.end(), 0);
        for (c = 0; c < mNumCols; ++c)
        {
            if (mVisited[c] == 0 && mPermutation[c] != c)
            {
                // The item starts a cycle with 2 or more elements.
                int start = c, current = c, next;
                for (r = 0; r < mNumCols; ++r)
                {
                    mWVector[r] = vMatrix[c + mNumCols*r];
                }
                while ((next = mPermutation[current]) != start)
                {
                    mVisited[current] = 1;
                    for (r = 0; r < mNumCols; ++r)
                    {
                        vMatrix[current + mNumCols*r] =
                            vMatrix[next + mNumCols*r];
                    }
                    current = next;
                }
                mVisited[current] = 1;
                for (r = 0; r < mNumCols; ++r)
                {
                    vMatrix[current + mNumCols*r] = mWVector[r];
                }
            }
        }
    }
}

template <typename Real>
void SingularValueDecomposition<Real>::GetUColumn(int index,
    Real* uColumn) const
{
    if (0 <= index && index < mNumRows)
    {
        // y = H*x, then x and y are swapped for the next H
        Real* x = uColumn;
        Real* y = &mWVector[0];

        // Start with the Euclidean basis vector.
        memset(x, 0, mNumRows * sizeof(Real));
        if (index < mNumCols && mPermutation[0] >= 0)
        {
            // Sorting was requested.
            x[mPermutation[index]] = (Real)1;
        }
        else
        {
            x[index] = (Real)1;
        }

        // Apply the Givens rotations.
        for (auto const& givens : gte::reverse(mLGivens))
        {
            Real& xr0 = x[givens.index0];
            Real& xr1 = x[givens.index1];
            Real tmp0 = givens.cs * xr0 + givens.sn * xr1;
            Real tmp1 = -givens.sn * xr0 + givens.cs * xr1;
            xr0 = tmp0;
            xr1 = tmp1;
        }

        // Apply the Householder reflections.
        for (int c = mNumCols - 1; c >= 0; --c)
        {
            // Get the Householder vector u.
            int r;
            for (r = 0; r < c; ++r)
            {
                y[r] = x[r];
            }

            // Compute s = Dot(x,u) * 2/u^T*u.
            Real s = x[r];  // r = c, u[r] = 1
            for (int j = r + 1; j < mNumRows; ++j)
            {
                s += x[j] * mMatrix[c + mNumCols * j];
            }
            s *= mTwoInvUTU[c];

            // r = c, y[r] = x[r]*u[r] - s = x[r] - s because u[r] = 1
            y[r] = x[r] - s;

            // Compute the remaining components of y.
            for (++r; r < mNumRows; ++r)
            {
                y[r] = x[r] - s * mMatrix[c + mNumCols * r];
            }

            std::swap(x, y);
        }

        // The final product is stored in x.
        if (x != uColumn)
        {
            size_t numBytes = mNumRows*sizeof(Real);
            Memcpy(uColumn, x, numBytes);
        }
    }
}

template <typename Real>
void SingularValueDecomposition<Real>::GetVColumn(int index,
    Real* vColumn) const
{
    if (0 <= index && index < mNumCols)
    {
        // y = H*x, then x and y are swapped for the next H
        Real* x = vColumn;
        Real* y = &mWVector[0];

        // Start with the Euclidean basis vector.
        memset(x, 0, mNumCols * sizeof(Real));
        if (mPermutation[0] >= 0)
        {
            // Sorting was requested.
            int p = mPermutation[index];
            x[p] = mFixupDiagonal[p];
        }
        else
        {
            x[index] = mFixupDiagonal[index];
        }

        // Apply the Givens rotations.
        for (auto const& givens : gte::reverse(mRGivens))
        {
            Real& xr0 = x[givens.index0];
            Real& xr1 = x[givens.index1];
            Real tmp0 = givens.cs * xr0 + givens.sn * xr1;
            Real tmp1 = -givens.sn * xr0 + givens.cs * xr1;
            xr0 = tmp0;
            xr1 = tmp1;
        }

        // Apply the Householder reflections.
        for (int r = mNumCols - 3; r >= 0; --r)
        {
            // Get the Householder vector v.
            int c;
            for (c = 0; c < r + 1; ++c)
            {
                y[c] = x[c];
            }

            // Compute s = Dot(x,v) * 2/v^T*v.
            Real s = x[c];  // c = r+1, v[c] = 1
            for (int j = c + 1; j < mNumCols; ++j)
            {
                s += x[j] * mMatrix[j + mNumCols * r];
            }
            s *= mTwoInvVTV[r];

            // c = r+1, y[c] = x[c]*v[c] - s = x[c] - s because v[c] = 1
            y[c] = x[c] - s;

            // Compute the remaining components of y.
            for (++c; c < mNumCols; ++c)
            {
                y[c] = x[c] - s * mMatrix[c + mNumCols * r];
            }

            std::swap(x, y);
        }

        // The final product is stored in x.
        if (x != vColumn)
        {
            size_t numBytes = mNumCols*sizeof(Real);
            Memcpy(vColumn, x, numBytes);
        }
    }
}

template <typename Real>
Real SingularValueDecomposition<Real>::GetSingularValue(int index) const
{
    if (0 <= index && index < mNumCols)
    {
        if (mPermutation[0] >= 0)
        {
            // Sorting was requested.
            return mDiagonal[mPermutation[index]];
        }
        else
        {
            // Sorting was not requested.
            return mDiagonal[index];
        }
    }
    else
    {
        return (Real)0;
    }
}

template <typename Real>
void SingularValueDecomposition<Real>::Bidiagonalize()
{
    int r, c;
    for (int i = 0, ip1 = 1; i < mNumCols; ++i, ++ip1)
    {
        // Compute the U-Householder vector.
        Real length = (Real)0;
        for (r = i; r < mNumRows; ++r)
        {
            Real ur = mMatrix[i + mNumCols*r];
            mUVector[r] = ur;
            length += ur * ur;
        }
        Real udu = (Real)1;
        length = std::sqrt(length);
        if (length >(Real)0)
        {
            Real& u1 = mUVector[i];
            Real sgn = (u1 >= (Real)0 ? (Real)1 : (Real)-1);
            Real invDenom = ((Real)1) / (u1 + sgn * length);
            u1 = (Real)1;
            for (r = ip1; r < mNumRows; ++r)
            {
                Real& ur = mUVector[r];
                ur *= invDenom;
                udu += ur * ur;
            }
        }

        // Compute the rank-1 offset u*w^T.
        Real invudu = (Real)1 / udu;
        Real twoinvudu = invudu * (Real)2;
        for (c = i; c < mNumCols; ++c)
        {
            mWVector[c] = (Real)0;
            for (r = i; r < mNumRows; ++r)
            {
                mWVector[c] += mMatrix[c + mNumCols*r] * mUVector[r];
            }
            mWVector[c] *= twoinvudu;
        }

        // Update the input matrix.
        for (r = i; r < mNumRows; ++r)
        {
            for (c = i; c < mNumCols; ++c)
            {
                mMatrix[c + mNumCols*r] -= mUVector[r] * mWVector[c];
            }
        }

        if (i < mNumCols - 2)
        {
            // Compute the V-Householder vectors.
            length = (Real)0;
            for (c = ip1; c < mNumCols; ++c)
            {
                Real vc = mMatrix[c + mNumCols*i];
                mVVector[c] = vc;
                length += vc * vc;
            }
            Real vdv = (Real)1;
            length = std::sqrt(length);
            if (length >(Real)0)
            {
                Real& v1 = mVVector[ip1];
                Real sgn = (v1 >= (Real)0 ? (Real)1 : (Real)-1);
                Real invDenom = ((Real)1) / (v1 + sgn * length);
                v1 = (Real)1;
                for (c = ip1 + 1; c < mNumCols; ++c)
                {
                    Real& vc = mVVector[c];
                    vc *= invDenom;
                    vdv += vc * vc;
                }
            }

            // Compute the rank-1 offset w*v^T.
            Real invvdv = (Real)1 / vdv;
            Real twoinvvdv = invvdv * (Real)2;
            for (r = i; r < mNumRows; ++r)
            {
                mWVector[r] = (Real)0;
                for (c = ip1; c < mNumCols; ++c)
                {
                    mWVector[r] += mMatrix[c + mNumCols*r] * mVVector[c];
                }
                mWVector[r] *= twoinvvdv;
            }

            // Update the input matrix.
            for (r = i; r < mNumRows; ++r)
            {
                for (c = ip1; c < mNumCols; ++c)
                {
                    mMatrix[c + mNumCols*r] -= mWVector[r] * mVVector[c];
                }
            }

            mTwoInvVTV[i] = twoinvvdv;
            for (c = i + 2; c < mNumCols; ++c)
            {
                mMatrix[c + mNumCols*i] = mVVector[c];
            }
        }

        mTwoInvUTU[i] = twoinvudu;
        for (r = ip1; r < mNumRows; ++r)
        {
            mMatrix[i + mNumCols*r] = mUVector[r];
        }
    }

    // Copy the diagonal and subdiagonal for cache coherence in the
    // Golub-Kahan iterations.
    int k, ksup = mNumCols - 1, index = 0, delta = mNumCols + 1;
    for (k = 0; k < ksup; ++k, index += delta)
    {
        mDiagonal[k] = mMatrix[index];
        mSuperdiagonal[k] = mMatrix[index + 1];
    }
    mDiagonal[k] = mMatrix[index];
}

template <typename Real>
void SingularValueDecomposition<Real>::GetSinCos(Real x, Real y, Real& cs,
    Real& sn)
{
    // Solves sn*x + cs*y = 0 robustly.
    Real tau;
    if (y != (Real)0)
    {
        if (std::abs(y) > std::abs(x))
        {
            tau = -x / y;
            sn = ((Real)1) / std::sqrt(((Real)1) + tau*tau);
            cs = sn * tau;
        }
        else
        {
            tau = -y / x;
            cs = ((Real)1) / std::sqrt(((Real)1) + tau*tau);
            sn = cs * tau;
        }
    }
    else
    {
        cs = (Real)1;
        sn = (Real)0;
    }
}

template <typename Real>
bool SingularValueDecomposition<Real>::DiagonalEntriesNonzero(int imin,
    int imax, Real threshold)
{
    for (int i = imin; i <= imax; ++i)
    {
        if (std::abs(mDiagonal[i]) <= threshold)
        {
            // Use planar rotations to case the superdiagonal entry out of
            // the matrix, thus producing a row of zeros.
            Real x, z, cs, sn;
            Real y = mSuperdiagonal[i];
            mSuperdiagonal[i] = (Real)0;
            for (int j = i + 1; j <= imax + 1; ++j)
            {
                x = mDiagonal[j];
                GetSinCos(x, y, cs, sn);
                mLGivens.push_back(GivensRotation(i, j, cs, sn));
                mDiagonal[j] = cs*x - sn*y;
                if (j <= imax)
                {
                    z = mSuperdiagonal[j];
                    mSuperdiagonal[j] = cs*z;
                    y = sn*z;
                }
            }
            return false;
        }
    }
    return true;
}

template <typename Real>
void SingularValueDecomposition<Real>::DoGolubKahanStep(int imin, int imax)
{
    // The implicit shift.  Compute the eigenvalue u of the lower-right 2x2
    // block of A = B^T*B that is closer to b11.
    Real f0 = (imax >= (Real)1 ? mSuperdiagonal[imax - 1] : (Real)0);
    Real d1 = mDiagonal[imax];
    Real f1 = mSuperdiagonal[imax];
    Real d2 = mDiagonal[imax + 1];
    Real a00 = d1*d1 + f0*f0;
    Real a01 = d1*f1;
    Real a11 = d2*d2 + f1*f1;
    Real dif = (a00 - a11) * (Real)0.5;
    Real sgn = (dif >= (Real)0 ? (Real)1 : (Real)-1);
    Real a01sqr = a01 * a01;
    Real u = a11 - a01sqr / (dif + sgn * std::sqrt(dif*dif + a01sqr));
    Real x = mDiagonal[imin] * mDiagonal[imin] - u;
    Real y = mDiagonal[imin] * mSuperdiagonal[imin];

    Real a12, a21, a22, a23, cs, sn;
    Real a02 = (Real)0;
    int i0 = imin - 1, i1 = imin, i2 = imin + 1;
    for (/**/; i1 <= imax; ++i0, ++i1, ++i2)
    {
        // Compute the Givens rotation G and save it for use in computing
        // V in U^T*A*V = S.
        GetSinCos(x, y, cs, sn);
        mRGivens.push_back(GivensRotation(i1, i2, cs, sn));

        // Update B0 = B*G.
        if (i1 > imin)
        {
            mSuperdiagonal[i0] = cs*mSuperdiagonal[i0] - sn*a02;
        }

        a11 = mDiagonal[i1];
        a12 = mSuperdiagonal[i1];
        a22 = mDiagonal[i2];
        mDiagonal[i1] = cs*a11 - sn*a12;
        mSuperdiagonal[i1] = sn*a11 + cs*a12;
        mDiagonal[i2] = cs*a22;
        a21 = -sn*a22;

        // Update the parameters for the next Givens rotations.
        x = mDiagonal[i1];
        y = a21;

        // Compute the Givens rotation G and save it for use in computing
        // U in U^T*A*V = S.
        GetSinCos(x, y, cs, sn);
        mLGivens.push_back(GivensRotation(i1, i2, cs, sn));

        // Update B1 = G^T*B0.
        a11 = mDiagonal[i1];
        a12 = mSuperdiagonal[i1];
        a22 = mDiagonal[i2];
        mDiagonal[i1] = cs*a11 - sn*a21;
        mSuperdiagonal[i1] = cs*a12 - sn*a22;
        mDiagonal[i2] = sn*a12 + cs*a22;

        if (i1 < imax)
        {
            a23 = mSuperdiagonal[i2];
            a02 = -sn*a23;
            mSuperdiagonal[i2] = cs*a23;

            // Update the parameters for the next Givens rotations.
            x = mSuperdiagonal[i1];
            y = a02;
        }
    }
}

template <typename Real>
void SingularValueDecomposition<Real>::EnsureNonnegativeDiagonal()
{
    for (int i = 0; i < mNumCols; ++i)
    {
        if (mDiagonal[i] >= (Real)0)
        {
            mFixupDiagonal[i] = (Real)1;
        }
        else
        {
            mDiagonal[i] = -mDiagonal[i];
            mFixupDiagonal[i] = (Real)-1;
        }
    }
}

template <typename Real>
void SingularValueDecomposition<Real>::ComputePermutation(int sortType)
{
    if (sortType == 0)
    {
        // Set a flag for GetSingularValues() and GetOrthogonalMatrices() to
        // know that sorted output was not requested.
        mPermutation[0] = -1;
        return;
    }

    // Compute the permutation induced by sorting.  Initially, we start with
    // the identity permutation I = (0,1,...,N-1).
    struct SortItem
    {
        Real singularValue;
        int index;
    };

    std::vector<SortItem> items(mNumCols);
    int i;
    for (i = 0; i < mNumCols; ++i)
    {
        items[i].singularValue = mDiagonal[i];
        items[i].index = i;
    }

    if (sortType > 0)
    {
        std::sort(items.begin(), items.end(),
            [](SortItem const& item0, SortItem const& item1)
        {
            return item0.singularValue < item1.singularValue;
        }
        );
    }
    else
    {
        std::sort(items.begin(), items.end(),
            [](SortItem const& item0, SortItem const& item1)
        {
            return item0.singularValue > item1.singularValue;
        }
        );
    }

    i = 0;
    for (auto const& item : items)
    {
        mPermutation[i++] = item.index;
    }

    // GetOrthogonalMatrices() has nontrivial code for computing the
    // orthogonal U and V from the reflections and rotations.  To avoid
    // complicating the code further when sorting is requested, U and V are
    // computed as in the unsorted case.  We then need to swap columns of
    // U and V to be consistent with the sorting of the singular values.  To
    // minimize copying due to column swaps, we use permutation P.  The
    // minimum number of transpositions to obtain P from I is N minus the
    // number of cycles of P.  Each cycle is reordered with a minimum number
    // of transpositions; that is, the singular items are cyclically swapped,
    // leading to a minimum amount of copying.  For example, if there is a
    // cycle i0 -> i1 -> i2 -> i3, then the copying is
    //   save = singularitem[i0];
    //   singularitem[i1] = singularitem[i2];
    //   singularitem[i2] = singularitem[i3];
    //   singularitem[i3] = save;
}

template <typename Real>
SingularValueDecomposition<Real>::GivensRotation::GivensRotation()
{
    // No default initialization for fast creation of std::vector of objects
    // of this type.
}

template <typename Real>
SingularValueDecomposition<Real>::GivensRotation::GivensRotation(int inIndex0,
    int inIndex1, Real inCs, Real inSn)
    :
    index0(inIndex0),
    index1(inIndex1),
    cs(inCs),
    sn(inSn)
{
}


}


// ----------------------------------------------
// GteHyperplane.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2016/07/28)

// The plane is represented as Dot(U,X) = c where U is a unit-length normal
// vector, c is the plane constant, and X is any point on the plane.  The user
// must ensure that the normal vector is unit length.

namespace gte
{

template <int N, typename Real>
class Hyperplane
{
public:
    // Construction and destruction.  The default constructor sets the normal
    // to (0,...,0,1) and the constant to zero (plane z = 0).
    Hyperplane();

    // Specify U and c directly.
    Hyperplane(Vector<N, Real> const& inNormal, Real inConstant);

    // U is specified, c = Dot(U,p) where p is a point on the hyperplane.
    Hyperplane(Vector<N, Real> const& inNormal, Vector<N, Real> const& p);

    // U is a unit-length vector in the orthogonal complement of the set
    // {p[1]-p[0],...,p[n-1]-p[0]} and c = Dot(U,p[0]), where the p[i] are
    // pointson the hyperplane.
    Hyperplane(std::array<Vector<N, Real>, N> const& p);

    // Public member access.
    Vector<N, Real> normal;
    Real constant;

public:
    // Comparisons to support sorted containers.
    bool operator==(Hyperplane const& hyperplane) const;
    bool operator!=(Hyperplane const& hyperplane) const;
    bool operator< (Hyperplane const& hyperplane) const;
    bool operator<=(Hyperplane const& hyperplane) const;
    bool operator> (Hyperplane const& hyperplane) const;
    bool operator>=(Hyperplane const& hyperplane) const;
};

// Template alias for convenience.
template <typename Real>
using Plane3 = Hyperplane<3, Real>;


template <int N, typename Real>
Hyperplane<N, Real>::Hyperplane()
    :
    constant((Real)0)
{
    normal.MakeUnit(N - 1);
}

template <int N, typename Real>
Hyperplane<N, Real>::Hyperplane(Vector<N, Real> const& inNormal,
    Real inConstant)
    :
    normal(inNormal),
    constant(inConstant)
{
}

template <int N, typename Real>
Hyperplane<N, Real>::Hyperplane(Vector<N, Real> const& inNormal,
    Vector<N, Real> const& p)
    :
    normal(inNormal),
    constant(Dot(inNormal, p))
{
}

template <int N, typename Real>
Hyperplane<N, Real>::Hyperplane(std::array<Vector<N, Real>, N> const& p)
{
    Matrix<N, N - 1, Real> edge;
    for (int i = 0; i < N - 1; ++i)
    {
        edge.SetCol(i, p[i + 1] - p[0]);
    }

    // Compute the 1-dimensional orthogonal complement of the edges of the
    // simplex formed by the points p[].
    SingularValueDecomposition<Real> svd(N, N - 1, 32);
    svd.Solve(&edge[0], -1);
    svd.GetUColumn(N - 1, &normal[0]);

    constant = Dot(normal, p[0]);
}

template <int N, typename Real>
bool Hyperplane<N, Real>::operator==(Hyperplane const& hyperplane) const
{
    return normal == hyperplane.normal && constant == hyperplane.constant;
}

template <int N, typename Real>
bool Hyperplane<N, Real>::operator!=(Hyperplane const& hyperplane) const
{
    return !operator==(hyperplane);
}

template <int N, typename Real>
bool Hyperplane<N, Real>::operator<(Hyperplane const& hyperplane) const
{
    if (normal < hyperplane.normal)
    {
        return true;
    }

    if (normal > hyperplane.normal)
    {
        return false;
    }

    return constant < hyperplane.constant;
}

template <int N, typename Real>
bool Hyperplane<N, Real>::operator<=(Hyperplane const& hyperplane) const
{
    return operator<(hyperplane) || operator==(hyperplane);
}

template <int N, typename Real>
bool Hyperplane<N, Real>::operator>(Hyperplane const& hyperplane) const
{
    return !operator<=(hyperplane);
}

template <int N, typename Real>
bool Hyperplane<N, Real>::operator>=(Hyperplane const& hyperplane) const
{
    return !operator<(hyperplane);
}


}



// ----------------------------------------------
// GteConvexHull3.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

// Compute the convex hull of 3D points using incremental insertion.  The only
// way to ensure a correct result for the input vertices (assumed to be exact)
// is to choose ComputeType for exact rational arithmetic.  You may use
// BSNumber.  No divisions are performed in this computation, so you do not
// have to use BSRational.
//
// The worst-case choices of N for Real of type BSNumber or BSRational with
// integer storage UIntegerFP32<N> are listed in the next table.  The numerical
// computations are encapsulated in PrimalQuery3<Real>::ToPlane.  We recommend
// using only BSNumber, because no divisions are performed in the convex-hull
// computations.
//
//    input type | compute type | N
//    -----------+--------------+------
//    float      | BSNumber     |    27
//    double     | BSNumber     |   197
//    float      | BSRational   |  2882
//    double     | BSRational   | 21688

namespace gte
{

template <typename InputType, typename ComputeType>
class ConvexHull3
{
public:
    // The class is a functor to support computing the convex hull of multiple
    // data sets using the same class object.  For multithreading in Update,
    // choose 'numThreads' subject to the constraints
    //     1 <= numThreads <= std::thread::hardware_concurrency().
    ConvexHull3(unsigned int numThreads = 1);

    // The input is the array of points whose convex hull is required.  The
    // epsilon value is used to determine the intrinsic dimensionality of the
    // vertices (d = 0, 1, 2, or 3).  When epsilon is positive, the
    // determination is fuzzy--points approximately the same point,
    // approximately on a line, approximately planar, or volumetric.
    bool operator()(int numPoints, Vector3<InputType> const* points,
        InputType epsilon);

    // Dimensional information.  If GetDimension() returns 1, the points lie
    // on a line P+t*D (fuzzy comparison when epsilon > 0).  You can sort
    // these if you need a polyline output by projecting onto the line each
    // vertex X = P+t*D, where t = Dot(D,X-P).  If GetDimension() returns 2,
    // the points line on a plane P+s*U+t*V (fuzzy comparison when
    // epsilon > 0).  You can project each point X = P+s*U+t*V, where
    // s = Dot(U,X-P) and t = Dot(V,X-P), then apply ConvexHull2 to the (s,t)
    // tuples.
    inline InputType GetEpsilon() const;
    inline int GetDimension() const;
    inline Line3<InputType> const& GetLine() const;
    inline Plane3<InputType> const& GetPlane() const;

    // Member access.
    inline int GetNumPoints() const;
    inline int GetNumUniquePoints() const;
    inline Vector3<InputType> const* GetPoints() const;
    inline PrimalQuery3<ComputeType> const& GetQuery() const;

    // The convex hull is a convex polyhedron with triangular faces.
    inline std::vector<TriangleKey<true>> const& GetHullUnordered() const;
    ETManifoldMesh const& GetHullMesh() const;

private:
    // Support for incremental insertion.
    void Update(int i);

    // The epsilon value is used for fuzzy determination of intrinsic
    // dimensionality.  If the dimension is 0, 1, or 2, the constructor
    // returns early.  The caller is responsible for retrieving the dimension
    // and taking an alternate path should the dimension be smaller than 3.
    // If the dimension is 0, the caller may as well treat all points[] as a
    // single point, say, points[0].  If the dimension is 1, the caller can
    // query for the approximating line and project points[] onto it for
    // further processing.  If the dimension is 2, the caller can query for
    // the approximating plane and project points[] onto it for further
    // processing.
    InputType mEpsilon;
    int mDimension;
    Line3<InputType> mLine;
    Plane3<InputType> mPlane;

    // The array of points used for geometric queries.  If you want to be
    // certain of a correct result, choose ComputeType to be BSNumber.
    std::vector<Vector3<ComputeType>> mComputePoints;
    PrimalQuery3<ComputeType> mQuery;

    int mNumPoints;
    int mNumUniquePoints;
    Vector3<InputType> const* mPoints;
    std::vector<TriangleKey<true>> mHullUnordered;
    mutable ETManifoldMesh mHullMesh;
    unsigned int mNumThreads;
};


template <typename InputType, typename ComputeType>
ConvexHull3<InputType, ComputeType>::ConvexHull3(unsigned int numThreads)
    :
    mEpsilon((InputType)0),
    mDimension(0),
    mLine(Vector3<InputType>::Zero(), Vector3<InputType>::Zero()),
    mPlane(Vector3<InputType>::Zero(), (InputType)0),
    mNumPoints(0),
    mNumUniquePoints(0),
    mPoints(nullptr),
    mNumThreads(numThreads)
{
}

template <typename InputType, typename ComputeType>
bool ConvexHull3<InputType, ComputeType>::operator()(int numPoints,
    Vector3<InputType> const* points, InputType epsilon)
{
    mEpsilon = std::max(epsilon, (InputType)0);
    mDimension = 0;
    mLine.origin = Vector3<InputType>::Zero();
    mLine.direction = Vector3<InputType>::Zero();
    mPlane.normal = Vector3<InputType>::Zero();
    mPlane.constant = (InputType)0;
    mNumPoints = numPoints;
    mNumUniquePoints = 0;
    mPoints = points;
    mHullUnordered.clear();
    mHullMesh.Clear();

    int i, j;
    if (mNumPoints < 4)
    {
        // ConvexHull3 should be called with at least four points.
        return false;
    }

    IntrinsicsVector3<InputType> info(mNumPoints, mPoints, mEpsilon);
    if (info.dimension == 0)
    {
        // The set is (nearly) a point.
        return false;
    }

    if (info.dimension == 1)
    {
        // The set is (nearly) collinear.
        mDimension = 1;
        mLine = Line3<InputType>(info.origin, info.direction[0]);
        return false;
    }

    if (info.dimension == 2)
    {
        // The set is (nearly) coplanar.
        mDimension = 2;
        mPlane = Plane3<InputType>(UnitCross(info.direction[0],
            info.direction[1]), info.origin);
        return false;
    }

    mDimension = 3;

    // Compute the vertices for the queries.
    mComputePoints.resize(mNumPoints);
    mQuery.Set(mNumPoints, &mComputePoints[0]);
    for (i = 0; i < mNumPoints; ++i)
    {
        for (j = 0; j < 3; ++j)
        {
            mComputePoints[i][j] = points[i][j];
        }
    }

    // Insert the faces of the (nondegenerate) tetrahedron constructed by the
    // call to GetInformation.
    if (!info.extremeCCW)
    {
        std::swap(info.extreme[2], info.extreme[3]);
    }

    mHullUnordered.push_back(TriangleKey<true>(info.extreme[1],
        info.extreme[2], info.extreme[3]));
    mHullUnordered.push_back(TriangleKey<true>(info.extreme[0],
        info.extreme[3], info.extreme[2]));
    mHullUnordered.push_back(TriangleKey<true>(info.extreme[0],
        info.extreme[1], info.extreme[3]));
    mHullUnordered.push_back(TriangleKey<true>(info.extreme[0],
        info.extreme[2], info.extreme[1]));

    // Incrementally update the hull.  The set of processed points is
    // maintained to eliminate duplicates, either in the original input
    // points or in the points obtained by snap rounding.
    std::set<Vector3<InputType>> processed;
    for (i = 0; i < 4; ++i)
    {
        processed.insert(points[info.extreme[i]]);
    }
    for (i = 0; i < mNumPoints; ++i)
    {
        if (processed.find(points[i]) == processed.end())
        {
            Update(i);
            processed.insert(points[i]);
        }
    }
    mNumUniquePoints = static_cast<int>(processed.size());
    return true;
}

template <typename InputType, typename ComputeType> inline
InputType ConvexHull3<InputType, ComputeType>::GetEpsilon() const
{
    return mEpsilon;
}

template <typename InputType, typename ComputeType> inline
int ConvexHull3<InputType, ComputeType>::GetDimension() const
{
    return mDimension;
}

template <typename InputType, typename ComputeType> inline
Line3<InputType> const& ConvexHull3<InputType, ComputeType>::GetLine() const
{
    return mLine;
}

template <typename InputType, typename ComputeType> inline
Plane3<InputType> const& ConvexHull3<InputType, ComputeType>::GetPlane() const
{
    return mPlane;
}

template <typename InputType, typename ComputeType> inline
int ConvexHull3<InputType, ComputeType>::GetNumPoints() const
{
    return mNumPoints;
}

template <typename InputType, typename ComputeType> inline
int ConvexHull3<InputType, ComputeType>::GetNumUniquePoints() const
{
    return mNumUniquePoints;
}

template <typename InputType, typename ComputeType> inline
Vector3<InputType> const* ConvexHull3<InputType, ComputeType>::GetPoints() const
{
    return mPoints;
}

template <typename InputType, typename ComputeType> inline
PrimalQuery3<ComputeType> const& ConvexHull3<InputType, ComputeType>::GetQuery() const
{
    return mQuery;
}

template <typename InputType, typename ComputeType> inline
std::vector<TriangleKey<true>> const& ConvexHull3<InputType, ComputeType>::GetHullUnordered() const
{
    return mHullUnordered;
}

template <typename InputType, typename ComputeType>
ETManifoldMesh const& ConvexHull3<InputType, ComputeType>::GetHullMesh() const
{
    // Create the mesh only on demand.
    if (mHullMesh.GetTriangles().size() == 0)
    {
        for (auto const& tri : mHullUnordered)
        {
            mHullMesh.Insert(tri.V[0], tri.V[1], tri.V[2]);
        }
    }

    return mHullMesh;
}

template <typename InputType, typename ComputeType>
void ConvexHull3<InputType, ComputeType>::Update(int i)
{
    // The terminator that separates visible faces from nonvisible faces is
    // constructed by this code.  Visible faces for the incoming hull are
    // removed, and the boundary of that set of triangles is the terminator.
    // New visible faces are added using the incoming point and the edges of
    // the terminator.
    //
    // A simple algorithm for computing terminator edges is the following.
    // Back-facing triangles are located and the three edges are processed.
    // The first time an edge is visited, insert it into the terminator.  If
    // it is visited a second time, the edge is removed because it is shared
    // by another back-facing triangle and, therefore, cannot be a terminator
    // edge.  After visiting all back-facing triangles, the only remaining
    // edges in the map are the terminator edges.
    //
    // The order of vertices of an edge is important for adding new faces with
    // the correct vertex winding.  However, the edge "toggle" (insert edge,
    // remove edge) should use edges with unordered vertices, because the
    // edge shared by one triangle has opposite ordering relative to that of
    // the other triangle.  The map uses unordered edges as the keys but
    // stores the ordered edge as the value.  This avoids having to look up
    // an edge twice in a map with ordered edge keys.

    unsigned int numFaces = static_cast<unsigned int>(mHullUnordered.size());
    std::vector<int> queryResult(numFaces);
    if (mNumThreads > 1 && numFaces >= mNumThreads)
    {
        // Partition the data for multiple threads.
        unsigned int numFacesPerThread = numFaces / mNumThreads;
        std::vector<unsigned int> jmin(mNumThreads), jmax(mNumThreads);
        for (unsigned int t = 0; t < mNumThreads; ++t)
        {
            jmin[t] = t * numFacesPerThread;
            jmax[t] = jmin[t] + numFacesPerThread - 1;
        }
        jmax[mNumThreads - 1] = numFaces - 1;

        // Execute the point-plane queries in multiple threads.
        std::vector<std::thread> process(mNumThreads);
        for (unsigned int t = 0; t < mNumThreads; ++t)
        {
            process[t] = std::thread([this, i, t, &jmin, &jmax, 
                &queryResult]()
            {
                for (unsigned int j = jmin[t]; j <= jmax[t]; ++j)
                {
                    TriangleKey<true> const& tri = mHullUnordered[j];
                    queryResult[j] = mQuery.ToPlane(i, tri.V[0], tri.V[1], tri.V[2]);
                }
            });
        }

        // Wait for all threads to finish.
        for (unsigned int t = 0; t < mNumThreads; ++t)
        {
            process[t].join();
        }
    }
    else
    {
        unsigned int j = 0;
        for (auto const& tri : mHullUnordered)
        {
            queryResult[j++] = mQuery.ToPlane(i, tri.V[0], tri.V[1], tri.V[2]);
        }
    }

    std::map<EdgeKey<false>, std::pair<int, int>> terminator;
    std::vector<TriangleKey<true>> backFaces;
    bool existsFrontFacingTriangle = false;
    unsigned int j = 0;
    for (auto const& tri : mHullUnordered)
    {
        if (queryResult[j++] <= 0)
        {
            // The triangle is back facing.  These include triangles that
            // are coplanar with the incoming point.
            backFaces.push_back(tri);

            // The current hull is a 2-manifold watertight mesh.  The
            // terminator edges are those shared with a front-facing triangle.
            // The first time an edge of a back-facing triangle is visited,
            // insert it into the terminator.  If it is visited a second time,
            // the edge is removed because it is shared by another back-facing
            // triangle.  After all back-facing triangles are visited, the
            // only remaining edges are shared by a single back-facing
            // triangle, which makes them terminator edges.
            for (int j0 = 2, j1 = 0; j1 < 3; j0 = j1++)
            {
                int v0 = tri.V[j0], v1 = tri.V[j1];
                EdgeKey<false> edge(v0, v1);
                auto iter = terminator.find(edge);
                if (iter == terminator.end())
                {
                    // The edge is visited for the first time.
                    terminator.insert(std::make_pair(edge, std::make_pair(v0, v1)));
                }
                else
                {
                    // The edge is visited for the second time.
                    terminator.erase(edge);
                }
            }
        }
        else
        {
            // If there are no strictly front-facing triangles, then the
            // incoming point is inside or on the convex hull.  If we get
            // to this code, then the point is truly outside and we can
            // update the hull.
            existsFrontFacingTriangle = true;
        }
    }

    if (!existsFrontFacingTriangle)
    {
        // The incoming point is inside or on the current hull, so no
        // update of the hull is necessary.
        return;
    }

    // The updated hull contains the triangles not visible to the incoming
    // point.
    mHullUnordered = backFaces;

    // Insert the triangles formed by the incoming point and the terminator
    // edges.
    for (auto const& edge : terminator)
    {
        mHullUnordered.push_back(TriangleKey<true>(i, edge.second.second, edge.second.first));
    }
}


}


// ----------------------------------------------
// GteVector2.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.2 (2018/10/04)

namespace gte
{

// Template alias for convenience.
template <typename Real>
using Vector2 = Vector<2, Real>;

// Compute the perpendicular using the formal determinant,
//   perp = det{{e0,e1},{x0,x1}} = (x1,-x0)
// where e0 = (1,0), e1 = (0,1), and v = (x0,x1).
template <typename Real>
Vector2<Real> Perp(Vector2<Real> const& v);

// Compute the normalized perpendicular.
template <typename Real>
Vector2<Real> UnitPerp(Vector2<Real> const& v, bool robust = false);

// Compute Dot((x0,x1),Perp(y0,y1)) = x0*y1 - x1*y0, where v0 = (x0,x1)
// and v1 = (y0,y1).
template <typename Real>
Real DotPerp(Vector2<Real> const& v0, Vector2<Real> const& v1);

// Compute a right-handed orthonormal basis for the orthogonal complement
// of the input vectors.  The function returns the smallest length of the
// unnormalized vectors computed during the process.  If this value is nearly
// zero, it is possible that the inputs are linearly dependent (within
// numerical round-off errors).  On input, numInputs must be 1 and v[0]
// must be initialized.  On output, the vectors v[0] and v[1] form an
// orthonormal set.
template <typename Real>
Real ComputeOrthogonalComplement(int numInputs, Vector2<Real>* v, bool robust = false);

// Compute the barycentric coordinates of the point P with respect to the
// triangle <V0,V1,V2>, P = b0*V0 + b1*V1 + b2*V2, where b0 + b1 + b2 = 1.
// The return value is 'true' iff {V0,V1,V2} is a linearly independent set.
// Numerically, this is measured by |det[V0 V1 V2]| <= epsilon.  The values
// bary[] are valid only when the return value is 'true' but set to zero when
// the return value is 'false'.
template <typename Real>
bool ComputeBarycentrics(Vector2<Real> const& p, Vector2<Real> const& v0,
    Vector2<Real> const& v1, Vector2<Real> const& v2, Real bary[3],
    Real epsilon = (Real)0);

// Get intrinsic information about the input array of vectors.  The return
// value is 'true' iff the inputs are valid (numVectors > 0, v is not null,
// and epsilon >= 0), in which case the class members are valid.
template <typename Real>
class IntrinsicsVector2
{
public:
    // The constructor sets the class members based on the input set.
    IntrinsicsVector2(int numVectors, Vector2<Real> const* v, Real inEpsilon);

    // A nonnegative tolerance that is used to determine the intrinsic
    // dimension of the set.
    Real epsilon;

    // The intrinsic dimension of the input set, computed based on the
    // nonnegative tolerance mEpsilon.
    int dimension;

    // Axis-aligned bounding box of the input set.  The maximum range is
    // the larger of max[0]-min[0] and max[1]-min[1].
    Real min[2], max[2];
    Real maxRange;

    // Coordinate system.  The origin is valid for any dimension d.  The
    // unit-length direction vector is valid only for 0 <= i < d.  The
    // extreme index is relative to the array of input points, and is also
    // valid only for 0 <= i < d.  If d = 0, all points are effectively
    // the same, but the use of an epsilon may lead to an extreme index
    // that is not zero.  If d = 1, all points effectively lie on a line
    // segment.  If d = 2, the points are not collinear.
    Vector2<Real> origin;
    Vector2<Real> direction[2];

    // The indices that define the maximum dimensional extents.  The
    // values extreme[0] and extreme[1] are the indices for the points
    // that define the largest extent in one of the coordinate axis
    // directions.  If the dimension is 2, then extreme[2] is the index
    // for the point that generates the largest extent in the direction
    // perpendicular to the line through the points corresponding to
    // extreme[0] and extreme[1].  The triangle formed by the points
    // V[extreme[0]], V[extreme[1]], and V[extreme[2]] is clockwise or
    // counterclockwise, the condition stored in extremeCCW.
    int extreme[3];
    bool extremeCCW;
};


template <typename Real>
Vector2<Real> Perp(Vector2<Real> const& v)
{
    return Vector2<Real>{ v[1], -v[0] };
}

template <typename Real>
Vector2<Real> UnitPerp(Vector2<Real> const& v, bool robust)
{
    Vector2<Real> unitPerp{ v[1], -v[0] };
    Normalize(unitPerp, robust);
    return unitPerp;
}

template <typename Real>
Real DotPerp(Vector2<Real> const& v0, Vector2<Real> const& v1)
{
    return Dot(v0, Perp(v1));
}

template <typename Real>
Real ComputeOrthogonalComplement(int numInputs, Vector2<Real>* v, bool robust)
{
    if (numInputs == 1)
    {
        v[1] = -Perp(v[0]);
        return Orthonormalize<2, Real>(2, v, robust);
    }

    return (Real)0;
}

template <typename Real>
bool ComputeBarycentrics(Vector2<Real> const& p, Vector2<Real> const& v0,
    Vector2<Real> const& v1, Vector2<Real> const& v2, Real bary[3],
    Real epsilon)
{
    // Compute the vectors relative to V2 of the triangle.
    Vector2<Real> diff[3] = { v0 - v2, v1 - v2, p - v2 };

    Real det = DotPerp(diff[0], diff[1]);
    if (det < -epsilon || det > epsilon)
    {
        Real invDet = ((Real)1) / det;
        bary[0] = DotPerp(diff[2], diff[1])*invDet;
        bary[1] = DotPerp(diff[0], diff[2])*invDet;
        bary[2] = (Real)1 - bary[0] - bary[1];
        return true;
    }

    for (int i = 0; i < 3; ++i)
    {
        bary[i] = (Real)0;
    }
    return false;
}

template <typename Real>
IntrinsicsVector2<Real>::IntrinsicsVector2(int numVectors,
    Vector2<Real> const* v, Real inEpsilon)
    :
    epsilon(inEpsilon),
    dimension(0),
    maxRange((Real)0),
    origin({ (Real)0, (Real)0 }),
    extremeCCW(false)
{
    min[0] = (Real)0;
    min[1] = (Real)0;
    direction[0] = { (Real)0, (Real)0 };
    direction[1] = { (Real)0, (Real)0 };
    extreme[0] = 0;
    extreme[1] = 0;
    extreme[2] = 0;

    if (numVectors > 0 && v && epsilon >= (Real)0)
    {
        // Compute the axis-aligned bounding box for the input vectors.  Keep
        // track of the indices into 'vectors' for the current min and max.
        int j, indexMin[2], indexMax[2];
        for (j = 0; j < 2; ++j)
        {
            min[j] = v[0][j];
            max[j] = min[j];
            indexMin[j] = 0;
            indexMax[j] = 0;
        }

        int i;
        for (i = 1; i < numVectors; ++i)
        {
            for (j = 0; j < 2; ++j)
            {
                if (v[i][j] < min[j])
                {
                    min[j] = v[i][j];
                    indexMin[j] = i;
                }
                else if (v[i][j] > max[j])
                {
                    max[j] = v[i][j];
                    indexMax[j] = i;
                }
            }
        }

        // Determine the maximum range for the bounding box.
        maxRange = max[0] - min[0];
        extreme[0] = indexMin[0];
        extreme[1] = indexMax[0];
        Real range = max[1] - min[1];
        if (range > maxRange)
        {
            maxRange = range;
            extreme[0] = indexMin[1];
            extreme[1] = indexMax[1];
        }

        // The origin is either the vector of minimum x0-value or vector of
        // minimum x1-value.
        origin = v[extreme[0]];

        // Test whether the vector set is (nearly) a vector.
        if (maxRange <= epsilon)
        {
            dimension = 0;
            for (j = 0; j < 2; ++j)
            {
                extreme[j + 1] = extreme[0];
            }
            return;
        }

        // Test whether the vector set is (nearly) a line segment.  We need
        // direction[1] to span the orthogonal complement of direction[0].
        direction[0] = v[extreme[1]] - origin;
        Normalize(direction[0], false);
        direction[1] = -Perp(direction[0]);

        // Compute the maximum distance of the points from the line
        // origin+t*direction[0].
        Real maxDistance = (Real)0;
        Real maxSign = (Real)0;
        extreme[2] = extreme[0];
        for (i = 0; i < numVectors; ++i)
        {
            Vector2<Real> diff = v[i] - origin;
            Real distance = Dot(direction[1], diff);
            Real sign = (distance >(Real)0 ? (Real)1 :
                (distance < (Real)0 ? (Real)-1 : (Real)0));
            distance = std::abs(distance);
            if (distance > maxDistance)
            {
                maxDistance = distance;
                maxSign = sign;
                extreme[2] = i;
            }
        }

        if (maxDistance <= epsilon * maxRange)
        {
            // The points are (nearly) on the line origin+t*direction[0].
            dimension = 1;
            extreme[2] = extreme[1];
            return;
        }

        dimension = 2;
        extremeCCW = (maxSign > (Real)0);
        return;
    }
}


}


// ----------------------------------------------
// GtePrimalQuery2.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

// Queries about the relation of a point to various geometric objects.  The
// choices for N when using UIntegerFP32<N> for either BSNumber of BSRational
// are determined in GeometricTools/GTEngine/Tools/PrecisionCalculator.  These
// N-values are worst case scenarios. Your specific input data might require
// much smaller N, in which case you can modify PrecisionCalculator to use the
// BSPrecision(int32_t,int32_t,int32_t,bool) constructors.

namespace gte
{

template <typename Real>
class PrimalQuery2
{
public:
    // The caller is responsible for ensuring that the array is not empty
    // before calling queries and that the indices passed to the queries are
    // valid.  The class does no range checking.
    PrimalQuery2();
    PrimalQuery2(int numVertices, Vector2<Real> const* vertices);

    // Member access.
    inline void Set(int numVertices, Vector2<Real> const* vertices);
    inline int GetNumVertices() const;
    inline Vector2<Real> const* GetVertices() const;

    // In the following, point P refers to vertices[i] or 'test' and Vi refers
    // to vertices[vi].

    // For a line with origin V0 and direction <V0,V1>, ToLine returns
    //   +1, P on right of line
    //   -1, P on left of line
    //    0, P on the line
    //
    // Choice of N for UIntegerFP32<N>.
    //    input type | compute type | N
    //    -----------+--------------+-----
    //    float      | BSNumber     |   18
    //    double     | BSNumber     |  132
    //    float      | BSRational   |  214
    //    double     | BSRational   | 1587
    int ToLine(int i, int v0, int v1) const;
    int ToLine(Vector2<Real> const& test, int v0, int v1) const;

    // For a line with origin V0 and direction <V0,V1>, ToLine returns
    //   +1, P on right of line
    //   -1, P on left of line
    //    0, P on the line
    // The 'order' parameter is
    //   -3, points not collinear, P on left of line
    //   -2, P strictly left of V0 on the line
    //   -1, P = V0
    //    0, P interior to line segment [V0,V1]
    //   +1, P = V1
    //   +2, P strictly right of V0 on the line
    //
    // Choice of N for UIntegerFP32<N>.
    //    input type | compute type | N
    //    -----------+--------------+-----
    //    float      | BSNumber     |   18
    //    double     | BSNumber     |  132
    //    float      | BSRational   |  214
    //    double     | BSRational   | 1587
    // This is the same as the first-listed ToLine calls because the worst-case
    // path has the same computational complexity.
    int ToLine(int i, int v0, int v1, int& order) const;
    int ToLine(Vector2<Real> const& test, int v0, int v1, int& order) const;

    // For a triangle with counterclockwise vertices V0, V1, and V2,
    // ToTriangle returns
    //   +1, P outside triangle
    //   -1, P inside triangle
    //    0, P on triangle
    //
    // Choice of N for UIntegerFP32<N>.
    //    input type | compute type | N
    //    -----------+--------------+-----
    //    float      | BSNumber     |   18
    //    double     | BSNumber     |  132
    //    float      | BSRational   |  214
    //    double     | BSRational   | 1587
    // The query involves three calls to ToLine, so the numbers match those
    // of ToLine.
    int ToTriangle(int i, int v0, int v1, int v2) const;
    int ToTriangle(Vector2<Real> const& test, int v0, int v1, int v2) const;

    // For a triangle with counterclockwise vertices V0, V1, and V2,
    // ToCircumcircle returns
    //   +1, P outside circumcircle of triangle
    //   -1, P inside circumcircle of triangle
    //    0, P on circumcircle of triangle
    //
    // Choice of N for UIntegerFP32<N>.
    //    input type | compute type | N
    //    -----------+--------------+------
    //    float      | BSNumber     |    35
    //    double     | BSNumber     |   263
    //    float      | BSRational   | 12302
    //    double     | BSRational   | 92827
    // The query involves three calls of ToLine, so the numbers match those
    // of ToLine.
    int ToCircumcircle(int i, int v0, int v1, int v2) const;
    int ToCircumcircle(Vector2<Real> const& test, int v0, int v1, int v2) const;

    // An extended classification of the relationship of a point to a line
    // segment.  For noncollinear points, the return value is
    //   ORDER_POSITIVE when <P,Q0,Q1> is a counterclockwise triangle
    //   ORDER_NEGATIVE when <P,Q0,Q1> is a clockwise triangle
    // For collinear points, the line direction is Q1-Q0.  The return value is
    //   ORDER_COLLINEAR_LEFT when the line ordering is <P,Q0,Q1>
    //   ORDER_COLLINEAR_RIGHT when the line ordering is <Q0,Q1,P>
    //   ORDER_COLLINEAR_CONTAIN when the line ordering is <Q0,P,Q1>
    enum OrderType
    {
        ORDER_Q0_EQUALS_Q1,
        ORDER_P_EQUALS_Q0,
        ORDER_P_EQUALS_Q1,
        ORDER_POSITIVE,
        ORDER_NEGATIVE,
        ORDER_COLLINEAR_LEFT,
        ORDER_COLLINEAR_RIGHT,
        ORDER_COLLINEAR_CONTAIN
    };

    // Choice of N for UIntegerFP32<N>.
    //    input type | compute type | N
    //    -----------+--------------+-----
    //    float      | BSNumber     |   18
    //    double     | BSNumber     |  132
    //    float      | BSRational   |  214
    //    double     | BSRational   | 1587
    // This is the same as the first-listed ToLine calls because the worst-case
    // path has the same computational complexity.
    OrderType ToLineExtended(Vector2<Real> const& P, Vector2<Real> const& Q0, Vector2<Real> const& Q1) const;

private:
    int mNumVertices;
    Vector2<Real> const* mVertices;
};


template <typename Real>
PrimalQuery2<Real>::PrimalQuery2()
    :
    mNumVertices(0),
    mVertices(nullptr)
{
}

template <typename Real>
PrimalQuery2<Real>::PrimalQuery2(int numVertices,
    Vector2<Real> const* vertices)
    :
    mNumVertices(numVertices),
    mVertices(vertices)
{
}

template <typename Real> inline
void PrimalQuery2<Real>::Set(int numVertices, Vector2<Real> const* vertices)
{
    mNumVertices = numVertices;
    mVertices = vertices;
}

template <typename Real> inline
int PrimalQuery2<Real>::GetNumVertices() const
{
    return mNumVertices;
}

template <typename Real> inline
Vector2<Real> const* PrimalQuery2<Real>::GetVertices() const
{
    return mVertices;
}

template <typename Real>
int PrimalQuery2<Real>::ToLine(int i, int v0, int v1) const
{
    return ToLine(mVertices[i], v0, v1);
}

template <typename Real>
int PrimalQuery2<Real>::ToLine(Vector2<Real> const& test, int v0, int v1) const
{
    Vector2<Real> const& vec0 = mVertices[v0];
    Vector2<Real> const& vec1 = mVertices[v1];

    Real x0 = test[0] - vec0[0];
    Real y0 = test[1] - vec0[1];
    Real x1 = vec1[0] - vec0[0];
    Real y1 = vec1[1] - vec0[1];
    Real x0y1 = x0*y1;
    Real x1y0 = x1*y0;
    Real det = x0y1 - x1y0;
    Real const zero(0);

    return (det > zero ? +1 : (det < zero ? -1 : 0));
}

template <typename Real>
int PrimalQuery2<Real>::ToLine(int i, int v0, int v1, int& order) const
{
    return ToLine(mVertices[i], v0, v1, order);
}

template <typename Real>
int PrimalQuery2<Real>::ToLine(Vector2<Real> const& test, int v0, int v1, int& order) const
{
    Vector2<Real> const& vec0 = mVertices[v0];
    Vector2<Real> const& vec1 = mVertices[v1];

    Real x0 = test[0] - vec0[0];
    Real y0 = test[1] - vec0[1];
    Real x1 = vec1[0] - vec0[0];
    Real y1 = vec1[1] - vec0[1];
    Real x0y1 = x0*y1;
    Real x1y0 = x1*y0;
    Real det = x0y1 - x1y0;
    Real const zero(0);

    if (det > zero)
    {
        order = +3;
        return +1;
    }

    if (det < zero)
    {
        order = -3;
        return -1;
    }

    Real x0x1 = x0*x1;
    Real y0y1 = y0*y1;
    Real dot = x0x1 + y0y1;
    if (dot == zero)
    {
        order = -1;
    }
    else if (dot < zero)
    {
        order = -2;
    }
    else
    {
        Real x0x0 = x0*x0;
        Real y0y0 = y0*y0;
        Real sqrLength = x0x0 + y0y0;
        if (dot == sqrLength)
        {
            order = +1;
        }
        else if (dot > sqrLength)
        {
            order = +2;
        }
        else
        {
            order = 0;
        }
    }

    return 0;
}

template <typename Real>
int PrimalQuery2<Real>::ToTriangle(int i, int v0, int v1, int v2) const
{
    return ToTriangle(mVertices[i], v0, v1, v2);
}

template <typename Real>
int PrimalQuery2<Real>::ToTriangle(Vector2<Real> const& test, int v0, int v1, int v2) const
{
    int sign0 = ToLine(test, v1, v2);
    if (sign0 > 0)
    {
        return +1;
    }

    int sign1 = ToLine(test, v0, v2);
    if (sign1 < 0)
    {
        return +1;
    }

    int sign2 = ToLine(test, v0, v1);
    if (sign2 > 0)
    {
        return +1;
    }

    return ((sign0 && sign1 && sign2) ? -1 : 0);
}

template <typename Real>
int PrimalQuery2<Real>::ToCircumcircle(int i, int v0, int v1, int v2) const
{
    return ToCircumcircle(mVertices[i], v0, v1, v2);
}

template <typename Real>
int PrimalQuery2<Real>::ToCircumcircle(Vector2<Real> const& test, int v0, int v1, int v2) const
{
    Vector2<Real> const& vec0 = mVertices[v0];
    Vector2<Real> const& vec1 = mVertices[v1];
    Vector2<Real> const& vec2 = mVertices[v2];

    Real x0 = vec0[0] - test[0];
    Real y0 = vec0[1] - test[1];
    Real s00 = vec0[0] + test[0];
    Real s01 = vec0[1] + test[1];
    Real t00 = s00*x0;
    Real t01 = s01*y0;
    Real z0 = t00 + t01;

    Real x1 = vec1[0] - test[0];
    Real y1 = vec1[1] - test[1];
    Real s10 = vec1[0] + test[0];
    Real s11 = vec1[1] + test[1];
    Real t10 = s10*x1;
    Real t11 = s11*y1;
    Real z1 = t10 + t11;

    Real x2 = vec2[0] - test[0];
    Real y2 = vec2[1] - test[1];
    Real s20 = vec2[0] + test[0];
    Real s21 = vec2[1] + test[1];
    Real t20 = s20*x2;
    Real t21 = s21*y2;
    Real z2 = t20 + t21;

    Real y0z1 = y0*z1;
    Real y0z2 = y0*z2;
    Real y1z0 = y1*z0;
    Real y1z2 = y1*z2;
    Real y2z0 = y2*z0;
    Real y2z1 = y2*z1;
    Real c0 = y1z2 - y2z1;
    Real c1 = y2z0 - y0z2;
    Real c2 = y0z1 - y1z0;
    Real x0c0 = x0*c0;
    Real x1c1 = x1*c1;
    Real x2c2 = x2*c2;
    Real term = x0c0 + x1c1;
    Real det = term + x2c2;
    Real const zero(0);

    return (det < zero ? 1 : (det > zero ? -1 : 0));
}

template <typename Real>
typename PrimalQuery2<Real>::OrderType PrimalQuery2<Real>::ToLineExtended(
    Vector2<Real> const& P, Vector2<Real> const& Q0, Vector2<Real> const& Q1) const
{
    Real const zero(0);

    Real x0 = Q1[0] - Q0[0];
    Real y0 = Q1[1] - Q0[1];
    if (x0 == zero && y0 == zero)
    {
        return ORDER_Q0_EQUALS_Q1;
    }

    Real x1 = P[0] - Q0[0];
    Real y1 = P[1] - Q0[1];
    if (x1 == zero && y1 == zero)
    {
        return ORDER_P_EQUALS_Q0;
    }

    Real x2 = P[0] - Q1[0];
    Real y2 = P[1] - Q1[1];
    if (x2 == zero && y2 == zero)
    {
        return ORDER_P_EQUALS_Q1;
    }

    // The theoretical classification relies on computing exactly the sign of
    // the determinant.  Numerical roundoff errors can cause misclassification.
    Real x0y1 = x0 * y1;
    Real x1y0 = x1 * y0;
    Real det = x0y1 - x1y0;

    if (det != zero)
    {
        if (det > zero)
        {
            // The points form a counterclockwise triangle <P,Q0,Q1>.
            return ORDER_POSITIVE;
        }
        else
        {
            // The points form a clockwise triangle <P,Q1,Q0>.
            return ORDER_NEGATIVE;
        }
    }
    else
    {
        // The points are collinear; P is on the line through Q0 and Q1.
        Real x0x1 = x0 * x1;
        Real y0y1 = y0 * y1;
        Real dot = x0x1 + y0y1;
        if (dot < zero)
        {
            // The line ordering is <P,Q0,Q1>.
            return ORDER_COLLINEAR_LEFT;
        }

        Real x0x0 = x0 * x0;
        Real y0y0 = y0 * y0;
        Real sqrLength = x0x0 + y0y0;
        if (dot > sqrLength)
        {
            // The line ordering is <Q0,Q1,P>.
            return ORDER_COLLINEAR_RIGHT;
        }

        // The line ordering is <Q0,P,Q1> with P strictly between Q0 and Q1.
        return ORDER_COLLINEAR_CONTAIN;
    }
}


}


// ----------------------------------------------
// GteConvexHull2.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

// Compute the convex hull of 2D points using a divide-and-conquer algorithm.
// This is an O(N log N) algorithm for N input points.  The only way to ensure
// a correct result for the input vertices (assumed to be exact) is to choose
// ComputeType for exact rational arithmetic.  You may use BSNumber.  No
// divisions are performed in this computation, so you do not have to use
// BSRational.
//
// The worst-case choices of N for Real of type BSNumber or BSRational with
// integer storage UIntegerFP32<N> are listed in the next table.  The numerical
// computations are encapsulated in PrimalQuery2<Real>::ToLineExtended.  We
// recommend using only BSNumber, because no divisions are performed in the
// convex-hull computations.
//
//    input type | compute type | N
//    -----------+--------------+------
//    float      | BSNumber     |   18
//    double     | BSNumber     |  132
//    float      | BSRational   |  214
//    double     | BSRational   | 1587

namespace gte
{

template <typename InputType, typename ComputeType>
class ConvexHull2
{
public:
    // The class is a functor to support computing the convex hull of multiple
    // data sets using the same class object.
    ConvexHull2();

    // The input is the array of points whose convex hull is required.  The
    // epsilon value is used to determine the intrinsic dimensionality of the
    // vertices (d = 0, 1, or 2).  When epsilon is positive, the determination
    // is fuzzy--points approximately the same point, approximately on a
    // line, or planar.  The return value is 'true' if and only if the hull
    // construction is successful.
    bool operator()(int numPoints, Vector2<InputType> const* points, InputType epsilon);

    // Dimensional information.  If GetDimension() returns 1, the points lie
    // on a line P+t*D (fuzzy comparison when epsilon > 0).  You can sort
    // these if you need a polyline output by projecting onto the line each
    // vertex X = P+t*D, where t = Dot(D,X-P).
    inline InputType GetEpsilon() const;
    inline int GetDimension() const;
    inline Line2<InputType> const& GetLine() const;

    // Member access.
    inline int GetNumPoints() const;
    inline int GetNumUniquePoints() const;
    inline Vector2<InputType> const* GetPoints() const;
    inline PrimalQuery2<ComputeType> const& GetQuery() const;

    // The convex hull is a convex polygon whose vertices are listed in
    // counterclockwise order.
    inline std::vector<int> const& GetHull() const;

private:
    // Support for divide-and-conquer.
    void GetHull(int& i0, int& i1);
    void Merge(int j0, int j1, int j2, int j3, int& i0, int& i1);
    void GetTangent(int j0, int j1, int j2, int j3, int& i0, int& i1);

    // The epsilon value is used for fuzzy determination of intrinsic
    // dimensionality.  If the dimension is 0 or 1, the constructor returns
    // early.  The caller is responsible for retrieving the dimension and
    // taking an alternate path should the dimension be smaller than 2.  If
    // the dimension is 0, the caller may as well treat all points[] as a
    // single point, say, points[0].  If the dimension is 1, the caller can
    // query for the approximating line and project points[] onto it for
    // further processing.
    InputType mEpsilon;
    int mDimension;
    Line2<InputType> mLine;

    // The array of points used for geometric queries.  If you want to be
    // certain of a correct result, choose ComputeType to be BSNumber.
    std::vector<Vector2<ComputeType>> mComputePoints;
    PrimalQuery2<ComputeType> mQuery;

    int mNumPoints;
    int mNumUniquePoints;
    Vector2<InputType> const* mPoints;
    std::vector<int> mMerged, mHull;
};


template <typename InputType, typename ComputeType>
ConvexHull2<InputType, ComputeType>::ConvexHull2()
    :
    mEpsilon((InputType)0),
    mDimension(0),
    mLine(Vector2<InputType>::Zero(), Vector2<InputType>::Zero()),
    mNumPoints(0),
    mNumUniquePoints(0),
    mPoints(nullptr)
{
}

template <typename InputType, typename ComputeType>
bool ConvexHull2<InputType, ComputeType>::operator()(int numPoints,
    Vector2<InputType> const* points, InputType epsilon)
{
    mEpsilon = std::max(epsilon, (InputType)0);
    mDimension = 0;
    mLine.origin = Vector2<InputType>::Zero();
    mLine.direction = Vector2<InputType>::Zero();
    mNumPoints = numPoints;
    mNumUniquePoints = 0;
    mPoints = points;
    mMerged.clear();
    mHull.clear();

    int i, j;
    if (mNumPoints < 3)
    {
        // ConvexHull2 should be called with at least three points.
        return false;
    }

    IntrinsicsVector2<InputType> info(mNumPoints, mPoints, mEpsilon);
    if (info.dimension == 0)
    {
        // mDimension is 0
        return false;
    }

    if (info.dimension == 1)
    {
        // The set is (nearly) collinear.
        mDimension = 1;
        mLine = Line2<InputType>(info.origin, info.direction[0]);
        return false;
    }

    mDimension = 2;

    // Compute the points for the queries.
    mComputePoints.resize(mNumPoints);
    mQuery.Set(mNumPoints, &mComputePoints[0]);
    for (i = 0; i < mNumPoints; ++i)
    {
        for (j = 0; j < 2; ++j)
        {
            mComputePoints[i][j] = points[i][j];
        }
    }

    // Sort the points.
    mHull.resize(mNumPoints);
    for (i = 0; i < mNumPoints; ++i)
    {
        mHull[i] = i;
    }
    std::sort(mHull.begin(), mHull.end(),
        [points](int i0, int i1)
        {
            if (points[i0][0] < points[i1][0]) { return true; }
            if (points[i0][0] > points[i1][0]) { return false; }
            return points[i0][1] < points[i1][1];
        }
    );

    // Remove duplicates.
    auto newEnd = std::unique(mHull.begin(), mHull.end(),
        [points](int i0, int i1)
        {
            return points[i0] == points[i1];
        }
    );
    mHull.erase(newEnd, mHull.end());
    mNumUniquePoints = static_cast<int>(mHull.size());

    // Use a divide-and-conquer algorithm.  The merge step computes the
    // convex hull of two convex polygons.
    mMerged.resize(mNumUniquePoints);
    int i0 = 0, i1 = mNumUniquePoints - 1;
    GetHull(i0, i1);
    mHull.resize(i1 - i0 + 1);
    return true;
}

template <typename InputType, typename ComputeType> inline
InputType ConvexHull2<InputType, ComputeType>::GetEpsilon() const
{
    return mEpsilon;
}

template <typename InputType, typename ComputeType> inline
int ConvexHull2<InputType, ComputeType>::GetDimension() const
{
    return mDimension;
}

template <typename InputType, typename ComputeType> inline
Line2<InputType> const& ConvexHull2<InputType, ComputeType>::GetLine() const
{
    return mLine;
}

template <typename InputType, typename ComputeType> inline
int ConvexHull2<InputType, ComputeType>::GetNumPoints() const
{
    return mNumPoints;
}

template <typename InputType, typename ComputeType> inline
int ConvexHull2<InputType, ComputeType>::GetNumUniquePoints() const
{
    return mNumUniquePoints;
}

template <typename InputType, typename ComputeType> inline
Vector2<InputType> const* ConvexHull2<InputType, ComputeType>::GetPoints() const
{
    return mPoints;
}

template <typename InputType, typename ComputeType> inline
PrimalQuery2<ComputeType> const& ConvexHull2<InputType, ComputeType>::GetQuery() const
{
    return mQuery;
}

template <typename InputType, typename ComputeType> inline
std::vector<int> const& ConvexHull2<InputType, ComputeType>::GetHull() const
{
    return mHull;
}

template <typename InputType, typename ComputeType> inline
void ConvexHull2<InputType, ComputeType>::GetHull(int& i0, int& i1)
{
    int numVertices = i1 - i0 + 1;
    if (numVertices > 1)
    {
        // Compute the middle index of input range.
        int mid = (i0 + i1) / 2;

        // Compute the hull of subsets (mid-i0+1 >= i1-mid).
        int j0 = i0, j1 = mid, j2 = mid + 1, j3 = i1;
        GetHull(j0, j1);
        GetHull(j2, j3);

        // Merge the convex hulls into a single convex hull.
        Merge(j0, j1, j2, j3, i0, i1);
    }
    // else: The convex hull is a single point.
}

template <typename InputType, typename ComputeType> inline
void ConvexHull2<InputType, ComputeType>::Merge(int j0, int j1, int j2, int j3,
    int& i0, int& i1)
{
    // Subhull0 is to the left of subhull1 because of the initial sorting of
    // the points by x-components.  We need to find two mutually visible
    // points, one on the left subhull and one on the right subhull.
    int size0 = j1 - j0 + 1;
    int size1 = j3 - j2 + 1;

    int i;
    Vector2<ComputeType> p;

    // Find the right-most point of the left subhull.
    Vector2<ComputeType> pmax0 = mComputePoints[mHull[j0]];
    int imax0 = j0;
    for (i = j0 + 1; i <= j1; ++i)
    {
        p = mComputePoints[mHull[i]];
        if (pmax0 < p)
        {
            pmax0 = p;
            imax0 = i;
        }
    }

    // Find the left-most point of the right subhull.
    Vector2<ComputeType> pmin1 = mComputePoints[mHull[j2]];
    int imin1 = j2;
    for (i = j2 + 1; i <= j3; ++i)
    {
        p = mComputePoints[mHull[i]];
        if (p < pmin1)
        {
            pmin1 = p;
            imin1 = i;
        }
    }

    // Get the lower tangent to hulls (LL = lower-left, LR = lower-right).
    int iLL = imax0, iLR = imin1;
    GetTangent(j0, j1, j2, j3, iLL, iLR);

    // Get the upper tangent to hulls (UL = upper-left, UR = upper-right).
    int iUL = imax0, iUR = imin1;
    GetTangent(j2, j3, j0, j1, iUR, iUL);

    // Construct the counterclockwise-ordered merged-hull vertices.
    int k;
    int numMerged = 0;

    i = iUL;
    for (k = 0; k < size0; ++k)
    {
        mMerged[numMerged++] = mHull[i];
        if (i == iLL)
        {
            break;
        }
        i = (i < j1 ? i + 1 : j0);
    }
    LogAssert(k < size0, "Unexpected condition.");

    i = iLR;
    for (k = 0; k < size1; ++k)
    {
        mMerged[numMerged++] = mHull[i];
        if (i == iUR)
        {
            break;
        }
        i = (i < j3 ? i + 1 : j2);
    }
    LogAssert(k < size1, "Unexpected condition.");

    int next = j0;
    for (k = 0; k < numMerged; ++k)
    {
        mHull[next] = mMerged[k];
        ++next;
    }

    i0 = j0;
    i1 = next - 1;
}

template <typename InputType, typename ComputeType> inline
void ConvexHull2<InputType, ComputeType>::GetTangent(int j0, int j1, int j2, int j3,
    int& i0, int& i1)
{
    // In theory the loop terminates in a finite number of steps, but the
    // upper bound for the loop variable is used to trap problems caused by
    // floating point roundoff errors that might lead to an infinite loop.

    int size0 = j1 - j0 + 1;
    int size1 = j3 - j2 + 1;
    int const imax = size0 + size1;
    int i, iLm1, iRp1;
    Vector2<ComputeType> L0, L1, R0, R1;

    for (i = 0; i < imax; i++)
    {
        // Get the endpoints of the potential tangent.
        L1 = mComputePoints[mHull[i0]];
        R0 = mComputePoints[mHull[i1]];

        // Walk along the left hull to find the point of tangency.
        if (size0 > 1)
        {
            iLm1 = (i0 > j0 ? i0 - 1 : j1);
            L0 = mComputePoints[mHull[iLm1]];
            auto order = mQuery.ToLineExtended(R0, L0, L1);
            if (order == PrimalQuery2<ComputeType>::ORDER_NEGATIVE
                || order == PrimalQuery2<ComputeType>::ORDER_COLLINEAR_RIGHT)
            {
                i0 = iLm1;
                continue;
            }
        }

        // Walk along right hull to find the point of tangency.
        if (size1 > 1)
        {
            iRp1 = (i1 < j3 ? i1 + 1 : j2);
            R1 = mComputePoints[mHull[iRp1]];
            auto order = mQuery.ToLineExtended(L1, R0, R1);
            if (order == PrimalQuery2<ComputeType>::ORDER_NEGATIVE
                || order == PrimalQuery2<ComputeType>::ORDER_COLLINEAR_LEFT)
            {
                i1 = iRp1;
                continue;
            }
        }

        // The tangent segment has been found.
        break;
    }

    // Detect an "infinite loop" caused by floating point round-off errors.
    LogAssert(i < imax, "Unexpected condition.");
}


}



// ----------------------------------------------
// GteMinimumAreaBox2.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.2 (2018/10/05)

// Compute a minimum-area oriented box containing the specified points.  The
// algorithm uses the rotating calipers method.
//   http://www-cgrl.cs.mcgill.ca/~godfried/research/calipers.html
//   http://cgm.cs.mcgill.ca/~orm/rotcal.html
// The box is supported by the convex hull of the points, so the algorithm
// is really about computing the minimum-area box containing a convex polygon.
// The rotating calipers approach is O(n) in time for n polygon edges.
//
// A detailed description of the algorithm and implementation is found in
//   http://www.geometrictools.com/Documentation/MinimumAreaRectangle.pdf
//
// NOTE: This algorithm guarantees a correct output only when ComputeType is
// an exact arithmetic type that supports division.  In GTEngine, one such
// type is BSRational<UIntegerAP32> (arbitrary precision).  Another such type
// is BSRational<UIntegerFP32<N>> (fixed precision), where N is chosen large
// enough for your input data sets.  If you choose ComputeType to be 'float'
// or 'double', the output is not guaranteed to be correct.
//
// See GeometricTools/GTEngine/Samples/Geometrics/MinimumAreaBox2 for an
// example of how to use the code.

namespace gte
{

template <typename InputType, typename ComputeType>
class MinimumAreaBox2
{
public:
    // The class is a functor to support computing the minimum-area box of
    // multiple data sets using the same class object.
    MinimumAreaBox2();

    // The points are arbitrary, so we must compute the convex hull from
    // them in order to compute the minimum-area box.  The input parameters
    // are necessary for using ConvexHull2.  NOTE:  ConvexHull2 guarantees
    // that the hull does not have three consecutive collinear points.
    OrientedBox2<InputType> operator()(int numPoints,
        Vector2<InputType> const* points,
        bool useRotatingCalipers =
            !std::is_floating_point<ComputeType>::value);

    // The points already form a counterclockwise, nondegenerate convex
    // polygon.  If the points directly are the convex polygon, set
    // numIndices to 0 and indices to nullptr.  If the polygon vertices
    // are a subset of the incoming points, that subset is identified by
    // numIndices >= 3 and indices having numIndices elements.
    OrientedBox2<InputType> operator()(int numPoints,
        Vector2<InputType> const* points, int numIndices, int const* indices,
        bool useRotatingCalipers =
            !std::is_floating_point<ComputeType>::value);

    // Member access.
    inline int GetNumPoints() const;
    inline Vector2<InputType> const* GetPoints() const;
    inline std::vector<int> const& GetHull() const;
    inline std::array<int, 4> const& GetSupportIndices() const;
    inline InputType GetArea() const;

private:
    // The box axes are U[i] and are usually not unit-length in order to allow
    // exact arithmetic.  The box is supported by mPoints[index[i]], where i
    // is one of the enumerations above.  The box axes are not necessarily unit
    // length, but they have the same length.  They need to be normalized for
    // conversion back to InputType.
    struct Box
    {
        Vector2<ComputeType> U[2];
        std::array<int, 4> index;  // order: bottom, right, top, left
        ComputeType sqrLenU0, area;
    };

    // The rotating calipers algorithm has a loop invariant that requires
    // the convex polygon not to have collinear points.  Any such points
    // must be removed first.  The code is also executed for the O(n^2)
    // algorithm to reduce the number of process edges.
    void RemoveCollinearPoints(std::vector<Vector2<ComputeType>>& vertices);

    // This is the slow O(n^2) search.
    Box ComputeBoxForEdgeOrderNSqr(
        std::vector<Vector2<ComputeType>> const& vertices);

    // The fast O(n) search.
    Box ComputeBoxForEdgeOrderN(
        std::vector<Vector2<ComputeType>> const& vertices);

    // Compute the smallest box for the polygon edge <V[i0],V[i1]>.
    Box SmallestBox(int i0, int i1,
        std::vector<Vector2<ComputeType>> const& vertices);

    // Compute (sin(angle))^2 for the polygon edges emanating from the support
    // vertices of the box.  The return value is 'true' if at least one angle
    // is in [0,pi/2); otherwise, the return value is 'false' and the original
    // polygon must be a rectangle.
    bool ComputeAngles(std::vector<Vector2<ComputeType>> const& vertices,
        Box const& box, std::array<std::pair<ComputeType, int>, 4>& A,
        int& numA) const;

    // Sort the angles indirectly.  The sorted indices are returned.  This
    // avoids swapping elements of A[], which can be expensive when
    // ComputeType is an exact rational type.
    std::array<int, 4> SortAngles(
        std::array<std::pair<ComputeType, int>, 4> const& A, int numA) const;

    bool UpdateSupport(std::array<std::pair<ComputeType, int>, 4> const& A,
        int numA, std::array<int, 4> const& sort,
        std::vector<Vector2<ComputeType>> const& vertices,
        std::vector<bool>& visited, Box& box);

    // Convert the ComputeType box to the InputType box.  When the ComputeType
    // is an exact rational type, the conversions are performed to avoid
    // precision loss until necessary at the last step.
    void ConvertTo(Box const& minBox,
        std::vector<Vector2<ComputeType>> const& computePoints,
        OrientedBox2<InputType>& itMinBox);

    // The input points to be bound.
    int mNumPoints;
    Vector2<InputType> const* mPoints;

    // The indices into mPoints/mComputePoints for the convex hull vertices.
    std::vector<int> mHull;

    // The support indices for the minimum-area box.
    std::array<int, 4> mSupportIndices;

    // The area of the minimum-area box.  The ComputeType value is exact,
    // so the only rounding errors occur in the conversion from ComputeType
    // to InputType (default rounding mode is round-to-nearest-ties-to-even).
    InputType mArea;

    // Convenient values that occur regularly in the code.  When using
    // rational ComputeType, we construct these numbers only once.
    ComputeType mZero, mOne, mNegOne, mHalf;
};


template <typename InputType, typename ComputeType>
MinimumAreaBox2<InputType, ComputeType>::MinimumAreaBox2()
    :
    mNumPoints(0),
    mPoints(nullptr),
    mArea((InputType)0),
    mZero(0),
    mOne(1),
    mNegOne(-1),
    mHalf((InputType)0.5)
{
	mSupportIndices = { { 0, 0, 0, 0 } };
}

template <typename InputType, typename ComputeType>
OrientedBox2<InputType> MinimumAreaBox2<InputType, ComputeType>::operator()(
    int numPoints, Vector2<InputType> const* points, bool useRotatingCalipers)
{
    mNumPoints = numPoints;
    mPoints = points;
    mHull.clear();

    // Get the convex hull of the points.
    ConvexHull2<InputType, ComputeType> ch2;
    ch2(mNumPoints, mPoints, (InputType)0);
    int dimension = ch2.GetDimension();

    OrientedBox2<InputType> minBox;

    if (dimension == 0)
    {
        // The points are all effectively the same (using fuzzy epsilon).
        minBox.center = mPoints[0];
        minBox.axis[0] = Vector2<InputType>::Unit(0);
        minBox.axis[1] = Vector2<InputType>::Unit(1);
        minBox.extent[0] = (InputType)0;
        minBox.extent[1] = (InputType)0;
        mHull.resize(1);
        mHull[0] = 0;
        return minBox;
    }

    if (dimension == 1)
    {
        // The points effectively lie on a line (using fuzzy epsilon).
        // Determine the extreme t-values for the points represented as
        // P = origin + t*direction.  We know that 'origin' is an input
        // vertex, so we can start both t-extremes at zero.
        Line2<InputType> const& line = ch2.GetLine();
        InputType tmin = (InputType)0, tmax = (InputType)0;
        int imin = 0, imax = 0;
        for (int i = 0; i < mNumPoints; ++i)
        {
            Vector2<InputType> diff = mPoints[i] - line.origin;
            InputType t = Dot(diff, line.direction);
            if (t > tmax)
            {
                tmax = t;
                imax = i;
            }
            else if (t < tmin)
            {
                tmin = t;
                imin = i;
            }
        }

        minBox.center = line.origin +
            ((InputType)0.5)*(tmin + tmax) * line.direction;
        minBox.extent[0] = ((InputType)0.5)*(tmax - tmin);
        minBox.extent[1] = (InputType)0;
        minBox.axis[0] = line.direction;
        minBox.axis[1] = -Perp(line.direction);
        mHull.resize(2);
        mHull[0] = imin;
        mHull[1] = imax;
        return minBox;
    }

    mHull = ch2.GetHull();
    Vector2<ComputeType> const* queryPoints = ch2.GetQuery().GetVertices();
    std::vector<Vector2<ComputeType>> computePoints(mHull.size());
    for (size_t i = 0; i < mHull.size(); ++i)
    {
        computePoints[i] = queryPoints[mHull[i]];
    }

    RemoveCollinearPoints(computePoints);

    Box box;
    if (useRotatingCalipers)
    {
        box = ComputeBoxForEdgeOrderN(computePoints);
    }
    else
    {
        box = ComputeBoxForEdgeOrderNSqr(computePoints);
    }

    ConvertTo(box, computePoints, minBox);
    return minBox;
}

template <typename InputType, typename ComputeType>
OrientedBox2<InputType> MinimumAreaBox2<InputType, ComputeType>::operator()(
    int numPoints, Vector2<InputType> const* points, int numIndices,
    int const* indices, bool useRotatingCalipers)
{
    mHull.clear();

    OrientedBox2<InputType> minBox;

    if (numPoints < 3 || !points || (indices && numIndices < 3))
    {
        minBox.center = Vector2<InputType>::Zero();
        minBox.axis[0] = Vector2<InputType>::Unit(0);
        minBox.axis[1] = Vector2<InputType>::Unit(1);
        minBox.extent = Vector2<InputType>::Zero();
        return minBox;
    }

    if (indices)
    {
        mHull.resize(numIndices);
        std::copy(indices, indices + numIndices, mHull.begin());
    }
    else
    {
        numIndices = numPoints;
        mHull.resize(numIndices);
        for (int i = 0; i < numIndices; ++i)
        {
            mHull[i] = i;
        }
    }

    std::vector<Vector2<ComputeType>> computePoints(numIndices);
    for (int i = 0; i < numIndices; ++i)
    {
        int h = mHull[i];
        computePoints[i][0] = (ComputeType)points[h][0];
        computePoints[i][1] = (ComputeType)points[h][1];
    }

    RemoveCollinearPoints(computePoints);

    Box box;
    if (useRotatingCalipers)
    {
        box = ComputeBoxForEdgeOrderN(computePoints);
    }
    else
    {
        box = ComputeBoxForEdgeOrderNSqr(computePoints);
    }

    ConvertTo(box, computePoints, minBox);
    return minBox;
}

template <typename InputType, typename ComputeType> inline
int MinimumAreaBox2<InputType, ComputeType>::GetNumPoints() const
{
    return mNumPoints;
}

template <typename InputType, typename ComputeType> inline
Vector2<InputType> const* MinimumAreaBox2<InputType, ComputeType>::GetPoints()
    const
{
    return mPoints;
}

template <typename InputType, typename ComputeType> inline
std::vector<int> const&
MinimumAreaBox2<InputType, ComputeType>::GetHull() const
{
    return mHull;
}

template <typename InputType, typename ComputeType> inline
std::array<int, 4> const&
MinimumAreaBox2<InputType, ComputeType>::GetSupportIndices() const
{
    return mSupportIndices;
}

template <typename InputType, typename ComputeType> inline
InputType MinimumAreaBox2<InputType, ComputeType>::GetArea() const
{
    return mArea;
}

template <typename InputType, typename ComputeType>
void MinimumAreaBox2<InputType, ComputeType>::RemoveCollinearPoints(
    std::vector<Vector2<ComputeType>>& vertices)
{
    std::vector<Vector2<ComputeType>> tmpVertices = vertices;

    int const numVertices = static_cast<int>(vertices.size());
    int numNoncollinear = 0;
    Vector2<ComputeType> ePrev = tmpVertices[0] - tmpVertices.back();
    for (int i0 = 0, i1 = 1; i0 < numVertices; ++i0)
    {
        Vector2<ComputeType> eNext = tmpVertices[i1] - tmpVertices[i0];

        ComputeType dp = DotPerp(ePrev, eNext);
        if (dp != mZero)
        {
            vertices[numNoncollinear++] = tmpVertices[i0];
        }

        ePrev = eNext;
        if (++i1 == numVertices)
        {
            i1 = 0;
        }
    }

    vertices.resize(numNoncollinear);
}

template <typename InputType, typename ComputeType>
typename MinimumAreaBox2<InputType, ComputeType>::Box
MinimumAreaBox2<InputType, ComputeType>::ComputeBoxForEdgeOrderNSqr(
    std::vector<Vector2<ComputeType>> const& vertices)
{
    Box minBox;
    minBox.area = mNegOne;
    int const numIndices = static_cast<int>(vertices.size());
    for (int i0 = numIndices - 1, i1 = 0; i1 < numIndices; i0 = i1++)
    {
        Box box = SmallestBox(i0, i1, vertices);
        if (minBox.area == mNegOne || box.area < minBox.area)
        {
            minBox = box;
        }
    }
    return minBox;
}

template <typename InputType, typename ComputeType>
typename MinimumAreaBox2<InputType, ComputeType>::Box
MinimumAreaBox2<InputType, ComputeType>::ComputeBoxForEdgeOrderN(
    std::vector<Vector2<ComputeType>> const& vertices)
{
    // The inputs are assumed to be the vertices of a convex polygon that
    // is counterclockwise ordered.  The input points must not contain three
    // consecutive collinear points.

    // When the bounding box corresponding to a polygon edge is computed,
    // we mark the edge as visited.  If the edge is encountered later, the
    // algorithm terminates.
    std::vector<bool> visited(vertices.size());
    std::fill(visited.begin(), visited.end(), false);

    // Start the minimum-area rectangle search with the edge from the last
    // polygon vertex to the first.  When updating the extremes, we want the
    // bottom-most point on the left edge, the top-most point on the right
    // edge, the left-most point on the top edge, and the right-most point
    // on the bottom edge.  The polygon edges starting at these points are
    // then guaranteed not to coincide with a box edge except when an extreme
    // point is shared by two box edges (at a corner).
    Box minBox = SmallestBox((int)vertices.size() - 1, 0, vertices);
    visited[minBox.index[0]] = true;

    // Execute the rotating calipers algorithm.
    Box box = minBox;
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        std::array<std::pair<ComputeType, int>, 4> A;
        int numA;
        if (!ComputeAngles(vertices, box, A, numA))
        {
            // The polygon is a rectangle, so the search is over.
            break;
        }

        // Indirectly sort the A-array.
        std::array<int, 4> sort = SortAngles(A, numA);

        // Update the supporting indices (box.index[]) and the box axis
        // directions (box.U[]).
        if (!UpdateSupport(A, numA, sort, vertices, visited, box))
        {
            // We have already processed the box polygon edge, so the search
            // is over.
            break;
        }

        if (box.area < minBox.area)
        {
            minBox = box;
        }
    }

    return minBox;
}

template <typename InputType, typename ComputeType>
typename MinimumAreaBox2<InputType, ComputeType>::Box
MinimumAreaBox2<InputType, ComputeType>::SmallestBox(int i0, int i1,
    std::vector<Vector2<ComputeType>> const& vertices)
{
    Box box;
    box.U[0] = vertices[i1] - vertices[i0];
    box.U[1] = -Perp(box.U[0]);
	box.index = { { i1, i1, i1, i1 } };
    box.sqrLenU0 = Dot(box.U[0], box.U[0]);

    Vector2<ComputeType> const& origin = vertices[i1];
    Vector2<ComputeType> support[4];
    for (int j = 0; j < 4; ++j)
    {
        support[j] = { mZero, mZero };
    }

    int i = 0;
    for (auto const& vertex : vertices)
    {
        Vector2<ComputeType> diff = vertex - origin;
        Vector2<ComputeType> v = { Dot(box.U[0], diff), Dot(box.U[1], diff) };

        // The right-most vertex of the bottom edge is vertices[i1].  The
        // assumption of no triple of collinear vertices guarantees that
        // box.index[0] is i1, which is the initial value assigned at the
        // beginning of this function.  Therefore, there is no need to test
        // for other vertices farther to the right than vertices[i1].

        if (v[0] > support[1][0] ||
            (v[0] == support[1][0] && v[1] > support[1][1]))
        {
            // New right maximum OR same right maximum but closer to top.
            box.index[1] = i;
            support[1] = v;
        }

        if (v[1] > support[2][1] ||
            (v[1] == support[2][1] && v[0] < support[2][0]))
        {
            // New top maximum OR same top maximum but closer to left.
            box.index[2] = i;
            support[2] = v;
        }

        if (v[0] < support[3][0] ||
            (v[0] == support[3][0] && v[1] < support[3][1]))
        {
            // New left minimum OR same left minimum but closer to bottom.
            box.index[3] = i;
            support[3] = v;
        }

        ++i;
    }

    // The comment in the loop has the implication that support[0] = { 0, 0 },
    // so the scaled height (support[2][1] - support[0][1]) is simply
    // support[2][1].
    ComputeType scaledWidth = support[1][0] - support[3][0];
    ComputeType scaledHeight = support[2][1];
    box.area = scaledWidth * scaledHeight / box.sqrLenU0;
    return box;
}

template <typename InputType, typename ComputeType>
bool MinimumAreaBox2<InputType, ComputeType>::ComputeAngles(
    std::vector<Vector2<ComputeType>> const& vertices, Box const& box,
    std::array<std::pair<ComputeType, int>, 4>& A, int& numA) const
{
    int const numVertices = static_cast<int>(vertices.size());
    numA = 0;
    for (int k0 = 3, k1 = 0; k1 < 4; k0 = k1++)
    {
        if (box.index[k0] != box.index[k1])
        {
            // The box edges are ordered in k1 as U[0], U[1], -U[0], -U[1].
            Vector2<ComputeType> D =
                ((k0 & 2) ? -box.U[k0 & 1] : box.U[k0 & 1]);
            int j0 = box.index[k0], j1 = j0 + 1;
            if (j1 == numVertices)
            {
                j1 = 0;
            }
            Vector2<ComputeType> E = vertices[j1] - vertices[j0];
            ComputeType dp = DotPerp(D, E);
            ComputeType esqrlen = Dot(E, E);
            ComputeType sinThetaSqr = (dp * dp) / esqrlen;
            A[numA++] = std::make_pair(sinThetaSqr, k0);
        }
    }
    return numA > 0;
}

template <typename InputType, typename ComputeType>
std::array<int, 4> MinimumAreaBox2<InputType, ComputeType>::SortAngles(
    std::array<std::pair<ComputeType, int>, 4> const& A, int numA) const
{
    std::array<int, 4> sort = {{ 0, 1, 2, 3 }};
    if (numA > 1)
    {
        if (numA == 2)
        {
            if (A[sort[0]].first > A[sort[1]].first)
            {
                std::swap(sort[0], sort[1]);
            }
        }
        else if (numA == 3)
        {
            if (A[sort[0]].first > A[sort[1]].first)
            {
                std::swap(sort[0], sort[1]);
            }
            if (A[sort[0]].first > A[sort[2]].first)
            {
                std::swap(sort[0], sort[2]);
            }
            if (A[sort[1]].first > A[sort[2]].first)
            {
                std::swap(sort[1], sort[2]);
            }
        }
        else  // numA == 4
        {
            if (A[sort[0]].first > A[sort[1]].first)
            {
                std::swap(sort[0], sort[1]);
            }
            if (A[sort[2]].first > A[sort[3]].first)
            {
                std::swap(sort[2], sort[3]);
            }
            if (A[sort[0]].first > A[sort[2]].first)
            {
                std::swap(sort[0], sort[2]);
            }
            if (A[sort[1]].first > A[sort[3]].first)
            {
                std::swap(sort[1], sort[3]);
            }
            if (A[sort[1]].first > A[sort[2]].first)
            {
                std::swap(sort[1], sort[2]);
            }
        }
    }
    return sort;
}

template <typename InputType, typename ComputeType>
bool MinimumAreaBox2<InputType, ComputeType>::UpdateSupport(
    std::array<std::pair<ComputeType, int>, 4> const& A, int numA,
    std::array<int, 4> const& sort,
    std::vector<Vector2<ComputeType>> const& vertices,
    std::vector<bool>& visited, Box& box)
{
    // Replace the support vertices of those edges attaining minimum angle
    // with the other endpoints of the edges.
    int const numVertices = static_cast<int>(vertices.size());
    auto const& amin = A[sort[0]];
    for (int k = 0; k < numA; ++k)
    {
        auto const& a = A[sort[k]];
        if (a.first == amin.first)
        {
            if (++box.index[a.second] == numVertices)
            {
                box.index[a.second] = 0;
            }
        }
        else
        {
            break;
        }
    }

    int bottom = box.index[amin.second];
    if (visited[bottom])
    {
        // We have already processed this polygon edge.
        return false;
    }
    visited[bottom] = true;

    // Cycle the vertices so that the bottom support occurs first.
    std::array<int, 4> nextIndex;
    for (int k = 0; k < 4; ++k)
    {
        nextIndex[k] = box.index[(amin.second + k) % 4];
    }
    box.index = nextIndex;

    // Compute the box axis directions.
    int j1 = box.index[0], j0 = j1 - 1;
    if (j0 < 0)
    {
        j0 = numVertices - 1;
    }
    box.U[0] = vertices[j1] - vertices[j0];
    box.U[1] = -Perp(box.U[0]);
    box.sqrLenU0 = Dot(box.U[0], box.U[0]);

    // Compute the box area.
    Vector2<ComputeType> diff[2] =
    {
        vertices[box.index[1]] - vertices[box.index[3]],
        vertices[box.index[2]] - vertices[box.index[0]]
    };
    box.area = Dot(box.U[0], diff[0]) * Dot(box.U[1], diff[1]) / box.sqrLenU0;
    return true;
}

template <typename InputType, typename ComputeType>
void MinimumAreaBox2<InputType, ComputeType>::ConvertTo(Box const& minBox,
    std::vector<Vector2<ComputeType>> const& computePoints,
    OrientedBox2<InputType>& itMinBox)
{
    // The sum, difference, and center are all computed exactly.
    Vector2<ComputeType> sum[2] =
    {
        computePoints[minBox.index[1]] + computePoints[minBox.index[3]],
        computePoints[minBox.index[2]] + computePoints[minBox.index[0]]
    };

    Vector2<ComputeType> difference[2] =
    {
        computePoints[minBox.index[1]] - computePoints[minBox.index[3]],
        computePoints[minBox.index[2]] - computePoints[minBox.index[0]]
    };

    Vector2<ComputeType> center = mHalf * (
        Dot(minBox.U[0], sum[0]) * minBox.U[0] +
        Dot(minBox.U[1], sum[1]) * minBox.U[1]) / minBox.sqrLenU0;

    // Calculate the squared extent using ComputeType to avoid loss of
    // precision before computing a squared root.
    Vector2<ComputeType> sqrExtent;
    for (int i = 0; i < 2; ++i)
    {
        sqrExtent[i] = mHalf * Dot(minBox.U[i], difference[i]);
        sqrExtent[i] *= sqrExtent[i];
        sqrExtent[i] /= minBox.sqrLenU0;
    }

    for (int i = 0; i < 2; ++i)
    {
        itMinBox.center[i] = (InputType)center[i];
        itMinBox.extent[i] = std::sqrt((InputType)sqrExtent[i]);

        // Before converting to floating-point, factor out the maximum
        // component using ComputeType to generate rational numbers in a
        // range that avoids loss of precision during the conversion and
        // normalization.
        Vector2<ComputeType> const& axis = minBox.U[i];
        ComputeType cmax = std::max(std::abs(axis[0]), std::abs(axis[1]));
        ComputeType invCMax = mOne / cmax;
        for (int j = 0; j < 2; ++j)
        {
            itMinBox.axis[i][j] = (InputType)(axis[j] * invCMax);
        }
        Normalize(itMinBox.axis[i]);
    }

    mSupportIndices = minBox.index;
    mArea = (InputType)minBox.area;
}


}



// ----------------------------------------------
// GteMinimumVolumeBox3.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2018/10/05)

// Compute a minimum-volume oriented box containing the specified points.  The
// algorithm is really about computing the minimum-volume box containing the
// convex hull of the points, so we must compute the convex hull or you must
// pass an already built hull to the code.
//
// The minimum-volume oriented box has a face coincident with a hull face
// or has three mutually orthogonal edges coincident with three hull edges
// that (of course) are mutually orthogonal.
//    J.O'Rourke, "Finding minimal enclosing boxes",
//    Internat. J. Comput. Inform. Sci., 14:183-199, 1985.
//
// A detailed description of the algorithm and implementation is found in
// the documents
//   http://www.geometrictools.com/Documentation/MinimumVolumeBox.pdf
//   http://www.geometrictools.com/Documentation/MinimumAreaRectangle.pdf
//
// NOTE: This algorithm guarantees a correct output only when ComputeType is
// an exact arithmetic type that supports division.  In GTEngine, one such
// type is BSRational<UIntegerAP32> (arbitrary precision).  Another such type
// is BSRational<UIntegerFP32<N>> (fixed precision), where N is chosen large
// enough for your input data sets.  If you choose ComputeType to be 'float'
// or 'double', the output is not guaranteed to be correct.
//
// See GeometricTools/GTEngine/Samples/Geometrics/MinimumVolumeBox3 for an
// example of how to use the code.

namespace gte
{

template <typename InputType, typename ComputeType>
class MinimumVolumeBox3
{
public:
    // The class is a functor to support computing the minimum-volume box of
    // multiple data sets using the same class object.  For multithreading
    // in ProcessFaces, choose 'numThreads' subject to the constraints
    //     1 <= numThreads <= std::thread::hardware_concurrency()
    // To execute ProcessEdges in a thread separate from the main thrad,
    // choose 'threadProcessEdges' to 'true'.
    MinimumVolumeBox3(unsigned int numThreads = 1, bool threadProcessEdges = false);

	// The points are arbitrary, so we must compute the convex hull from
	// them in order to compute the minimum-area box.  The input parameters
	// are necessary for using ConvexHull3.
	OrientedBox3<InputType> operator()(int numPoints,
									   Vector3<InputType> const* points,
									   FHEProgressCancel* Progress,
									   bool useRotatingCalipers = !std::is_floating_point<ComputeType>::value);

	// The points form a nondegenerate convex polyhedron.  The indices input
	// must be nonnull and specify the triangle faces.
	OrientedBox3<InputType> operator()(int numPoints,
									   Vector3<InputType> const* points,
									   int numIndices,
									   int const* indices,
									   FHEProgressCancel* Progress,
									   bool useRotatingCalipers = !std::is_floating_point<ComputeType>::value);

    // Member access.
    inline int GetNumPoints() const;
    inline Vector3<InputType> const* GetPoints() const;
    inline std::vector<int> const& GetHull() const;
    inline InputType GetVolume() const;

private:
    struct Box
    {
        Vector3<ComputeType> P, U[3];
        ComputeType sqrLenU[3], range[3][2], volume;
    };

    struct ExtrudeRectangle
    {
        Vector3<ComputeType> U[2];
        std::array<int, 4> index;
        ComputeType sqrLenU[2], area;
    };

    // Compute the minimum-volume box relative to each hull face.
    void ProcessFaces(ETManifoldMesh const& mesh, Box& minBox, FHEProgressCancel* Progress = nullptr);

    // Compute the minimum-volume box for each triple of orthgonal hull edges.
    void ProcessEdges(ETManifoldMesh const& mesh, Box& minBox);

    // Compute the minimum-volume box relative to a single hull face.
    typedef ETManifoldMesh::Triangle Triangle;
    void ProcessFace(std::shared_ptr<Triangle> const& supportTri,
        std::vector<Vector3<ComputeType>> const& normal,
        std::map<std::shared_ptr<Triangle>, int> const& triNormalMap,
        ETManifoldMesh::EMap const& emap, Box& localMinBox);

    // The rotating calipers algorithm has a loop invariant that requires
    // the convex polygon not to have collinear points.  Any such points
    // must be removed first.  The code is also executed for the O(n^2)
    // algorithm to reduce the number of process edges.
    void RemoveCollinearPoints(Vector3<ComputeType> const& N, std::vector<int>& polyline);

    // This is the slow order O(n^2) search.
    void ComputeBoxForFaceOrderNSqr(Vector3<ComputeType> const& N, std::vector<int> const& polyline, Box& box);

    // This is the rotating calipers version, which is O(n).
    void ComputeBoxForFaceOrderN(Vector3<ComputeType> const& N, std::vector<int> const& polyline, Box& box);

    // Compute the smallest rectangle for the polyline edge <V[i0],V[i1]>.
    ExtrudeRectangle SmallestRectangle(int i0, int i1, Vector3<ComputeType> const& N, std::vector<int> const& polyline);

    // Compute (sin(angle))^2 for the polyline edges emanating from the
    // support vertices of the rectangle.  The return value is 'true' if at
    // least one angle is in [0,pi/2); otherwise, the return value is 'false'
    // and the original polyline must project to a rectangle.
    bool ComputeAngles(Vector3<ComputeType> const& N,
        std::vector<int> const& polyline, ExtrudeRectangle const& rct,
        std::array<std::pair<ComputeType, int>, 4>& A, int& numA) const;

    // Sort the angles indirectly.  The sorted indices are returned.  This
    // avoids swapping elements of A[], which can be expensive when
    // ComputeType is an exact rational type.
    std::array<int, 4> SortAngles(std::array<std::pair<ComputeType, int>, 4> const& A, int numA) const;

    bool UpdateSupport(std::array<std::pair<ComputeType, int>, 4> const& A, int numA,
        std::array<int, 4> const& sort, Vector3<ComputeType> const& N, std::vector<int> const& polyline,
        std::vector<bool>& visited, ExtrudeRectangle& rct);

    // Convert the extruded box to the minimum-volume box of InputType.  When
    // the ComputeType is an exact rational type, the conversions are
    // performed to avoid precision loss until necessary at the last step.
    void ConvertTo(Box const& minBox, OrientedBox3<InputType>& itMinBox);

    // The code is multithreaded, both for convex hull computation and
    // computing minimum-volume extruded boxes for the hull faces.  The
    // default value is 1, which implies a single-threaded computation (on
    // the main thread).
    unsigned int mNumThreads;
    bool mThreadProcessEdges;

    // The input points to be bound.
    int mNumPoints;
    Vector3<InputType> const* mPoints;

    // The ComputeType conversions of the input points.  Only points of the
    // convex hull (vertices of a convex polyhedron) are converted for
    // performance when ComputeType is rational.
    Vector3<ComputeType> const* mComputePoints;

    // The indices into mPoints/mComputePoints for the convex hull vertices.
    std::vector<int> mHull;

    // The unique indices in mHull.  This set allows us to compute only for
    // the hull vertices and avoids redundant computations if the indices
    // were to have repeated indices into mPoints/mComputePoints.  This is
    // a performance improvement for rational ComputeType.
    std::set<int> mUniqueIndices;

    // The caller can specify whether to use rotating calipers or the slower
    // all-edge processing for computing an extruded bounding box.
    bool mUseRotatingCalipers;

    // The volume of the minimum-volume box.  The ComputeType value is exact,
    // so the only rounding errors occur in the conversion from ComputeType
    // to InputType (default rounding mode is round-to-nearest-ties-to-even).
    InputType mVolume;

    // Convenient values that occur regularly in the code.  When using
    // rational ComputeType, we construct these numbers only once.
    ComputeType mZero, mOne, mNegOne, mHalf;
};


template <typename InputType, typename ComputeType>
MinimumVolumeBox3<InputType, ComputeType>::MinimumVolumeBox3(unsigned int numThreads, bool threadProcessEdges)
    :
    mNumThreads(numThreads),
    mThreadProcessEdges(threadProcessEdges),
    mNumPoints(0),
    mPoints(nullptr),
    mComputePoints(nullptr),
    mUseRotatingCalipers(true),
    mVolume((InputType)0),
    mZero(0),
    mOne(1),
    mNegOne(-1),
    mHalf((InputType)0.5)
{
}

template <typename InputType, typename ComputeType>
OrientedBox3<InputType> MinimumVolumeBox3<InputType, ComputeType>::operator()(
    int numPoints, Vector3<InputType> const* points, FHEProgressCancel* Progress, bool useRotatingCalipers)
{
    mNumPoints = numPoints;
    mPoints = points;
    mUseRotatingCalipers = useRotatingCalipers;
    mHull.clear();
    mUniqueIndices.clear();

    // Get the convex hull of the points.
    ConvexHull3<InputType, ComputeType> ch3;
    ch3(mNumPoints, mPoints, (InputType)0);
    int dimension = ch3.GetDimension();

    OrientedBox3<InputType> itMinBox;

    if (dimension == 0)
    {
        // The points are all effectively the same (using fuzzy epsilon).
        itMinBox.center = mPoints[0];
        itMinBox.axis[0] = Vector3<InputType>::Unit(0);
        itMinBox.axis[1] = Vector3<InputType>::Unit(1);
        itMinBox.axis[2] = Vector3<InputType>::Unit(2);
        itMinBox.extent[0] = (InputType)0;
        itMinBox.extent[1] = (InputType)0;
        itMinBox.extent[2] = (InputType)0;
        mHull.resize(1);
        mHull[0] = 0;
        return itMinBox;
    }

    if (dimension == 1)
    {
        // The points effectively lie on a line (using fuzzy epsilon).
        // Determine the extreme t-values for the points represented as
        // P = origin + t*direction.  We know that 'origin' is an input
        // vertex, so we can start both t-extremes at zero.
        Line3<InputType> const& line = ch3.GetLine();
        InputType tmin = (InputType)0, tmax = (InputType)0;
        int imin = 0, imax = 0;
        for (int i = 0; i < mNumPoints; ++i)
        {
            Vector3<InputType> diff = mPoints[i] - line.origin;
            InputType t = Dot(diff, line.direction);
            if (t > tmax)
            {
                tmax = t;
                imax = i;
            }
            else if (t < tmin)
            {
                tmin = t;
                imin = i;
            }
        }

        itMinBox.center = line.origin + ((InputType)0.5)*(tmin + tmax) * line.direction;
        itMinBox.extent[0] = ((InputType)0.5)*(tmax - tmin);
        itMinBox.extent[1] = (InputType)0;
        itMinBox.extent[2] = (InputType)0;
        itMinBox.axis[0] = line.direction;
        ComputeOrthogonalComplement(1, &itMinBox.axis[0]);
        mHull.resize(2);
        mHull[0] = imin;
        mHull[1] = imax;
        return itMinBox;
    }

    if (dimension == 2)
    {
        // The points effectively line on a plane (using fuzzy epsilon).
        // Project the points onto the plane and compute the minimum-area
        // bounding box of them.
        Plane3<InputType> const& plane = ch3.GetPlane();

        // Get a coordinate system relative to the plane of the points.
        // Choose the origin to be any of the input points.
        Vector3<InputType> origin = mPoints[0];
        Vector3<InputType> basis[3];
        basis[0] = plane.normal;
        ComputeOrthogonalComplement(1, basis);

        // Project the input points onto the plane.
        std::vector<Vector2<InputType>> projection(mNumPoints);
        for (int i = 0; i < mNumPoints; ++i)
        {
            Vector3<InputType> diff = mPoints[i] - origin;
            projection[i][0] = Dot(basis[1], diff);
            projection[i][1] = Dot(basis[2], diff);
        }

        // Compute the minimum area box in 2D.
        MinimumAreaBox2<InputType, ComputeType> mab2;
        OrientedBox2<InputType> rectangle = mab2(mNumPoints, &projection[0]);

        // Lift the values into 3D.
        itMinBox.center = origin + rectangle.center[0] * basis[1] + rectangle.center[1] * basis[2];
        itMinBox.axis[0] = rectangle.axis[0][0] * basis[1] + rectangle.axis[0][1] * basis[2];
        itMinBox.axis[1] = rectangle.axis[1][0] * basis[1] + rectangle.axis[1][1] * basis[2];
        itMinBox.axis[2] = basis[0];
        itMinBox.extent[0] = rectangle.extent[0];
        itMinBox.extent[1] = rectangle.extent[1];
        itMinBox.extent[2] = (InputType)0;
        mHull = mab2.GetHull();
        return itMinBox;
    }

    // Get the set of unique indices of the hull.  This is used to project
    // hull vertices onto lines.
    ETManifoldMesh const& mesh = ch3.GetHullMesh();
    mHull.resize(3 * mesh.GetTriangles().size());
    int h = 0;
    for (auto const& element : mesh.GetTriangles())
    {
        for (int i = 0; i < 3; ++i, ++h)
        {
            int index = element.first.V[i];
            mHull[h] = index;
            mUniqueIndices.insert(index);
        }
    }

    mComputePoints = ch3.GetQuery().GetVertices();

    Box minBox, minBoxEdges;
    minBox.volume = mNegOne;
    minBoxEdges.volume = mNegOne;

    if (mThreadProcessEdges)
    {
        std::thread doEdges([this, &mesh, &minBoxEdges]()
        {
            ProcessEdges(mesh, minBoxEdges);
        });
        ProcessFaces(mesh, minBox);
        doEdges.join();
    }
    else
    {
        ProcessEdges(mesh, minBoxEdges);
        ProcessFaces(mesh, minBox);
    }

    if (minBoxEdges.volume != mNegOne
        && minBoxEdges.volume < minBox.volume)
    {
        minBox = minBoxEdges;
    }

    ConvertTo(minBox, itMinBox);
    mComputePoints = nullptr;
    return itMinBox;
}

template <typename InputType, typename ComputeType>
OrientedBox3<InputType> MinimumVolumeBox3<InputType, ComputeType>::operator()(
	int numPoints, Vector3<InputType> const* points, int numIndices,
	int const* indices, FHEProgressCancel* Progress, bool useRotatingCalipers)
{
    mNumPoints = numPoints;
    mPoints = points;
    mUseRotatingCalipers = useRotatingCalipers;
    mUniqueIndices.clear();

    // Build the mesh from the indices.  The box construction uses the edge
    // map of the mesh.
    ETManifoldMesh mesh;
    int numTriangles = numIndices / 3;
    for (int t = 0; t < numTriangles; ++t)
    {
        int v0 = *indices++;
        int v1 = *indices++;
        int v2 = *indices++;
        mesh.Insert(v0, v1, v2);
    }

    // Get the set of unique indices of the hull.  This is used to project
    // hull vertices onto lines.
    mHull.resize(3 * mesh.GetTriangles().size());
    int h = 0;
    for (auto const& element : mesh.GetTriangles())
    {
        for (int i = 0; i < 3; ++i, ++h)
        {
            int index = element.first.V[i];
            mHull[h] = index;
            mUniqueIndices.insert(index);
        }
    }

    // Create the ComputeType points to be used downstream.
    std::vector<Vector3<ComputeType>> computePoints(mNumPoints);
    for (auto i : mUniqueIndices)
    {
        for (int j = 0; j < 3; ++j)
        {
            computePoints[i][j] = (ComputeType)mPoints[i][j];
        }
    }

    OrientedBox3<InputType> itMinBox;
    mComputePoints = &computePoints[0];

    Box minBox, minBoxEdges;
    minBox.volume = mNegOne;
    minBoxEdges.volume = mNegOne;

    if (mThreadProcessEdges)
    {
        std::thread doEdges([this, &mesh, &minBoxEdges]()
        {
            ProcessEdges(mesh, minBoxEdges);
        });
        ProcessFaces(mesh, minBox);
        doEdges.join();
    }
    else
    {
        ProcessEdges(mesh, minBoxEdges);
        ProcessFaces(mesh, minBox, Progress);

		if (Progress && Progress->Cancelled())
		{
			return itMinBox;
		}
    }

    if (minBoxEdges.volume != mNegOne && minBoxEdges.volume < minBox.volume)
    {
        minBox = minBoxEdges;
    }

    ConvertTo(minBox, itMinBox);
    mComputePoints = nullptr;
    return itMinBox;
}

template <typename InputType, typename ComputeType> inline
int MinimumVolumeBox3<InputType, ComputeType>::GetNumPoints() const
{
    return mNumPoints;
}

template <typename InputType, typename ComputeType> inline
Vector3<InputType> const*
MinimumVolumeBox3<InputType, ComputeType>::GetPoints() const
{
    return mPoints;
}

template <typename InputType, typename ComputeType> inline
std::vector<int> const& MinimumVolumeBox3<InputType, ComputeType>::GetHull()
    const
{
    return mHull;
}

template <typename InputType, typename ComputeType> inline
InputType MinimumVolumeBox3<InputType, ComputeType>::GetVolume() const
{
    return mVolume;
}

template <typename InputType, typename ComputeType>
void MinimumVolumeBox3<InputType, ComputeType>::ProcessFaces(ETManifoldMesh const& mesh, Box& minBox, FHEProgressCancel* Progress)
{
    // Get the mesh data structures.
    auto const& tmap = mesh.GetTriangles();
    auto const& emap = mesh.GetEdges();

    // Compute inner-pointing face normals for searching boxes supported by
    // a face and an extreme vertex.  The indirection in triNormalMap, using
    // an integer index instead of the normal/sqrlength pair itself, avoids
    // expensive copies when using exact arithmetic.
    std::vector<Vector3<ComputeType>> normal(tmap.size());
    std::map<std::shared_ptr<Triangle>, int> triNormalMap;
    int index = 0;
    for (auto const& element : tmap)
    {
        auto tri = element.second;
        Vector3<ComputeType> const& v0 = mComputePoints[tri->V[0]];
        Vector3<ComputeType> const& v1 = mComputePoints[tri->V[1]];
        Vector3<ComputeType> const& v2 = mComputePoints[tri->V[2]];
        Vector3<ComputeType> edge1 = v1 - v0;
        Vector3<ComputeType> edge2 = v2 - v0;
        normal[index] = Cross(edge2, edge1);  // inner-pointing normal
        triNormalMap[tri] = index++;
    }

    // Process the triangle faces.  For each face, compute the polyline of
    // edges that supports the bounding box with a face coincident to the
    // triangle face.  The projection of the polyline onto the plane of the
    // triangle face is a convex polygon, so we can use the method of rotating
    // calipers to compute its minimum-area box efficiently.
    unsigned int numFaces = static_cast<unsigned int>(tmap.size());
    if (mNumThreads > 1 && numFaces >= mNumThreads)
    {
        // Repackage the triangle pointers to support the partitioning of
        // faces for multithreaded face processing.
        std::vector<std::shared_ptr<Triangle>> triangles;
        triangles.reserve(numFaces);
        for (auto const& element : tmap)
        {
            triangles.push_back(element.second);
        }

        // Partition the data for multiple threads.
        unsigned int numFacesPerThread = numFaces / mNumThreads;
        std::vector<unsigned int> imin(mNumThreads), imax(mNumThreads);
        std::vector<Box> localMinBox(mNumThreads);
        for (unsigned int t = 0; t < mNumThreads; ++t)
        {
            imin[t] = t * numFacesPerThread;
            imax[t] = imin[t] + numFacesPerThread - 1;
            localMinBox[t].volume = mNegOne;
        }
        imax[mNumThreads - 1] = numFaces - 1;

        // Execute the face processing in multiple threads.
        std::vector<std::thread> process(mNumThreads);
        for (unsigned int t = 0; t < mNumThreads; ++t)
        {
            process[t] = std::thread([this, t, &imin, &imax, &triangles,
                &normal, &triNormalMap, &emap, &localMinBox]()
            {
                for (unsigned int i = imin[t]; i <= imax[t]; ++i)
                {
                    auto const& supportTri = triangles[i];
                    ProcessFace(supportTri, normal, triNormalMap, emap, localMinBox[t]);
                }
            });
        }

        // Wait for all threads to finish.
        for (unsigned int t = 0; t < mNumThreads; ++t)
        {
            process[t].join();

            // Update the minimum-volume box candidate.
            if (minBox.volume == mNegOne || localMinBox[t].volume < minBox.volume)
            {
                minBox = localMinBox[t];
            }
        }
    }
    else
    {
        for (auto const& element : tmap)
        {
            auto const& supportTri = element.second;
            ProcessFace(supportTri, normal, triNormalMap, emap, minBox);

			if (Progress && Progress->Cancelled())
			{
				return;
			}
        }
    }
}

template <typename InputType, typename ComputeType>
void MinimumVolumeBox3<InputType, ComputeType>::ProcessEdges(ETManifoldMesh const& mesh, Box& minBox)
{
    // The minimum-volume box can also be supported by three mutually
    // orthogonal edges of the convex hull.  For each triple of orthogonal
    // edges, compute the minimum-volume box for that coordinate frame by
    // projecting the points onto the axes of the frame.  Use a hull vertex
    // as the origin.
    int index = mesh.GetTriangles().begin()->first.V[0];
    Vector3<ComputeType> const& origin = mComputePoints[index];
    Vector3<ComputeType> U[3];
    std::array<ComputeType, 3> sqrLenU;

    auto const& emap = mesh.GetEdges();
    auto e2 = emap.begin(), end = emap.end();
    for (/**/; e2 != end; ++e2)
    {
        U[2] = mComputePoints[e2->first.V[1]] - mComputePoints[e2->first.V[0]];
        auto e1 = e2;
        for (++e1; e1 != end; ++e1)
        {
            U[1] = mComputePoints[e1->first.V[1]] - mComputePoints[e1->first.V[0]];
            if (Dot(U[1], U[2]) != mZero)
            {
                continue;
            }
            sqrLenU[1] = Dot(U[1], U[1]);

            auto e0 = e1;
            for (++e0; e0 != end; ++e0)
            {
                U[0] = mComputePoints[e0->first.V[1]] - mComputePoints[e0->first.V[0]];
                sqrLenU[0] = Dot(U[0], U[0]);
                if (Dot(U[0], U[1]) != mZero || Dot(U[0], U[2]) != mZero)
                {
                    continue;
                }

                // The three edges are mutually orthogonal.  To support exact
                // rational arithmetic for volume computation, we replace U[2]
                // by a parallel vector.
                U[2] = Cross(U[0], U[1]);
                sqrLenU[2] = sqrLenU[0] * sqrLenU[1];

                // Project the vertices onto the lines containing the edges.
                // Use vertex 0 as the origin.
                std::array<ComputeType, 3> umin, umax;
                for (int j = 0; j < 3; ++j)
                {
                    umin[j] = mZero;
                    umax[j] = mZero;
                }

                for (auto i : mUniqueIndices)
                {
                    Vector3<ComputeType> diff = mComputePoints[i] - origin;
                    for (int j = 0; j < 3; ++j)
                    {
                        ComputeType dot = Dot(diff, U[j]);
                        if (dot < umin[j])
                        {
                            umin[j] = dot;
                        }
                        else if (dot > umax[j])
                        {
                            umax[j] = dot;
                        }
                    }
                }

                ComputeType volume = (umax[0] - umin[0]) / sqrLenU[0];
                volume *= (umax[1] - umin[1]) / sqrLenU[1];
                volume *= (umax[2] - umin[2]);

                // Update current minimum-volume box (if necessary).
                if (minBox.volume == mOne || volume < minBox.volume)
                {
                    // The edge keys have unordered vertices, so it is
                    // possible that {U[0],U[1],U[2]} is a left-handed set.
                    // We need a right-handed set.
                    if (DotCross(U[0], U[1], U[2]) < mZero)
                    {
                        U[2] = -U[2];
                    }

                    minBox.P = origin;
                    for (int j = 0; j < 3; ++j)
                    {
                        minBox.U[j] = U[j];
                        minBox.sqrLenU[j] = sqrLenU[j];
                        for (int k = 0; k < 3; ++k)
                        {
                            minBox.range[k][0] = umin[k];
                            minBox.range[k][1] = umax[k];
                        }
                    }
                    minBox.volume = volume;

                }
            }
        }
    }
}

template <typename InputType, typename ComputeType>
void MinimumVolumeBox3<InputType, ComputeType>::ProcessFace(std::shared_ptr<Triangle> const& supportTri,
    std::vector<Vector3<ComputeType>> const& normal, std::map<std::shared_ptr<Triangle>, int> const& triNormalMap,
    ETManifoldMesh::EMap const& emap, Box& localMinBox)
{
    // Get the supporting triangle information.
	auto foundSupport = triNormalMap.find(supportTri);
	if (foundSupport == triNormalMap.end()) return;
    Vector3<ComputeType> const& supportNormal = normal[foundSupport->second];

    // Build the polyline of supporting edges.  The pair (v,polyline[v])
    // represents an edge directed appropriately (see next set of
    // comments).
    std::vector<int> polyline(mNumPoints);
    int polylineStart = -1;
    for (auto const& edgeElement : emap)
    {
        auto const& edge = *edgeElement.second;
        auto const& tri0 = edge.T[0].lock();
        auto const& tri1 = edge.T[1].lock();
		auto found0 = triNormalMap.find(tri0);
		if (found0 == triNormalMap.end()) return;
		auto found1 = triNormalMap.find(tri1);
		if (found1 == triNormalMap.end()) return;
		auto const& normal0 = normal[found0->second];
        auto const& normal1 = normal[found1->second];
        ComputeType dot0 = Dot(supportNormal, normal0);
        ComputeType dot1 = Dot(supportNormal, normal1);

        std::shared_ptr<Triangle> tri;
        if (dot0 < mZero && dot1 >= mZero)
        {
            tri = tri0;
        }
        else if (dot1 < mZero && dot0 >= mZero)
        {
            tri = tri1;
        }

        if (tri)
        {
            // The edge supports the bounding box.  Insert the edge in the
            // list using clockwise order relative to tri.  This will lead
            // to a polyline whose projection onto the plane of the hull
            // face is a convex polygon that is counterclockwise oriented.
            for (int j0 = 2, j1 = 0; j1 < 3; j0 = j1++)
            {
                if (tri->V[j1] == edge.V[0])
                {
                    if (tri->V[j0] == edge.V[1])
                    {
                        polyline[edge.V[1]] = edge.V[0];
                    }
                    else
                    {
                        polyline[edge.V[0]] = edge.V[1];
                    }
                    polylineStart = edge.V[0];
                    break;
                }
            }
        }
    }

	if (polylineStart < 0)	// abort if start search failed
	{
		return;
	}

    // Rearrange the edges to form a closed polyline.  For M vertices, each
    // ComputeBoxFor*() function starts with the edge from closedPolyline[M-1]
    // to closedPolyline[0].
    std::vector<int> closedPolyline(mNumPoints);
    int numClosedPolyline = 0;
    int v = polylineStart;
    for (auto& cp : closedPolyline)
    {
        cp = v;
        ++numClosedPolyline;
        v = polyline[v];
        if (v == polylineStart)
        {
            break;
        }
    }
    closedPolyline.resize(numClosedPolyline);

    // This avoids redundant face testing in the O(n^2) or O(n) algorithms
    // and it simplifies the O(n) implementation.
    RemoveCollinearPoints(supportNormal, closedPolyline);

	if (closedPolyline.size() < 2)	// abort if we no longer have a polygon
	{
		return;
	}

    // Compute the box coincident to the hull triangle that has minimum
    // area on the face coincident with the triangle.
    Box faceBox;
    if (mUseRotatingCalipers)
    {
        ComputeBoxForFaceOrderN(supportNormal, closedPolyline, faceBox);
    }
    else
    {
        ComputeBoxForFaceOrderNSqr(supportNormal, closedPolyline, faceBox);
    }

    // Update the minimum-volume box candidate.
    if (localMinBox.volume == mNegOne || faceBox.volume < localMinBox.volume)
    {
        localMinBox = faceBox;
    }
}

template <typename InputType, typename ComputeType>
void MinimumVolumeBox3<InputType, ComputeType>::RemoveCollinearPoints(
    Vector3<ComputeType> const& N, std::vector<int>& polyline)
{
    std::vector<int> tmpPolyline = polyline;

    int const numPolyline = static_cast<int>(polyline.size());
    int numNoncollinear = 0;
    Vector3<ComputeType> ePrev =
        mComputePoints[tmpPolyline[0]] - mComputePoints[tmpPolyline.back()];

    for (int i0 = 0, i1 = 1; i0 < numPolyline; ++i0)
    {
        Vector3<ComputeType> eNext =
            mComputePoints[tmpPolyline[i1]] - mComputePoints[tmpPolyline[i0]];

        ComputeType tsp = DotCross(ePrev, eNext, N);
        if (tsp != mZero)
        {
            polyline[numNoncollinear++] = tmpPolyline[i0];
        }

        ePrev = eNext;
        if (++i1 == numPolyline)
        {
            i1 = 0;
        }
    }

    polyline.resize(numNoncollinear);
}

template <typename InputType, typename ComputeType>
void MinimumVolumeBox3<InputType, ComputeType>::ComputeBoxForFaceOrderNSqr(
    Vector3<ComputeType> const& N, std::vector<int> const& polyline,
    Box& box)
{
    // This code processes the polyline terminator associated with a convex
    // hull face of inner-pointing normal N.  The polyline is usually not
    // contained by a plane, and projecting the polyline to a convex polygon
    // in a plane perpendicular to N introduces floating-point rounding
    // errors.  The minimum-area box for the projected polyline is computed
    // indirectly to support exact rational arithmetic.

    box.P = mComputePoints[polyline[0]];
    box.U[2] = N;
    box.sqrLenU[2] = Dot(N, N);
    box.range[1][0] = mZero;
    box.volume = mNegOne;
    int const numPolyline = static_cast<int>(polyline.size());
    for (int i0 = numPolyline - 1, i1 = 0; i1 < numPolyline; i0 = i1++)
    {
        // Create a coordinate system for the plane perpendicular to the face
        // normal and containing a polyline vertex.
        Vector3<ComputeType> const& P = mComputePoints[polyline[i0]];
        Vector3<ComputeType> E =
            mComputePoints[polyline[i1]] - mComputePoints[polyline[i0]];

        Vector3<ComputeType> U1 = Cross(N, E);
        Vector3<ComputeType> U0 = Cross(U1, N);

        // Compute the smallest rectangle containing the projected polyline.
        ComputeType min0 = mZero, max0 = mZero, max1 = mZero;
        for (int j = 0; j < numPolyline; ++j)
        {
            Vector3<ComputeType> diff = mComputePoints[polyline[j]] - P;
            ComputeType dot = Dot(U0, diff);
            if (dot < min0)
            {
                min0 = dot;
            }
            else if (dot > max0)
            {
                max0 = dot;
            }

            dot = Dot(U1, diff);
            if (dot > max1)
            {
                max1 = dot;
            }
        }

        // The true area is Area(rectangle)*Length(N).  After the smallest
        // scaled-area rectangle is computed and returned, the box.volume is
        // updated to be the actual squared volume of the box.
        ComputeType sqrLenU1 = Dot(U1, U1);
        ComputeType volume = (max0 - min0) * max1 / sqrLenU1;
        if (box.volume == mNegOne || volume < box.volume)
        {
            box.P = P;
            box.U[0] = U0;
            box.U[1] = U1;
            box.sqrLenU[0] = sqrLenU1 * box.sqrLenU[2];
            box.sqrLenU[1] = sqrLenU1;
            box.range[0][0] = min0;
            box.range[0][1] = max0;
            box.range[1][1] = max1;
            box.volume = volume;
        }
    }

    // Compute the range of points in the support-normal direction.
    box.range[2][0] = mZero;
    box.range[2][1] = mZero;
    for (auto i : mUniqueIndices)
    {
        Vector3<ComputeType> diff = mComputePoints[i] - box.P;
        ComputeType height = Dot(box.U[2], diff);
        if (height < box.range[2][0])
        {
            box.range[2][0] = height;
        }
        else if (height > box.range[2][1])
        {
            box.range[2][1] = height;
        }
    }

    // Compute the actual volume.
    box.volume *= (box.range[2][1] - box.range[2][0]) / box.sqrLenU[2];
}

template <typename InputType, typename ComputeType>
void MinimumVolumeBox3<InputType, ComputeType>::ComputeBoxForFaceOrderN(
    Vector3<ComputeType> const& N, std::vector<int> const& polyline, Box& box)
{
    // This code processes the polyline terminator associated with a convex
    // hull face of inner-pointing normal N.  The polyline is usually not
    // contained by a plane, and projecting the polyline to a convex polygon
    // in a plane perpendicular to N introduces floating-point rounding
    // errors.  The minimum-area box for the projected polyline is computed
    // indirectly to support exact rational arithmetic.

    // When the bounding box corresponding to a polyline edge is computed,
    // we mark the edge as visited.  If the edge is encountered later, the
    // algorithm terminates.
    std::vector<bool> visited(polyline.size());
    std::fill(visited.begin(), visited.end(), false);

    // Start the minimum-area rectangle search with the edge from the last
    // polyline vertex to the first.  When updating the extremes, we want the
    // bottom-most point on the left edge, the top-most point on the right
    // edge, the left-most point on the top edge, and the right-most point
    // on the bottom edge.  The polygon edges starting at these points are
    // then guaranteed not to coincide with a box edge except when an extreme
    // point is shared by two box edges (at a corner).
    ExtrudeRectangle minRct = SmallestRectangle((int)polyline.size() - 1, 0,
        N, polyline);
    visited[minRct.index[0]] = true;

    // Execute the rotating calipers algorithm.
    ExtrudeRectangle rct = minRct;
    for (size_t i = 0; i < polyline.size(); ++i)
    {
        std::array<std::pair<ComputeType, int>, 4> A;
        int numA;
        if (!ComputeAngles(N, polyline, rct, A, numA))
        {
            // The polyline projects to a rectangle, so the search is over.
            break;
        }

        // Indirectly sort the A-array.
        std::array<int, 4> sort = SortAngles(A, numA);

        // Update the supporting indices (rct.index[]) and the rectangle axis
        // directions (rct.U[]).
        if (!UpdateSupport(A, numA, sort, N, polyline, visited, rct))
        {
            // We have already processed the rectangle polygon edge, so the
            // search is over.
            break;
        }

        if (rct.area < minRct.area)
        {
            minRct = rct;
        }
    }

    // Store relevant box information for computing volume and converting to
    // an InputType bounding box.
    box.P = mComputePoints[polyline[minRct.index[0]]];
    box.U[0] = minRct.U[0];
    box.U[1] = minRct.U[1];
    box.U[2] = N;
    box.sqrLenU[0] = minRct.sqrLenU[0];
    box.sqrLenU[1] = minRct.sqrLenU[1];
    box.sqrLenU[2] = Dot(box.U[2], box.U[2]);

    // Compute the range of points in the plane perpendicular to the support
    // normal.
    box.range[0][0] = Dot(box.U[0],
        mComputePoints[polyline[minRct.index[3]]] - box.P);
    box.range[0][1] = Dot(box.U[0],
        mComputePoints[polyline[minRct.index[1]]] - box.P);
    box.range[1][0] = mZero;
    box.range[1][1] = Dot(box.U[1],
        mComputePoints[polyline[minRct.index[2]]] - box.P);

    // Compute the range of points in the support-normal direction.
    box.range[2][0] = mZero;
    box.range[2][1] = mZero;
    for (auto i : mUniqueIndices)
    {
        Vector3<ComputeType> diff = mComputePoints[i] - box.P;
        ComputeType height = Dot(box.U[2], diff);
        if (height < box.range[2][0])
        {
            box.range[2][0] = height;
        }
        else if (height > box.range[2][1])
        {
            box.range[2][1] = height;
        }
    }

    // Compute the actual volume.
    box.volume =
        (box.range[0][1] - box.range[0][0]) *
        ((box.range[1][1] - box.range[1][0]) / box.sqrLenU[1]) *
        ((box.range[2][1] - box.range[2][0]) / box.sqrLenU[2]);
}

template <typename InputType, typename ComputeType>
typename MinimumVolumeBox3<InputType, ComputeType>::ExtrudeRectangle
MinimumVolumeBox3<InputType, ComputeType>::SmallestRectangle(int i0, int i1,
    Vector3<ComputeType> const& N, std::vector<int> const& polyline)
{
    ExtrudeRectangle rct;
    Vector3<ComputeType> E =
        mComputePoints[polyline[i1]] - mComputePoints[polyline[i0]];
    rct.U[1] = Cross(N, E);
    rct.U[0] = Cross(rct.U[1], N);
	rct.index = { { i1, i1, i1, i1 } };
    rct.sqrLenU[0] = Dot(rct.U[0], rct.U[0]);
    rct.sqrLenU[1] = Dot(rct.U[1], rct.U[1]);

    Vector3<ComputeType> const& origin = mComputePoints[polyline[i1]];
    Vector2<ComputeType> support[4];
    for (int j = 0; j < 4; ++j)
    {
        support[j] = { mZero, mZero };
    }

    int i = 0;
    for (auto p : polyline)
    {
        Vector3<ComputeType> diff = mComputePoints[p] - origin;
        Vector2<ComputeType> v = { Dot(rct.U[0], diff), Dot(rct.U[1], diff) };

        // The right-most vertex of the bottom edge is vertices[i1].  The
        // assumption of no triple of collinear vertices guarantees that
        // box.index[0] is i1, which is the initial value assigned at the
        // beginning of this function.  Therefore, there is no need to test
        // for other vertices farther to the right than vertices[i1].

        if (v[0] > support[1][0] ||
            (v[0] == support[1][0] && v[1] > support[1][1]))
        {
            // New right maximum OR same right maximum but closer to top.
            rct.index[1] = i;
            support[1] = v;
        }

        if (v[1] > support[2][1] ||
            (v[1] == support[2][1] && v[0] < support[2][0]))
        {
            // New top maximum OR same top maximum but closer to left.
            rct.index[2] = i;
            support[2] = v;
        }

        if (v[0] < support[3][0] ||
            (v[0] == support[3][0] && v[1] < support[3][1]))
        {
            // New left minimum OR same left minimum but closer to bottom.
            rct.index[3] = i;
            support[3] = v;
        }

        ++i;
    }

    // The comment in the loop has the implication that support[0] = { 0, 0 },
    // so the scaled height (support[2][1] - support[0][1]) is simply
    // support[2][1].
    ComputeType scaledWidth = support[1][0] - support[3][0];
    ComputeType scaledHeight = support[2][1];
    rct.area = scaledWidth * scaledHeight / rct.sqrLenU[1];
    return rct;
}

template <typename InputType, typename ComputeType>
bool MinimumVolumeBox3<InputType, ComputeType>::ComputeAngles(
    Vector3<ComputeType> const& N, std::vector<int> const& polyline,
    ExtrudeRectangle const& rct,
    std::array<std::pair<ComputeType, int>, 4>& A, int& numA) const
{
    int const numPolyline = static_cast<int>(polyline.size());
    numA = 0;
    for (int k0 = 3, k1 = 0; k1 < 4; k0 = k1++)
    {
        if (rct.index[k0] != rct.index[k1])
        {
            // The rct edges are ordered in k1 as U[0], U[1], -U[0], -U[1].
            int lookup = (k0 & 1);
            Vector3<ComputeType> D =
                ((k0 & 2) ? -rct.U[lookup] : rct.U[lookup]);
            int j0 = rct.index[k0], j1 = j0 + 1;
            if (j1 == numPolyline)
            {
                j1 = 0;
            }
            Vector3<ComputeType> E = 
                mComputePoints[polyline[j1]] - mComputePoints[polyline[j0]];
            Vector3<ComputeType> Eperp = Cross(N, E);
            E = Cross(Eperp, N);
            Vector3<ComputeType> DxE = Cross(D, E);
            ComputeType csqrlen = Dot(DxE, DxE);
            ComputeType dsqrlen = rct.sqrLenU[lookup];
            ComputeType esqrlen = Dot(E, E);
            ComputeType sinThetaSqr = csqrlen / (dsqrlen * esqrlen);
            A[numA++] = std::make_pair(sinThetaSqr, k0);
        }
    }
    return numA > 0;
}

template <typename InputType, typename ComputeType>
std::array<int, 4> MinimumVolumeBox3<InputType, ComputeType>::SortAngles(
    std::array<std::pair<ComputeType, int>, 4> const& A, int numA) const
{
    std::array<int, 4> sort = {{ 0, 1, 2, 3 }};
    if (numA > 1)
    {
        if (numA == 2)
        {
            if (A[sort[0]].first > A[sort[1]].first)
            {
                std::swap(sort[0], sort[1]);
            }
        }
        else if (numA == 3)
        {
            if (A[sort[0]].first > A[sort[1]].first)
            {
                std::swap(sort[0], sort[1]);
            }
            if (A[sort[0]].first > A[sort[2]].first)
            {
                std::swap(sort[0], sort[2]);
            }
            if (A[sort[1]].first > A[sort[2]].first)
            {
                std::swap(sort[1], sort[2]);
            }
        }
        else  // numA == 4
        {
            if (A[sort[0]].first > A[sort[1]].first)
            {
                std::swap(sort[0], sort[1]);
            }
            if (A[sort[2]].first > A[sort[3]].first)
            {
                std::swap(sort[2], sort[3]);
            }
            if (A[sort[0]].first > A[sort[2]].first)
            {
                std::swap(sort[0], sort[2]);
            }
            if (A[sort[1]].first > A[sort[3]].first)
            {
                std::swap(sort[1], sort[3]);
            }
            if (A[sort[1]].first > A[sort[2]].first)
            {
                std::swap(sort[1], sort[2]);
            }
        }
    }
    return sort;
}

template <typename InputType, typename ComputeType>
bool MinimumVolumeBox3<InputType, ComputeType>::UpdateSupport(
    std::array<std::pair<ComputeType, int>, 4> const& A, int numA,
    std::array<int, 4> const& sort, Vector3<ComputeType> const& N,
    std::vector<int> const& polyline, std::vector<bool>& visited,
    ExtrudeRectangle& rct)
{
    // Replace the support vertices of those edges attaining minimum angle
    // with the other endpoints of the edges.
    int const numPolyline = static_cast<int>(polyline.size());
    auto const& amin = A[sort[0]];
    for (int k = 0; k < numA; ++k)
    {
        auto const& a = A[sort[k]];
        if (a.first == amin.first)
        {
            if (++rct.index[a.second] == numPolyline)
            {
                rct.index[a.second] = 0;
            }
        }
        else
        {
            break;
        }
    }

    int bottom = rct.index[amin.second];
    if (visited[bottom])
    {
        // We have already processed this polyline edge.
        return false;
    }
    visited[bottom] = true;

    // Cycle the vertices so that the bottom support occurs first.
    std::array<int, 4> nextIndex;
    for (int k = 0; k < 4; ++k)
    {
        nextIndex[k] = rct.index[(amin.second + k) % 4];
    }
    rct.index = nextIndex;

    // Compute the rectangle axis directions.
    int j1 = rct.index[0], j0 = j1 - 1;
    if (j1 < 0)
    {
        j1 = numPolyline - 1;
    }
    Vector3<ComputeType> E =
        mComputePoints[polyline[j1]] - mComputePoints[polyline[j0]];
    rct.U[1] = Cross(N, E);
    rct.U[0] = Cross(rct.U[1], N);
    rct.sqrLenU[0] = Dot(rct.U[0], rct.U[0]);
    rct.sqrLenU[1] = Dot(rct.U[1], rct.U[1]);

    // Compute the rectangle area.
    Vector3<ComputeType> diff[2] =
    {
        mComputePoints[polyline[rct.index[1]]]
            - mComputePoints[polyline[rct.index[3]]],
        mComputePoints[polyline[rct.index[2]]]
            - mComputePoints[polyline[rct.index[0]]]
    };
    ComputeType scaledWidth = Dot(rct.U[0], diff[0]);
    ComputeType scaledHeight = Dot(rct.U[1], diff[1]);
    rct.area = scaledWidth * scaledHeight / rct.sqrLenU[1];
    return true;
}

template <typename InputType, typename ComputeType>
void MinimumVolumeBox3<InputType, ComputeType>::ConvertTo(Box const& minBox,
    OrientedBox3<InputType>& itMinBox)
{
    Vector3<ComputeType> center = minBox.P;
    for (int i = 0; i < 3; ++i)
    {
        ComputeType average =
            mHalf * (minBox.range[i][0] + minBox.range[i][1]);
        center += (average / minBox.sqrLenU[i]) * minBox.U[i];
    }

    // Calculate the squared extent using ComputeType to avoid loss of
    // precision before computing a squared root.
    Vector3<ComputeType> sqrExtent;
    for (int i = 0; i < 3; ++i)
    {
        sqrExtent[i] = mHalf * (minBox.range[i][1] - minBox.range[i][0]);
        sqrExtent[i] *= sqrExtent[i];
        sqrExtent[i] /= minBox.sqrLenU[i];
    }

    for (int i = 0; i < 3; ++i)
    {
        itMinBox.center[i] = (InputType)center[i];
        itMinBox.extent[i] = std::sqrt((InputType)sqrExtent[i]);

        // Before converting to floating-point, factor out the maximum
        // component using ComputeType to generate rational numbers in a
        // range that avoids loss of precision during the conversion and
        // normalization.
        Vector3<ComputeType> const& axis = minBox.U[i];
        ComputeType cmax = std::max(std::abs(axis[0]), std::abs(axis[1]));
        cmax = std::max(cmax, std::abs(axis[2]));
        ComputeType invCMax = mOne / cmax;
        for (int j = 0; j < 3; ++j)
        {
            itMinBox.axis[i][j] = (InputType)(axis[j] * invCMax);
        }
        Normalize(itMinBox.axis[i]);
    }

    mVolume = (InputType)minBox.volume;
}

}


// ----------------------------------------------
// GteMath.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.17.1 (2019/01/17)

// This file extends the <cmath> support to include convenient constants and
// functions.  The shared constants for CPU, Intel SSE and GPU lead to
// correctly rounded approximations of the constants when using 'float' or
// 'double'.  The file also includes a type trait, is_arbitrary_precision,
// to support selecting between floating-point arithmetic (float, double,
//long double) or arbitrary-precision arithmetic (BSNumber<T>, BSRational<T>)
// in an implementation using std::enable_if.  There is also a type trait,
// has_division_operator, to support selecting between numeric types that
// have a division operator (BSRational<T>) and those that do not have a
// division operator (BSNumber<T>).

// Maximum number of iterations for bisection before a subinterval
// degenerates to a single point. TODO: Verify these.  I used the formula:
// 3 + std::numeric_limits<T>::digits - std::numeric_limits<T>::min_exponent.
//   IEEEBinary16:  digits = 11, min_exponent = -13
//   float:         digits = 27, min_exponent = -125
//   double:        digits = 53, min_exponent = -1021
// BSNumber and BSRational use std::numeric_limits<unsigned int>::max(),
// but maybe these should be set to a large number or be user configurable.
// The MAX_BISECTIONS_GENERIC is an arbitrary choice for now and is used
// in template code where Real is the template parameter.
#define GTE_C_MAX_BISECTIONS_FLOAT16    27u
#define GTE_C_MAX_BISECTIONS_FLOAT32    155u
#define GTE_C_MAX_BISECTIONS_FLOAT64    1077u
#define GTE_C_MAX_BISECTIONS_BSNUMBER   0xFFFFFFFFu
#define GTE_C_MAX_BISECTIONS_BSRATIONAL 0xFFFFFFFFu
#define GTE_C_MAX_BISECTIONS_GENERIC    2048u

// Constants involving pi.
#define GTE_C_PI 3.1415926535897931
#define GTE_C_HALF_PI 1.5707963267948966
#define GTE_C_QUARTER_PI 0.7853981633974483
#define GTE_C_TWO_PI 6.2831853071795862
#define GTE_C_INV_PI 0.3183098861837907
#define GTE_C_INV_TWO_PI 0.1591549430918953
#define GTE_C_INV_HALF_PI 0.6366197723675813

// Conversions between degrees and radians.
#define GTE_C_DEG_TO_RAD 0.0174532925199433
#define GTE_C_RAD_TO_DEG 57.295779513082321

// Common constants.
#define GTE_C_SQRT_2 1.4142135623730951
#define GTE_C_INV_SQRT_2 0.7071067811865475
#define GTE_C_LN_2 0.6931471805599453
#define GTE_C_INV_LN_2 1.4426950408889634
#define GTE_C_LN_10 2.3025850929940459
#define GTE_C_INV_LN_10 0.43429448190325176

// Constants for minimax polynomial approximations to sqrt(x).
// The algorithm minimizes the maximum absolute error on [1,2].
#define GTE_C_SQRT_DEG1_C0 +1.0
#define GTE_C_SQRT_DEG1_C1 +4.1421356237309505e-01
#define GTE_C_SQRT_DEG1_MAX_ERROR 1.7766952966368793e-2

#define GTE_C_SQRT_DEG2_C0 +1.0
#define GTE_C_SQRT_DEG2_C1 +4.8563183076125260e-01
#define GTE_C_SQRT_DEG2_C2 -7.1418268388157458e-02
#define GTE_C_SQRT_DEG2_MAX_ERROR 1.1795695163108744e-3

#define GTE_C_SQRT_DEG3_C0 +1.0
#define GTE_C_SQRT_DEG3_C1 +4.9750045320242231e-01
#define GTE_C_SQRT_DEG3_C2 -1.0787308044477850e-01
#define GTE_C_SQRT_DEG3_C3 +2.4586189615451115e-02
#define GTE_C_SQRT_DEG3_MAX_ERROR 1.1309620116468910e-4

#define GTE_C_SQRT_DEG4_C0 +1.0
#define GTE_C_SQRT_DEG4_C1 +4.9955939832918816e-01
#define GTE_C_SQRT_DEG4_C2 -1.2024066151943025e-01
#define GTE_C_SQRT_DEG4_C3 +4.5461507257698486e-02
#define GTE_C_SQRT_DEG4_C4 -1.0566681694362146e-02
#define GTE_C_SQRT_DEG4_MAX_ERROR 1.2741170151556180e-5

#define GTE_C_SQRT_DEG5_C0 +1.0
#define GTE_C_SQRT_DEG5_C1 +4.9992197660031912e-01
#define GTE_C_SQRT_DEG5_C2 -1.2378506719245053e-01
#define GTE_C_SQRT_DEG5_C3 +5.6122776972699739e-02
#define GTE_C_SQRT_DEG5_C4 -2.3128836281145482e-02
#define GTE_C_SQRT_DEG5_C5 +5.0827122737047148e-03
#define GTE_C_SQRT_DEG5_MAX_ERROR 1.5725568940708201e-6

#define GTE_C_SQRT_DEG6_C0 +1.0
#define GTE_C_SQRT_DEG6_C1 +4.9998616695784914e-01
#define GTE_C_SQRT_DEG6_C2 -1.2470733323278438e-01
#define GTE_C_SQRT_DEG6_C3 +6.0388587356982271e-02
#define GTE_C_SQRT_DEG6_C4 -3.1692053551807930e-02
#define GTE_C_SQRT_DEG6_C5 +1.2856590305148075e-02
#define GTE_C_SQRT_DEG6_C6 -2.6183954624343642e-03
#define GTE_C_SQRT_DEG6_MAX_ERROR 2.0584155535630089e-7

#define GTE_C_SQRT_DEG7_C0 +1.0
#define GTE_C_SQRT_DEG7_C1 +4.9999754817809228e-01
#define GTE_C_SQRT_DEG7_C2 -1.2493243476353655e-01
#define GTE_C_SQRT_DEG7_C3 +6.1859954146370910e-02
#define GTE_C_SQRT_DEG7_C4 -3.6091595023208356e-02
#define GTE_C_SQRT_DEG7_C5 +1.9483946523450868e-02
#define GTE_C_SQRT_DEG7_C6 -7.5166134568007692e-03
#define GTE_C_SQRT_DEG7_C7 +1.4127567687864939e-03
#define GTE_C_SQRT_DEG7_MAX_ERROR 2.8072302919734948e-8

#define GTE_C_SQRT_DEG8_C0 +1.0
#define GTE_C_SQRT_DEG8_C1 +4.9999956583056759e-01
#define GTE_C_SQRT_DEG8_C2 -1.2498490369914350e-01
#define GTE_C_SQRT_DEG8_C3 +6.2318494667579216e-02
#define GTE_C_SQRT_DEG8_C4 -3.7982961896432244e-02
#define GTE_C_SQRT_DEG8_C5 +2.3642612312869460e-02
#define GTE_C_SQRT_DEG8_C6 -1.2529377587270574e-02
#define GTE_C_SQRT_DEG8_C7 +4.5382426960713929e-03
#define GTE_C_SQRT_DEG8_C8 -7.8810995273670414e-04
#define GTE_C_SQRT_DEG8_MAX_ERROR 3.9460605685825989e-9

// Constants for minimax polynomial approximations to 1/sqrt(x).
// The algorithm minimizes the maximum absolute error on [1,2].
#define GTE_C_INVSQRT_DEG1_C0 +1.0
#define GTE_C_INVSQRT_DEG1_C1 -2.9289321881345254e-01
#define GTE_C_INVSQRT_DEG1_MAX_ERROR 3.7814314552701983e-2

#define GTE_C_INVSQRT_DEG2_C0 +1.0
#define GTE_C_INVSQRT_DEG2_C1 -4.4539812104566801e-01
#define GTE_C_INVSQRT_DEG2_C2 +1.5250490223221547e-01
#define GTE_C_INVSQRT_DEG2_MAX_ERROR 4.1953446330581234e-3

#define GTE_C_INVSQRT_DEG3_C0 +1.0
#define GTE_C_INVSQRT_DEG3_C1 -4.8703230993068791e-01
#define GTE_C_INVSQRT_DEG3_C2 +2.8163710486669835e-01
#define GTE_C_INVSQRT_DEG3_C3 -8.7498013749463421e-02
#define GTE_C_INVSQRT_DEG3_MAX_ERROR 5.6307702007266786e-4

#define GTE_C_INVSQRT_DEG4_C0 +1.0
#define GTE_C_INVSQRT_DEG4_C1 -4.9710061558048779e-01
#define GTE_C_INVSQRT_DEG4_C2 +3.4266247597676802e-01
#define GTE_C_INVSQRT_DEG4_C3 -1.9106356536293490e-01
#define GTE_C_INVSQRT_DEG4_C4 +5.2608486153198797e-02
#define GTE_C_INVSQRT_DEG4_MAX_ERROR 8.1513919987605266e-5

#define GTE_C_INVSQRT_DEG5_C0 +1.0
#define GTE_C_INVSQRT_DEG5_C1 -4.9937760586004143e-01
#define GTE_C_INVSQRT_DEG5_C2 +3.6508741295133973e-01
#define GTE_C_INVSQRT_DEG5_C3 -2.5884890281853501e-01
#define GTE_C_INVSQRT_DEG5_C4 +1.3275782221320753e-01
#define GTE_C_INVSQRT_DEG5_C5 -3.2511945299404488e-02
#define GTE_C_INVSQRT_DEG5_MAX_ERROR 1.2289367475583346e-5

#define GTE_C_INVSQRT_DEG6_C0 +1.0
#define GTE_C_INVSQRT_DEG6_C1 -4.9987029229547453e-01
#define GTE_C_INVSQRT_DEG6_C2 +3.7220923604495226e-01
#define GTE_C_INVSQRT_DEG6_C3 -2.9193067713256937e-01
#define GTE_C_INVSQRT_DEG6_C4 +1.9937605991094642e-01
#define GTE_C_INVSQRT_DEG6_C5 -9.3135712130901993e-02
#define GTE_C_INVSQRT_DEG6_C6 +2.0458166789566690e-02
#define GTE_C_INVSQRT_DEG6_MAX_ERROR 1.9001451223750465e-6

#define GTE_C_INVSQRT_DEG7_C0 +1.0
#define GTE_C_INVSQRT_DEG7_C1 -4.9997357250704977e-01
#define GTE_C_INVSQRT_DEG7_C2 +3.7426216884998809e-01
#define GTE_C_INVSQRT_DEG7_C3 -3.0539882498248971e-01
#define GTE_C_INVSQRT_DEG7_C4 +2.3976005607005391e-01
#define GTE_C_INVSQRT_DEG7_C5 -1.5410326351684489e-01
#define GTE_C_INVSQRT_DEG7_C6 +6.5598809723041995e-02
#define GTE_C_INVSQRT_DEG7_C7 -1.3038592450470787e-02
#define GTE_C_INVSQRT_DEG7_MAX_ERROR 2.9887724993168940e-7

#define GTE_C_INVSQRT_DEG8_C0 +1.0
#define GTE_C_INVSQRT_DEG8_C1 -4.9999471066120371e-01
#define GTE_C_INVSQRT_DEG8_C2 +3.7481415745794067e-01
#define GTE_C_INVSQRT_DEG8_C3 -3.1023804387422160e-01
#define GTE_C_INVSQRT_DEG8_C4 +2.5977002682930106e-01
#define GTE_C_INVSQRT_DEG8_C5 -1.9818790717727097e-01
#define GTE_C_INVSQRT_DEG8_C6 +1.1882414252613671e-01
#define GTE_C_INVSQRT_DEG8_C7 -4.6270038088550791e-02
#define GTE_C_INVSQRT_DEG8_C8 +8.3891541755747312e-03
#define GTE_C_INVSQRT_DEG8_MAX_ERROR 4.7596926146947771e-8

// Constants for minimax polynomial approximations to sin(x).
// The algorithm minimizes the maximum absolute error on [-pi/2,pi/2].
#define GTE_C_SIN_DEG3_C0 +1.0
#define GTE_C_SIN_DEG3_C1 -1.4727245910375519e-01
#define GTE_C_SIN_DEG3_MAX_ERROR 1.3481903639145865e-2

#define GTE_C_SIN_DEG5_C0 +1.0
#define GTE_C_SIN_DEG5_C1 -1.6600599923812209e-01
#define GTE_C_SIN_DEG5_C2 +7.5924178409012000e-03
#define GTE_C_SIN_DEG5_MAX_ERROR 1.4001209384639779e-4

#define GTE_C_SIN_DEG7_C0 +1.0
#define GTE_C_SIN_DEG7_C1 -1.6665578084732124e-01
#define GTE_C_SIN_DEG7_C2 +8.3109378830028557e-03
#define GTE_C_SIN_DEG7_C3 -1.8447486103462252e-04
#define GTE_C_SIN_DEG7_MAX_ERROR 1.0205878936686563e-6

#define GTE_C_SIN_DEG9_C0 +1.0
#define GTE_C_SIN_DEG9_C1 -1.6666656235308897e-01
#define GTE_C_SIN_DEG9_C2 +8.3329962509886002e-03
#define GTE_C_SIN_DEG9_C3 -1.9805100675274190e-04
#define GTE_C_SIN_DEG9_C4 +2.5967200279475300e-06
#define GTE_C_SIN_DEG9_MAX_ERROR 5.2010746265374053e-9

#define GTE_C_SIN_DEG11_C0 +1.0
#define GTE_C_SIN_DEG11_C1 -1.6666666601721269e-01
#define GTE_C_SIN_DEG11_C2 +8.3333303183525942e-03
#define GTE_C_SIN_DEG11_C3 -1.9840782426250314e-04
#define GTE_C_SIN_DEG11_C4 +2.7521557770526783e-06
#define GTE_C_SIN_DEG11_C5 -2.3828544692960918e-08
#define GTE_C_SIN_DEG11_MAX_ERROR 1.9295870457014530e-11

// Constants for minimax polynomial approximations to cos(x).
// The algorithm minimizes the maximum absolute error on [-pi/2,pi/2].
#define GTE_C_COS_DEG2_C0 +1.0
#define GTE_C_COS_DEG2_C1 -4.0528473456935105e-01
#define GTE_C_COS_DEG2_MAX_ERROR 5.4870946878404048e-2

#define GTE_C_COS_DEG4_C0 +1.0
#define GTE_C_COS_DEG4_C1 -4.9607181958647262e-01
#define GTE_C_COS_DEG4_C2 +3.6794619653489236e-02
#define GTE_C_COS_DEG4_MAX_ERROR 9.1879932449712154e-4

#define GTE_C_COS_DEG6_C0 +1.0
#define GTE_C_COS_DEG6_C1 -4.9992746217057404e-01
#define GTE_C_COS_DEG6_C2 +4.1493920348353308e-02
#define GTE_C_COS_DEG6_C3 -1.2712435011987822e-03
#define GTE_C_COS_DEG6_MAX_ERROR 9.2028470133065365e-6

#define GTE_C_COS_DEG8_C0 +1.0
#define GTE_C_COS_DEG8_C1 -4.9999925121358291e-01
#define GTE_C_COS_DEG8_C2 +4.1663780117805693e-02
#define GTE_C_COS_DEG8_C3 -1.3854239405310942e-03
#define GTE_C_COS_DEG8_C4 +2.3154171575501259e-05
#define GTE_C_COS_DEG8_MAX_ERROR 5.9804533020235695e-8

#define GTE_C_COS_DEG10_C0 +1.0
#define GTE_C_COS_DEG10_C1 -4.9999999508695869e-01
#define GTE_C_COS_DEG10_C2 +4.1666638865338612e-02
#define GTE_C_COS_DEG10_C3 -1.3888377661039897e-03
#define GTE_C_COS_DEG10_C4 +2.4760495088926859e-05
#define GTE_C_COS_DEG10_C5 -2.6051615464872668e-07
#define GTE_C_COS_DEG10_MAX_ERROR 2.7006769043325107e-10

// Constants for minimax polynomial approximations to tan(x).
// The algorithm minimizes the maximum absolute error on [-pi/4,pi/4].
#define GTE_C_TAN_DEG3_C0 1.0
#define GTE_C_TAN_DEG3_C1 4.4295926544736286e-01
#define GTE_C_TAN_DEG3_MAX_ERROR 1.1661892256204731e-2

#define GTE_C_TAN_DEG5_C0 1.0
#define GTE_C_TAN_DEG5_C1 3.1401320403542421e-01
#define GTE_C_TAN_DEG5_C2 2.0903948109240345e-01
#define GTE_C_TAN_DEG5_MAX_ERROR 5.8431854390143118e-4

#define GTE_C_TAN_DEG7_C0 1.0
#define GTE_C_TAN_DEG7_C1 3.3607213284422555e-01
#define GTE_C_TAN_DEG7_C2 1.1261037305184907e-01
#define GTE_C_TAN_DEG7_C3 9.8352099470524479e-02
#define GTE_C_TAN_DEG7_MAX_ERROR 3.5418688397723108e-5

#define GTE_C_TAN_DEG9_C0 1.0
#define GTE_C_TAN_DEG9_C1 3.3299232843941784e-01
#define GTE_C_TAN_DEG9_C2 1.3747843432474838e-01
#define GTE_C_TAN_DEG9_C3 3.7696344813028304e-02
#define GTE_C_TAN_DEG9_C4 4.6097377279281204e-02
#define GTE_C_TAN_DEG9_MAX_ERROR 2.2988173242199927e-6

#define GTE_C_TAN_DEG11_C0 1.0
#define GTE_C_TAN_DEG11_C1 3.3337224456224224e-01
#define GTE_C_TAN_DEG11_C2 1.3264516053824593e-01
#define GTE_C_TAN_DEG11_C3 5.8145237645931047e-02
#define GTE_C_TAN_DEG11_C4 1.0732193237572574e-02
#define GTE_C_TAN_DEG11_C5 2.1558456793513869e-02
#define GTE_C_TAN_DEG11_MAX_ERROR 1.5426257940140409e-7

#define GTE_C_TAN_DEG13_C0 1.0
#define GTE_C_TAN_DEG13_C1 3.3332916426394554e-01
#define GTE_C_TAN_DEG13_C2 1.3343404625112498e-01
#define GTE_C_TAN_DEG13_C3 5.3104565343119248e-02
#define GTE_C_TAN_DEG13_C4 2.5355038312682154e-02
#define GTE_C_TAN_DEG13_C5 1.8253255966556026e-03
#define GTE_C_TAN_DEG13_C6 1.0069407176615641e-02
#define GTE_C_TAN_DEG13_MAX_ERROR 1.0550264249037378e-8

// Constants for minimax polynomial approximations to acos(x), where the
// approximation is of the form acos(x) = sqrt(1 - x)*p(x) with p(x) a
// polynomial.  The algorithm minimizes the maximum error
// |acos(x)/sqrt(1-x) - p(x)| on [0,1].  At the same time we get an
// approximation for asin(x) = pi/2 - acos(x).
#define GTE_C_ACOS_DEG1_C0 +1.5707963267948966
#define GTE_C_ACOS_DEG1_C1 -1.5658276442180141e-01
#define GTE_C_ACOS_DEG1_MAX_ERROR 1.1659002803738105e-2

#define GTE_C_ACOS_DEG2_C0 +1.5707963267948966
#define GTE_C_ACOS_DEG2_C1 -2.0347053865798365e-01
#define GTE_C_ACOS_DEG2_C2 +4.6887774236182234e-02
#define GTE_C_ACOS_DEG2_MAX_ERROR 9.0311602490029258e-4

#define GTE_C_ACOS_DEG3_C0 +1.5707963267948966
#define GTE_C_ACOS_DEG3_C1 -2.1253291899190285e-01
#define GTE_C_ACOS_DEG3_C2 +7.4773789639484223e-02
#define GTE_C_ACOS_DEG3_C3 -1.8823635069382449e-02
#define GTE_C_ACOS_DEG3_MAX_ERROR 9.3066396954288172e-5

#define GTE_C_ACOS_DEG4_C0 +1.5707963267948966
#define GTE_C_ACOS_DEG4_C1 -2.1422258835275865e-01
#define GTE_C_ACOS_DEG4_C2 +8.4936675142844198e-02
#define GTE_C_ACOS_DEG4_C3 -3.5991475120957794e-02
#define GTE_C_ACOS_DEG4_C4 +8.6946239090712751e-03
#define GTE_C_ACOS_DEG4_MAX_ERROR 1.0930595804481413e-5

#define GTE_C_ACOS_DEG5_C0 +1.5707963267948966
#define GTE_C_ACOS_DEG5_C1 -2.1453292139805524e-01
#define GTE_C_ACOS_DEG5_C2 +8.7973089282889383e-02
#define GTE_C_ACOS_DEG5_C3 -4.5130266382166440e-02
#define GTE_C_ACOS_DEG5_C4 +1.9467466687281387e-02
#define GTE_C_ACOS_DEG5_C5 -4.3601326117634898e-03
#define GTE_C_ACOS_DEG5_MAX_ERROR 1.3861070257241426-6

#define GTE_C_ACOS_DEG6_C0 +1.5707963267948966
#define GTE_C_ACOS_DEG6_C1 -2.1458939285677325e-01
#define GTE_C_ACOS_DEG6_C2 +8.8784960563641491e-02
#define GTE_C_ACOS_DEG6_C3 -4.8887131453156485e-02
#define GTE_C_ACOS_DEG6_C4 +2.7011519960012720e-02
#define GTE_C_ACOS_DEG6_C5 -1.1210537323478320e-02
#define GTE_C_ACOS_DEG6_C6 +2.3078166879102469e-03
#define GTE_C_ACOS_DEG6_MAX_ERROR 1.8491291330427484e-7

#define GTE_C_ACOS_DEG7_C0 +1.5707963267948966
#define GTE_C_ACOS_DEG7_C1 -2.1459960076929829e-01
#define GTE_C_ACOS_DEG7_C2 +8.8986946573346160e-02
#define GTE_C_ACOS_DEG7_C3 -5.0207843052845647e-02
#define GTE_C_ACOS_DEG7_C4 +3.0961594977611639e-02
#define GTE_C_ACOS_DEG7_C5 -1.7162031184398074e-02
#define GTE_C_ACOS_DEG7_C6 +6.7072304676685235e-03
#define GTE_C_ACOS_DEG7_C7 -1.2690614339589956e-03
#define GTE_C_ACOS_DEG7_MAX_ERROR 2.5574620927948377e-8

#define GTE_C_ACOS_DEG8_C0 +1.5707963267948966
#define GTE_C_ACOS_DEG8_C1 -2.1460143648688035e-01
#define GTE_C_ACOS_DEG8_C2 +8.9034700107934128e-02
#define GTE_C_ACOS_DEG8_C3 -5.0625279962389413e-02
#define GTE_C_ACOS_DEG8_C4 +3.2683762943179318e-02
#define GTE_C_ACOS_DEG8_C5 -2.0949278766238422e-02
#define GTE_C_ACOS_DEG8_C6 +1.1272900916992512e-02
#define GTE_C_ACOS_DEG8_C7 -4.1160981058965262e-03
#define GTE_C_ACOS_DEG8_C8 +7.1796493341480527e-04
#define GTE_C_ACOS_DEG8_MAX_ERROR 3.6340015129032732e-9

// Constants for minimax polynomial approximations to atan(x).
// The algorithm minimizes the maximum absolute error on [-1,1].
#define GTE_C_ATAN_DEG3_C0 +1.0
#define GTE_C_ATAN_DEG3_C1 -2.1460183660255172e-01
#define GTE_C_ATAN_DEG3_MAX_ERROR 1.5970326392614240e-2

#define GTE_C_ATAN_DEG5_C0 +1.0
#define GTE_C_ATAN_DEG5_C1 -3.0189478312144946e-01
#define GTE_C_ATAN_DEG5_C2 +8.7292946518897740e-02
#define GTE_C_ATAN_DEG5_MAX_ERROR 1.3509832247372636e-3

#define GTE_C_ATAN_DEG7_C0 +1.0
#define GTE_C_ATAN_DEG7_C1 -3.2570157599356531e-01
#define GTE_C_ATAN_DEG7_C2 +1.5342994884206673e-01
#define GTE_C_ATAN_DEG7_C3 -4.2330209451053591e-02
#define GTE_C_ATAN_DEG7_MAX_ERROR 1.5051227215514412e-4

#define GTE_C_ATAN_DEG9_C0 +1.0
#define GTE_C_ATAN_DEG9_C1 -3.3157878236439586e-01
#define GTE_C_ATAN_DEG9_C2 +1.8383034738018011e-01
#define GTE_C_ATAN_DEG9_C3 -8.9253037587244677e-02
#define GTE_C_ATAN_DEG9_C4 +2.2399635968909593e-02
#define GTE_C_ATAN_DEG9_MAX_ERROR 1.8921598624582064e-5

#define GTE_C_ATAN_DEG11_C0 +1.0
#define GTE_C_ATAN_DEG11_C1 -3.3294527685374087e-01
#define GTE_C_ATAN_DEG11_C2 +1.9498657165383548e-01
#define GTE_C_ATAN_DEG11_C3 -1.1921576270475498e-01
#define GTE_C_ATAN_DEG11_C4 +5.5063351366968050e-02
#define GTE_C_ATAN_DEG11_C5 -1.2490720064867844e-02
#define GTE_C_ATAN_DEG11_MAX_ERROR 2.5477724974187765e-6

#define GTE_C_ATAN_DEG13_C0 +1.0
#define GTE_C_ATAN_DEG13_C1 -3.3324998579202170e-01
#define GTE_C_ATAN_DEG13_C2 +1.9856563505717162e-01
#define GTE_C_ATAN_DEG13_C3 -1.3374657325451267e-01
#define GTE_C_ATAN_DEG13_C4 +8.1675882859940430e-02
#define GTE_C_ATAN_DEG13_C5 -3.5059680836411644e-02
#define GTE_C_ATAN_DEG13_C6 +7.2128853633444123e-03
#define GTE_C_ATAN_DEG13_MAX_ERROR 3.5859104691865484e-7

// Constants for minimax polynomial approximations to exp2(x) = 2^x.
// The algorithm minimizes the maximum absolute error on [0,1].
#define GTE_C_EXP2_DEG1_C0 1.0
#define GTE_C_EXP2_DEG1_C1 1.0
#define GTE_C_EXP2_DEG1_MAX_ERROR 8.6071332055934313e-2

#define GTE_C_EXP2_DEG2_C0 1.0
#define GTE_C_EXP2_DEG2_C1 6.5571332605741528e-01
#define GTE_C_EXP2_DEG2_C2 3.4428667394258472e-01
#define GTE_C_EXP2_DEG2_MAX_ERROR 3.8132476831060358e-3

#define GTE_C_EXP2_DEG3_C0 1.0
#define GTE_C_EXP2_DEG3_C1 6.9589012084456225e-01
#define GTE_C_EXP2_DEG3_C2 2.2486494900110188e-01
#define GTE_C_EXP2_DEG3_C3 7.9244930154334980e-02
#define GTE_C_EXP2_DEG3_MAX_ERROR 1.4694877755186408e-4

#define GTE_C_EXP2_DEG4_C0 1.0
#define GTE_C_EXP2_DEG4_C1 6.9300392358459195e-01
#define GTE_C_EXP2_DEG4_C2 2.4154981722455560e-01
#define GTE_C_EXP2_DEG4_C3 5.1744260331489045e-02
#define GTE_C_EXP2_DEG4_C4 1.3701998859367848e-02
#define GTE_C_EXP2_DEG4_MAX_ERROR 4.7617792624521371e-6

#define GTE_C_EXP2_DEG5_C0 1.0
#define GTE_C_EXP2_DEG5_C1 6.9315298010274962e-01
#define GTE_C_EXP2_DEG5_C2 2.4014712313022102e-01
#define GTE_C_EXP2_DEG5_C3 5.5855296413199085e-02
#define GTE_C_EXP2_DEG5_C4 8.9477503096873079e-03
#define GTE_C_EXP2_DEG5_C5 1.8968500441332026e-03
#define GTE_C_EXP2_DEG5_MAX_ERROR 1.3162098333463490e-7

#define GTE_C_EXP2_DEG6_C0 1.0
#define GTE_C_EXP2_DEG6_C1 6.9314698914837525e-01
#define GTE_C_EXP2_DEG6_C2 2.4023013440952923e-01
#define GTE_C_EXP2_DEG6_C3 5.5481276898206033e-02
#define GTE_C_EXP2_DEG6_C4 9.6838443037086108e-03
#define GTE_C_EXP2_DEG6_C5 1.2388324048515642e-03
#define GTE_C_EXP2_DEG6_C6 2.1892283501756538e-04
#define GTE_C_EXP2_DEG6_MAX_ERROR 3.1589168225654163e-9

#define GTE_C_EXP2_DEG7_C0 1.0
#define GTE_C_EXP2_DEG7_C1 6.9314718588750690e-01
#define GTE_C_EXP2_DEG7_C2 2.4022637363165700e-01
#define GTE_C_EXP2_DEG7_C3 5.5505235570535660e-02
#define GTE_C_EXP2_DEG7_C4 9.6136265387940512e-03
#define GTE_C_EXP2_DEG7_C5 1.3429234504656051e-03
#define GTE_C_EXP2_DEG7_C6 1.4299202757683815e-04
#define GTE_C_EXP2_DEG7_C7 2.1662892777385423e-05
#define GTE_C_EXP2_DEG7_MAX_ERROR 6.6864513925679603e-11

// Constants for minimax polynomial approximations to log2(x).
// The algorithm minimizes the maximum absolute error on [1,2].
// The polynomials all have constant term zero.
#define GTE_C_LOG2_DEG1_C1 +1.0
#define GTE_C_LOG2_DEG1_MAX_ERROR 8.6071332055934202e-2

#define GTE_C_LOG2_DEG2_C1 +1.3465553856377803 
#define GTE_C_LOG2_DEG2_C2 -3.4655538563778032e-01
#define GTE_C_LOG2_DEG2_MAX_ERROR 7.6362868906658110e-3

#define GTE_C_LOG2_DEG3_C1 +1.4228653756681227
#define GTE_C_LOG2_DEG3_C2 -5.8208556916449616e-01
#define GTE_C_LOG2_DEG3_C3 +1.5922019349637218e-01
#define GTE_C_LOG2_DEG3_MAX_ERROR 8.7902902652883808e-4

#define GTE_C_LOG2_DEG4_C1 +1.4387257478171547
#define GTE_C_LOG2_DEG4_C2 -6.7778401359918661e-01
#define GTE_C_LOG2_DEG4_C3 +3.2118898377713379e-01
#define GTE_C_LOG2_DEG4_C4 -8.2130717995088531e-02
#define GTE_C_LOG2_DEG4_MAX_ERROR 1.1318551355360418e-4

#define GTE_C_LOG2_DEG5_C1 +1.4419170408633741
#define GTE_C_LOG2_DEG5_C2 -7.0909645927612530e-01
#define GTE_C_LOG2_DEG5_C3 +4.1560609399164150e-01
#define GTE_C_LOG2_DEG5_C4 -1.9357573729558908e-01
#define GTE_C_LOG2_DEG5_C5 +4.5149061716699634e-02
#define GTE_C_LOG2_DEG5_MAX_ERROR 1.5521274478735858e-5

#define GTE_C_LOG2_DEG6_C1 +1.4425449435950917
#define GTE_C_LOG2_DEG6_C2 -7.1814525675038965e-01
#define GTE_C_LOG2_DEG6_C3 +4.5754919692564044e-01
#define GTE_C_LOG2_DEG6_C4 -2.7790534462849337e-01
#define GTE_C_LOG2_DEG6_C5 +1.2179791068763279e-01
#define GTE_C_LOG2_DEG6_C6 -2.5841449829670182e-02
#define GTE_C_LOG2_DEG6_MAX_ERROR 2.2162051216689793e-6

#define GTE_C_LOG2_DEG7_C1 +1.4426664401536078
#define GTE_C_LOG2_DEG7_C2 -7.2055423726162360e-01
#define GTE_C_LOG2_DEG7_C3 +4.7332419162501083e-01
#define GTE_C_LOG2_DEG7_C4 -3.2514018752954144e-01
#define GTE_C_LOG2_DEG7_C5 +1.9302965529095673e-01
#define GTE_C_LOG2_DEG7_C6 -7.8534970641157997e-02
#define GTE_C_LOG2_DEG7_C7 +1.5209108363023915e-02
#define GTE_C_LOG2_DEG7_MAX_ERROR 3.2546531700261561e-7

#define GTE_C_LOG2_DEG8_C1 +1.4426896453621882
#define GTE_C_LOG2_DEG8_C2 -7.2115893912535967e-01
#define GTE_C_LOG2_DEG8_C3 +4.7861716616785088e-01
#define GTE_C_LOG2_DEG8_C4 -3.4699935395019565e-01
#define GTE_C_LOG2_DEG8_C5 +2.4114048765477492e-01
#define GTE_C_LOG2_DEG8_C6 -1.3657398692885181e-01
#define GTE_C_LOG2_DEG8_C7 +5.1421382871922106e-02
#define GTE_C_LOG2_DEG8_C8 -9.1364020499895560e-03
#define GTE_C_LOG2_DEG8_MAX_ERROR 4.8796219218050219e-8

// These functions are convenient for some applications.  The classes
// BSNumber, BSRational and IEEEBinary16 have implementations that
// (for now) use typecasting to call the 'float' or 'double' versions.
namespace gte
{
    inline float atandivpi(float x)
    {
        return std::atan(x) * (float)GTE_C_INV_PI;
    }

    inline float atan2divpi(float y, float x)
    {
        return std::atan2(y, x) * (float)GTE_C_INV_PI;
    }

    inline float clamp(float x, float xmin, float xmax)
    {
        return (x <= xmin ? xmin : (x >= xmax ? xmax : x));
    }

    inline float cospi(float x)
    {
        return std::cos(x * (float)GTE_C_PI);
    }

    inline float exp10(float x)
    {
        return std::exp(x * (float)GTE_C_LN_10);
    }

    inline float invsqrt(float x)
    {
        return 1.0f / std::sqrt(x);
    }

    inline int isign(float x)
    {
        return (x > 0.0f ? 1 : (x < 0.0f ? -1 : 0));
    }

    inline float saturate(float x)
    {
        return (x <= 0.0f ? 0.0f : (x >= 1.0f ? 1.0f : x));
    }

    inline float sign(float x)
    {
        return (x > 0.0f ? 1.0f : (x < 0.0f ? -1.0f : 0.0f));
    }

    inline float sinpi(float x)
    {
        return std::sin(x * (float)GTE_C_PI);
    }

    inline float sqr(float x)
    {
        return x * x;
    }


    inline double atandivpi(double x)
    {
        return std::atan(x) * GTE_C_INV_PI;
    }

    inline double atan2divpi(double y, double x)
    {
        return std::atan2(y, x) * GTE_C_INV_PI;
    }

    inline double clamp(double x, double xmin, double xmax)
    {
        return (x <= xmin ? xmin : (x >= xmax ? xmax : x));
    }

    inline double cospi(double x)
    {
        return std::cos(x * GTE_C_PI);
    }

    inline double exp10(double x)
    {
        return std::exp(x * GTE_C_LN_10);
    }

    inline double invsqrt(double x)
    {
        return 1.0 / std::sqrt(x);
    }

    inline double sign(double x)
    {
        return (x > 0.0 ? 1.0 : (x < 0.0 ? -1.0 : 0.0f));
    }

    inline int isign(double x)
    {
        return (x > 0.0 ? 1 : (x < 0.0 ? -1 : 0));
    }

    inline double saturate(double x)
    {
        return (x <= 0.0 ? 0.0 : (x >= 1.0 ? 1.0 : x));
    }

    inline double sinpi(double x)
    {
        return std::sin(x * GTE_C_PI);
    }

    inline double sqr(double x)
    {
        return x * x;
    }
}

// Type traits to support std::enable_if conditional compilation for
// numerical computations.
namespace gte
{
    // The trait is_arbitrary_precision<T> for type T of float, double or
    // long double generates is_arbitrary_precision<T>::value of false.  The
    // implementations for arbitrary-precision arithmetic are found in
    // GteArbitraryPrecision.h.
    template <typename T>
    struct is_arbitrary_precision_internal : std::false_type {};

    template <typename T>
    struct is_arbitrary_precision : is_arbitrary_precision_internal<std::remove_cv_t<T>>::type {};

    // The trait has_division_operator<T> for type T of float, double or
    // long double generates has_division_operator<T>::value of true.  The
    // implementations for arbitrary-precision arithmetic are found in
    // GteArbitraryPrecision.h.
    template <typename T>
    struct has_division_operator_internal : std::false_type {};

    template <typename T>
    struct has_division_operator : has_division_operator_internal<std::remove_cv_t<T>>::type {};

    template <>
    struct has_division_operator_internal<float> : std::true_type {};

    template <>
    struct has_division_operator_internal<double> : std::true_type {};

    template <>
    struct has_division_operator_internal<long double> : std::true_type {};
}


// ----------------------------------------------
// GteDCPQuery.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2018/10/05)

namespace gte
{

// Distance and closest-point queries.
template <typename Real, typename Type0, typename Type1>
class DCPQuery
{
public:
    struct Result
    {
        // A DCPQuery-base class B must define a B::Result struct with member
        // 'Real distance'.  A DCPQuery-derived class D must also derive a
        // D::Result from B:Result but may have no members.  The idea is to
        // allow Result to store closest-point information in addition to the
        // distance.  The operator() is non-const to allow DCPQuery to store
        // and modify private state that supports the query.
    };
    Result operator()(Type0 const& primitive0, Type1 const& primitive1);
};

}


// ----------------------------------------------
// GteApprQuery.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

// Class ApprQuery supports the RANSAC algorithm for fitting and uses the
// Curiously Recurring Template Pattern.  The ModelType must be a class or
// struct with the following interfaces:
//
//   // The minimum number of observations required to fit the model.
//   int ModelType::GetMinimumRequired() const;
//
//   // Compute the model error for the specified observation for the current
//   // model parameters.
//   Real Error(ObservationType const& observation) const;
//
//   // Estimate the model parameters for all observations specified by the
//   // indices.  The three Fit() functions of ApprQuery manipulate their
//   // inputs in order to pass them to ModelType::Fit().
//   ModelType::Fit(std::vector<ObservationType> const& observations,
//       std::vector<size_t> const& indices);

namespace gte
{

template <typename Real, typename ModelType, typename ObservationType>
class ApprQuery
{
public:
    // Estimate the model parameters for all observations.
    bool Fit(std::vector<ObservationType> const& observations);

    // Estimate the model parameters for a contiguous subset of observations.
    bool Fit(std::vector<ObservationType> const& observations,
        int const imin, int const imax);

    // Estimate the model parameters for the subset of observations specified
    // by the indices and the number of indices that is possibly smaller than
    // indices.size().
    bool Fit(std::vector<ObservationType> const& observations,
        std::vector<int> const& indices, int const numIndices);

    // Apply the RANdom SAmple Consensus algorithm for fitting a model to
    // observations.
    static bool RANSAC(
        ModelType& candidateModel,
        std::vector<ObservationType> const& observations,
        int const numRequiredForGoodFit, Real const maxErrorForGoodFit,
        int const numIterations, std::vector<int>& bestConsensus,
        ModelType& bestModel);
};


template <typename Real, typename ModelType, typename ObservationType>
bool ApprQuery<Real, ModelType, ObservationType>::Fit(
    std::vector<ObservationType> const& observations)
{
    std::vector<int> indices(observations.size());
    int i = 0;
    for (auto& index : indices)
    {
        index = i++;
    }

    return ((ModelType*)this)->Fit(observations, indices);
}

template <typename Real, typename ModelType, typename ObservationType>
bool ApprQuery<Real, ModelType, ObservationType>::Fit(
    std::vector<ObservationType> const& observations,
    int const imin, int const imax)
{
    if (imin <= imax)
    {
        int numIndices = imax - imin + 1;
        std::vector<int> indices(numIndices);
        int i = imin;
        for (auto& index : indices)
        {
            index = i++;
        }

        return ((ModelType*)this)->Fit(observations, indices);
    }
    else
    {
        return false;
    }
}

template <typename Real, typename ModelType, typename ObservationType>
bool ApprQuery<Real, ModelType, ObservationType>::Fit(
    std::vector<ObservationType> const& observations,
    std::vector<int> const& indices, int const numIndices)
{
    int imax = std::min(numIndices, static_cast<int>(observations.size()));
    std::vector<int> localindices(imax);
    int i = 0;
    for (auto& index : localindices)
    {
        index = indices[i++];
    }

    return ((ModelType*)this)->Fit(observations, indices);
}

template <typename Real, typename ModelType, typename ObservationType>
bool ApprQuery<Real, ModelType, ObservationType>::RANSAC(
    ModelType& candidateModel,
    std::vector<ObservationType> const& observations,
    int const numRequiredForGoodFit, Real const maxErrorForGoodFit,
    int const numIterations, std::vector<int>& bestConsensus,
    ModelType& bestModel)
{
    int const numObservations = static_cast<int>(observations.size());
    int const minRequired = candidateModel.GetMinimumRequired();
    if (numObservations < minRequired)
    {
        // Too few observations for model fitting.
        return false;
    }

    // The first part of the array will store the consensus set, initially
    // filled with the minimumu number of indices that correspond to the
    // candidate inliers.  The last part will store the remaining indices.
    // These points are tested against the model and are added to the
    // consensus set when they fit.  All the index manipulation is done
    // in place.  Initially, the candidates are the identity permutation.
    std::vector<int> candidates(numObservations);
    int j = 0;
    for (auto& c : candidates)
    {
        c = j++;
    }

    if (numObservations == minRequired)
    {
        // We have the minimum number of observations to generate the model,
        // so RANSAC cannot be used.  Compute the model with the entire set
        // of observations.
        bestConsensus = candidates;
        return bestModel.Fit(observations);
    }

    int bestNumFittedObservations = minRequired;

    for (int i = 0; i < numIterations; ++i)
    {
        // Randomly permute the previous candidates, partitioning the array
        // into GetMinimumRequired() indices (the candidate inliers) followed
        // by the remaining indices (candidates for testing against the
        // model).
        std::shuffle(candidates.begin(), candidates.end(),
            std::default_random_engine());

        // Fit the model to the inliers.
        if (candidateModel.Fit(observations, candidates, minRequired))
        {
            // Test each remaining observation whether it fits the model.  If
            // it does, include it in the consensus set.
            int numFittedObservations = minRequired;
            for (j = minRequired; j < numObservations; ++j)
            {
                if (candidateModel.Error(observations[candidates[j]])
                    <= maxErrorForGoodFit)
                {
                    std::swap(candidates[j],
                        candidates[numFittedObservations]);
                    ++numFittedObservations;
                }
            }

            if (numFittedObservations >= numRequiredForGoodFit)
            {
                // We have observations that fit the model.  Update the best
                // model using the consensus set.
                candidateModel.Fit(observations, candidates,
                    numFittedObservations);
                if (numFittedObservations > bestNumFittedObservations)
                {
                    // The consensus set is larger than the previous consensus
                    // set, so its model becomes the best one.
                    bestModel = candidateModel;
                    bestConsensus.resize(numFittedObservations);
                    std::copy(candidates.begin(),
                        candidates.begin() + numFittedObservations,
                        bestConsensus.begin());
                    bestNumFittedObservations = numFittedObservations;
                }
            }
        }
    }

    return bestNumFittedObservations >= numRequiredForGoodFit;
}


}


// ----------------------------------------------
// GteSymmetricEigensolver3x3.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.5 (2018/10/09)

// The document
// http://www.geometrictools.com/Documentation/RobustEigenSymmetric3x3.pdf
// describes algorithms for solving the eigensystem associated with a 3x3
// symmetric real-valued matrix.  The iterative algorithm is implemented
// by class SymmmetricEigensolver3x3.  The noniterative algorithm is
// implemented by class NISymmetricEigensolver3x3.  The code does not use
// GTEngine objects.

namespace gte
{

template <typename Real>
class SymmetricEigensolver3x3
{
public:
    // The input matrix must be symmetric, so only the unique elements must
    // be specified: a00, a01, a02, a11, a12, and a22.
    //
    // If 'aggressive' is 'true', the iterations occur until a superdiagonal
    // entry is exactly zero.  If 'aggressive' is 'false', the iterations
    // occur until a superdiagonal entry is effectively zero compared to the
    // sum of magnitudes of its diagonal neighbors.  Generally, the
    // nonaggressive convergence is acceptable.
    //
    // The order of the eigenvalues is specified by sortType: -1 (decreasing),
    // 0 (no sorting), or +1 (increasing).  When sorted, the eigenvectors are
    // ordered accordingly, and {evec[0], evec[1], evec[2]} is guaranteed to
    // be a right-handed orthonormal set.  The return value is the number of
    // iterations used by the algorithm.

    int operator()(Real a00, Real a01, Real a02, Real a11, Real a12, Real a22,
        bool aggressive, int sortType, std::array<Real, 3>& eval,
        std::array<std::array<Real, 3>, 3>& evec) const;

private:
    // Update Q = Q*G in-place using G = {{c,0,-s},{s,0,c},{0,0,1}}.
    void Update0(Real Q[3][3], Real c, Real s) const;

    // Update Q = Q*G in-place using G = {{0,1,0},{c,0,s},{-s,0,c}}.
    void Update1(Real Q[3][3], Real c, Real s) const;

    // Update Q = Q*H in-place using H = {{c,s,0},{s,-c,0},{0,0,1}}.
    void Update2(Real Q[3][3], Real c, Real s) const;

    // Update Q = Q*H in-place using H = {{1,0,0},{0,c,s},{0,s,-c}}.
    void Update3(Real Q[3][3], Real c, Real s) const;

    // Normalize (u,v) robustly, avoiding floating-point overflow in the sqrt
    // call.  The normalized pair is (cs,sn) with cs <= 0.  If (u,v) = (0,0),
    // the function returns (cs,sn) = (-1,0).  When used to generate a
    // Householder reflection, it does not matter whether (cs,sn) or (-cs,-sn)
    // is used.  When generating a Givens reflection, cs = cos(2*theta) and
    // sn = sin(2*theta).  Having a negative cosine for the double-angle
    // term ensures that the single-angle terms c = cos(theta) and
    // s = sin(theta) satisfy |c| <= |s|.
    void GetCosSin(Real u, Real v, Real& cs, Real& sn) const;

    // The convergence test.  When 'aggressive' is 'true', the superdiagonal
    // test is "bSuper == 0".  When 'aggressive' is 'false', the superdiagonal
    // test is "|bDiag0| + |bDiag1| + |bSuper| == |bDiag0| + |bDiag1|, which
    // means bSuper is effectively zero compared to the sizes of the diagonal
    // entries.
    bool Converged(bool aggressive, Real bDiag0, Real bDiag1,
        Real bSuper) const;

    // Support for sorting the eigenvalues and eigenvectors.  The output
    // (i0,i1,i2) is a permutation of (0,1,2) so that d[i0] <= d[i1] <= d[i2].
    // The 'bool' return indicates whether the permutation is odd.  If it is
    // not, the handedness of the Q matrix must be adjusted.
    bool Sort(std::array<Real, 3> const& d, int& i0, int& i1, int& i2) const;
};


template <typename Real>
class NISymmetricEigensolver3x3
{
public:
    // The input matrix must be symmetric, so only the unique elements must
    // be specified: a00, a01, a02, a11, a12, and a22.  The eigenvalues are
    // sorted in ascending order: eval0 <= eval1 <= eval2.

    void operator()(Real a00, Real a01, Real a02, Real a11, Real a12, Real a22,
        std::array<Real, 3>& eval, std::array<std::array<Real, 3>, 3>& evec) const;

private:
    static std::array<Real, 3> Multiply(Real s, std::array<Real, 3> const& U);
    static std::array<Real, 3> Subtract(std::array<Real, 3> const& U, std::array<Real, 3> const& V);
    static std::array<Real, 3> Divide(std::array<Real, 3> const& U, Real s);
    static Real Dot(std::array<Real, 3> const& U, std::array<Real, 3> const& V);
    static std::array<Real, 3> Cross(std::array<Real, 3> const& U, std::array<Real, 3> const& V);

    void ComputeOrthogonalComplement(std::array<Real, 3> const& W,
        std::array<Real, 3>& U, std::array<Real, 3>& V) const;

    void ComputeEigenvector0(Real a00, Real a01, Real a02, Real a11, Real a12, Real a22,
        Real eval0, std::array<Real, 3>& evec0) const;

    void ComputeEigenvector1(Real a00, Real a01, Real a02, Real a11, Real a12, Real a22,
        std::array<Real, 3> const& evec0, Real eval1, std::array<Real, 3>& evec1) const;
};


template <typename Real>
int SymmetricEigensolver3x3<Real>::operator()(Real a00, Real a01, Real a02,
    Real a11, Real a12, Real a22, bool aggressive, int sortType,
    std::array<Real, 3>& eval, std::array<std::array<Real, 3>, 3>& evec) const
{
    // Compute the Householder reflection H and B = H*A*H, where b02 = 0.
    Real const zero = (Real)0, one = (Real)1, half = (Real)0.5;
    bool isRotation = false;
    Real c, s;
    GetCosSin(a12, -a02, c, s);
    Real Q[3][3] = { { c, s, zero }, { s, -c, zero }, { zero, zero, one } };
    Real term0 = c * a00 + s * a01;
    Real term1 = c * a01 + s * a11;
    Real b00 = c * term0 + s * term1;
    Real b01 = s * term0 - c * term1;
    term0 = s * a00 - c * a01;
    term1 = s * a01 - c * a11;
    Real b11 = s * term0 - c * term1;
    Real b12 = s * a02 - c * a12;
    Real b22 = a22;

    // Givens reflections, B' = G^T*B*G, preserve tridiagonal matrices.
    int const maxIteration = 2 * (1 + std::numeric_limits<Real>::digits -
        std::numeric_limits<Real>::min_exponent);
    int iteration;
    Real c2, s2;

    if (std::abs(b12) <= std::abs(b01))
    {
        Real saveB00, saveB01, saveB11;
        for (iteration = 0; iteration < maxIteration; ++iteration)
        {
            // Compute the Givens reflection.
            GetCosSin(half * (b00 - b11), b01, c2, s2);
            s = std::sqrt(half * (one - c2));  // >= 1/sqrt(2)
            c = half * s2 / s;

            // Update Q by the Givens reflection.
            Update0(Q, c, s);
            isRotation = !isRotation;

            // Update B <- Q^T*B*Q, ensuring that b02 is zero and |b12| has
            // strictly decreased.
            saveB00 = b00;
            saveB01 = b01;
            saveB11 = b11;
            term0 = c * saveB00 + s * saveB01;
            term1 = c * saveB01 + s * saveB11;
            b00 = c * term0 + s * term1;
            b11 = b22;
            term0 = c * saveB01 - s * saveB00;
            term1 = c * saveB11 - s * saveB01;
            b22 = c * term1 - s * term0;
            b01 = s * b12;
            b12 = c * b12;

            if (Converged(aggressive, b00, b11, b01))
            {
                // Compute the Householder reflection.
                GetCosSin(half * (b00 - b11), b01, c2, s2);
                s = std::sqrt(half * (one - c2));
                c = half * s2 / s;  // >= 1/sqrt(2)

                // Update Q by the Householder reflection.
                Update2(Q, c, s);
                isRotation = !isRotation;

                // Update D = Q^T*B*Q.
                saveB00 = b00;
                saveB01 = b01;
                saveB11 = b11;
                term0 = c * saveB00 + s * saveB01;
                term1 = c * saveB01 + s * saveB11;
                b00 = c * term0 + s * term1;
                term0 = s * saveB00 - c * saveB01;
                term1 = s * saveB01 - c * saveB11;
                b11 = s * term0 - c * term1;
                break;
            }
        }
    }
    else
    {
        Real saveB11, saveB12, saveB22;
        for (iteration = 0; iteration < maxIteration; ++iteration)
        {
            // Compute the Givens reflection.
            GetCosSin(half * (b22 - b11), b12, c2, s2);
            s = std::sqrt(half * (one - c2));  // >= 1/sqrt(2)
            c = half * s2 / s;

            // Update Q by the Givens reflection.
            Update1(Q, c, s);
            isRotation = !isRotation;

            // Update B <- Q^T*B*Q, ensuring that b02 is zero and |b12| has
            // strictly decreased.  MODIFY...
            saveB11 = b11;
            saveB12 = b12;
            saveB22 = b22;
            term0 = c * saveB22 + s * saveB12;
            term1 = c * saveB12 + s * saveB11;
            b22 = c * term0 + s * term1;
            b11 = b00;
            term0 = c * saveB12 - s * saveB22;
            term1 = c * saveB11 - s * saveB12;
            b00 = c * term1 - s * term0;
            b12 = s * b01;
            b01 = c * b01;

            if (Converged(aggressive, b11, b22, b12))
            {
                // Compute the Householder reflection.
                GetCosSin(half * (b11 - b22), b12, c2, s2);
                s = std::sqrt(half * (one - c2));
                c = half * s2 / s;  // >= 1/sqrt(2)

                // Update Q by the Householder reflection.
                Update3(Q, c, s);
                isRotation = !isRotation;

                // Update D = Q^T*B*Q.
                saveB11 = b11;
                saveB12 = b12;
                saveB22 = b22;
                term0 = c * saveB11 + s * saveB12;
                term1 = c * saveB12 + s * saveB22;
                b11 = c * term0 + s * term1;
                term0 = s * saveB11 - c * saveB12;
                term1 = s * saveB12 - c * saveB22;
                b22 = s * term0 - c * term1;
                break;
            }
        }
    }

	std::array<Real, 3> diagonal = { { b00, b11, b22 } };
    int i0, i1, i2;
    if (sortType >= 1)
    {
        // diagonal[i0] <= diagonal[i1] <= diagonal[i2]
        bool isOdd = Sort(diagonal, i0, i1, i2);
        if (!isOdd)
        {
            isRotation = !isRotation;
        }
    }
    else if (sortType <= -1)
    {
        // diagonal[i0] >= diagonal[i1] >= diagonal[i2]
        bool isOdd = Sort(diagonal, i0, i1, i2);
        std::swap(i0, i2);  // (i0,i1,i2)->(i2,i1,i0) is odd
        if (isOdd)
        {
            isRotation = !isRotation;
        }
    }
    else
    {
        i0 = 0;
        i1 = 1;
        i2 = 2;
    }

    eval[0] = diagonal[i0];
    eval[1] = diagonal[i1];
    eval[2] = diagonal[i2];
    evec[0][0] = Q[0][i0];
    evec[0][1] = Q[1][i0];
    evec[0][2] = Q[2][i0];
    evec[1][0] = Q[0][i1];
    evec[1][1] = Q[1][i1];
    evec[1][2] = Q[2][i1];
    evec[2][0] = Q[0][i2];
    evec[2][1] = Q[1][i2];
    evec[2][2] = Q[2][i2];

    // Ensure the columns of Q form a right-handed set.
    if (!isRotation)
    {
        for (int j = 0; j < 3; ++j)
        {
            evec[2][j] = -evec[2][j];
        }
    }

    return iteration;
}

template <typename Real>
void SymmetricEigensolver3x3<Real>::Update0(Real Q[3][3], Real c, Real s)
const
{
    for (int r = 0; r < 3; ++r)
    {
        Real tmp0 = c * Q[r][0] + s * Q[r][1];
        Real tmp1 = Q[r][2];
        Real tmp2 = c * Q[r][1] - s * Q[r][0];
        Q[r][0] = tmp0;
        Q[r][1] = tmp1;
        Q[r][2] = tmp2;
    }
}

template <typename Real>
void SymmetricEigensolver3x3<Real>::Update1(Real Q[3][3], Real c, Real s)
const
{
    for (int r = 0; r < 3; ++r)
    {
        Real tmp0 = c * Q[r][1] - s * Q[r][2];
        Real tmp1 = Q[r][0];
        Real tmp2 = c * Q[r][2] + s * Q[r][1];
        Q[r][0] = tmp0;
        Q[r][1] = tmp1;
        Q[r][2] = tmp2;
    }
}

template <typename Real>
void SymmetricEigensolver3x3<Real>::Update2(Real Q[3][3], Real c, Real s)
const
{
    for (int r = 0; r < 3; ++r)
    {
        Real tmp0 = c * Q[r][0] + s * Q[r][1];
        Real tmp1 = s * Q[r][0] - c * Q[r][1];
        Q[r][0] = tmp0;
        Q[r][1] = tmp1;
    }
}

template <typename Real>
void SymmetricEigensolver3x3<Real>::Update3(Real Q[3][3], Real c, Real s)
const
{
    for (int r = 0; r < 3; ++r)
    {
        Real tmp0 = c * Q[r][1] + s * Q[r][2];
        Real tmp1 = s * Q[r][1] - c * Q[r][2];
        Q[r][1] = tmp0;
        Q[r][2] = tmp1;
    }
}

template <typename Real>
void SymmetricEigensolver3x3<Real>::GetCosSin(Real u, Real v, Real& cs,
    Real& sn) const
{
    Real maxAbsComp = std::max(std::abs(u), std::abs(v));
    if (maxAbsComp > (Real)0)
    {
        u /= maxAbsComp;  // in [-1,1]
        v /= maxAbsComp;  // in [-1,1]
        Real length = std::sqrt(u * u + v * v);
        cs = u / length;
        sn = v / length;
        if (cs > (Real)0)
        {
            cs = -cs;
            sn = -sn;
        }
    }
    else
    {
        cs = (Real)-1;
        sn = (Real)0;
    }
}

template <typename Real>
bool SymmetricEigensolver3x3<Real>::Converged(bool aggressive, Real bDiag0,
    Real bDiag1, Real bSuper) const
{
    if (aggressive)
    {
        return bSuper == (Real)0;
    }
    else
    {
        Real sum = std::abs(bDiag0) + std::abs(bDiag1);
        return sum + std::abs(bSuper) == sum;
    }
}

template <typename Real>
bool SymmetricEigensolver3x3<Real>::Sort(std::array<Real, 3> const& d,
    int& i0, int& i1, int& i2) const
{
    bool odd;
    if (d[0] < d[1])
    {
        if (d[2] < d[0])
        {
            i0 = 2; i1 = 0; i2 = 1; odd = true;
        }
        else if (d[2] < d[1])
        {
            i0 = 0; i1 = 2; i2 = 1; odd = false;
        }
        else
        {
            i0 = 0; i1 = 1; i2 = 2; odd = true;
        }
    }
    else
    {
        if (d[2] < d[1])
        {
            i0 = 2; i1 = 1; i2 = 0; odd = false;
        }
        else if (d[2] < d[0])
        {
            i0 = 1; i1 = 2; i2 = 0; odd = true;
        }
        else
        {
            i0 = 1; i1 = 0; i2 = 2; odd = false;
        }
    }
    return odd;
}


template <typename Real>
void NISymmetricEigensolver3x3<Real>::operator() (Real a00, Real a01, Real a02,
    Real a11, Real a12, Real a22, std::array<Real, 3>& eval,
    std::array<std::array<Real, 3>, 3>& evec) const
{
    // Precondition the matrix by factoring out the maximum absolute value
    // of the components.  This guards against floating-point overflow when
    // computing the eigenvalues.
    Real max0 = std::max(std::abs(a00), std::abs(a01));
    Real max1 = std::max(std::abs(a02), std::abs(a11));
    Real max2 = std::max(std::abs(a12), std::abs(a22));
    Real maxAbsElement = std::max(std::max(max0, max1), max2);
    if (maxAbsElement == (Real)0)
    {
        // A is the zero matrix.
        eval[0] = (Real)0;
        eval[1] = (Real)0;
        eval[2] = (Real)0;
        evec[0] = { (Real)1, (Real)0, (Real)0 };
        evec[1] = { (Real)0, (Real)1, (Real)0 };
        evec[2] = { (Real)0, (Real)0, (Real)1 };
        return;
    }

    Real invMaxAbsElement = (Real)1 / maxAbsElement;
    a00 *= invMaxAbsElement;
    a01 *= invMaxAbsElement;
    a02 *= invMaxAbsElement;
    a11 *= invMaxAbsElement;
    a12 *= invMaxAbsElement;
    a22 *= invMaxAbsElement;

    Real norm = a01 * a01 + a02 * a02 + a12 * a12;
    if (norm > (Real)0)
    {
        // Compute the eigenvalues of A.

        // In the PDF mentioned previously, B = (A - q*I)/p, where q = tr(A)/3 
        // with tr(A) the trace of A (sum of the diagonal entries of A) and where
        // p = sqrt(tr((A - q*I)^2)/6).
        Real q = (a00 + a11 + a22) / (Real)3;

        // The matrix A - q*I is represented by the following, where b00, b11 and
        // b22 are computed after these comments,
        //   +-           -+
        //   | b00 a01 a02 |
        //   | a01 b11 a12 |
        //   | a02 a12 b22 |
        //   +-           -+
        Real b00 = a00 - q;
        Real b11 = a11 - q;
        Real b22 = a22 - q;

        // The is the variable p mentioned in the PDF.
        Real p = std::sqrt((b00 * b00 + b11 * b11 + b22 * b22 + norm * (Real)2) / (Real)6);

        // We need det(B) = det((A - q*I)/p) = det(A - q*I)/p^3.  The value
        // det(A - q*I) is computed using a cofactor expansion by the first
        // row of A - q*I.  The cofactors are c00, c01 and c02 and the
        // determinant is b00*c00 - a01*c01 + a02*c02.  The det(B) is then
        // computed finally by the division with p^3.
        Real c00 = b11 * b22 - a12 * a12;
        Real c01 = a01 * b22 - a12 * a02;
        Real c02 = a01 * a12 - b11 * a02;
        Real det = (b00 * c00 - a01 * c01 + a02 * c02) / (p * p * p);

        // The halfDet value is cos(3*theta) mentioned in the PDF. The acos(z)
        // function requires |z| <= 1, but will fail silently and return NaN
        // if the input is larger than 1 in magnitude.  To avoid this problem
        // due to rounding errors, the halfDet/ value is clamped to [-1,1].
        Real halfDet = det * (Real)0.5;
        halfDet = std::min(std::max(halfDet, (Real)-1), (Real)1);

        // The eigenvalues of B are ordered as beta0 <= beta1 <= beta2.  The
        // number of digits in twoThirdsPi is chosen so that, whether float or
        // double, the floating-point number is the closest to theoretical 2*pi/3.
        Real angle = std::acos(halfDet) / (Real)3;
        Real const twoThirdsPi = (Real)2.09439510239319549;
        Real beta2 = std::cos(angle) * (Real)2;
        Real beta0 = std::cos(angle + twoThirdsPi) * (Real)2;
        Real beta1 = -(beta0 + beta2);

        // The eigenvalues of A are ordered as alpha0 <= alpha1 <= alpha2.
        eval[0] = q + p * beta0;
        eval[1] = q + p * beta1;
        eval[2] = q + p * beta2;

        // Compute the eigenvectors so that the set {evec[0], evec[1], evec[2]}
        // is right handed and orthonormal.
        if (halfDet >= (Real)0)
        {
            ComputeEigenvector0(a00, a01, a02, a11, a12, a22, eval[2], evec[2]);
            ComputeEigenvector1(a00, a01, a02, a11, a12, a22, evec[2], eval[1], evec[1]);
            evec[0] = Cross(evec[1], evec[2]);
        }
        else
        {
            ComputeEigenvector0(a00, a01, a02, a11, a12, a22, eval[0], evec[0]);
            ComputeEigenvector1(a00, a01, a02, a11, a12, a22, evec[0], eval[1], evec[1]);
            evec[2] = Cross(evec[0], evec[1]);
        }
    }
    else
    {
        // The matrix is diagonal.
        eval[0] = a00;
        eval[1] = a11;
        eval[2] = a22;
        evec[0] = { (Real)1, (Real)0, (Real)0 };
        evec[1] = { (Real)0, (Real)1, (Real)0 };
        evec[2] = { (Real)0, (Real)0, (Real)1 };
    }

    // The preconditioning scaled the matrix A, which scales the eigenvalues.
    // Revert the scaling.
    eval[0] *= maxAbsElement;
    eval[1] *= maxAbsElement;
    eval[2] *= maxAbsElement;
}

template <typename Real>
std::array<Real, 3> NISymmetricEigensolver3x3<Real>::Multiply(
    Real s, std::array<Real, 3> const& U)
{
    std::array<Real, 3> product = { s * U[0], s * U[1], s * U[2] };
    return product;
}

template <typename Real>
std::array<Real, 3> NISymmetricEigensolver3x3<Real>::Subtract(
    std::array<Real, 3> const& U, std::array<Real, 3> const& V)
{
    std::array<Real, 3> difference = { U[0] - V[0], U[1] - V[1], U[2] - V[2] };
    return difference;
}

template <typename Real>
std::array<Real, 3> NISymmetricEigensolver3x3<Real>::Divide(
    std::array<Real, 3> const& U, Real s)
{
    Real invS = (Real)1 / s;
    std::array<Real, 3> division = { U[0] * invS, U[1] * invS, U[2] * invS };
    return division;
}

template <typename Real>
Real NISymmetricEigensolver3x3<Real>::Dot(std::array<Real, 3> const& U,
    std::array<Real, 3> const& V)
{
    Real dot = U[0] * V[0] + U[1] * V[1] + U[2] * V[2];
    return dot;
}

template <typename Real>
std::array<Real, 3> NISymmetricEigensolver3x3<Real>::Cross(std::array<Real, 3> const& U,
    std::array<Real, 3> const& V)
{
    std::array<Real, 3> cross =
    {
        U[1] * V[2] - U[2] * V[1],
        U[2] * V[0] - U[0] * V[2],
        U[0] * V[1] - U[1] * V[0]
    };
    return cross;
}

template <typename Real>
void NISymmetricEigensolver3x3<Real>::ComputeOrthogonalComplement(
    std::array<Real, 3> const& W, std::array<Real, 3>& U, std::array<Real, 3>& V) const
{
    // Robustly compute a right-handed orthonormal set { U, V, W }.  The
    // vector W is guaranteed to be unit-length, in which case there is no
    // need to worry about a division by zero when computing invLength.
    Real invLength;
    if (std::abs(W[0]) > std::abs(W[1]))
    {
        // The component of maximum absolute value is either W[0] or W[2].
        invLength = (Real)1 / std::sqrt(W[0] * W[0] + W[2] * W[2]);
        U = { -W[2] * invLength, (Real)0, +W[0] * invLength };
    }
    else
    {
        // The component of maximum absolute value is either W[1] or W[2].
        invLength = (Real)1 / std::sqrt(W[1] * W[1] + W[2] * W[2]);
        U = { (Real)0, +W[2] * invLength, -W[1] * invLength };
    }
    V = Cross(W, U);
}

template <typename Real>
void NISymmetricEigensolver3x3<Real>::ComputeEigenvector0(Real a00, Real a01,
    Real a02, Real a11, Real a12, Real a22, Real eval0, std::array<Real, 3>& evec0) const
{
    // Compute a unit-length eigenvector for eigenvalue[i0].  The matrix is
    // rank 2, so two of the rows are linearly independent.  For a robust
    // computation of the eigenvector, select the two rows whose cross product
    // has largest length of all pairs of rows.
    std::array<Real, 3> row0 = { a00 - eval0, a01, a02 };
    std::array<Real, 3> row1 = { a01, a11 - eval0, a12 };
    std::array<Real, 3> row2 = { a02, a12, a22 - eval0 };
    std::array<Real, 3>  r0xr1 = Cross(row0, row1);
    std::array<Real, 3>  r0xr2 = Cross(row0, row2);
    std::array<Real, 3>  r1xr2 = Cross(row1, row2);
    Real d0 = Dot(r0xr1, r0xr1);
    Real d1 = Dot(r0xr2, r0xr2);
    Real d2 = Dot(r1xr2, r1xr2);

    Real dmax = d0;
    int imax = 0;
    if (d1 > dmax)
    {
        dmax = d1;
        imax = 1;
    }
    if (d2 > dmax)
    {
        imax = 2;
    }

    if (imax == 0)
    {
        evec0 = Divide(r0xr1, std::sqrt(d0));
    }
    else if (imax == 1)
    {
        evec0 = Divide(r0xr2, std::sqrt(d1));
    }
    else
    {
        evec0 = Divide(r1xr2, std::sqrt(d2));
    }
}

template <typename Real>
void NISymmetricEigensolver3x3<Real>::ComputeEigenvector1(Real a00, Real a01,
    Real a02, Real a11, Real a12, Real a22, std::array<Real, 3> const& evec0,
    Real eval1, std::array<Real, 3>& evec1) const
{
    // Robustly compute a right-handed orthonormal set { U, V, evec0 }.
    std::array<Real, 3> U, V;
    ComputeOrthogonalComplement(evec0, U, V);

    // Let e be eval1 and let E be a corresponding eigenvector which is a
    // solution to the linear system (A - e*I)*E = 0.  The matrix (A - e*I)
    // is 3x3, not invertible (so infinitely many solutions), and has rank 2
    // when eval1 and eval are different.  It has rank 1 when eval1 and eval2
    // are equal.  Numerically, it is difficult to compute robustly the rank
    // of a matrix.  Instead, the 3x3 linear system is reduced to a 2x2 system
    // as follows.  Define the 3x2 matrix J = [U V] whose columns are the U
    // and V computed previously.  Define the 2x1 vector X = J*E.  The 2x2
    // system is 0 = M * X = (J^T * (A - e*I) * J) * X where J^T is the
    // transpose of J and M = J^T * (A - e*I) * J is a 2x2 matrix.  The system
    // may be written as
    //     +-                        -++-  -+       +-  -+
    //     | U^T*A*U - e  U^T*A*V     || x0 | = e * | x0 |
    //     | V^T*A*U      V^T*A*V - e || x1 |       | x1 |
    //     +-                        -++   -+       +-  -+
    // where X has row entries x0 and x1.

    std::array<Real, 3> AU =
    {
        a00 * U[0] + a01 * U[1] + a02 * U[2],
        a01 * U[0] + a11 * U[1] + a12 * U[2],
        a02 * U[0] + a12 * U[1] + a22 * U[2]
    };

    std::array<Real, 3> AV =
    {
        a00 * V[0] + a01 * V[1] + a02 * V[2],
        a01 * V[0] + a11 * V[1] + a12 * V[2],
        a02 * V[0] + a12 * V[1] + a22 * V[2]
    };

    Real m00 = U[0] * AU[0] + U[1] * AU[1] + U[2] * AU[2] - eval1;
    Real m01 = U[0] * AV[0] + U[1] * AV[1] + U[2] * AV[2];
    Real m11 = V[0] * AV[0] + V[1] * AV[1] + V[2] * AV[2] - eval1;

    // For robustness, choose the largest-length row of M to compute the
    // eigenvector.  The 2-tuple of coefficients of U and V in the
    // assignments to eigenvector[1] lies on a circle, and U and V are
    // unit length and perpendicular, so eigenvector[1] is unit length
    // (within numerical tolerance).
    Real absM00 = std::abs(m00);
    Real absM01 = std::abs(m01);
    Real absM11 = std::abs(m11);
    Real maxAbsComp;
    if (absM00 >= absM11)
    {
        maxAbsComp = std::max(absM00, absM01);
        if (maxAbsComp > (Real)0)
        {
            if (absM00 >= absM01)
            {
                m01 /= m00;
                m00 = (Real)1 / std::sqrt((Real)1 + m01 * m01);
                m01 *= m00;
            }
            else
            {
                m00 /= m01;
                m01 = (Real)1 / std::sqrt((Real)1 + m00 * m00);
                m00 *= m01;
            }
            evec1 = Subtract(Multiply(m01, U), Multiply(m00, V));
        }
        else
        {
            evec1 = U;
        }
    }
    else
    {
        maxAbsComp = std::max(absM11, absM01);
        if (maxAbsComp > (Real)0)
        {
            if (absM11 >= absM01)
            {
                m01 /= m11;
                m11 = (Real)1 / std::sqrt((Real)1 + m01 * m01);
                m01 *= m11;
            }
            else
            {
                m11 /= m01;
                m01 = (Real)1 / std::sqrt((Real)1 + m11 * m11);
                m11 *= m01;
            }
            evec1 = Subtract(Multiply(m11, U), Multiply(m01, V));
        }
        else
        {
            evec1 = U;
        }
    }
}

}


// ----------------------------------------------
// TODO: GteApprOrthogonalLine3.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

// Least-squares fit of a line to (x,y,z) data by using distance measurements
// orthogonal to the proposed line.  The return value is 'true' iff the fit
// is unique (always successful, 'true' when a minimum eigenvalue is unique).
// The mParameters value is a line with (P,D) = (origin,direction).  The
// error for S = (x0,y0,z0) is (S-P)^T*(I - D*D^T)*(S-P).

namespace gte
{

template <typename Real>
class ApprOrthogonalLine3
    :
    public ApprQuery<Real, ApprOrthogonalLine3<Real>, Vector3<Real>>
{
public:
    // Initialize the model parameters to zero.
    ApprOrthogonalLine3();

    // Basic fitting algorithm.
    bool Fit(int numPoints, Vector3<Real> const* points);
    Line3<Real> const& GetParameters() const;

    // Functions called by ApprQuery::RANSAC.  See GteApprQuery.h for a
    // detailed description.
    int GetMinimumRequired() const;
    Real Error(Vector3<Real> const& observation) const;
    bool Fit(std::vector<Vector3<Real>> const& observations,
        std::vector<int> const& indices);

private:
    Line3<Real> mParameters;
};


template <typename Real>
ApprOrthogonalLine3<Real>::ApprOrthogonalLine3()
    :
    mParameters(Vector3<Real>::Zero(), Vector3<Real>::Zero())
{
}

template <typename Real>
bool ApprOrthogonalLine3<Real>::Fit(int numPoints, Vector3<Real> const* points)
{
    if (numPoints >= GetMinimumRequired() && points)
    {
        // Compute the mean of the points.
        Vector3<Real> mean = Vector3<Real>::Zero();
        for (int i = 0; i < numPoints; ++i)
        {
            mean += points[i];
        }
        Real invSize = ((Real)1) / (Real)numPoints;
        mean *= invSize;

        // Compute the covariance matrix of the points.
        Real covar00 = (Real)0, covar01 = (Real)0, covar02 = (Real)0;
        Real covar11 = (Real)0, covar12 = (Real)0, covar22 = (Real)0;
        for (int i = 0; i < numPoints; ++i)
        {
            Vector3<Real> diff = points[i] - mean;
            covar00 += diff[0] * diff[0];
            covar01 += diff[0] * diff[1];
            covar02 += diff[0] * diff[2];
            covar11 += diff[1] * diff[1];
            covar12 += diff[1] * diff[2];
            covar22 += diff[2] * diff[2];
        }
        covar00 *= invSize;
        covar01 *= invSize;
        covar02 *= invSize;
        covar11 *= invSize;
        covar12 *= invSize;
        covar22 *= invSize;

        // Solve the eigensystem.
        SymmetricEigensolver3x3<Real> es;
        std::array<Real, 3> eval;
        std::array<std::array<Real, 3>, 3> evec;
        es(covar00, covar01, covar02, covar11, covar12, covar22, false, +1,
            eval, evec);

        // The line direction is the eigenvector in the direction of largest
        // variance of the points.
        mParameters.origin = mean;
        mParameters.direction = evec[2];

        // The fitted line is unique when the maximum eigenvalue has
        // multiplicity 1.
        return eval[1]< eval[2];
    }

    mParameters = Line3<Real>(Vector3<Real>::Zero(), Vector3<Real>::Zero());
    return false;
}

template <typename Real>
Line3<Real> const& ApprOrthogonalLine3<Real>::GetParameters() const
{
    return mParameters;
}

template <typename Real>
int ApprOrthogonalLine3<Real>::GetMinimumRequired() const
{
    return 2;
}

template <typename Real>
Real ApprOrthogonalLine3<Real>::Error(Vector3<Real> const& observation) const
{
    Vector3<Real> diff = observation - mParameters.origin;
    Real sqrlen = Dot(diff, diff);
    Real dot = Dot(diff, mParameters.direction);
    Real error = std::abs(sqrlen - dot*dot);
    return error;
}

template <typename Real>
bool ApprOrthogonalLine3<Real>::Fit(
    std::vector<Vector3<Real>> const& observations,
    std::vector<int> const& indices)
{
    if (static_cast<int>(indices.size()) >= GetMinimumRequired())
    {
        // Compute the mean of the points.
        Vector3<Real> mean = Vector3<Real>::Zero();
        for (auto index : indices)
        {
            mean += observations[index];
        }
        Real invSize = ((Real)1) / (Real)indices.size();
        mean *= invSize;

        // Compute the covariance matrix of the points.
        Real covar00 = (Real)0, covar01 = (Real)0, covar02 = (Real)0;
        Real covar11 = (Real)0, covar12 = (Real)0, covar22 = (Real)0;
        for (auto index : indices)
        {
            Vector3<Real> diff = observations[index] - mean;
            covar00 += diff[0] * diff[0];
            covar01 += diff[0] * diff[1];
            covar02 += diff[0] * diff[2];
            covar11 += diff[1] * diff[1];
            covar12 += diff[1] * diff[2];
            covar22 += diff[2] * diff[2];
        }
        covar00 *= invSize;
        covar01 *= invSize;
        covar02 *= invSize;
        covar11 *= invSize;
        covar12 *= invSize;
        covar22 *= invSize;

        // Solve the eigensystem.
        SymmetricEigensolver3x3<Real> es;
        std::array<Real, 3> eval;
        std::array<std::array<Real, 3>, 3> evec;
        es(covar00, covar01, covar02, covar11, covar12, covar22, false, +1,
            eval, evec);

        // The line direction is the eigenvector in the direction of largest
        // variance of the points.
        mParameters.origin = mean;
        mParameters.direction = evec[2];

        // The fitted line is unique when the maximum eigenvalue has
        // multiplicity 1.
        return eval[1]< eval[2];
    }

    mParameters = Line3<Real>(Vector3<Real>::Zero(), Vector3<Real>::Zero());
    return false;
}


}



// ----------------------------------------------
// GteDistPointLine.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2018/10/05)

namespace gte
{

template <int N, typename Real>
class DCPQuery<Real, Vector<N, Real>, Line<N, Real>>
{
public:
    struct Result
    {
        Real distance, sqrDistance;
        Real lineParameter;  // t in (-infinity,+infinity)
        Vector<N, Real> lineClosest;  // origin + t * direction
    };

    Result operator()(Vector<N, Real> const& point,
        Line<N, Real> const& line);
};

// Template aliases for convenience.
template <int N, typename Real>
using DCPPointLine =
DCPQuery<Real, Vector<N, Real>, Line<N, Real>>;

template <typename Real>
using DCPPoint2Line2 = DCPPointLine<2, Real>;

template <typename Real>
using DCPPoint3Line3 = DCPPointLine<3, Real>;


template <int N, typename Real>
typename DCPQuery<Real, Vector<N, Real>, Line<N, Real>>::Result
DCPQuery<Real, Vector<N, Real>, Line<N, Real>>::operator()(
    Vector<N, Real> const& point, Line<N, Real> const& line)
{
    Result result;

    Vector<N, Real> diff = point - line.origin;
    result.lineParameter = Dot(line.direction, diff);
    result.lineClosest = line.origin + result.lineParameter*line.direction;

    diff = point - result.lineClosest;
    result.sqrDistance = Dot(diff, diff);
    result.distance = std::sqrt(result.sqrDistance);

    return result;
}


}


// ----------------------------------------------
// GteSegment.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

// The segment is represented by (1-t)*P0 + t*P1, where P0 and P1 are the
// endpoints of the segment and 0 <= t <= 1.  Some algorithms prefer a
// centered representation that is similar to how oriented bounding boxes are
// defined.  This representation is C + s*D, where C = (P0 + P1)/2 is the
// center of the segment, D = (P1 - P0)/|P1 - P0| is a unit-length direction
// vector for the segment, and |t| <= e.  The value e = |P1 - P0|/2 is the
// extent (or radius or half-length) of the segment.

namespace gte
{

template <int N, typename Real>
class Segment
{
public:
    // Construction and destruction.  The default constructor sets p0 to
    // (-1,0,...,0) and p1 to (1,0,...,0).  NOTE:  If you set p0 and p1;
    // compute C, D, and e; and then recompute q0 = C-e*D and q1 = C+e*D,
    // numerical round-off errors can lead to q0 not exactly equal to p0
    // and q1 not exactly equal to p1.
    Segment();
    Segment(Vector<N, Real> const& p0, Vector<N, Real> const& p1);
    Segment(std::array<Vector<N, Real>, 2> const& inP);
    Segment(Vector<N, Real> const& center, Vector<N, Real> const& direction,
        Real extent);

    // Manipulation via the centered form.
    void SetCenteredForm(Vector<N, Real> const& center,
        Vector<N, Real> const& direction, Real extent);

    void GetCenteredForm(Vector<N, Real>& center, Vector<N, Real>& direction,
        Real& extent) const;

    // Public member access.
    std::array<Vector<N, Real>, 2> p;

public:
    // Comparisons to support sorted containers.
    bool operator==(Segment const& segment) const;
    bool operator!=(Segment const& segment) const;
    bool operator< (Segment const& segment) const;
    bool operator<=(Segment const& segment) const;
    bool operator> (Segment const& segment) const;
    bool operator>=(Segment const& segment) const;
};

// Template aliases for convenience.
template <typename Real>
using Segment2 = Segment<2, Real>;

template <typename Real>
using Segment3 = Segment<3, Real>;


template <int N, typename Real>
Segment<N, Real>::Segment()
{
    p[1].MakeUnit(0);
    p[0] = -p[1];
}

template <int N, typename Real>
Segment<N, Real>::Segment(Vector<N, Real> const& p0,
    Vector<N, Real> const& p1)
{
    p[0] = p0;
    p[1] = p1;
}

template <int N, typename Real>
Segment<N, Real>::Segment(std::array<Vector<N, Real>, 2> const& inP)
{
    p = inP;
}

template <int N, typename Real>
Segment<N, Real>::Segment(Vector<N, Real> const& center,
    Vector<N, Real> const& direction, Real extent)
{
    SetCenteredForm(center, direction, extent);
}

template <int N, typename Real>
void Segment<N, Real>::SetCenteredForm(Vector<N, Real> const& center,
    Vector<N, Real> const& direction, Real extent)
{
    p[0] = center - extent * direction;
    p[1] = center + extent * direction;
}

template <int N, typename Real>
void Segment<N, Real>::GetCenteredForm(Vector<N, Real>& center,
    Vector<N, Real>& direction, Real& extent) const
{
    center = ((Real)0.5)*(p[0] + p[1]);
    direction = p[1] - p[0];
    extent = ((Real)0.5)*Normalize(direction);
}

template <int N, typename Real>
bool Segment<N, Real>::operator==(Segment const& segment) const
{
    return p == segment.p;
}

template <int N, typename Real>
bool Segment<N, Real>::operator!=(Segment const& segment) const
{
    return !operator==(segment);
}

template <int N, typename Real>
bool Segment<N, Real>::operator<(Segment const& segment) const
{
    return p < segment.p;
}

template <int N, typename Real>
bool Segment<N, Real>::operator<=(Segment const& segment) const
{
    return operator<(segment) || operator==(segment);
}

template <int N, typename Real>
bool Segment<N, Real>::operator>(Segment const& segment) const
{
    return !operator<=(segment);
}

template <int N, typename Real>
bool Segment<N, Real>::operator>=(Segment const& segment) const
{
    return !operator<(segment);
}


}


// ----------------------------------------------
// GteDistPointSegment.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2018/10/05)

namespace gte
{

template <int N, typename Real>
class DCPQuery<Real, Vector<N, Real>, Segment<N, Real>>
{
public:
    struct Result
    {
        Real distance, sqrDistance;
        Real segmentParameter;  // t in [0,1]
        Vector<N, Real> segmentClosest;  // (1-t)*p[0] + t*p[1]
    };

    Result operator()(Vector<N, Real> const& point,
        Segment<N, Real> const& segment);
};

// Template aliases for convenience.
template <int N, typename Real>
using DCPPointSegment =
DCPQuery<Real, Vector<N, Real>, Segment<N, Real>>;

template <typename Real>
using DCPPoint2Segment2 = DCPPointSegment<2, Real>;

template <typename Real>
using DCPPoint3Segment3 = DCPPointSegment<3, Real>;


template <int N, typename Real>
typename DCPQuery<Real, Vector<N, Real>, Segment<N, Real>>::Result
DCPQuery<Real, Vector<N, Real>, Segment<N, Real>>::operator()(
    Vector<N, Real> const& point, Segment<N, Real> const& segment)
{
    Result result;

    // The direction vector is not unit length.  The normalization is deferred
    // until it is needed.
    Vector<N, Real> direction = segment.p[1] - segment.p[0];
    Vector<N, Real> diff = point - segment.p[1];
    Real t = Dot(direction, diff);
    if (t >= (Real)0)
    {
        result.segmentParameter = (Real)1;
        result.segmentClosest = segment.p[1];
    }
    else
    {
        diff = point - segment.p[0];
        t = Dot(direction, diff);
        if (t <= (Real)0)
        {
            result.segmentParameter = (Real)0;
            result.segmentClosest = segment.p[0];
        }
        else
        {
            Real sqrLength = Dot(direction, direction);
            if (sqrLength > (Real)0)
            {
                t /= sqrLength;
                result.segmentParameter = t;
                result.segmentClosest = segment.p[0] + t * direction;
            }
            else
            {
                result.segmentParameter = (Real)0;
                result.segmentClosest = segment.p[0];
            }
        }
    }

    diff = point - result.segmentClosest;
    result.sqrDistance = Dot(diff, diff);
    result.distance = std::sqrt(result.sqrDistance);

    return result;
}


}


// ----------------------------------------------
// GteCapsule.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

namespace gte
{

// A capsule is the set of points that are equidistant from a segment, the
// common distance called the radius.

template <int N, typename Real>
class Capsule
{
public:
    // Construction and destruction.  The default constructor sets the segment
    // to have endpoints p0 = (-1,0,...,0) and p1 = (1,0,...,0), and the
    // radius is 1.
    Capsule();
    Capsule(Segment<N, Real> const& inSegment, Real inRadius);

    // Public member access.
    Segment<N, Real> segment;
    Real radius;

public:
    // Comparisons to support sorted containers.
    bool operator==(Capsule const& capsule) const;
    bool operator!=(Capsule const& capsule) const;
    bool operator< (Capsule const& capsule) const;
    bool operator<=(Capsule const& capsule) const;
    bool operator> (Capsule const& capsule) const;
    bool operator>=(Capsule const& capsule) const;
};

// Template alias for convenience.
template <typename Real>
using Capsule3 = Capsule<3, Real>;


template <int N, typename Real>
Capsule<N, Real>::Capsule()
    :
    radius((Real)1)
{
}

template <int N, typename Real>
Capsule<N, Real>::Capsule(Segment<N, Real> const& inSegment, Real inRadius)
    :
    segment(inSegment),
    radius(inRadius)
{
}

template <int N, typename Real>
bool Capsule<N, Real>::operator==(Capsule const& capsule) const
{
    return segment == capsule.segment && radius == capsule.radius;
}

template <int N, typename Real>
bool Capsule<N, Real>::operator!=(Capsule const& capsule) const
{
    return !operator==(capsule);
}

template <int N, typename Real>
bool Capsule<N, Real>::operator<(Capsule const& capsule) const
{
    if (segment < capsule.segment)
    {
        return true;
    }

    if (segment > capsule.segment)
    {
        return false;
    }

    return radius < capsule.radius;
}

template <int N, typename Real>
bool Capsule<N, Real>::operator<=(Capsule const& capsule) const
{
    return operator<(capsule) || operator==(capsule);
}

template <int N, typename Real>
bool Capsule<N, Real>::operator>(Capsule const& capsule) const
{
    return !operator<=(capsule);
}

template <int N, typename Real>
bool Capsule<N, Real>::operator>=(Capsule const& capsule) const
{
    return !operator<(capsule);
}


}


// ----------------------------------------------
// GteContCapsule3.h
// ----------------------------------------------

// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2018/10/05)

namespace gte
{

// Compute the axis of the capsule segment using least-squares fitting.  The
// radius is the maximum distance from the points to the axis.  Hemispherical
// caps are chosen as close together as possible.
template <typename Real>
bool GetContainer(int numPoints, Vector3<Real> const* points,
    Capsule3<Real>& capsule);

// Test for containment of a point by a capsule.
template <typename Real>
bool InContainer(Vector3<Real> const& point, Capsule3<Real> const& capsule);

// Test for containment of a sphere by a capsule.
template <typename Real>
bool InContainer(Sphere3<Real> const& sphere, Capsule3<Real> const& capsule);

// Test for containment of a capsule by a capsule.
template <typename Real>
bool InContainer(Capsule3<Real> const& testCapsule,
    Capsule3<Real> const& capsule);

// Compute a capsule that contains the input capsules.  The returned capsule
// is not necessarily the one of smallest volume that contains the inputs.
template <typename Real>
bool MergeContainers(Capsule3<Real> const& capsule0,
     Capsule3<Real> const& capsule1, Capsule3<Real>& merge);


template <typename Real>
bool GetContainer(int numPoints, Vector3<Real> const* points,
    Capsule3<Real>& capsule)
{
    ApprOrthogonalLine3<Real> fitter;
    fitter.Fit(numPoints, points);
    Line3<Real> line = fitter.GetParameters();

    DCPQuery<Real, Vector3<Real>, Line3<Real>> plQuery;
    Real maxRadiusSqr = (Real)0;
    for (int i = 0; i < numPoints; ++i)
    {
        auto result = plQuery(points[i], line);
        if (result.sqrDistance > maxRadiusSqr)
        {
            maxRadiusSqr = result.sqrDistance;
        }
    }

    Vector3<Real> basis[3];
    basis[0] = line.direction;
    ComputeOrthogonalComplement(1, basis);

    Real minValue = std::numeric_limits<Real>::max();
    Real maxValue = -std::numeric_limits<Real>::max();
    for (int i = 0; i < numPoints; ++i)
    {
        Vector3<Real> diff = points[i] - line.origin;
        Real uDotDiff = Dot(diff, basis[1]);
        Real vDotDiff = Dot(diff, basis[2]);
        Real wDotDiff = Dot(diff, basis[0]);
        Real discr = maxRadiusSqr - (uDotDiff*uDotDiff + vDotDiff*vDotDiff);
        Real radical = std::sqrt(std::max(discr, (Real)0));

        Real test = wDotDiff + radical;
        if (test < minValue)
        {
            minValue = test;
        }

        test = wDotDiff - radical;
        if (test > maxValue)
        {
            maxValue = test;
        }
    }

    Vector3<Real> center = line.origin +
        (((Real)0.5)*(minValue + maxValue))*line.direction;

    Real extent;
    if (maxValue > minValue)
    {
        // Container is a capsule.
        extent = ((Real)0.5)*(maxValue - minValue);
    }
    else
    {
        // Container is a sphere.
        extent = (Real)0;
    }

    capsule.segment = Segment3<Real>(center, line.direction, extent);
    capsule.radius = std::sqrt(maxRadiusSqr);
    return true;
}

template <typename Real>
bool InContainer(Vector3<Real> const& point, Capsule3<Real> const& capsule)
{
    DCPQuery<Real, Vector3<Real>, Segment3<Real>> psQuery;
    auto result = psQuery(point, capsule.segment);
    return result.distance <= capsule.radius;
}

template <typename Real>
bool InContainer(Sphere3<Real> const& sphere, Capsule3<Real> const& capsule)
{
    Real rDiff = capsule.radius - sphere.radius;
    if (rDiff >= (Real)0)
    {
        DCPQuery<Real, Vector3<Real>, Segment3<Real>> psQuery;
        auto result = psQuery(sphere.center, capsule.segment);
        return result.distance <= rDiff;
    }
    return false;
}

template <typename Real>
bool InContainer(Capsule3<Real> const& testCapsule,
    Capsule3<Real> const& capsule)
{
    Sphere3<Real> spherePosEnd(testCapsule.segment.p[1], testCapsule.radius);
    Sphere3<Real> sphereNegEnd(testCapsule.segment.p[0], testCapsule.radius);
    return InContainer<Real>(spherePosEnd, capsule)
        && InContainer<Real>(sphereNegEnd, capsule);
}

template <typename Real>
bool MergeContainers(Capsule3<Real> const& capsule0,
    Capsule3<Real> const& capsule1, Capsule3<Real>& merge)
{
    if (InContainer<Real>(capsule0, capsule1))
    {
        merge = capsule1;
        return true;
    }

    if (InContainer<Real>(capsule1, capsule0))
    {
        merge = capsule0;
        return true;
    }

    Vector3<Real> P0, P1, D0, D1;
    Real extent0, extent1;
    capsule0.segment.GetCenteredForm(P0, D0, extent0);
    capsule1.segment.GetCenteredForm(P1, D1, extent1);

    // Axis of final capsule.
    Line3<Real> line;

    // Axis center is average of input axis centers.
    line.origin = ((Real)0.5)*(P0 + P1);

    // Axis unit direction is average of input axis unit directions.
    if (Dot(D0, D1) >= (Real)0)
    {
        line.direction = D0 + D1;
    }
    else
    {
        line.direction = D0 - D1;
    }
    Normalize(line.direction);

    // Cylinder with axis 'line' must contain the spheres centered at the
    // endpoints of the input capsules.
    DCPQuery<Real, Vector3<Real>, Line3<Real>> plQuery;
    Vector3<Real> posEnd0 = capsule0.segment.p[1];
    Real radius = plQuery(posEnd0, line).distance + capsule0.radius;

    Vector3<Real> negEnd0 = capsule0.segment.p[0];
    Real tmp = plQuery(negEnd0, line).distance + capsule0.radius;

    Vector3<Real> posEnd1 = capsule1.segment.p[1];
    tmp = plQuery(posEnd1, line).distance + capsule1.radius;
    if (tmp > radius)
    {
        radius = tmp;
    }

    Vector3<Real> negEnd1 = capsule1.segment.p[0];
    tmp = plQuery(negEnd1, line).distance + capsule1.radius;
    if (tmp > radius)
    {
        radius = tmp;
    }

    // Process sphere <posEnd0,r0>.
    Real rDiff = radius - capsule0.radius;
    Real rDiffSqr = rDiff*rDiff;
    Vector3<Real> diff = line.origin - posEnd0;
    Real k0 = Dot(diff, diff) - rDiffSqr;
    Real k1 = Dot(diff, line.direction);
    Real discr = k1*k1 - k0;  // assert:  k1*k1-k0 >= 0, guard against anyway
    Real root = std::sqrt(std::max(discr, (Real)0));
    Real tPos = -k1 - root;
    Real tNeg = -k1 + root;

    // Process sphere <negEnd0,r0>.
    diff = line.origin - negEnd0;
    k0 = Dot(diff, diff) - rDiffSqr;
    k1 = Dot(diff, line.direction);
    discr = k1*k1 - k0;  // assert:  k1*k1-k0 >= 0, guard against anyway
    root = std::sqrt(std::max(discr, (Real)0));
    tmp = -k1 - root;
    if (tmp > tPos)
    {
        tPos = tmp;
    }
    tmp = -k1 + root;
    if (tmp < tNeg)
    {
        tNeg = tmp;
    }

    // Process sphere <posEnd1,r1>.
    rDiff = radius - capsule1.radius;
    rDiffSqr = rDiff*rDiff;
    diff = line.origin - posEnd1;
    k0 = Dot(diff, diff) - rDiffSqr;
    k1 = Dot(diff, line.direction);
    discr = k1*k1 - k0;  // assert:  k1*k1-k0 >= 0, guard against anyway
    root = std::sqrt(std::max(discr, (Real)0));
    tmp = -k1 - root;
    if (tmp > tPos)
    {
        tPos = tmp;
    }
    tmp = -k1 + root;
    if (tmp < tNeg)
    {
        tNeg = tmp;
    }

    // Process sphere <negEnd1,r1>.
    diff = line.origin - negEnd1;
    k0 = Dot(diff, diff) - rDiffSqr;
    k1 = Dot(diff, line.direction);
    discr = k1*k1 - k0;  // assert:  k1*k1-k0 >= 0, guard against anyway
    root = std::sqrt(std::max(discr, (Real)0));
    tmp = -k1 - root;
    if (tmp > tPos)
    {
        tPos = tmp;
    }
    tmp = -k1 + root;
    if (tmp < tNeg)
    {
        tNeg = tmp;
    }

    Vector3<Real> center = line.origin +
        ((Real)0.5)*(tPos + tNeg)*line.direction;

    Real extent;
    if (tPos > tNeg)
    {
        // Container is a capsule.
        extent = ((Real)0.5)*(tPos - tNeg);
    }
    else
    {
        // Container is a sphere.
        extent = (Real)0;
    }

    merge.segment = Segment3<Real>(center, line.direction, extent);
    merge.radius = radius;
    return true;
}


}



// ----------------------------------------------
// GteUtil.h
// ----------------------------------------------

template<typename Real>
gte::Vector3<Real> Convert(const FVector& Vec)
{
	return gte::Vector3<Real>({ Vec.X, Vec.Y, Vec.Z });
}

template<typename Real>
FVector Convert(const gte::Vector3<Real>& Vec)
{
	return FVector(Vec[0], Vec[1], Vec[2]);
}
