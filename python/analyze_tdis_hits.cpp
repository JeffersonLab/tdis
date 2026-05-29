// analyze_tdis_hits.cpp
// Analyze TDIS hits CSV data and create histograms using ROOT
// Compile: g++ -o analyze_tdis_hits analyze_tdis_hits.cpp `root-config --cflags --libs`
// Usage: ./analyze_tdis_hits input.csv [output_dir]

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
#include "TLatex.h"
#include "TLegend.h"
#include "TPaveStats.h"
#include "TSystem.h"
#include "TLine.h"

struct HitData {
    // Event and track identification
    int evt;
    int trk_id;
    
    // Measurement properties
    double meas_time;
    long long meas_surface;
    double meas_loc0;
    double meas_loc1;
    double meas_cov0;
    double meas_cov1;
    double meas_cov_time;
    
    // Tracker hit properties
    int hit_id;
    long long hit_cell_id;
    double hit_x;
    double hit_y;
    double hit_z;
    double hit_time;
    double hit_adc;
    
    // MC hit properties
    int mc_hit_id;
    int mc_hit_plane;
    int mc_hit_ring;
    int mc_hit_pad;
    double mc_hit_time;
    double mc_hit_adc;
    double mc_hit_ztogem;
    double mc_hit_true_x;
    double mc_hit_true_y;
    double mc_hit_true_z;
    
    HitData() : evt(-1), trk_id(-1), meas_time(-999), meas_surface(-1),
                meas_loc0(-999), meas_loc1(-999), meas_cov0(-999), 
                meas_cov1(-999), meas_cov_time(-999), hit_id(-1), 
                hit_cell_id(-1), hit_x(-999), hit_y(-999), hit_z(-999),
                hit_time(-999), hit_adc(-999), mc_hit_id(-1), 
                mc_hit_plane(-1), mc_hit_ring(-1), mc_hit_pad(-999),
                mc_hit_time(-999), mc_hit_adc(-999), mc_hit_ztogem(-999),
                mc_hit_true_x(-999), mc_hit_true_y(-999), mc_hit_true_z(-999) {}
};

class TDISAnalyzer {
private:
    std::string outputDir;
    std::map<std::string, TH1D*> histograms;
    std::map<std::string, TH2D*> histograms2D;
    std::vector<HitData> hits;
    
public:
    TDISAnalyzer(const std::string& outDir) : outputDir(outDir) {
        gSystem->mkdir(outputDir.c_str(), kTRUE);
        gStyle->SetOptStat(1111);
        gStyle->SetOptFit(1111);
    }
    
    ~TDISAnalyzer() {
        for (auto& h : histograms) delete h.second;
        for (auto& h : histograms2D) delete h.second;
    }
    
    void CreateHistograms() {
        // Event and track identification
        histograms["evt"] = new TH1D("evt", "Event Number;Event Number;Counts", 100, 0, 1000);
        histograms["trk_id"] = new TH1D("trk_id", "Track ID;Track ID;Counts", 50, 0, 50);
        
        // Measurement properties
        histograms["meas_time"] = new TH1D("meas_time", "Measurement Time;Time [ns];Counts", 100, 0, 100);
        histograms["meas_surface"] = new TH1D("meas_surface", "Measurement Surface ID;Surface ID;Counts", 100, 0, 1e10);
        histograms["meas_loc0"] = new TH1D("meas_loc0", "Measurement Local 0;Local 0 [mm];Counts", 100, -300, 300);
        histograms["meas_loc1"] = new TH1D("meas_loc1", "Measurement Local 1;Local 1 [mm];Counts", 100, -300, 300);
        
        // Measurement covariances
        histograms["meas_cov0"] = new TH1D("meas_cov0", "Meas Cov(0,0);Cov(0,0) [mm^{2}];Counts", 100, 0, 10);
        histograms["meas_cov1"] = new TH1D("meas_cov1", "Meas Cov(1,1);Cov(1,1) [mm^{2}];Counts", 100, 0, 10);
        histograms["meas_cov_time"] = new TH1D("meas_cov_time", "Meas Cov(time);Cov(time) [ns^{2}];Counts", 100, 0, 1000);
        
        // Tracker hit properties
        histograms["hit_id"] = new TH1D("hit_id", "Hit ID;Hit ID;Counts", 100, 0, 1000);
        histograms["hit_cell_id"] = new TH1D("hit_cell_id", "Hit Cell ID;Cell ID;Counts", 100, 0, 20000000);
        histograms["hit_x"] = new TH1D("hit_x", "Hit X Position;X [mm];Counts", 100, -500, 500);
        histograms["hit_y"] = new TH1D("hit_y", "Hit Y Position;Y [mm];Counts", 100, -500, 500);
        histograms["hit_z"] = new TH1D("hit_z", "Hit Z Position;Z [mm];Counts", 100, -2000, 2000);
        histograms["hit_time"] = new TH1D("hit_time", "Hit Time;Time [ns];Counts", 100, 0, 100);
        histograms["hit_adc"] = new TH1D("hit_adc", "Hit ADC;ADC (Energy Deposit);Counts", 100, 0, 1000);
        
        // MC hit properties
        histograms["mc_hit_id"] = new TH1D("mc_hit_id", "MC Hit ID;MC Hit ID;Counts", 100, 0, 1000);
        histograms["mc_hit_plane"] = new TH1D("mc_hit_plane", "MC Hit Plane;Plane;Counts", 20, 0, 20);
        histograms["mc_hit_ring"] = new TH1D("mc_hit_ring", "MC Hit Ring;Ring;Counts", 20, 0, 20);
        histograms["mc_hit_pad"] = new TH1D("mc_hit_pad", "MC Hit Pad;Pad;Counts", 200, -1000, 1000);
        histograms["mc_hit_time"] = new TH1D("mc_hit_time", "MC Hit Time;Time [ns];Counts", 100, 0, 100);
        histograms["mc_hit_adc"] = new TH1D("mc_hit_adc", "MC Hit ADC;ADC;Counts", 100, 0, 1000);
        histograms["mc_hit_ztogem"] = new TH1D("mc_hit_ztogem", "MC Hit Z to GEM;Z to GEM [mm];Counts", 100, -50, 50);
        histograms["mc_hit_true_x"] = new TH1D("mc_hit_true_x", "MC Hit True X;X [mm];Counts", 100, -500, 500);
        histograms["mc_hit_true_y"] = new TH1D("mc_hit_true_y", "MC Hit True Y;Y [mm];Counts", 100, -500, 500);
        histograms["mc_hit_true_z"] = new TH1D("mc_hit_true_z", "MC Hit True Z;Z [mm];Counts", 100, -2000, 2000);
        
        // Resolution histograms
        histograms["res_x"] = new TH1D("res_x", "X Resolution;Hit X - MC X [mm];Counts", 100, -50, 50);
        histograms["res_y"] = new TH1D("res_y", "Y Resolution;Hit Y - MC Y [mm];Counts", 100, -50, 50);
        histograms["res_z"] = new TH1D("res_z", "Z Resolution;Hit Z - MC Z [mm];Counts", 100, -50, 50);
        histograms["res_time"] = new TH1D("res_time", "Time Resolution;Hit Time - MC Time [ns];Counts", 100, -10, 10);
        histograms["res_r"] = new TH1D("res_r", "3D Distance;|Hit - MC| [mm];Counts", 100, 0, 50);
        
        // 2D histograms
        histograms2D["hit_xy"] = new TH2D("hit_xy", "Hit XY Distribution;X [mm];Y [mm]", 100, -500, 500, 100, -500, 500);
        histograms2D["mc_hit_xy"] = new TH2D("mc_hit_xy", "MC Hit XY Distribution;X [mm];Y [mm]", 100, -500, 500, 100, -500, 500);
    }
    
    bool ReadCSV(const std::string& filename, int maxRows = -1) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file " << filename << std::endl;
            return false;
        }
        
        std::string line;
        std::vector<std::string> headers;
        
        // Read header
        if (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string header;
            while (std::getline(ss, header, ',')) {
                // Trim whitespace
                header.erase(0, header.find_first_not_of(" \t\r\n"));
                header.erase(header.find_last_not_of(" \t\r\n") + 1);
                headers.push_back(header);
            }
        }
        
        std::cout << "Found " << headers.size() << " columns" << std::endl;
        
        // Create column index map
        std::map<std::string, int> colIndex;
        for (size_t i = 0; i < headers.size(); i++) {
            colIndex[headers[i]] = i;
        }
        
        // Read data
        int rowCount = 0;
        while (std::getline(file, line) && (maxRows < 0 || rowCount < maxRows)) {
            std::stringstream ss(line);
            std::string cell;
            std::vector<std::string> values;
            
            while (std::getline(ss, cell, ',')) {
                values.push_back(cell);
            }
            
            if (values.size() != headers.size()) continue;
            
            HitData hit;
            
            // Parse data using helper function
            auto getVal = [&](const std::string& name, auto& var) {
                if (colIndex.find(name) != colIndex.end()) {
                    int idx = colIndex[name];
                    if (!values[idx].empty() && values[idx] != "nan") {
                        std::stringstream ss(values[idx]);
                        ss >> var;
                    }
                }
            };
            
            getVal("evt", hit.evt);
            getVal("trk_id", hit.trk_id);
            getVal("meas_time", hit.meas_time);
            getVal("meas_surface", hit.meas_surface);
            getVal("meas_loc0", hit.meas_loc0);
            getVal("meas_loc1", hit.meas_loc1);
            getVal("meas_cov0", hit.meas_cov0);
            getVal("meas_cov1", hit.meas_cov1);
            getVal("meas_cov_time", hit.meas_cov_time);
            getVal("hit_id", hit.hit_id);
            getVal("hit_cell_id", hit.hit_cell_id);
            getVal("hit_x", hit.hit_x);
            getVal("hit_y", hit.hit_y);
            getVal("hit_z", hit.hit_z);
            getVal("hit_time", hit.hit_time);
            getVal("hit_adc", hit.hit_adc);
            getVal("mc_hit_id", hit.mc_hit_id);
            getVal("mc_hit_plane", hit.mc_hit_plane);
            getVal("mc_hit_ring", hit.mc_hit_ring);
            getVal("mc_hit_pad", hit.mc_hit_pad);
            getVal("mc_hit_time", hit.mc_hit_time);
            getVal("mc_hit_adc", hit.mc_hit_adc);
            getVal("mc_hit_ztogem", hit.mc_hit_ztogem);
            getVal("mc_hit_true_x", hit.mc_hit_true_x);
            getVal("mc_hit_true_y", hit.mc_hit_true_y);
            getVal("mc_hit_true_z", hit.mc_hit_true_z);
            
            hits.push_back(hit);
            rowCount++;
        }
        
        file.close();
        std::cout << "Loaded " << hits.size() << " hits" << std::endl;
        return true;
    }
    
    void FillHistograms() {
        std::cout << "Filling histograms..." << std::endl;
        
        for (const auto& hit : hits) {
            // Helper function to fill if valid
            auto fill = [](TH1D* h, double val) {
                if (val > -900) h->Fill(val);
            };
            
            fill(histograms["evt"], hit.evt);
            fill(histograms["trk_id"], hit.trk_id);
            fill(histograms["meas_time"], hit.meas_time);
            fill(histograms["meas_surface"], hit.meas_surface);
            fill(histograms["meas_loc0"], hit.meas_loc0);
            fill(histograms["meas_loc1"], hit.meas_loc1);
            fill(histograms["meas_cov0"], hit.meas_cov0);
            fill(histograms["meas_cov1"], hit.meas_cov1);
            fill(histograms["meas_cov_time"], hit.meas_cov_time);
            fill(histograms["hit_id"], hit.hit_id);
            fill(histograms["hit_cell_id"], hit.hit_cell_id);
            fill(histograms["hit_x"], hit.hit_x);
            fill(histograms["hit_y"], hit.hit_y);
            fill(histograms["hit_z"], hit.hit_z);
            fill(histograms["hit_time"], hit.hit_time);
            fill(histograms["hit_adc"], hit.hit_adc);
            fill(histograms["mc_hit_id"], hit.mc_hit_id);
            fill(histograms["mc_hit_plane"], hit.mc_hit_plane);
            fill(histograms["mc_hit_ring"], hit.mc_hit_ring);
            fill(histograms["mc_hit_pad"], hit.mc_hit_pad);
            fill(histograms["mc_hit_time"], hit.mc_hit_time);
            fill(histograms["mc_hit_adc"], hit.mc_hit_adc);
            fill(histograms["mc_hit_ztogem"], hit.mc_hit_ztogem);
            fill(histograms["mc_hit_true_x"], hit.mc_hit_true_x);
            fill(histograms["mc_hit_true_y"], hit.mc_hit_true_y);
            fill(histograms["mc_hit_true_z"], hit.mc_hit_true_z);
            
            // Fill resolution histograms
            if (hit.hit_x > -900 && hit.mc_hit_true_x > -900) {
                histograms["res_x"]->Fill(hit.hit_x - hit.mc_hit_true_x);
            }
            if (hit.hit_y > -900 && hit.mc_hit_true_y > -900) {
                histograms["res_y"]->Fill(hit.hit_y - hit.mc_hit_true_y);
            }
            if (hit.hit_z > -900 && hit.mc_hit_true_z > -900) {
                histograms["res_z"]->Fill(hit.hit_z - hit.mc_hit_true_z);
            }
            if (hit.hit_time > -900 && hit.mc_hit_time > -900) {
                histograms["res_time"]->Fill(hit.hit_time - hit.mc_hit_time);
            }
            if (hit.hit_x > -900 && hit.mc_hit_true_x > -900 &&
                hit.hit_y > -900 && hit.mc_hit_true_y > -900 &&
                hit.hit_z > -900 && hit.mc_hit_true_z > -900) {
                double dx = hit.hit_x - hit.mc_hit_true_x;
                double dy = hit.hit_y - hit.mc_hit_true_y;
                double dz = hit.hit_z - hit.mc_hit_true_z;
                histograms["res_r"]->Fill(std::sqrt(dx*dx + dy*dy + dz*dz));
            }
            
            // Fill 2D histograms
            if (hit.hit_x > -900 && hit.hit_y > -900) {
                histograms2D["hit_xy"]->Fill(hit.hit_x, hit.hit_y);
            }
            if (hit.mc_hit_true_x > -900 && hit.mc_hit_true_y > -900) {
                histograms2D["mc_hit_xy"]->Fill(hit.mc_hit_true_x, hit.mc_hit_true_y);
            }
        }
    }
    
    void PlotHistograms() {
        std::cout << "Creating plots..." << std::endl;
        
        // Plot 1D histograms
        for (auto& pair : histograms) {
            if (pair.second->GetEntries() == 0) continue;
            
            TCanvas* c = new TCanvas("c", "c", 800, 600);
            pair.second->SetLineColor(kBlue);
            pair.second->SetLineWidth(2);
            pair.second->Draw("HIST");
            
            c->Update();
            
            std::string filename = outputDir + "/" + pair.first + ".png";
            c->SaveAs(filename.c_str());
            std::cout << "  Saved: " << pair.first << ".png" << std::endl;
            delete c;
        }
        
        // Plot 2D histograms
        for (auto& pair : histograms2D) {
            if (pair.second->GetEntries() == 0) continue;
            
            TCanvas* c = new TCanvas("c", "c", 800, 600);
            pair.second->Draw("COLZ");
            
            std::string filename = outputDir + "/" + pair.first + ".png";
            c->SaveAs(filename.c_str());
            std::cout << "  Saved: " << pair.first << ".png" << std::endl;
            delete c;
        }
        
        // Create combined resolution plot
        CreateResolutionSummary();
        
        // Create detector geometry plots
        CreateDetectorGeometryPlots();
    }
    
    void CreateResolutionSummary() {
        TCanvas* c = new TCanvas("c_res", "Resolution Summary", 1200, 1000);
        c->Divide(2, 2);
        
        std::vector<std::string> res_names = {"res_x", "res_y", "res_z", "res_time"};
        
        for (int i = 0; i < 4; i++) {
            c->cd(i+1);
            if (histograms[res_names[i]]->GetEntries() > 0) {
                histograms[res_names[i]]->SetLineColor(kBlue);
                histograms[res_names[i]]->SetLineWidth(2);
                histograms[res_names[i]]->Draw("HIST");
                
                // Add mean line
                double mean = histograms[res_names[i]]->GetMean();
                TLine* line = new TLine(mean, 0, mean, histograms[res_names[i]]->GetMaximum());
                line->SetLineColor(kRed);
                line->SetLineStyle(2);
                line->SetLineWidth(2);
                line->Draw();
            }
        }
        
        std::string filename = outputDir + "/summary_resolutions.png";
        c->SaveAs(filename.c_str());
        std::cout << "  Saved: summary_resolutions.png" << std::endl;
        delete c;
    }
    
    void CreateDetectorGeometryPlots() {
        // XY distributions
        TCanvas* c = new TCanvas("c_xy", "XY Distributions", 1400, 600);
        c->Divide(2, 1);
        
        c->cd(1);
        histograms2D["hit_xy"]->Draw("COLZ");
        
        c->cd(2);
        histograms2D["mc_hit_xy"]->Draw("COLZ");
        
        std::string filename = outputDir + "/detector_xy_distribution.png";
        c->SaveAs(filename.c_str());
        std::cout << "  Saved: detector_xy_distribution.png" << std::endl;
        delete c;
        
        // Z distribution comparison
        TCanvas* c2 = new TCanvas("c_z", "Z Distribution", 1000, 600);
        histograms["hit_z"]->SetLineColor(kBlue);
        histograms["hit_z"]->SetLineWidth(2);
        histograms["hit_z"]->Draw("HIST");
        
        histograms["mc_hit_true_z"]->SetLineColor(kRed);
        histograms["mc_hit_true_z"]->SetLineWidth(2);
        histograms["mc_hit_true_z"]->Draw("HIST SAME");
        
        TLegend* leg = new TLegend(0.7, 0.7, 0.9, 0.9);
        leg->AddEntry(histograms["hit_z"], "Reconstructed", "l");
        leg->AddEntry(histograms["mc_hit_true_z"], "MC Truth", "l");
        leg->Draw();
        
        filename = outputDir + "/detector_z_distribution.png";
        c2->SaveAs(filename.c_str());
        std::cout << "  Saved: detector_z_distribution.png" << std::endl;
        delete c2;
    }
    
    void SaveToROOTFile() {
        std::string filename = outputDir + "/histograms.root";
        TFile* f = new TFile(filename.c_str(), "RECREATE");
        
        for (auto& pair : histograms) {
            pair.second->Write();
        }
        for (auto& pair : histograms2D) {
            pair.second->Write();
        }
        
        f->Close();
        delete f;
        std::cout << "Saved all histograms to " << filename << std::endl;
    }
    
    void PrintSummary() {
        std::cout << "\n=== Summary Statistics ===" << std::endl;
        std::cout << "Total hits: " << hits.size() << std::endl;
        
        if (histograms["evt"]->GetEntries() > 0) {
            int nEvents = histograms["evt"]->GetXaxis()->GetXmax();
            std::cout << "Number of events: ~" << nEvents << std::endl;
        }
        
        std::cout << "\nResolution Summary:" << std::endl;
        std::vector<std::string> res_names = {"res_x", "res_y", "res_z", "res_time", "res_r"};
        for (const auto& name : res_names) {
            if (histograms[name]->GetEntries() > 0) {
                double mean = histograms[name]->GetMean();
                double rms = histograms[name]->GetRMS();
                std::cout << "  " << name << ": mean=" << mean 
                         << ", std=" << rms << std::endl;
            }
        }
    }
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <input.csv> [output_dir]" << std::endl;
        return 1;
    }
    
    std::string inputFile = argv[1];
    std::string outputDir = (argc > 2) ? argv[2] : "plots_in_hits";
    
    std::cout << "TDIS Hits Analysis with ROOT" << std::endl;
    std::cout << "Input file: " << inputFile << std::endl;
    std::cout << "Output directory: " << outputDir << std::endl;
    
    TDISAnalyzer analyzer(outputDir);
    
    analyzer.CreateHistograms();
    
    if (!analyzer.ReadCSV(inputFile)) {
        return 1;
    }
    
    analyzer.FillHistograms();
    analyzer.PlotHistograms();
    analyzer.SaveToROOTFile();
    analyzer.PrintSummary();
    
    std::cout << "\nAnalysis complete!" << std::endl;
    
    return 0;
}
