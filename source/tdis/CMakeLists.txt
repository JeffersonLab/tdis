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
        GIT_TAG        v2.5.0
)
FetchContent_MakeAvailable(CLI11)


FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.15.3
)
FetchContent_MakeAvailable(spdlog)

# --------- GENERATE PODIO --------------
find_package(podio REQUIRED)
# PODIO_GENERATE_DATAMODEL(podio_model "layout.yaml" DATAMODEL_HEADERS DATAMODEL_SOURCES OUTPUT_FOLDER "")

PODIO_GENERATE_DATAMODEL(podio_model "edm.yaml" DATAMODEL_HEADERS DATAMODEL_SOURCES
        UPSTREAM_EDM acts:${CMAKE_CURRENT_SOURCE_DIR}/acts_edm.yml
        OUTPUT_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/podio_model"
)

PODIO_ADD_DATAMODEL_CORE_LIB(podio_model_lib "${DATAMODEL_HEADERS}" "${DATAMODEL_SOURCES}")
PODIO_ADD_ROOT_IO_DICT(podio_model_dict podio_model_lib "${DATAMODEL_HEADERS}" "podio_model/src/selection.xml")

target_include_directories(podio_model_lib PRIVATE "podio_model")
target_include_directories(podio_model_dict PRIVATE "podio_model")

# --------- CREATE TDIS EXECUTABLE ----------------
add_executable(tdis
        main.cpp
        services/LogService.hpp
        PadGeometryHelper.hpp

        io/DigitizedDataEventSource.hpp
        io/PodioWriteProcessor.hpp
        io/CsvWriteProcessor.hpp

        tracking/ActsGeometryService.cc
        tracking/ActsGeometryService.h
        tracking/ReconstructedHitFactory.h

        tracking/BuildCylindricalDetector.h
        tracking/BuildCylindricalDetector.cpp

        tracking/MtpcDetectorElement.cpp
        tracking/MtpcDetectorElement.hpp

        tracking/TruthTracksHitsSeedsFactory.h
        tracking/TruthTracksHitsSeedsFactory.cpp

        tracking/KalmanFittingFactory.h
        tracking/KalmanFittingFactory.cpp
        tracking/PodioSurfaceIdConverter.h
)

# ---------- FIND REQUIRED PACKAGES -------------
find_package(JANA REQUIRED)
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(Boost REQUIRED)
find_package(ROOT COMPONENTS Core RIO Hist Graf Gpad Geom Tree Postscript Matrix Physics MathCore)
find_package(Acts)
# Get all variables
GET_CMAKE_PROPERTY(vars VARIABLES)

# Filter by prefix and build the "res" list
FOREACH(var ${vars})
    STRING(REGEX MATCH "^(ACTS|acts|Acts)" item ${var})
    IF(item)
        message(VERBOSE "ACTS params: ${var}=${${var}}")
    ENDIF(item)
ENDFOREACH(var)
find_package(Acts REQUIRED COMPONENTS Core PluginPodio)


# Filter by prefix and build the "ROOT" list
FOREACH(var ${vars})
    STRING(REGEX MATCH "^(ROOT|root)" item ${var})
    IF(item)
        message(VERBOSE "ROOT params: ${var}=${${var}}")
    ENDIF(item)
ENDFOREACH(var)
find_package(Acts REQUIRED COMPONENTS Core PluginPodio)

# ----------- Configure ACTS ExamplesLibrary --------
# ExamplesLibrary actually creates ACTS event model
# Get ActsExamples base
get_target_property(ActsCore_LOCATION ActsCore LOCATION)
get_filename_component(ActsCore_PATH ${ActsCore_LOCATION} DIRECTORY)
set(ActsExamples_LIB ${ActsCore_PATH}/${CMAKE_SHARED_LIBRARY_PREFIX}ActsExamplesFramework${CMAKE_SHARED_LIBRARY_SUFFIX})

# If acts is installed to / we are good but if acts is installed in some directory with structure /bin /include /lib
# we need to get this directory and try to include include
get_filename_component(Acts_HOME "${ActsCore_PATH}" DIRECTORY)
set(Acts_HOME_INCLUDE "${Acts_HOME}/include")

# List all ACTS variables
message(STATUS "ACTS List all variables  : ${ActsExample_LIB}")
get_cmake_property(_variableNames VARIABLES)
foreach (_variableName ${_variableNames})
    if (_variableName MATCHES "^Acts")
        message(STATUS "   ${_variableName} = ${${_variableName}}")
    endif()
endforeach()

# Add examples library and includes

# Check if the directory exists and is accessible on the disk
if(EXISTS "${Acts_HOME_INCLUDE}")
    # Add the directory to the target's include directories only if it exists
    target_include_directories(tdis PUBLIC SYSTEM "${Acts_HOME_INCLUDE}")
    message(STATUS "Added include directory: ${Acts_HOME_INCLUDE}")
else()
    message(STATUS "Directory does not exist or is not accessible: ${Acts_HOME_INCLUDE}. It is OK if ACTS is installed in /")
endif()

target_include_directories(tdis PUBLIC ${CMAKE_CURRENT_LIST_DIR} "${CMAKE_CURRENT_LIST_DIR}/..")
target_link_libraries(tdis
        ${JANA_LIB}
        stdc++fs
        ROOT::RIO ROOT::Core ROOT::Geom
        podio::podio podio::podioRootIO podio_model_lib podio_model_dict
        spdlog::spdlog
        fmt::fmt
        CLI11::CLI11
        Boost::boost
        ActsCore
        ActsPluginJson
        ActsPluginPodio
        ${ActsExamples_LIB}
        #/container/app/acts/acts-v37.4.0/lib/libActsExamplesFramework.so
        # ${ActsCore_PATH}/libActsExamplesFramework.so
)

set_target_properties(tdis PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
target_include_directories(tdis PRIVATE "podio_model")
target_include_directories(tdis SYSTEM PUBLIC ${JANA_INCLUDE_DIR} ${ROOT_INCLUDE_DIRS})
target_include_directories(tdis PUBLIC ${CMAKE_CURRENT_LIST_DIR} "${CMAKE_CURRENT_LIST_DIR}/..")

# ----------- install destination -------------------
install(TARGETS tdis DESTINATION bin)
install(TARGETS podio_model_lib DESTINATION lib)
install(TARGETS podio_model_dict DESTINATION lib)


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
