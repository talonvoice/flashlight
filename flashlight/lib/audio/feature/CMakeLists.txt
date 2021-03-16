cmake_minimum_required(VERSION 3.10)

# ----------------------------- Dependencies -----------------------------
# CBLAS
if (FL_LIBRARIES_USE_ACCELERATE)
  set(CBLAS_LIBRARIES "-framework Accelerate")
  set(CBLAS_INCLUDE_DIR "")
  set(FFT_LIBRARIES "")
  add_definitions(-DFL_LIBRARIES_USE_ACCELERATE)
else()
  if (FL_LIBRARIES_USE_MKL)
    find_package(MKL REQUIRED)
    set(CBLAS_LIBRARIES ${MKL_LIBRARIES})
    set(CBLAS_INCLUDE_DIR ${MKL_INCLUDE_DIR})
    setup_install_find_module(${CMAKE_MODULE_PATH}/FindMKL.cmake)
  else ()
    find_package(CBLAS REQUIRED)
    setup_install_find_module(${CMAKE_MODULE_PATH}/FindCBLAS.cmake)
  endif ()
  message(STATUS "CBLAS found (include: ${CBLAS_INCLUDE_DIR}, library: ${CBLAS_LIBRARIES})")

  find_library(LIBIPPS NAMES libipps.a HINTS /opt/intel/ipp/lib)
  find_library(LIBIPPVM NAMES libippvm.a HINTS /opt/intel/ipp/lib)
  find_library(LIBIPPCORE NAMES libippcore.a HINTS /opt/intel/ipp/lib)
  set(FFT_LIBRARIES ${LIBIPPS} ${LIBIPPCORE} ${LIBIPPVM})
  add_definitions(-DFL_LIBRARIES_USE_IPP)
endif()

# OpenMP
find_package(OpenMP REQUIRED)
find_package(Threads REQUIRED)

# Threads/OpenMP pollutes other compilers (e.g. nvcc) with flags
target_compile_options(fl-libraries PRIVATE $<$<COMPILE_LANGUAGE:C>:${OpenMP_C_FLAGS}>)
target_compile_options(fl-libraries PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${OpenMP_CXX_FLAGS}>)
target_compile_options(fl-libraries PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${CMAKE_THREAD_LIBS_INIT}>)

# ----------------------------- Lib -----------------------------
target_sources(
  fl-libraries
  PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/Ceplifter.cpp
  ${CMAKE_CURRENT_LIST_DIR}/Dct.cpp
  ${CMAKE_CURRENT_LIST_DIR}/Derivatives.cpp
  ${CMAKE_CURRENT_LIST_DIR}/Dither.cpp
  ${CMAKE_CURRENT_LIST_DIR}/Mfcc.cpp
  ${CMAKE_CURRENT_LIST_DIR}/Mfsc.cpp
  ${CMAKE_CURRENT_LIST_DIR}/PowerSpectrum.cpp
  ${CMAKE_CURRENT_LIST_DIR}/PreEmphasis.cpp
  ${CMAKE_CURRENT_LIST_DIR}/SpeechUtils.cpp
  ${CMAKE_CURRENT_LIST_DIR}/TriFilterbank.cpp
  ${CMAKE_CURRENT_LIST_DIR}/Windowing.cpp
  )

if (NOT FL_LIBRARIES_USE_MKL)
  # openmp required by PowerSpectrum: provided by iomp if MKL ON
  # Not using the target OpenMP::OpenMP_CXX: it does add fopenmp to nvcc which does not understand it
  set(OMPLIB ${OpenMP_CXX_LIBRARIES})
endif()

target_link_libraries(
  fl-libraries
  PRIVATE
    ${OMPLIB}
    ${CBLAS_LIBRARIES}
    ${FFT_LIBRARIES}
  )

target_include_directories(
  fl-libraries
  PRIVATE
  ${CBLAS_INCLUDE_DIR}
  )
