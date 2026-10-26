#!/usr/bin/env python3
"""
Analyze TDIS hits CSV data and create histograms for all columns.
Creates plots in plots_in_hits directory.
"""

import argparse
import os
import sys
from pathlib import Path
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from hist import Hist
from hist.axis import Regular
import warnings


# Suppress matplotlib warnings
warnings.filterwarnings('ignore', category=UserWarning)

def create_histograms():
    """Create all histograms with predefined binning for hit data"""
    hists = {}
    
    # Event and track identification
    hists['evt'] = Hist(
        Regular(100, 0, 1000, name="evt", label="Event Number")
    )
    hists['trk_id'] = Hist(
        Regular(50, 0, 50, name="trk_id", label="Track ID")
    )
    
    # Measurement properties
    hists['meas_time'] = Hist(
        Regular(100, 0, 100, name="meas_time", label="Measurement Time [ns]")
    )
    hists['meas_surface'] = Hist(
        Regular(100, 0, 1e10, name="meas_surface", label="Measurement Surface ID")
    )
    hists['meas_loc0'] = Hist(
        Regular(100, -300, 300, name="meas_loc0", label="Measurement Local 0 [mm]")
    )
    hists['meas_loc1'] = Hist(
        Regular(100, -300, 300, name="meas_loc1", label="Measurement Local 1 [mm]")
    )
    
    # Measurement covariances
    hists['meas_cov0'] = Hist(
        Regular(100, 0, 10, name="meas_cov0", label=r"Meas Cov(0,0) [mm$^2$]")
    )
    hists['meas_cov1'] = Hist(
        Regular(100, 0, 10, name="meas_cov1", label=r"Meas Cov(1,1) [mm$^2$]")
    )
    hists['meas_cov_time'] = Hist(
        Regular(100, 0, 1000, name="meas_cov_time", label=r"Meas Cov(time) [ns$^2$]")
    )
    
    # Tracker hit properties
    hists['hit_id'] = Hist(
        Regular(100, 0, 1000, name="hit_id", label="Hit ID")
    )
    hists['hit_cell_id'] = Hist(
        Regular(100, 0, 20000000, name="hit_cell_id", label="Hit Cell ID")
    )
    hists['hit_x'] = Hist(
        Regular(100, -500, 500, name="hit_x", label="Hit X Position [mm]")
    )
    hists['hit_y'] = Hist(
        Regular(100, -500, 500, name="hit_y", label="Hit Y Position [mm]")
    )
    hists['hit_z'] = Hist(
        Regular(100, -2000, 2000, name="hit_z", label="Hit Z Position [mm]")
    )
    hists['hit_time'] = Hist(
        Regular(100, 0, 100, name="hit_time", label="Hit Time [ns]")
    )
    hists['hit_adc'] = Hist(
        Regular(100, 0, 1000, name="hit_adc", label="Hit ADC (Energy Deposit)")
    )
    
    # MC hit properties
    hists['mc_hit_id'] = Hist(
        Regular(100, 0, 1000, name="mc_hit_id", label="MC Hit ID")
    )
    hists['mc_hit_plane'] = Hist(
        Regular(20, 0, 20, name="mc_hit_plane", label="MC Hit Plane")
    )
    hists['mc_hit_ring'] = Hist(
        Regular(20, 0, 20, name="mc_hit_ring", label="MC Hit Ring")
    )
    hists['mc_hit_pad'] = Hist(
        Regular(200, -1000, 1000, name="mc_hit_pad", label="MC Hit Pad")
    )
    hists['mc_hit_time'] = Hist(
        Regular(100, 0, 100, name="mc_hit_time", label="MC Hit Time [ns]")
    )
    hists['mc_hit_adc'] = Hist(
        Regular(100, 0, 1000, name="mc_hit_adc", label="MC Hit ADC")
    )
    hists['mc_hit_ztogem'] = Hist(
        Regular(100, -50, 50, name="mc_hit_ztogem", label="MC Hit Z to GEM [mm]")
    )
    hists['mc_hit_true_x'] = Hist(
        Regular(100, -500, 500, name="mc_hit_true_x", label="MC Hit True X [mm]")
    )
    hists['mc_hit_true_y'] = Hist(
        Regular(100, -500, 500, name="mc_hit_true_y", label="MC Hit True Y [mm]")
    )
    hists['mc_hit_true_z'] = Hist(
        Regular(100, -2000, 2000, name="mc_hit_true_z", label="MC Hit True Z [mm]")
    )
    
    return hists


def create_resolution_histograms():
    """Create histograms for resolution studies (differences between reco and truth)"""
    res_hists = {}
    
    # Position resolutions
    res_hists['res_x'] = Hist(
        Regular(100, -50, 50, name="res_x", label="X Resolution (Hit - MC) [mm]")
    )
    res_hists['res_y'] = Hist(
        Regular(100, -50, 50, name="res_y", label="Y Resolution (Hit - MC) [mm]")
    )
    res_hists['res_z'] = Hist(
        Regular(100, -50, 50, name="res_z", label="Z Resolution (Hit - MC) [mm]")
    )
    res_hists['res_time'] = Hist(
        Regular(100, -10, 10, name="res_time", label="Time Resolution (Hit - MC) [ns]")
    )
    res_hists['res_r'] = Hist(
        Regular(100, 0, 50, name="res_r", label="3D Distance |Hit - MC| [mm]")
    )
    
    return res_hists


def fill_histograms(df, hists):
    """Fill histograms with data from dataframe"""
    filled_hists = {}
    
    for col_name, hist in hists.items():
        if col_name in df.columns:
            # Filter out NaN values
            data = df[col_name].dropna()
            if len(data) > 0:
                try:
                    hist.fill(data)
                    filled_hists[col_name] = hist
                except Exception as e:
                    print(f"Warning: Could not fill histogram for {col_name}: {e}")
        else:
            print(f"Warning: Column {col_name} not found in dataframe")
    
    return filled_hists


def compute_resolutions(df):
    """Compute resolution values between reconstructed and MC truth"""
    resolutions = {}
    
    # Check if necessary columns exist
    if all(col in df.columns for col in ['hit_x', 'mc_hit_true_x', 
                                          'hit_y', 'mc_hit_true_y',
                                          'hit_z', 'mc_hit_true_z']):
        # Position resolutions
        mask = df['hit_x'].notna() & df['mc_hit_true_x'].notna()
        resolutions['res_x'] = df.loc[mask, 'hit_x'] - df.loc[mask, 'mc_hit_true_x']
        
        mask = df['hit_y'].notna() & df['mc_hit_true_y'].notna()
        resolutions['res_y'] = df.loc[mask, 'hit_y'] - df.loc[mask, 'mc_hit_true_y']
        
        mask = df['hit_z'].notna() & df['mc_hit_true_z'].notna()
        resolutions['res_z'] = df.loc[mask, 'hit_z'] - df.loc[mask, 'mc_hit_true_z']
        
        # 3D distance
        mask = (df['hit_x'].notna() & df['mc_hit_true_x'].notna() &
                df['hit_y'].notna() & df['mc_hit_true_y'].notna() &
                df['hit_z'].notna() & df['mc_hit_true_z'].notna())
        if mask.any():
            dx = df.loc[mask, 'hit_x'] - df.loc[mask, 'mc_hit_true_x']
            dy = df.loc[mask, 'hit_y'] - df.loc[mask, 'mc_hit_true_y']
            dz = df.loc[mask, 'hit_z'] - df.loc[mask, 'mc_hit_true_z']
            resolutions['res_r'] = np.sqrt(dx**2 + dy**2 + dz**2)
    
    # Time resolution
    if 'hit_time' in df.columns and 'mc_hit_time' in df.columns:
        mask = df['hit_time'].notna() & df['mc_hit_time'].notna()
        resolutions['res_time'] = df.loc[mask, 'hit_time'] - df.loc[mask, 'mc_hit_time']
    
    return resolutions


def plot_histogram(hist, column_name, output_dir):
    """Plot and save a single histogram"""
    fig, ax = plt.subplots(figsize=(8, 6))
    
    # Plot the histogram
    hist.plot(ax=ax, histtype='step', linewidth=2, color='blue', label=column_name)
    
    # Add statistics box
    values = hist.values()
    if len(values) > 0 and np.sum(values) > 0:
        mean = hist.axes[0].centers @ values / np.sum(values)
        std = np.sqrt((hist.axes[0].centers**2 @ values / np.sum(values)) - mean**2)
        entries = np.sum(values)
        
        stats_text = f'Entries: {int(entries)}\nMean: {mean:.3f}\nStd: {std:.3f}'
        ax.text(0.7, 0.95, stats_text,
                transform=ax.transAxes,
                verticalalignment='top',
                bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    
    # Formatting
    ax.set_xlabel(hist.axes[0].label)
    ax.set_ylabel('Counts')
    ax.set_title(f'Distribution of {column_name}')
    ax.grid(True, alpha=0.3)
    
    # Set y-axis to start from 0
    ylim = ax.get_ylim()
    ax.set_ylim(0, ylim[1] * 1.1)
    
    # Save the plot
    output_file = output_dir / f'{column_name}.png'
    plt.savefig(output_file, dpi=100, bbox_inches='tight')
    plt.close()
    
    return output_file


def create_detector_geometry_plots(df, output_dir):
    """Create plots showing detector geometry from hit distributions"""
    
    # 1. XY hit distribution
    if all(col in df.columns for col in ['hit_x', 'hit_y']):
        fig, axes = plt.subplots(1, 2, figsize=(14, 6))
        
        # Reconstructed hits
        mask = df['hit_x'].notna() & df['hit_y'].notna()
        h1, xedges, yedges = np.histogram2d(
            df.loc[mask, 'hit_x'], 
            df.loc[mask, 'hit_y'], 
            bins=100
        )
        
        im1 = axes[0].imshow(h1.T, origin='lower',
                            extent=[xedges[0], xedges[-1], yedges[0], yedges[-1]],
                            aspect='equal', cmap='viridis')
        plt.colorbar(im1, ax=axes[0], label='Counts')
        axes[0].set_xlabel('Hit X [mm]')
        axes[0].set_ylabel('Hit Y [mm]')
        axes[0].set_title('Reconstructed Hit XY Distribution')
        axes[0].grid(True, alpha=0.3)
        
        # MC truth hits
        if all(col in df.columns for col in ['mc_hit_true_x', 'mc_hit_true_y']):
            mask = df['mc_hit_true_x'].notna() & df['mc_hit_true_y'].notna()
            h2, xedges, yedges = np.histogram2d(
                df.loc[mask, 'mc_hit_true_x'], 
                df.loc[mask, 'mc_hit_true_y'], 
                bins=100
            )
            
            im2 = axes[1].imshow(h2.T, origin='lower',
                                extent=[xedges[0], xedges[-1], yedges[0], yedges[-1]],
                                aspect='equal', cmap='viridis')
            plt.colorbar(im2, ax=axes[1], label='Counts')
            axes[1].set_xlabel('MC Hit True X [mm]')
            axes[1].set_ylabel('MC Hit True Y [mm]')
            axes[1].set_title('MC Truth Hit XY Distribution')
            axes[1].grid(True, alpha=0.3)
        
        plt.suptitle('Hit Position Distributions')
        output_file = output_dir / 'detector_xy_distribution.png'
        plt.savefig(output_file, dpi=100, bbox_inches='tight')
        plt.close()
    
    # 2. Z distribution comparison
    if 'hit_z' in df.columns:
        fig, ax = plt.subplots(figsize=(10, 6))
        
        # Reconstructed Z
        data_reco = df['hit_z'].dropna()
        ax.hist(data_reco, bins=100, alpha=0.6, label='Reconstructed', color='blue')
        
        # MC truth Z
        if 'mc_hit_true_z' in df.columns:
            data_mc = df['mc_hit_true_z'].dropna()
            ax.hist(data_mc, bins=100, alpha=0.6, label='MC Truth', color='red')
        
        ax.set_xlabel('Z Position [mm]')
        ax.set_ylabel('Counts')
        ax.set_title('Z Position Distribution')
        ax.legend()
        ax.grid(True, alpha=0.3)
        
        output_file = output_dir / 'detector_z_distribution.png'
        plt.savefig(output_file, dpi=100, bbox_inches='tight')
        plt.close()
    
    # 3. Plane, Ring, Pad occupancy
    if all(col in df.columns for col in ['mc_hit_plane', 'mc_hit_ring', 'mc_hit_pad']):
        fig, axes = plt.subplots(1, 3, figsize=(15, 5))
        
        # Plane occupancy
        data = df['mc_hit_plane'].dropna()
        if len(data) > 0:
            axes[0].hist(data, bins=range(int(data.min()), int(data.max())+2), 
                        edgecolor='black')
            axes[0].set_xlabel('Plane Number')
            axes[0].set_ylabel('Hit Count')
            axes[0].set_title('Hits per Plane')
            axes[0].grid(True, alpha=0.3)
        
        # Ring occupancy
        data = df['mc_hit_ring'].dropna()
        if len(data) > 0:
            axes[1].hist(data, bins=range(int(data.min()), int(data.max())+2), 
                        edgecolor='black')
            axes[1].set_xlabel('Ring Number')
            axes[1].set_ylabel('Hit Count')
            axes[1].set_title('Hits per Ring')
            axes[1].grid(True, alpha=0.3)
        
        # Pad distribution
        data = df['mc_hit_pad'].dropna()
        if len(data) > 0:
            axes[2].hist(data, bins=50, edgecolor='black')
            axes[2].set_xlabel('Pad Number')
            axes[2].set_ylabel('Hit Count')
            axes[2].set_title('Pad Occupancy')
            axes[2].grid(True, alpha=0.3)
        
        plt.suptitle('Detector Element Occupancy')
        output_file = output_dir / 'detector_occupancy.png'
        plt.savefig(output_file, dpi=100, bbox_inches='tight')
        plt.close()


def create_resolution_plots(resolutions, res_hists, output_dir):
    """Create and save resolution plots"""
    filled_res_hists = {}
    
    for res_name, res_data in resolutions.items():
        if res_name in res_hists and len(res_data) > 0:
            res_hists[res_name].fill(res_data)
            filled_res_hists[res_name] = res_hists[res_name]
    
    # Plot resolution histograms
    for res_name, hist in filled_res_hists.items():
        output_file = plot_histogram(hist, res_name, output_dir)
        print(f"  Saved resolution plot: {output_file.name}")
    
    # Create combined resolution plot
    if len(filled_res_hists) > 0:
        fig, axes = plt.subplots(2, 2, figsize=(12, 10))
        axes = axes.flatten()
        
        plot_idx = 0
        for res_name, hist in list(filled_res_hists.items())[:4]:
            if plot_idx < 4:
                hist.plot(ax=axes[plot_idx], histtype='step', linewidth=2)
                axes[plot_idx].set_xlabel(hist.axes[0].label)
                axes[plot_idx].set_ylabel('Counts')
                axes[plot_idx].set_title(f'{res_name}')
                axes[plot_idx].grid(True, alpha=0.3)
                
                # Add Gaussian fit if enough data
                values = hist.values()
                if np.sum(values) > 100:
                    centers = hist.axes[0].centers
                    mean = centers @ values / np.sum(values)
                    std = np.sqrt((centers**2 @ values / np.sum(values)) - mean**2)
                    axes[plot_idx].axvline(mean, color='red', linestyle='--', 
                                         label=f'Mean: {mean:.2f}')
                    axes[plot_idx].legend()
                
                plot_idx += 1
        
        plt.suptitle('Hit Position Resolutions')
        plt.tight_layout()
        output_file = output_dir / 'summary_resolutions.png'
        plt.savefig(output_file, dpi=100, bbox_inches='tight')
        plt.close()


def main():
    parser = argparse.ArgumentParser(description='Analyze TDIS hits CSV data')
    parser.add_argument('csv_file', help='Path to the input CSV file')
    parser.add_argument('--output-dir', default='plots_in_hits', 
                       help='Output directory for plots (default: plots_in_hits)')
    parser.add_argument('--max-rows', type=int, default=None,
                       help='Maximum number of rows to read (for testing)')
    args = parser.parse_args()
    
    # Check input file
    if not os.path.exists(args.csv_file):
        print(f"Error: Input file '{args.csv_file}' not found")
        sys.exit(1)
    
    # Create output directory
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    print(f"Output directory: {output_dir}")
    
    # Read CSV file
    print(f"Reading CSV file: {args.csv_file}")
    try:
        df = pd.read_csv(args.csv_file, nrows=args.max_rows)
        print(f"Loaded {len(df)} hits with {len(df.columns)} columns")
    except Exception as e:
        print(f"Error reading CSV file: {e}")
        sys.exit(1)
    
    # Print basic statistics
    print("\nDataFrame info:")
    print(f"Shape: {df.shape}")
    print(f"Columns: {list(df.columns)[:10]}...")  # Show first 10 columns
    
    # Create histograms
    print("\nCreating histograms...")
    hists = create_histograms()
    
    # Fill histograms with data
    print("Filling histograms with data...")
    filled_hists = fill_histograms(df, hists)
    
    # Plot all histograms
    print(f"Plotting {len(filled_hists)} histograms...")
    for col_name, hist in filled_hists.items():
        output_file = plot_histogram(hist, col_name, output_dir)
        print(f"  Saved: {output_file.name}")
    
    # Compute and plot resolutions
    print("\nComputing resolutions...")
    resolutions = compute_resolutions(df)
    if resolutions:
        res_hists = create_resolution_histograms()
        create_resolution_plots(resolutions, res_hists, output_dir)
    
    # Create detector geometry plots
    print("\nCreating detector geometry plots...")
    create_detector_geometry_plots(df, output_dir)
    
    print(f"\nAnalysis complete! All plots saved to {output_dir}")
    
    # Print summary statistics
    print("\nSummary Statistics:")
    if 'evt' in df.columns:
        print(f"  Number of events: {df['evt'].nunique()}")
    if 'trk_id' in df.columns:
        print(f"  Number of unique tracks: {len(df[['evt', 'trk_id']].drop_duplicates())}")
    print(f"  Total number of hits: {len(df)}")
    
    if 'mc_hit_plane' in df.columns:
        print(f"  Number of planes hit: {df['mc_hit_plane'].nunique()}")
    if 'mc_hit_ring' in df.columns:
        print(f"  Number of rings hit: {df['mc_hit_ring'].nunique()}")
    
    # Resolution summary if available
    if resolutions:
        print("\nResolution Summary:")
        for res_name, res_data in resolutions.items():
            if len(res_data) > 0:
                print(f"  {res_name}: mean={res_data.mean():.3f}, std={res_data.std():.3f}")


if __name__ == '__main__':
    main()
