INCLUDE (FindPackageHandleStandardArgs)

FIND_PATH (CRYPTOPP_ROOT_DIR
  NAMES cryptopp/cryptlib.h include/cryptopp/cryptlib.h
  PATHS ENV CRYPTOPPROOT
  DOC "CryptoPP root directory")

# Re-use the previous path:
FIND_PATH (CRYPTOPP_INCLUDE_DIR
  NAMES cryptopp/cryptlib.h
  HINTS ${CRYPTOPP_ROOT_DIR}
  PATH_SUFFIXES include
  DOC "CryptoPP include directory")

FIND_LIBRARY (CRYPTOPP_LIBRARY_DEBUG
  NAMES cryptlibd cryptoppd
  HINTS ${CRYPTOPP_ROOT_DIR}
  PATH_SUFFIXES lib
  DOC "CryptoPP debug library")

FIND_LIBRARY (CRYPTOPP_LIBRARY_RELEASE
  NAMES cryptlib cryptopp
  HINTS ${CRYPTOPP_ROOT_DIR}
  PATH_SUFFIXES lib
  DOC "CryptoPP release library")

IF (CRYPTOPP_LIBRARY_DEBUG AND CRYPTOPP_LIBRARY_RELEASE)
  SET (CRYPTOPP_LIBRARY
    optimized ${CRYPTOPP_LIBRARY_RELEASE}
    debug ${CRYPTOPP_LIBRARY_DEBUG} CACHE DOC "CryptoPP library")
ELSEIF (CRYPTOPP_LIBRARY_RELEASE)
  SET (CRYPTOPP_LIBRARY ${CRYPTOPP_LIBRARY_RELEASE} CACHE DOC
    "CryptoPP library")
ENDIF (CRYPTOPP_LIBRARY_DEBUG AND CRYPTOPP_LIBRARY_RELEASE)

IF (CRYPTOPP_INCLUDE_DIR)
  SET (_CRYPTOPP_VERSION_HEADER ${CRYPTOPP_INCLUDE_DIR}/cryptopp/config.h)

  IF (EXISTS ${_CRYPTOPP_VERSION_HEADER})
    FILE (STRINGS ${_CRYPTOPP_VERSION_HEADER} _CRYPTOPP_VERSION_TMP REGEX
      "^#define CRYPTOPP_VERSION[ \t]+[0-9]+$")

    STRING (REGEX REPLACE
      "^#define CRYPTOPP_VERSION[ \t]+([0-9]+)" "\\1" _CRYPTOPP_VERSION_TMP
      ${_CRYPTOPP_VERSION_TMP})

    STRING (REGEX REPLACE "([0-9]+)[0-9][0-9]" "\\1" CRYPTOPP_VERSION_MAJOR
      ${_CRYPTOPP_VERSION_TMP})
    STRING (REGEX REPLACE "[0-9]([0-9])[0-9]" "\\1" CRYPTOPP_VERSION_MINOR
      ${_CRYPTOPP_VERSION_TMP})
    STRING (REGEX REPLACE "[0-9][0-9]([0-9])" "\\1" CRYPTOPP_VERSION_PATCH
      ${_CRYPTOPP_VERSION_TMP})

    SET (CRYPTOPP_VERSION_COUNT 3)
    SET (CRYPTOPP_VERSION
      ${CRYPTOPP_VERSION_MAJOR}.${CRYPTOPP_VERSION_MINOR}.${CRYPTOPP_VERSION_PATCH})
  ENDIF (EXISTS ${_CRYPTOPP_VERSION_HEADER})
ENDIF (CRYPTOPP_INCLUDE_DIR)

SET (CRYPTOPP_INCLUDE_DIRS ${CRYPTOPP_INCLUDE_DIR})
SET (CRYPTOPP_LIBRARIES ${CRYPTOPP_LIBRARY})

MARK_AS_ADVANCED (CRYPTOPP_INCLUDE_DIR CRYPTOPP_LIBRARY CRYPTOPP_LIBRARY_DEBUG
  CRYPTOPP_LIBRARY_RELEASE)

FIND_PACKAGE_HANDLE_STANDARD_ARGS (CryptoPP REQUIRED_VARS CRYPTOPP_ROOT_DIR
  CRYPTOPP_INCLUDE_DIR CRYPTOPP_LIBRARY VERSION_VAR CRYPTOPP_VERSION)