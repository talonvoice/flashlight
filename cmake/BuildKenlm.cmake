cmake_minimum_required(VERSION 3.10.0)

include(ExternalProject)

set(kenlm_TEMP_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/extern/kenlm)
set(kenlm_URL https://github.com/talonvoice/kenlm.git)
set(kenlm_BUILD ${CMAKE_CURRENT_BINARY_DIR}/kenlm)
set(kenlm_TAG 83b4fcf26ed333f3fd25b669662e2f8bf4a2d1c2)

if (NOT TARGET kenlm)
  # Download kenlm
  ExternalProject_Add(
    kenlm
    PREFIX kenlm
    GIT_REPOSITORY ${kenlm_URL}
    GIT_TAG ${kenlm_TAG}
    BUILD_IN_SOURCE 1
    BUILD_COMMAND ${CMAKE_COMMAND} --build .
    CMAKE_CACHE_ARGS
      -DNO_COMPRESSION=ON
      -DFORCE_STATIC=ON
      -DBUILD_TESTING=OFF
      -DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}
      -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
      -DCMAKE_INSTALL_PREFIX:PATH=${kenlm_TEMP_INSTALL_DIR}
  )
endif ()

# Install the install executed at build time
install(DIRECTORY ${kenlm_TEMP_INSTALL_DIR}/include DESTINATION ${CMAKE_INSTALL_PREFIX})
install(DIRECTORY ${kenlm_TEMP_INSTALL_DIR}/lib DESTINATION ${CMAKE_INSTALL_PREFIX})
install(DIRECTORY ${kenlm_TEMP_INSTALL_DIR}/bin DESTINATION ${CMAKE_INSTALL_PREFIX})

if (BUILD_SHARED_LIBS)
  set(LIB_TYPE SHARED)
else()
  set(LIB_TYPE STATIC)
endif()

# Library and include dirs
set(KENLM_LIBRARIES
  "${kenlm_TEMP_INSTALL_DIR}/lib/${CMAKE_${LIB_TYPE}_LIBRARY_PREFIX}kenlm${CMAKE_${LIB_TYPE}_LIBRARY_SUFFIX}"
  "${kenlm_TEMP_INSTALL_DIR}/lib/${CMAKE_${LIB_TYPE}_LIBRARY_PREFIX}kenlm_util${CMAKE_${LIB_TYPE}_LIBRARY_SUFFIX}"
)
set(KENLM_INCLUDE_DIRS "${kenlm_TEMP_INSTALL_DIR}/include")
set(KENLM_INCLUDE_DIRS_LM "${kenlm_TEMP_INSTALL_DIR}/include/kenlm")
