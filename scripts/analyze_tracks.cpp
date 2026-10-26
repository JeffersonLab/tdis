// analyze_tracks.cpp
// Analyze TDIS track CSV data and create histograms using ROOT
// Compile: g++ -o analyze_tracks analyze_tracks.cpp `root-config --cflags --libs`
// Run: ./analyze_tracks input.csv [output_dir]

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>

#include "TFile.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TCanvas.h"
#include "TStyle.h"
#include "TLegend.h"
#include "TPaveStats.h"
#include "TSystem.h"
#include "TMath.h"

// Structure to hold track data
struct TrackData {
    std::map<std::string, std::vector<double>> columns;
    std::vector<std::string> column_names;
};

// Read CSV file
TrackData readCSV(const std::string& filename, int maxRows = -1) {
    TrackData data;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filename << std::endl;
        return data;
    }
    
    std::string line;
    // Read header
    if (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string col;
        while (std::getline(ss, col, ',')) {
            // Trim whitespace
            col.erase(0, col.find_first_not_of(" \t\r\n"));
            col.erase(col.find_last_not_of(" \t\r\n") + 1);
            data.column_names.push_back(col);
            data.columns[col] = std::vector<double>();
        }
    }
    
    // Read data rows
    int rowCount = 0;
    while (std::getline(file, line) && (maxRows < 0 || rowCount < maxRows)) {
        std::stringstream ss(line);
        std::string value;
        int colIdx = 0;
        
        while (std::getline(ss, value, ',') && colIdx < data.column_names.size()) {
            try {
                double val = std::stod(value);
                data.columns[data.column_names[colIdx]].push_back(val);
            } catch (...) {
                // Handle NaN or invalid values
                data.columns[data.column_names[colIdx]].push_back(NAN);
            }
            colIdx++;
        }
        rowCount++;
    }
    
    file.close();
    std::cout << "Loaded " << rowCount << " tracks with " 
              << data.column_names.size() << " columns" << std::endl;
    
    return data;
}

// Create and fill histograms
std::map<std::string, TH1D*> createHistograms(const TrackData& data, const std::string& outputDir) {
    std::map<std::string, TH1D*> histograms;
    
    // Event and track identification
    histograms["evt"] = new TH1D("evt", "Event Number;Event Number;Counts", 100, 0, 1000);
    histograms["trk_id"] = new TH1D("trk_id", "Track ID;Track ID;Counts", 50, 0, 50);
    
    // MC track properties
    histograms["mc_mom"] = new TH1D("mc_mom", "MC Momentum;MC Momentum [GeV/c];Counts", 100, 0, 10);
    histograms["mc_phi"] = new TH1D("mc_phi", "MC #phi;MC #phi [rad];Counts", 100, -TMath::Pi(), TMath::Pi());
    histograms["mc_theta"] = new TH1D("mc_theta", "MC #theta;MC #theta [rad];Counts", 100, 0, TMath::Pi());
    histograms["mc_vtx_z"] = new TH1D("mc_vtx_z", "MC Vertex Z;MC Vertex Z [mm];Counts", 100, -500, 500);
    histograms["mc_hits_count"] = new TH1D("mc_hits_count", "MC Hits Count;MC Hits Count;Counts", 50, 0, 50);
    
    // Track parameters
    histograms["pdg"] = new TH1D("pdg", "PDG Code;PDG Code;Counts", 100, -3000, 3000);
    histograms["tp_phi"] = new TH1D("tp_phi", "Track Param #phi;Track Param #phi [rad];Counts", 100, -TMath::Pi(), TMath::Pi());
    histograms["tp_theta"] = new TH1D("tp_theta", "Track Param #theta;Track Param #theta [rad];Counts", 100, 0, TMath::Pi());
    histograms["tp_time"] = new TH1D("tp_time", "Track Time;Track Time [ns];Counts", 100, -10, 100);
    histograms["qoverp"] = new TH1D("qoverp", "q/p;q/p [c/GeV];Counts", 100, -2, 2);
    
    // Surface and location
    histograms["surface"] = new TH1D("surface", "Surface ID;Surface ID;Counts", 100, 0, 1e10);
    histograms["loc0"] = new TH1D("loc0", "Local Position 0;Local Position 0 [mm];Counts", 100, -100, 100);
    histograms["loc1"] = new TH1D("loc1", "Local Position 1;Local Position 1 [mm];Counts", 100, -100, 100);
    
    // Covariance matrix elements
    histograms["cov_loc0"] = new TH1D("cov_loc0", "Cov(loc0,loc0);Cov(loc0,loc0) [mm^{2}];Counts", 100, 0, 10);
    histograms["cov_loc1"] = new TH1D("cov_loc1", "Cov(loc1,loc1);Cov(loc1,loc1) [mm^{2}];Counts", 100, 0, 10);
    histograms["cov_phi"] = new TH1D("cov_phi", "Cov(#phi,#phi);Cov(#phi,#phi) [rad^{2}];Counts", 100, 0, 0.1);
    histograms["cov_theta"] = new TH1D("cov_theta", "Cov(#theta,#theta);Cov(#theta,#theta) [rad^{2}];Counts", 100, 0, 0.1);
    histograms["cov_qoverp"] = new TH1D("cov_qoverp", "Cov(q/p,q/p);Cov(q/p,q/p) [(c/GeV)^{2}];Counts", 100, 0, 0.5);
    histograms["cov_time"] = new TH1D("cov_time", "Cov(time,time);Cov(time,time) [ns^{2}];Counts", 100, 0, 1e8);
    
    // Perigee coordinates
    histograms["perigee_x"] = new TH1D("perigee_x", "Perigee X;Perigee X [mm];Counts", 100, -50, 50);
    histograms["perigee_y"] = new TH1D("perigee_y", "Perigee Y;Perigee Y [mm];Counts", 100, -50, 50);
    histograms["perigee_z"] = new TH1D("perigee_z", "Perigee Z;Perigee Z [mm];Counts", 100, -500, 500);
    
    // First hit properties
    histograms["fhit_id"] = new TH1D("fhit_id", "First Hit ID;First Hit ID;Counts", 100, 0, 1000);
    histograms["fhit_time"] = new TH1D("fhit_time", "First Hit Time;First Hit Time [ns];Counts", 100, 0, 100);
    histograms["fhit_plane"] = new TH1D("fhit_plane", "First Hit Plane;First Hit Plane;Counts", 20, 0, 20);
    histograms["fhit_ring"] = new TH1D("fhit_ring", "First Hit Ring;First Hit Ring;Counts", 20, 0, 20);
    histograms["fhit_pad"] = new TH1D("fhit_pad", "First Hit Pad;First Hit Pad;Counts", 100, 0, 200);
    histograms["fhit_ztogem"] = new TH1D("fhit_ztogem", "First Hit Z to GEM;First Hit Z to GEM [mm];Counts", 100, -50, 50);
    histograms["fhit_true_x"] = new TH1D("fhit_true_x", "First Hit True X;First Hit True X [mm];Counts", 100, -500, 500);
    histograms["fhit_true_y"] = new TH1D("fhit_true_y", "First Hit True Y;First Hit True Y [mm];Counts", 100, -500, 500);
    histograms["fhit_true_z"] = new TH1D("fhit_true_z", "First Hit True Z;First Hit True Z [mm];Counts", 100, -2000, 2000);
    
    // Fill histograms
    std::cout << "\nFilling histograms..." << std::endl;
    for (auto& hist_pair : histograms) {
        const std::string& colName = hist_pair.first;
        TH1D* hist = hist_pair.second;
        
        auto it = data.columns.find(colName);
        if (it != data.columns.end()) {
            const std::vector<double>& values = it->second;
            for (double val : values) {
                if (!std::isnan(val)) {
                    hist->Fill(val);
                }
            }
            if (hist->GetEntries() > 0) {
                std::cout << "  Filled " << colName << " with " 
                          << hist->GetEntries() << " entries" << std::endl;
            }
        }
    }
    
    return histograms;
}

// Plot and save histogram
void plotHistogram(TH1D* hist, const std::string& name, const std::string& outputDir) {
    TCanvas* c = new TCanvas(("c_" + name).c_str(), name.c_str(), 800, 600);
    c->SetGrid();
    
    hist->SetLineColor(kBlue);
    hist->SetLineWidth(2);
    hist->SetStats(1);
    hist->Draw("HIST");
    
    // Update statistics box
    c->Update();
    TPaveStats* stats = (TPaveStats*)hist->FindObject("stats");
    if (stats) {
        stats->SetX1NDC(0.65);
        stats->SetX2NDC(0.95);
        stats->SetY1NDC(0.70);
        stats->SetY2NDC(0.95);
    }
    
    std::string outputFile = outputDir + "/" + name + ".png";
    c->SaveAs(outputFile.c_str());
    
    delete c;
}

// Create 2D correlation plot
void plot2DCorrelation(const TrackData& data, const std::string& col1, 
                       const std::string& col2, const std::string& outputDir) {
    auto it1 = data.columns.find(col1);
    auto it2 = data.columns.find(col2);
    
    if (it1 == data.columns.end() || it2 == data.columns.end()) {
        return;
    }
    
    std::string histName = "corr_" + col1 + "_vs_" + col2;
    TH2D* hist = new TH2D(histName.c_str(), 
                          (col2 + " vs " + col1).c_str(),
                          50, -1000, 1000, 50, -1000, 1000);
    
    hist->GetXaxis()->SetTitle(col1.c_str());
    hist->GetYaxis()->SetTitle(col2.c_str());
    
    const std::vector<double>& vals1 = it1->second;
    const std::vector<double>& vals2 = it2->second;
    
    size_t minSize = std::min(vals1.size(), vals2.size());
    for (size_t i = 0; i < minSize; ++i) {
        if (!std::isnan(vals1[i]) && !std::isnan(vals2[i])) {
            hist->Fill(vals1[i], vals2[i]);
        }
    }
    
    if (hist->GetEntries() > 0) {
        TCanvas* c = new TCanvas(("c_" + histName).c_str(), histName.c_str(), 800, 600);
        c->SetGrid();
        hist->Draw("COLZ");
        
        std::string outputFile = outputDir + "/" + histName + ".png";
        c->SaveAs(outputFile.c_str());
        std::cout << "  Saved: " << histName << ".png" << std::endl;
        
        delete c;
    }
    
    delete hist;
}

// Create summary plots
void createSummaryPlots(const TrackData& data, const std::string& outputDir) {
    std::cout << "\nCreating summary plots..." << std::endl;
    
    // Momentum vs angles
    if (data.columns.find("mc_mom") != data.columns.end() &&
        data.columns.find("mc_theta") != data.columns.end() &&
        data.columns.find("mc_phi") != data.columns.end()) {
        
        TCanvas* c = new TCanvas("c_kinematics", "Track Kinematics", 1400, 500);
        c->Divide(2, 1);
        
        // Momentum vs theta
        c->cd(1);
        gPad->SetGrid();
        TH2D* h1 = new TH2D("h_mom_theta", "Momentum vs Theta;MC #theta [rad];MC Momentum [GeV/c]",
                            50, 0, TMath::Pi(), 50, 0, 10);
        const auto& mom = data.columns.at("mc_mom");
        const auto& theta = data.columns.at("mc_theta");
        size_t minSize = std::min(mom.size(), theta.size());
        for (size_t i = 0; i < minSize; ++i) {
            if (!std::isnan(mom[i]) && !std::isnan(theta[i])) {
                h1->Fill(theta[i], mom[i]);
            }
        }
        h1->Draw("COLZ");
        
        // Momentum vs phi
        c->cd(2);
        gPad->SetGrid();
        TH2D* h2 = new TH2D("h_mom_phi", "Momentum vs Phi;MC #phi [rad];MC Momentum [GeV/c]",
                            50, -TMath::Pi(), TMath::Pi(), 50, 0, 10);
        const auto& phi = data.columns.at("mc_phi");
        minSize = std::min(mom.size(), phi.size());
        for (size_t i = 0; i < minSize; ++i) {
            if (!std::isnan(mom[i]) && !std::isnan(phi[i])) {
                h2->Fill(phi[i], mom[i]);
            }
        }
        h2->Draw("COLZ");
        
        std::string outputFile = outputDir + "/summary_kinematics.png";
        c->SaveAs(outputFile.c_str());
        
        delete h1;
        delete h2;
        delete c;
    }
    
    // Perigee XY distribution
    if (data.columns.find("perigee_x") != data.columns.end() &&
        data.columns.find("perigee_y") != data.columns.end()) {
        
        TCanvas* c = new TCanvas("c_perigee", "Perigee Distribution", 800, 800);
        c->SetGrid();
        
        TH2D* h = new TH2D("h_perigee_xy", "Perigee XY Distribution;Perigee X [mm];Perigee Y [mm]",
                           50, -50, 50, 50, -50, 50);
        
        const auto& px = data.columns.at("perigee_x");
        const auto& py = data.columns.at("perigee_y");
        size_t minSize = std::min(px.size(), py.size());
        for (size_t i = 0; i < minSize; ++i) {
            if (!std::isnan(px[i]) && !std::isnan(py[i])) {
                h->Fill(px[i], py[i]);
            }
        }
        h->Draw("COLZ");
        
        std::string outputFile = outputDir + "/summary_perigee_xy.png";
        c->SaveAs(outputFile.c_str());
        
        delete h;
        delete c;
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <csv_file> [output_dir]" << std::endl;
        return 1;
    }
    
    std::string csvFile = argv[1];
    std::string outputDir = (argc > 2) ? argv[2] : "plots_in_tracks";
    
    // Create output directory
    gSystem->mkdir(outputDir.c_str(), true);
    std::cout << "Output directory: " << outputDir << std::endl;
    
    // Set ROOT style
    gStyle->SetOptStat(1111);
    gStyle->SetPalette(kViridis);
    
    // Read CSV data
    std::cout << "Reading CSV file: " << csvFile << std::endl;
    TrackData data = readCSV(csvFile);
    
    if (data.columns.empty()) {
        std::cerr << "Error: No data loaded" << std::endl;
        return 1;
    }
    
    // Create and fill histograms
    std::map<std::string, TH1D*> histograms = createHistograms(data, outputDir);
    
    // Plot all histograms
    std::cout << "\nPlotting histograms..." << std::endl;
    for (auto& hist_pair : histograms) {
        if (hist_pair.second->GetEntries() > 0) {
            plotHistogram(hist_pair.second, hist_pair.first, outputDir);
            std::cout << "  Saved: " << hist_pair.first << ".png" << std::endl;
        }
    }
    
    // Create correlation plots
    std::cout << "\nCreating correlation plots..." << std::endl;
    std::vector<std::pair<std::string, std::string>> correlationPairs = {
        {"mc_mom", "mc_theta"},
        {"mc_mom", "mc_phi"},
        {"mc_phi", "mc_theta"},
        {"perigee_x", "perigee_y"},
        {"perigee_z", "mc_vtx_z"},
        {"tp_phi", "mc_phi"},
        {"tp_theta", "mc_theta"}
    };
    
    for (const auto& pair : correlationPairs) {
        plot2DCorrelation(data, pair.first, pair.second, outputDir);
    }
    
    // Create summary plots
    createSummaryPlots(data, outputDir);
    
    // Print summary statistics
    std::cout << "\nSummary Statistics:" << std::endl;
    if (data.columns.find("mc_mom") != data.columns.end()) {
        const auto& mom = data.columns.at("mc_mom");
        double sum = 0, count = 0;
        for (double val : mom) {
            if (!std::isnan(val)) {
                sum += val;
                count++;
            }
        }
        if (count > 0) {
            std::cout << "  Mean momentum: " << (sum/count) << " GeV/c" << std::endl;
        }
    }
    
    std::cout << "\nAnalysis complete! All plots saved to " << outputDir << std::endl;
    
    // Cleanup
    for (auto& hist_pair : histograms) {
        delete hist_pair.second;
    }
    
    return 0;
}
