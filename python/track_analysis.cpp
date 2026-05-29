#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iomanip>

// Simple CSV reader structure
struct CSVData {
    std::vector<std::string> headers;
    std::vector<std::map<std::string, double>> rows;
    
    bool loadCSV(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error opening file: " << filename << std::endl;
            return false;
        }
        
        std::string line;
        // Read header
        if (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string header;
            while (std::getline(ss, header, ',')) {
                headers.push_back(header);
            }
        }
        
        // Read data rows
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string value;
            std::map<std::string, double> row;
            size_t idx = 0;
            
            while (std::getline(ss, value, ',') && idx < headers.size()) {
                try {
                    row[headers[idx]] = std::stod(value);
                } catch (...) {
                    row[headers[idx]] = 0.0;
                }
                idx++;
            }
            rows.push_back(row);
        }
        
        file.close();
        return true;
    }
    
    size_t size() const { return rows.size(); }
    
    double get(size_t idx, const std::string& col) const {
        if (idx < rows.size()) {
            auto it = rows[idx].find(col);
            if (it != rows[idx].end()) {
                return it->second;
            }
        }
        return 0.0;
    }
};

// Simple histogram structure
struct Histogram {
    std::vector<int> bins;
    double min_val, max_val, bin_width;
    int num_bins;
    
    Histogram(int n_bins, double min, double max) 
        : num_bins(n_bins), min_val(min), max_val(max) {
        bins.resize(n_bins, 0);
        bin_width = (max_val - min_val) / num_bins;
    }
    
    void fill(double value) {
        if (value >= min_val && value < max_val) {
            int bin = static_cast<int>((value - min_val) / bin_width);
            if (bin >= 0 && bin < num_bins) {
                bins[bin]++;
            }
        }
    }
    
    void print() const {
        for (int i = 0; i < num_bins; i++) {
            double bin_center = min_val + (i + 0.5) * bin_width;
            std::cout << std::setw(8) << std::fixed << std::setprecision(2) 
                      << bin_center << ": ";
            for (int j = 0; j < bins[i]; j++) {
                std::cout << "*";
            }
            std::cout << " (" << bins[i] << ")" << std::endl;
        }
    }
};

// Calculate standard deviation
double calculateStd(const std::vector<double>& data) {
    double mean = std::accumulate(data.begin(), data.end(), 0.0) / data.size();
    double sq_sum = 0.0;
    for (double val : data) {
        sq_sum += (val - mean) * (val - mean);
    }
    return std::sqrt(sq_sum / data.size());
}

int main() {
    // Load all CSV files
    CSVData seeds_df, hits_df, tracks_df, states_df;
    
    std::cout << "Loading CSV files..." << std::endl;
    if (!seeds_df.loadCSV("../../tdis/build/source/tdis/tdis_output.seeds.csv") ||
        !hits_df.loadCSV("../../tdis/build/source/tdis/tdis_output.hits.csv") ||
        !tracks_df.loadCSV("../../tdis/build/source/tdis/tdis_output.fitted_tracks.csv") ||
        !states_df.loadCSV("../../tdis/build/source/tdis/tdis_output.track_states.csv")) {
        return 1;
    }
    
    // Example 1: Track reconstruction efficiency
    size_t total_seeds = seeds_df.size();
    size_t fitted_tracks = tracks_df.size();
    double efficiency = (fitted_tracks * 100.0) / total_seeds;
    std::cout << "\nTracking efficiency: " << std::fixed << std::setprecision(1) 
              << efficiency << "%" << std::endl;
    
    // Example 2: Momentum resolution study
    // Match seeds with fitted tracks by evt and seed_id
    std::vector<double> p_resolution;
    
    for (size_t i = 0; i < tracks_df.size(); i++) {
        double track_evt = tracks_df.get(i, "evt");
        double track_seed_id = tracks_df.get(i, "seed_id");
        double track_p = tracks_df.get(i, "p");
        
        // Find matching seed
        for (size_t j = 0; j < seeds_df.size(); j++) {
            double seed_evt = seeds_df.get(j, "evt");
            double seed_id = seeds_df.get(j, "seed_id");
            double seed_p = seeds_df.get(j, "p");
            
            if (track_evt == seed_evt && track_seed_id == seed_id) {
                // Calculate relative momentum difference as percentage
                if (seed_p != 0) {
                    double delta_p_rel = ((track_p - seed_p) / seed_p) * 100.0;
                    p_resolution.push_back(delta_p_rel);
                }
                break;
            }
        }
    }
    
    if (!p_resolution.empty()) {
        double rms = calculateStd(p_resolution);
        std::cout << "\nMomentum Resolution (RMS: " << std::setprecision(2) 
                  << rms << "%)" << std::endl;
        std::cout << "Number of matched tracks: " << p_resolution.size() << std::endl;
        
        Histogram mom_hist(50, -10, 10);
        for (double val : p_resolution) {
            mom_hist.fill(val);
        }
        std::cout << "Momentum Resolution Distribution:" << std::endl;
        mom_hist.print();
    } else {
        std::cout << "\nNo matched tracks found for momentum resolution." << std::endl;
    }
    
    // Example 3: Chi2/ndf distribution for quality assessment
    std::vector<double> quality_chi2;
    for (size_t i = 0; i < tracks_df.size(); i++) {
        double chi2ndf = tracks_df.get(i, "fit_chi2ndf");
        if (chi2ndf > 0) {
            quality_chi2.push_back(chi2ndf);
        }
    }
    
    std::cout << "\nχ²/ndf Distribution:" << std::endl;
    Histogram chi2_hist(50, 0, 10);
    for (double val : quality_chi2) {
        chi2_hist.fill(val);
    }
    chi2_hist.print();
    
    // Example 4: Residuals per layer
    std::map<double, std::vector<double>> layer_chi2;
    std::map<double, int> layer_counts;
    
    for (size_t i = 0; i < states_df.size(); i++) {
        double surface_id = states_df.get(i, "surface_id");
        double chi2 = states_df.get(i, "chi2");
        layer_chi2[surface_id].push_back(chi2);
        layer_counts[surface_id]++;
    }
    
    std::cout << "\nAverage chi2 per detector layer:" << std::endl;
    std::cout << std::setw(12) << "surface_id" << std::setw(12) << "avg_chi2" 
              << std::setw(12) << "count" << std::endl;
    for (const auto& pair : layer_chi2) {
        double avg_chi2 = std::accumulate(pair.second.begin(), pair.second.end(), 0.0) 
                          / pair.second.size();
        std::cout << std::setw(12) << pair.first 
                  << std::setw(12) << std::setprecision(4) << avg_chi2
                  << std::setw(12) << layer_counts[pair.first] << std::endl;
    }
    
    // Example 5: Outlier analysis
    int outlier_tracks = 0;
    for (size_t i = 0; i < tracks_df.size(); i++) {
        double n_outliers = tracks_df.get(i, "n_outliers");
        if (n_outliers > 0) {
            outlier_tracks++;
        }
    }
    
    std::cout << "\nTracks with outliers: " << outlier_tracks 
              << " / " << tracks_df.size() << std::endl;
    
    return 0;
}
