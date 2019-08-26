#pragma once

#include <cfloat> /* DBL_MAX, FLT_MAX */
#include <Gamma/gen.h>
#include <Gamma/ipl.h>
#include <Gamma/scl.h>
#include <Gamma/Domain.h>
#include <Gamma/Strategy.h>

#include "OscillatorBank.hpp"

/// Exponential envelope segment for smoothing out value changes.

/// \ingroup Envelope Interpolation
template <class T=gam::real, class Td=gam::DomainObserver>
class SegExpBypass : public Td{
public:

    /// \param[in] len		Length of segment in domain units
    /// \param[in] crv		Curvature of segment
    /// \param[in] start	Start value
    /// \param[in] end		End value
    SegExpBypass(T len, T crv=-3, T start=1, T end=0):
            mLen(len), mCrv(crv), mVal1(start), mVal0(end)
    {
      onDomainChange(1);
    }

    /// Returns whether envelope is done
    bool done() const { return mCurve.value() >= T(1); }

    /// Jumps to the end of the curve
    void finish() { mCurve.value(T(1)); }

    /// Generate next value
    T operator()(){
      if(done()) return mVal0;
      return gam::ipl::linear(gam::scl::min(mCurve(), T(1)), mVal1, mVal0);
    }

    /// Set new end value.  Start value is set to current value.
    void operator= (T v){
      mVal1 = gam::ipl::linear(gam::scl::min(mCurve.value(), T(1)), mVal1, mVal0);
      mVal0 = v;
      mCurve.reset();
    }

    T getEnd() { return mVal0; }

    /// Set curvature.  Negative gives faster change, positive gives slower change.
    void curve(T v){ set(mLen, v); }

    /// Set length in domain units.
    void period(T v){ set(v, mCrv); }

    void reset(){ mCurve.reset(); }

    /// Set length and curvature
    void set(T len, T crv){
      mLen = len; mCrv = crv;
      mCurve.set(len * Td::spu(), crv);
    }

    void onDomainChange(double r){ set(mLen, mCrv); }

protected:
    T mLen, mCrv, mVal1, mVal0;
    gam::Curve<T,T> mCurve;
};

