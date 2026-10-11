# July Bangla Keyboard for Linux: an IBus engine around the shared engine (src/engine).
#
#   cmake -S linux -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-linux && ctest --test-dir build-linux
#   cpack --config build-linux/CPackConfig.cmake        # -> .deb

cmake_minimum_required(VERSION 3.22)

# One version for every platform: VERSION in the root CMakeLists.txt.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../CMakeLists.txt" july_version_line REGEX "^[ \t]*VERSION [0-9]+\\.[0-9]+\\.[0-9]+")
string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" JULY_VERSION "${july_version_line}")

project(JulyBanglaLinux VERSION ${JULY_VERSION} LANGUAGES CXX)

get_filename_component(JULY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Packages install under /usr (IBus reads components from /usr/share/ibus/component).
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Install prefix" FORCE)
endif()
include(GNUInstallDirs)
find_package(PkgConfig REQUIRED)
pkg_check_modules(IBUS REQUIRED IMPORTED_TARGET ibus-1.0)

function(july_set_target_options target)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
endfunction()

# --- Table generators (run on the build machine) and the shared engine ----------------
add_subdirectory("${JULY_ROOT}/tools/layoutgen" layoutgen)

set(gen_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(bijoy_inc "${gen_dir}/july/engine/generated/BijoyLayout.inc")
set(classic_inc "${gen_dir}/july/engine/generated/ClassicTable.inc")
add_custom_command(
    OUTPUT "${bijoy_inc}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${gen_dir}/july/engine/generated"
    COMMAND july_layoutgen "${JULY_ROOT}/layouts/bijoy.layout" "${bijoy_inc}" --strict
    DEPENDS july_layoutgen "${JULY_ROOT}/layouts/bijoy.layout"
    COMMENT "Generating Bijoy layout table" VERBATIM)
add_custom_command(
    OUTPUT "${classic_inc}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${gen_dir}/july/engine/generated"
    COMMAND july_classicgen "${JULY_ROOT}/layouts/sutonnymj.classic" "${classic_inc}" --strict
    DEPENDS july_classicgen "${JULY_ROOT}/layouts/sutonnymj.classic"
    COMMENT "Generating SutonnyMJ classic table" VERBATIM)

file(GLOB engine_sources CONFIGURE_DEPENDS "${JULY_ROOT}/src/engine/src/*.cpp")
add_library(july_engine STATIC ${engine_sources} "${bijoy_inc}" "${classic_inc}")
target_include_directories(july_engine PUBLIC "${JULY_ROOT}/src/engine/include" "${gen_dir}")
july_set_target_options(july_engine)

# --- IBus engine ----------------------------------------------------------------------
set(JULY_LIBEXEC_DIR "${CMAKE_INSTALL_FULL_LIBEXECDIR}/ibus-july-bangla")
add_executable(ibus-engine-july-bangla ibus/Main.cpp ibus/Engine.cpp)
target_link_libraries(ibus-engine-july-bangla PRIVATE july_engine PkgConfig::IBUS)
target_compile_definitions(ibus-engine-july-bangla PRIVATE JULY_VERSION="${PROJECT_VERSION}")
july_set_target_options(ibus-engine-july-bangla)
# GObject type-registration macros trip -Wextra's unused-parameter and cast checks.
target_compile_options(ibus-engine-july-bangla PRIVATE -Wno-unused-parameter -Wno-cast-function-type)

configure_file(ibus/july-bangla.xml.in "${CMAKE_CURRENT_BINARY_DIR}/july-bangla.xml" @ONLY)

install(TARGETS ibus-engine-july-bangla DESTINATION "${JULY_LIBEXEC_DIR}")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/july-bangla.xml" DESTINATION "${CMAKE_INSTALL_DATADIR}/ibus/component")
install(FILES "${JULY_ROOT}/website/assets/logo.png"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/ibus-july-bangla" RENAME july-bangla.png)
install(FILES "${JULY_ROOT}/LICENSE" DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/ibus-july-bangla" RENAME copyright)

# --- Tests: the portable engine and golden tests --------------------------------------
enable_testing()
add_executable(july_unit_tests
    "${JULY_ROOT}/tests/TestMain.cpp"
    "${JULY_ROOT}/tests/unit/ComposerTests.cpp"
    "${JULY_ROOT}/tests/unit/InputModeTests.cpp"
    "${JULY_ROOT}/tests/unit/LayoutTests.cpp"
    "${JULY_ROOT}/tests/unit/RingBufferTests.cpp")
target_include_directories(july_unit_tests PRIVATE "${JULY_ROOT}/tests")
target_compile_definitions(july_unit_tests PRIVATE JULY_GOLDEN_DIR="${JULY_ROOT}/tests/golden")
target_link_libraries(july_unit_tests PRIVATE july_engine)
july_set_target_options(july_unit_tests)
add_test(NAME july_unit_tests COMMAND july_unit_tests)
add_test(NAME ibus_engine_self_test COMMAND ibus-engine-july-bangla --self-test)

# --- Debian package -------------------------------------------------------------------
set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_NAME "ibus-july-bangla")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "July Bangla Keyboard <https://github.com/promahbubul/july_bangla_keyboard/issues>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Bangla keyboard with the Bijoy-compatible layout (IBus)")
set(CPACK_PACKAGE_DESCRIPTION "Type Bangla with the Bijoy-compatible layout and typing rules in any app.
 Three modes: Unicode Bangla, Classic (SutonnyMJ) and English, switched with Ctrl+Alt+B.
 Works offline; collects no data.")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://julykeyboard.vercel.app/")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# A Bangla font comes with the keyboard (most desktops already have one of these).
set(CPACK_DEBIAN_PACKAGE_DEPENDS "ibus, fonts-noto-core | fonts-lohit-beng-bengali | fonts-beng")
set(CPACK_DEBIAN_FILE_NAME "JulyBanglaKeyboard-linux-amd64.deb")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/debian/postinst;${CMAKE_CURRENT_SOURCE_DIR}/debian/postrm")
include(CPack)
