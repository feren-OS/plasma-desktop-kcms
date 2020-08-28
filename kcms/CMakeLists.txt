remove_definitions(-DQT_NO_CAST_FROM_ASCII -DQT_STRICT_ITERATORS -DQT_NO_CAST_FROM_BYTEARRAY -DQT_NO_KEYWORDS)

find_package(Freetype)
set_package_properties(Freetype PROPERTIES DESCRIPTION "A font rendering engine"
                       URL "https://www.freetype.org"
                       TYPE OPTIONAL
                       PURPOSE "Needed to build kfontinst, a simple font installer."
                      )


set(libkxftconfig_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/fonts/kxftconfig.cpp )


add_subdirectory( colors )
add_subdirectory( lookandfeel )
add_subdirectory( desktoplayout )
