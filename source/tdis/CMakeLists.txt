cmake_minimum_required(VERSION 3.24)

project(tdis_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Default to C++ extensions being off. Clang's modules support have trouble
# with extensions right now and it is not required for any other compiler
set(CMAKE_CXX_EXTENSIONS OFF)


# Set the policy to NEW to use the environment variables in uppercase
cmake_policy(SET CMP0144 NEW)


# --------- FETCH DEPENDENCIES --------------
# Fetch CLI11
include(FetchContent)
FetchContent_Declare(
        CLI11
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG        v2.4.2  # Use the latest stable release
)
FetchContent_MakeAvailable(CLI11)


# --------- GENERATE PODIO --------------
find_package(podio REQUIRED)
PODIO_GENERATE_DATAMODEL(podio_model "layout.yaml" DATAMODEL_HEADERS DATAMODEL_SOURCES OUTPUT_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/podio_model")
PODIO_ADD_DATAMODEL_CORE_LIB(podio_model_lib "${DATAMODEL_HEADERS}" "${DATAMODEL_SOURCES}")
PODIO_ADD_ROOT_IO_DICT(podio_model_dict podio_model_lib "${DATAMODEL_HEADERS}" "podio_model/src/selection.xml")

target_include_directories(podio_model_lib PRIVATE "podio_model")
target_include_directories(podio_model_dict PRIVATE "podio_model")

# --------- CREATE TDIS EXECUTABLE ----------------
add_executable(tdis
        tdis_main.cpp
        services/LogService.hpp
        PadGeometryHelper.hpp
        io/DigitizedDataEventSource.hpp
        io/PodioWriteProcessor.hpp
        tracking/ActsGeometryProvider.cc
        tracking/ActsGeometryProvider.h
        tracking/ActsGeometryService.cc
        tracking/ActsGeometryService.h
        tracking/ReconstructedHitFactory.h
        # tracking/DD4hepBField.h
        # /tracking/DD4hepBField.cc
)

# ---------- FIND REQUIRED PACKAGES -------------
find_package(JANA REQUIRED)
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(Boost REQUIRED)
find_package(ROOT COMPONENTS Core RIO Hist Graf Gpad Tree Postscript Matrix Physics MathCore)
find_package(Acts REQUIRED COMPONENTS Core PluginTGeo PluginJson)

target_include_directories(tdis PUBLIC ${CMAKE_CURRENT_LIST_DIR} "${CMAKE_CURRENT_LIST_DIR}/..")
target_link_libraries(tdis
        ${JANA_LIB}
        ROOT::RIO ROOT::Core
        podio::podio podio::podioRootIO podio_model_lib podio_model_dict
        spdlog::spdlog
        fmt::fmt
        CLI11::CLI11
        Boost::boost
        ActsCore ActsPluginTGeo ActsPluginJson
)

set_target_properties(tdis PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
target_include_directories(tdis PRIVATE "podio_model")
target_include_directories(tdis SYSTEM PUBLIC ${JANA_INCLUDE_DIR} ${ROOT_INCLUDE_DIRS})



# ----------- install destination -------------------
install(TARGETS tdis DESTINATION bin)
install(TARGETS podio_model_lib DESTINATION lib)
# install(TARGETS podio_model_dict DESTINATION lib)



if(WITH_TESTS)
    find_package(Catch2 3 QUIET)
    if(Catch2_FOUND)
#        add_executable(tdis_tests
#                tests/SplitDataStringTests.cpp
#                # Add other test files here
#        )
#
#        # Include directories for tests
#        target_include_directories(tdis_tests PRIVATE
#                ${CMAKE_CURRENT_SOURCE_DIR}
#                # Add other include directories if needed
#        )
#
#        # Link the test executable with Catch2 and the code under test
#        target_link_libraries(tdis_tests PRIVATE
#                Catch2::Catch2WithMain
#                #tdis
#                # Add other libraries if needed
#        )
#
#        # Enable CTest
#        include(CTest)
#        include(Catch)
#        catch_discover_tests(tdis_tests)
    else()
        message(WARNING "Catch2 not found, unit tests will not be built.")
    endif()
endif()
