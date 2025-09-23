// Copyright 2023, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
// Created by Dmitry Romanov, 2024

#include <JANA/Components/JOmniFactoryGeneratorT.h>
#include <JANA/JApplication.h>
#include <JANA/Services/JParameterManager.h>

#include <utility>

#include "CLI/CLI.hpp"
#include "io/DigitizedDataEventSource.hpp"
#include "io/PodioWriteProcessor.hpp"
#include "services/LogService.hpp"
#include "tracking/ActsGeometryService.h"
#include "tracking/KalmanFittingFactory.h"
#include "tracking/TruthTrackSeedFactory.h"


struct ProgramArguments {
    std::map<std::string, std::string> params;
    std::vector<std::string> filePaths;
};

static inline ProgramArguments parseArguments(int argc, char** argv) {
    CLI::App app{"tdis MTPC Acts tracking analysis"};

    bool showHelp = false;
    bool showVersion = false;

    app.add_flag("--version,-v", showVersion, "Show version information");

    // Define parameters starting with -p or -P (case-insensitive)
    std::vector<std::string> params;
    app.allow_extras();  // Allow unrecognized options

    // Collect file paths (positional arguments)
    std::vector<std::string> filePaths;
    app.add_option("files", filePaths, "Input files");

    try {
        app.parse(argc, argv);
    } catch(const CLI::ParseError &e) {
        exit(app.exit(e));
    }


    if (showHelp) {
        std::cout << app.help() << std::endl;
        exit(0);
    }
    if (showVersion) {
        std::cout << "Version 1.0" << std::endl;
        exit(0);
    }

    // Process extra arguments to handle -p* options
    std::map<std::string, std::string> paramMap;
    auto extras = app.remaining();
    for (size_t i = 0; i < extras.size(); ++i) {
        std::string arg = extras[i];
        std::string key, value;

        // Check if argument starts with -p or -P
        if (arg.size() >= 2 && (arg[0] == '-' || arg[0] == '/') &&
            (arg[1] == 'p' || arg[1] == 'P')) {

            // Remove the prefix
            arg = arg.substr(2);

            // Handle -pParam=value
            auto equalPos = arg.find('=');
            if (equalPos != std::string::npos) {
                key = arg.substr(0, equalPos);
                value = arg.substr(equalPos + 1);
            } else if ((i + 1) < extras.size() && extras[i + 1][0] != '-') {
                // Handle -pParam value
                key = arg;
                value = extras[++i];
            } else {
                key = arg;
                value = "";
            }

            // Case-insensitive keys
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            paramMap[key] = value;
        } else if (arg[0] != '-') {
            // Assume positional argument (file path)
            filePaths.push_back(arg);
        }
    }

    return ProgramArguments{paramMap, filePaths};
}

int main(int argc, char* argv[]) {

    auto parsedArgs = parseArguments(argc, argv);

    // Initiate parameter manager based on program arguments
    auto parameterManager = new JParameterManager();
    for (const auto& [name, value] : parsedArgs.params)  {
        parameterManager->SetParameter(name, value);
    }

    // JANA2 Application coordinates and runs everything
    JApplication app(parameterManager);

    // Register services:
    app.ProvideService(std::make_shared<tdis::services::LogService>(&app));
    app.ProvideService(std::make_shared<tdis::tracking::ActsGeometryService>());

    // auto recoHitGenerator = new JOmniFactoryGeneratorT<tdis::tracking::ReconstructedHitFactory>();
    // recoHitGenerator->AddWiring(
    //     "TrackerHitGenerator",
    //     {"DigitizedMtpcMcHit"},
    //     {"TrackerHit", "Measurement2D"});
    // app.Add(recoHitGenerator);

    auto truthTrackInitGenerator = new JOmniFactoryGeneratorT<tdis::tracking::TruthTrackSeedFactory>();
    truthTrackInitGenerator->AddWiring(
        "TruthTrackParameterGenerator",
        {"DigitizedMtpcMcTrack"},
        {
            "TruthTrackSeeds",
            "TruthTrackParameters",
            "TrackerHits",
            "Measurements2D",
        });
    app.Add(truthTrackInitGenerator);

    auto kalmanFitterGenerator = new JOmniFactoryGeneratorT<tdis::tracking::KalmanFittingFactory>();
    kalmanFitterGenerator->AddWiring(
        "KalmanFitterGenerator",
        {"DigitizedMtpcMcTrack", "DigitizedMtpcMcHit", "TrackerHit", "Measurement2D"},
        {"FittedTrajectories", "FittedTrackParams", "FittedTracks"});
    app.Add(kalmanFitterGenerator);



    // auto measurement_2d_generator = new JOmniFactoryGeneratorT<tdis::tracking::Measurement2DFactory>();
    // measurement_2d_generator->AddWiring("TrackerHitGenerator", {"TrackerHit"}, {"Measurement2D"});
    // app.Add(measurement_2d_generator);


    app.Add(new JEventSourceGeneratorT<tdis::io::DigitizedDataEventSource>);
    app.Add(new tdis::io::PodioWriteProcessor(&app));

    // app.Add(new JEventProcessorPodio);
    // app.Add(new JFactoryGeneratorT<ExampleClusterFactory>());
    // app.Add(new JFactoryGeneratorT<ExampleMultifactory>());
    // app.Add(new PodioExampleSource("hits.root"));

    // Add source files (do we have them at all?)
    if(parsedArgs.filePaths.empty()) {
        std::cerr << "No input files specified" << std::endl;
        return 1;
    }

    for(auto& filePath : parsedArgs.filePaths) {
        app.Add(filePath);
    }

    app.Initialize();

    app.Run();

    //verify_clusters_file();
    return 0;
}
