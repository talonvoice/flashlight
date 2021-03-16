/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <mutex>

#if FL_LIBRARIES_USE_ACCELERATE
#include <Accelerate/Accelerate.h>
#elif FL_LIBRARIES_USE_IPP
#include <ipp.h>
#else
#error "No FFT library selected"
#endif

#include "flashlight/lib/audio/feature/Dither.h"
#include "flashlight/lib/audio/feature/FeatureParams.h"
#include "flashlight/lib/audio/feature/PreEmphasis.h"
#include "flashlight/lib/audio/feature/Windowing.h"

namespace fl {
namespace lib {
namespace audio {

// Computes Power Spectrum features for a speech signal.

class PowerSpectrum {
 public:
  explicit PowerSpectrum(const FeatureParams& params);

  virtual ~PowerSpectrum();

  // input - input speech signal (T)
  // Returns - Power spectrum (Col Major : FEAT X FRAMESZ)
  virtual std::vector<float> apply(const std::vector<float>& input);

  // input - input speech signal (Col Major : T X BATCHSZ)
  // Returns - Output features (Col Major : FEAT X FRAMESZ X BATCHSZ)
  std::vector<float> batchApply(const std::vector<float>& input, int batchSz);

  virtual int outputSize(int inputSz);

  FeatureParams getFeatureParams() const;

 protected:
  FeatureParams featParams_;

  // Helper function which takes input as signal after dividing the signal into
  // frames. Main purpose of this function is to reuse it in MFSC, MFCC code
  std::vector<float> powSpectrumImpl(std::vector<float>& frames);

  void validatePowSpecParams() const;

 private:
  // The following classes are defined in the order they are applied
  Dither dither_;
  PreEmphasis preEmphasis_;
  Windowing windowing_;

  std::vector<float> inFftBuf_, outFftBuf_;
  std::mutex fftMutex_;
#if FL_LIBRARIES_USE_ACCELERATE
  vDSP_DFT_Setup fftSetup_;
  DSPSplitComplex splitData_;
#elif FL_LIBRARIES_USE_IPP
  Ipp8u *memBuffer_;
  Ipp8u *memSpec_;
  IppsFFTSpec_R_32f *fftSpec_;
  Ipp32f *outPerm_;
  Ipp32fc *outComplex_;
#endif
};
} // namespace audio
} // namespace lib
} // namespace fl
