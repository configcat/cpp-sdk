FetchContent_Declare(hash-library
    GIT_REPOSITORY    https://github.com/stbrumme/hash-library.git
    GIT_TAG           hash_library_v8
    PATCH_COMMAND ${GIT_EXECUTABLE} apply ${CMAKE_CURRENT_SOURCE_DIR}/cmake/hash-library-fix-macos.patch
)

FetchContent_GetProperties(hash-library)
if(NOT hash-library_POPULATED)
    FetchContent_Populate(hash-library)

    # Create the include directory structure
    file(MAKE_DIRECTORY ${hash-library_BINARY_DIR}/include/hash-library)
    
    # Copy header files to the new include structure
    file(COPY 
        ${hash-library_SOURCE_DIR}/sha1.h
        ${hash-library_SOURCE_DIR}/sha256.h
        DESTINATION ${hash-library_BINARY_DIR}/include/hash-library
    )

    # Create a static library target for hash-library
    add_library(hash-library STATIC
        ${hash-library_SOURCE_DIR}/sha1.h
        ${hash-library_SOURCE_DIR}/sha256.h
        ${hash-library_SOURCE_DIR}/sha1.cpp
        ${hash-library_SOURCE_DIR}/sha256.cpp
    )

    # Set include directories for hash-library
    target_include_directories(hash-library
        PUBLIC
        $<BUILD_INTERFACE:${hash-library_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
endif()