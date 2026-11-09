include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(FIAISDK_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/FiaiSDK)

install(TARGETS fiaisdk
    EXPORT FiaiSDKTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/fiaisdk
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)
install(FILES ${FIAISDK_GENERATED_INCLUDE_DIR}/fiaisdk/version.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fiaisdk
)

install(EXPORT FiaiSDKTargets
    FILE FiaiSDKTargets.cmake
    NAMESPACE FiaiSDK::
    DESTINATION ${FIAISDK_INSTALL_CMAKEDIR}
)

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/FiaiSDKConfig.cmake.in
    ${PROJECT_BINARY_DIR}/FiaiSDKConfig.cmake
    INSTALL_DESTINATION ${FIAISDK_INSTALL_CMAKEDIR}
)

# Pre-1.0: a minor bump may break compatibility, so only the same MAJOR.MINOR matches.
if(PROJECT_VERSION_MAJOR EQUAL 0)
    set(_fiaisdk_compat SameMinorVersion)
else()
    set(_fiaisdk_compat SameMajorVersion)
endif()
write_basic_package_version_file(
    ${PROJECT_BINARY_DIR}/FiaiSDKConfigVersion.cmake
    COMPATIBILITY ${_fiaisdk_compat}
)

install(FILES
    ${PROJECT_BINARY_DIR}/FiaiSDKConfig.cmake
    ${PROJECT_BINARY_DIR}/FiaiSDKConfigVersion.cmake
    DESTINATION ${FIAISDK_INSTALL_CMAKEDIR}
)
