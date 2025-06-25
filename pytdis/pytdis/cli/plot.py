"""Plot command for PyTDIS CLI."""

import click
import matplotlib.pyplot as plt
from matplotlib.patches import Wedge
import numpy as np
import os
from pytdis.io import read_tdis_data_pandas
from pytdis.geometry import (
    get_pad_center,
    NUM_RINGS,
    NUM_PADS_PER_RING,
    FIRST_RING_INNER_RADIUS,
    LAST_RING_OUTER_RADIUS,
    RING_WIDTH,
    DELTA_THETA_DEG,
    DELTA_THETA
)


def draw_detector_pads(ax, alpha=0.3):
    """
    Draw the detector pad structure on the given axis.

    Parameters
    ----------
    ax : matplotlib axis
        The axis to draw on
    alpha : float
        Transparency of the pads (0-1)
    """
    # Loop over each ring
    for r in range(NUM_RINGS):
        r_inner = FIRST_RING_INNER_RADIUS + r * RING_WIDTH
        r_outer = FIRST_RING_INNER_RADIUS + (r + 1) * RING_WIDTH

        # Determine the angular offset for odd rings
        if (r+1) % 2 == 0:
            theta_offset = 0
        else:
            theta_offset = DELTA_THETA_DEG / 2

        # Loop over each pad in the ring
        for p in range(NUM_PADS_PER_RING):
            theta_start = p * DELTA_THETA_DEG + theta_offset
            theta_end = theta_start + DELTA_THETA_DEG

            # Create a wedge representing the pad
            wedge = Wedge(center=(0, 0),
                          r=r_outer,
                          theta1=theta_start,
                          theta2=theta_end,
                          width=RING_WIDTH,
                          facecolor='lightblue',
                          edgecolor='gray',
                          linewidth=0.2,
                          alpha=alpha)
            ax.add_patch(wedge)


def plot_hits(ax, df, max_events=None):
    """
    Plot hits on the detector.

    Parameters
    ----------
    ax : matplotlib axis
        The axis to plot on
    df : pd.DataFrame
        DataFrame containing hit data
    max_events : int or None
        Maximum number of events to plot
    """
    # Get unique tracks
    track_ids = df.index.get_level_values('track_id').unique()

    if max_events is not None:
        track_ids = track_ids[:max_events]

    # Plot hits for each track
    calc_x_list = []
    calc_y_list = []
    true_x_list = []
    true_y_list = []

    for track_id in track_ids:
        track_hits = df.loc[track_id]

        for _, hit in track_hits.iterrows():
            # Get calculated position from ring/pad
            ring = int(hit['ring'])
            pad = int(hit['pad'])

            try:
                calc_x, calc_y = get_pad_center(ring, pad)
                calc_x_list.append(calc_x)
                calc_y_list.append(calc_y)

                # True position (convert from meters to cm)
                true_x = hit['true_x'] * 100  # m to cm
                true_y = hit['true_y'] * 100  # m to cm
                true_x_list.append(true_x)
                true_y_list.append(true_y)
            except ValueError as e:
                click.echo(f"Warning: {e} for track {track_id}")

    # Plot all hits at once for better performance
    if calc_x_list:
        # Plot calculated positions in green
        ax.scatter(calc_x_list, calc_y_list, c='green', s=20, alpha=0.6, label='Calculated from Ring/Pad', zorder=3)

        # Plot true positions in red
        ax.scatter(true_x_list, true_y_list, c='red', s=2, alpha=0.6, label='True Position', zorder=2)

    return len(track_ids)


@click.command("plot", help="Plot TDIS detector data.")
@click.argument('filename', type=click.Path(exists=True))
@click.option('--events', '-n', type=int, help='Number of events to plot')
@click.option('--skip', '-s', type=int, default=0, help='Number of events to skip')
@click.option('--output', '-o', help='Output file for the plot')
@click.option('--format', '-f', type=click.Choice(['png', 'pdf', 'svg', 'html']),
              default='png', help='Output format')
@click.option('--no-pads', is_flag=True, help='Do not draw detector pads')
@click.option('--figsize', nargs=2, type=float, default=(10, 10),
              help='Figure size in inches (width height)')
@click.option('--dpi', type=int, default=100, help='DPI for output image')
@click.option('--title', help='Plot title')
def plot_command(filename, events, skip, output, format, no_pads, figsize, dpi, title):
    """
    Plot TDIS mTPC detector data.

    This command creates a visualization of track hits on the detector pads.
    Green dots show calculated positions from ring/pad indices, while red dots
    show true hit positions.

    Examples:
        pytdis plot data.txt -n 10
        pytdis plot data.txt --skip 5 --events 20 --output tracks.png
        pytdis plot data.txt --no-pads --title "Hit Distribution"
    """
    click.echo(f"Reading data from: {filename}")

    try:
        # Read the data
        df = read_tdis_data_pandas(filename, n_events=events, skip_events=skip)

        if df.empty:
            click.echo("Warning: No data found in file")
            return

        n_tracks = len(df.index.get_level_values('track_id').unique())
        n_hits = len(df)
        click.echo(f"Loaded {n_tracks} tracks with {n_hits} total hits")

        # Create figure
        fig, ax = plt.subplots(figsize=figsize, dpi=dpi)
        ax.set_aspect('equal')

        # Draw detector pads if requested
        if not no_pads:
            click.echo("Drawing detector pads...")
            draw_detector_pads(ax)

        # Plot hits
        click.echo("Plotting hits...")
        n_plotted = plot_hits(ax, df, max_events=events)
        click.echo(f"Plotted hits from {n_plotted} tracks")

        # Set plot properties
        ax.set_xlim(-LAST_RING_OUTER_RADIUS * 1.1, LAST_RING_OUTER_RADIUS * 1.1)
        ax.set_ylim(-LAST_RING_OUTER_RADIUS * 1.1, LAST_RING_OUTER_RADIUS * 1.1)
        ax.set_xlabel('X [cm]')
        ax.set_ylabel('Y [cm]')

        if title:
            ax.set_title(title)
        else:
            ax.set_title(f'TDIS mTPC Hit Distribution ({n_plotted} tracks)')

        # Add legend
        ax.legend(loc='upper right')

        # Add grid
        ax.grid(True, alpha=0.3)

        # Save or show
        if output:
            output_path = output
            if not output_path.endswith(f'.{format}'):
                output_path = f"{output_path}.{format}"

            plt.savefig(output_path, format=format, bbox_inches='tight', dpi=dpi)
            click.echo(f"Plot saved to: {output_path}")
        else:
            click.echo("Showing plot (close window to continue)...")
            plt.show()

        plt.close()

    except FileNotFoundError:
        click.echo(f"Error: File '{filename}' not found", err=True)
    except Exception as e:
        click.echo(f"Error: {e}", err=True)
        import traceback
        traceback.print_exc()