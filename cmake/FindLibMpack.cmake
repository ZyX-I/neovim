# - Try to find libmpack
# Once done this will define
#  LIBMPACK_FOUND - System has libmpack
#  LIBMPACK_INCLUDE_DIRS - The libmpack include directories
#  LIBMPACK_LIBRARIES - The libraries needed to use libmpack

if(NOT USE_BUNDLED_LIBMPACK)
  find_package(PkgConfig)
  if (PKG_CONFIG_FOUND)
    pkg_check_modules(PC_LIBMPACK QUIET mpack)
  endif()
else()
  set(PC_LIBMPACK_INCLUDEDIR)
  set(PC_LIBMPACK_INCLUDE_DIRS)
  set(PC_LIBMPACK_LIBDIR)
  set(PC_LIBMPACK_LIBRARY_DIRS)
  set(LIMIT_SEARCH NO_DEFAULT_PATH)
endif()

set(LIBMPACK_DEFINITIONS ${PC_LIBMPACK_CFLAGS_OTHER})

find_path(
  LIBMPACK_INCLUDE_DIR mpack.h
  PATHS ${PC_LIBMPACK_INCLUDEDIR} ${PC_LIBMPACK_INCLUDE_DIRS}
        ${LIMIT_SEARCH}
)

# If we're asked to use static linkage, add libmpack.a as a preferred library
# name.
if(LIBMPACK_USE_STATIC)
  list(APPEND LIBMPACK_NAMES
       "${CMAKE_STATIC_LIBRARY_PREFIX}mpack${CMAKE_STATIC_LIBRARY_SUFFIX}")
endif()

list(APPEND LIBMPACK_NAMES mpack)

find_library(LIBMPACK_LIBRARY NAMES ${LIBMPACK_NAMES}
  HINTS ${PC_LIBMPACK_LIBDIR} ${PC_LIBMPACK_LIBRARY_DIRS}
  ${LIMIT_SEARCH})

set(LIBMPACK_LIBRARIES ${LIBMPACK_LIBRARY})
set(LIBMPACK_INCLUDE_DIRS ${LIBMPACK_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBMPACK_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(
  LibMpack DEFAULT_MSG LIBMPACK_LIBRARY LIBMPACK_INCLUDE_DIR)

mark_as_advanced(LIBMPACK_INCLUDE_DIR LIBMPACK_LIBRARY)
