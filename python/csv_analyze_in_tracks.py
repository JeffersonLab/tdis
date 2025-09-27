#!/usr/bin/env python3
"""
Analyze TDIS track CSV data and create histograms for all columns.
Creates plots in plots_in_tracks directory.
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
    """Create all histograms with predefined binning for track data"""
    hists = {}
    
    # Event and track identification
    hists['evt'] = Hist(
        Regular(100, 0, 1000, name="evt", label="Event Number")
    )
    hists['trk_id'] = Hist(
        Regular(50, 0, 50, name="trk_id", label="Track ID")
    )
    
    # MC track properties
    hists['mc_mom'] = Hist(
        Regular(100, 0, 10, name="mc_mom", label=r"MC Momentum [GeV/c]")
    )
    hists['mc_phi'] = Hist(
        Regular(100, -np.pi, np.pi, name="mc_phi", label=r"MC $\phi$ [rad]")
    )
    hists['mc_theta'] = Hist(
        Regular(100, 0, np.pi, name="mc_theta", label=r"MC $\theta$ [rad]")
    )
    hists['mc_vtx_z'] = Hist(
        Regular(100, -500, 500, name="mc_vtx_z", label="MC Vertex Z [mm]")
    )
    hists['mc_hits_count'] = Hist(
        Regular(50, 0, 50, name="mc_hits_count", label="MC Hits Count")
    )
    
    # Track parameters
    hists['pdg'] = Hist(
        Regular(100, -3000, 3000, name="pdg", label="PDG Code")
    )
    hists['tp_phi'] = Hist(
        Regular(100, -np.pi, np.pi, name="tp_phi", label=r"Track Param $\phi$ [rad]")
    )
    hists['tp_theta'] = Hist(
        Regular(100, 0, np.pi, name="tp_theta", label=r"Track Param $\theta$ [rad]")
    )
    hists['tp_time'] = Hist(
        Regular(100, -10, 100, name="tp_time", label="Track Time [ns]")
    )
    hists['qoverp'] = Hist(
        Regular(100, -2, 2, name="qoverp", label=r"q/p [c/GeV]")
    )
    
    # Surface and location
    hists['surface'] = Hist(
        Regular(100, 0, 1e10, name="surface", label="Surface ID")
    )
    hists['loc0'] = Hist(
        Regular(100, -100, 100, name="loc0", label="Local Position 0 [mm]")
    )
    hists['loc1'] = Hist(
        Regular(100, -100, 100, name="loc1", label="Local Position 1 [mm]")
    )
    
    # Covariance matrix elements (log scale might be better for these)
    hists['cov_loc0'] = Hist(
        Regular(100, 0, 10, name="cov_loc0", label=r"Cov(loc0,loc0) [mm$^2$]")
    )
    hists['cov_loc1'] = Hist(
        Regular(100, 0, 10, name="cov_loc1", label=r"Cov(loc1,loc1) [mm$^2$]")
    )
    hists['cov_phi'] = Hist(
        Regular(100, 0, 0.1, name="cov_phi", label=r"Cov($\phi$,$\phi$) [rad$^2$]")
    )
    hists['cov_theta'] = Hist(
        Regular(100, 0, 0.1, name="cov_theta", label=r"Cov($\theta$,$\theta$) [rad$^2$]")
    )
    hists['cov_qoverp'] = Hist(
        Regular(100, 0, 0.5, name="cov_qoverp", label=r"Cov(q/p,q/p) [(c/GeV)$^2$]")
    )
    hists['cov_time'] = Hist(
        Regular(100, 0, 1e8, name="cov_time", label=r"Cov(time,time) [ns$^2$]")
    )
    
    # Perigee coordinates
    hists['perigee_x'] = Hist(
        Regular(100, -50, 50, name="perigee_x", label="Perigee X [mm]")
    )
    hists['perigee_y'] = Hist(
        Regular(100, -50, 50, name="perigee_y", label="Perigee Y [mm]")
    )
    hists['perigee_z'] = Hist(
        Regular(100, -500, 500, name="perigee_z", label="Perigee Z [mm]")
    )
    
    # First hit properties
    hists['fhit_id'] = Hist(
        Regular(100, 0, 1000, name="fhit_id", label="First Hit ID")
    )
    hists['fhit_time'] = Hist(
        Regular(100, 0, 100, name="fhit_time", label="First Hit Time [ns]")
    )
    hists['fhit_plane'] = Hist(
        Regular(20, 0, 20, name="fhit_plane", label="First Hit Plane")
    )
    hists['fhit_ring'] = Hist(
        Regular(20, 0, 20, name="fhit_ring", label="First Hit Ring")
    )
    hists['fhit_pad'] = Hist(
        Regular(100, 0, 200, name="fhit_pad", label="First Hit Pad")
    )
    hists['fhit_ztogem'] = Hist(
        Regular(100, -50, 50, name="fhit_ztogem", label="First Hit Z to GEM [mm]")
    )
    hists['fhit_true_x'] = Hist(
        Regular(100, -500, 500, name="fhit_true_x", label="First Hit True X [mm]")
    )
    hists['fhit_true_y'] = Hist(
        Regular(100, -500, 500, name="fhit_true_y", label="First Hit True Y [mm]")
    )
    hists['fhit_true_z'] = Hist(
        Regular(100, -2000, 2000, name="fhit_true_z", label="First Hit True Z [mm]")
    )
    
    return hists


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


def plot_2d_correlation(df, col1, col2, output_dir):
    """Create 2D correlation plots for selected variable pairs"""
    fig, ax = plt.subplots(figsize=(8, 6))
    
    # Filter out NaN values
    mask = df[col1].notna() & df[col2].notna()
    x_data = df.loc[mask, col1]
    y_data = df.loc[mask, col2]
    
    if len(x_data) > 0:
        # Create 2D histogram
        h, xedges, yedges = np.histogram2d(x_data, y_data, bins=50)
        extent = [xedges[0], xedges[-1], yedges[0], yedges[-1]]
        
        im = ax.imshow(h.T, origin='lower', extent=extent, aspect='auto', cmap='viridis')
        plt.colorbar(im, ax=ax, label='Counts')
        
        ax.set_xlabel(col1)
        ax.set_ylabel(col2)
        ax.set_title(f'{col2} vs {col1}')
        
        output_file = output_dir / f'corr_{col1}_vs_{col2}.png'
        plt.savefig(output_file, dpi=100, bbox_inches='tight')
        plt.close()
        
        return output_file
    return None


def create_summary_plots(df, output_dir):
    """Create summary plots combining multiple variables"""
    
    # 1. Momentum vs angles
    if all(col in df.columns for col in ['mc_mom', 'mc_theta', 'mc_phi']):
        fig, axes = plt.subplots(1, 2, figsize=(14, 5))
        
        # Momentum vs theta
        mask = df['mc_mom'].notna() & df['mc_theta'].notna()
        axes[0].scatter(df.loc[mask, 'mc_theta'], df.loc[mask, 'mc_mom'], 
                       alpha=0.5, s=1)
        axes[0].set_xlabel(r'MC $\theta$ [rad]')
        axes[0].set_ylabel('MC Momentum [GeV/c]')
        axes[0].set_title('Momentum vs Theta')
        axes[0].grid(True, alpha=0.3)
        
        # Momentum vs phi
        mask = df['mc_mom'].notna() & df['mc_phi'].notna()
        axes[1].scatter(df.loc[mask, 'mc_phi'], df.loc[mask, 'mc_mom'], 
                       alpha=0.5, s=1)
        axes[1].set_xlabel(r'MC $\phi$ [rad]')
        axes[1].set_ylabel('MC Momentum [GeV/c]')
        axes[1].set_title('Momentum vs Phi')
        axes[1].grid(True, alpha=0.3)
        
        plt.suptitle('Track Kinematics Correlations')
        output_file = output_dir / 'summary_kinematics.png'
        plt.savefig(output_file, dpi=100, bbox_inches='tight')
        plt.close()
    
    # 2. Perigee distribution
    if all(col in df.columns for col in ['perigee_x', 'perigee_y']):
        fig, ax = plt.subplots(figsize=(8, 8))
        
        mask = df['perigee_x'].notna() & df['perigee_y'].notna()
        h, xedges, yedges = np.histogram2d(
            df.loc[mask, 'perigee_x'], 
            df.loc[mask, 'perigee_y'], 
            bins=50
        )
        
        im = ax.imshow(h.T, origin='lower', 
                      extent=[xedges[0], xedges[-1], yedges[0], yedges[-1]], 
                      aspect='equal', cmap='viridis')
        plt.colorbar(im, ax=ax, label='Counts')
        
        ax.set_xlabel('Perigee X [mm]')
        ax.set_ylabel('Perigee Y [mm]')
        ax.set_title('Perigee XY Distribution')
        ax.grid(True, alpha=0.3)
        
        output_file = output_dir / 'summary_perigee_xy.png'
        plt.savefig(output_file, dpi=100, bbox_inches='tight')
        plt.close()


def main():
    parser = argparse.ArgumentParser(description='Analyze TDIS track CSV data')
    parser.add_argument('csv_file', help='Path to the input CSV file')
    parser.add_argument('--output-dir', default='plots_in_tracks', 
                       help='Output directory for plots (default: plots_in_tracks)')
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
        print(f"Loaded {len(df)} tracks with {len(df.columns)} columns")
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
    
    # Create correlation plots
    print("\nCreating correlation plots...")
    correlation_pairs = [
        ('mc_mom', 'mc_theta'),
        ('mc_mom', 'mc_phi'),
        ('mc_phi', 'mc_theta'),
        ('perigee_x', 'perigee_y'),
        ('perigee_z', 'mc_vtx_z'),
        ('tp_phi', 'mc_phi'),
        ('tp_theta', 'mc_theta')
    ]
    
    for col1, col2 in correlation_pairs:
        if col1 in df.columns and col2 in df.columns:
            output_file = plot_2d_correlation(df, col1, col2, output_dir)
            if output_file:
                print(f"  Saved: {output_file.name}")
    
    # Create summary plots
    print("\nCreating summary plots...")
    create_summary_plots(df, output_dir)
    
    print(f"\nAnalysis complete! All plots saved to {output_dir}")
    
    # Print summary statistics
    print("\nSummary Statistics:")
    if 'mc_mom' in df.columns:
        print(f"  Mean momentum: {df['mc_mom'].mean():.3f} GeV/c")
    if 'mc_hits_count' in df.columns:
        print(f"  Mean hits per track: {df['mc_hits_count'].mean():.1f}")
    if 'evt' in df.columns:
        print(f"  Number of events: {df['evt'].nunique()}")


if __name__ == '__main__':
    main()
