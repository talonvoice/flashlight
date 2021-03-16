/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "flashlight/lib/audio/feature/PowerSpectrum.h"

#include <algorithm>
#include <cstddef>
#include <unordered_map>

#include "flashlight/lib/audio/feature/SpeechUtils.h"

namespace fl {
namespace lib {
namespace audio {

PowerSpectrum::PowerSpectrum(const FeatureParams& params)
    : featParams_(params),
      dither_(params.ditherVal),
      preEmphasis_(params.preemCoef, params.numFrameSizeSamples()),
      windowing_(params.numFrameSizeSamples(), params.windowType) {
  validatePowSpecParams();
  auto nFft = featParams_.nFft();
  inFftBuf_.resize(nFft, 0.0);
  outFftBuf_.resize(2 * nFft);
#if FL_LIBRARIES_USE_ACCELERATE
  splitData_.realp = new float[nFft];
  splitData_.imagp = new float[nFft];
  fftSetup_ = vDSP_DFT_zrop_CreateSetup(NULL, nFft * 2, vDSP_DFT_FORWARD);
#elif FL_LIBRARIES_USE_IPP
  outPerm_ = ippsMalloc_32f(nFft * 2);
  int order = log2(nFft);
  int flags = IPP_FFT_NODIV_BY_ANY;
  IppHintAlgorithm quality = ippAlgHintNone;
  int sizeSpec, sizeInit, sizeBuffer;
  if (ippsFFTGetSize_R_32f(order, flags, quality, &sizeSpec, &sizeInit, &sizeBuffer) != ippStsNoErr) {
    throw std::runtime_error("ippsFFTGetSize_R_32f failed on order " + std::to_string(order));
  }
  memSpec_ = ippsMalloc_8u(sizeSpec);
  memBuffer_ = ippsMalloc_8u(sizeBuffer);
  Ipp8u *memInit = NULL;
  if (sizeInit > 0 ) {
    memInit = ippsMalloc_8u(sizeInit);
  }
  if (ippsFFTInit_R_32f(&fftSpec_, order, flags, quality, memSpec_, memInit) != ippStsNoErr) {
    throw std::runtime_error("ippsFFTInit_R_32f failed on order " + std::to_string(order));
  }
  ippFree(memInit);
#endif
}

std::vector<float> PowerSpectrum::apply(const std::vector<float>& input) {
  auto frames = frameSignal(input, featParams_);
  if (frames.empty()) {
    return {};
  }
  return powSpectrumImpl(frames);
}

std::vector<float> PowerSpectrum::powSpectrumImpl(std::vector<float>& frames) {
  int nSamples = featParams_.numFrameSizeSamples();
  int nFrames = frames.size() / nSamples;
  int nFft = featParams_.nFft();
  int K = featParams_.filterFreqResponseLen();

  if (featParams_.ditherVal != 0.0) {
    frames = dither_.apply(frames);
  }
  if (featParams_.zeroMeanFrame) {
    for (size_t f = 0; f < nFrames; ++f) {
      auto begin = frames.data() + f * nSamples;
      float mean = std::accumulate(begin, begin + nSamples, 0.0);
      mean /= nSamples;
      std::transform(
          begin, begin + nSamples, begin, [mean](float x) { return x - mean; });
    }
  }
  if (featParams_.preemCoef != 0) {
    preEmphasis_.applyInPlace(frames);
  }
  windowing_.applyInPlace(frames);
  std::vector<float> dft(K * nFrames);
  std::lock_guard<std::mutex> lock(fftMutex_);
  for (size_t f = 0; f < nFrames; ++f) {
    auto begin = frames.data() + f * nSamples;
    std::copy(begin, begin + nSamples, inFftBuf_.data());
#if FL_LIBRARIES_USE_ACCELERATE
    {
      vDSP_ctoz(reinterpret_cast<DSPComplex*>(inFftBuf_.data()), 2, &splitData_, 1, nFft);
      vDSP_DFT_Execute(fftSetup_, splitData_.realp, splitData_.imagp, splitData_.realp, splitData_.imagp);
      vDSP_zvmags(&splitData_, 1, &dft[f * K], 1, K);
    }
#elif FL_LIBRARIES_USE_IPP
    {
      ippsFFTFwd_RToPerm_32f(inFftBuf_.data(), m_outPerm, m_fftSpec, m_memBuffer);
      ippsConjPerm_32fc(m_outPerm, outFftBuf_.data(), nFft);
      ippsMagnitude_32fc(outFftBuf_.data(), &dft[f * K], K);
    }
#endif
  }
  return dft;
}

std::vector<float> PowerSpectrum::batchApply(
    const std::vector<float>& input,
    int batchSz) {
  if (batchSz <= 0) {
    throw std::invalid_argument("PowerSpectrum: negative batchSz");
  } else if (input.size() % batchSz != 0) {
    throw std::invalid_argument(
        "PowerSpectrum: input size is not divisible by batchSz");
  }
  int N = input.size() / batchSz;
  int outputSz = outputSize(N);
  std::vector<float> feat(outputSz * batchSz);

#pragma omp parallel for num_threads(batchSz)
  for (int b = 0; b < batchSz; ++b) {
    auto start = input.begin() + b * N;
    std::vector<float> inputBuf(start, start + N);
    auto curFeat = apply(inputBuf);
    if (outputSz != curFeat.size()) {
      throw std::logic_error("PowerSpectrum: apply() returned wrong size");
    }
    std::copy(
        curFeat.begin(), curFeat.end(), feat.begin() + b * curFeat.size());
  }
  return feat;
}

FeatureParams PowerSpectrum::getFeatureParams() const {
  return featParams_;
}

int PowerSpectrum::outputSize(int inputSz) {
  return featParams_.powSpecFeatSz() * featParams_.numFrames(inputSz);
}

void PowerSpectrum::validatePowSpecParams() const {
  if (featParams_.samplingFreq <= 0) {
    throw std::invalid_argument("PowerSpectrum: samplingFreq is negative");
  } else if (featParams_.frameSizeMs <= 0) {
    throw std::invalid_argument("PowerSpectrum: frameSizeMs is negative");
  } else if (featParams_.frameStrideMs <= 0) {
    throw std::invalid_argument("PowerSpectrum: frameStrideMs is negative");
  } else if (featParams_.numFrameSizeSamples() <= 0) {
    throw std::invalid_argument("PowerSpectrum: frameSizeMs is too low");
  } else if (featParams_.numFrameStrideSamples() <= 0) {
    throw std::invalid_argument("PowerSpectrum: frameStrideMs is too low");
  }
}

PowerSpectrum::~PowerSpectrum() {
#if FL_LIBRARIES_USE_ACCELERATE
  delete[] splitData_.realp;
  delete[] splitData_.imagp;
  vDSP_DFT_DestroySetup(fftSetup_);
#elif FL_LIBRARIES_USE_IPP
  ippFree(memSpec_);
  ippFree(memBuffer_);
  ippFree(outPerm_);
#endif
}
} // namespace audio
} // namespace lib
} // namespace fl
