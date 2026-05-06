#!/usr/bin/env python3
"""
Compare the 1D dam break to the 2D circular dam break at stations along the
radial (positive-x) axis.

The 2D station CSV files are read from <sim2d_dir>/stations/.
The 1D solution is sampled from solution_*.csv snapshots at the requested x positions.

Usage:
    python3 scripts/compare_1d_2d_stations.py \
        --sim2d  simulations/dambreak2d_XXX   \
        --sim1d  simulations/dambreak_XXX     \
        --output sphinx/source/_images/compare_1d_2d.png \
        --stations r15 r25 r35               \
        --x1d    15   25   35                \
        --origin1d 0                         \
        --origin2d -50

The 1D CSV coordinate system has origin at origin1d (usually 0).
The 2D CSV coordinate system has origin at origin2d (usually -50).
Pass --x1d as physical positions corresponding to each station name.
"""
import sys
import os
import glob
import argparse
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt


def read_station_csv(path):
    """Read a station CSV written by io::Stations. Columns: time,h,hu,hv,b."""
    times, etas = [], []
    with open(path) as f:
        header = None
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            parts = line.split(',')
            if header is None:
                header = parts
                continue
            t   = float(parts[0])
            h   = float(parts[1])
            b   = float(parts[4]) if len(parts) > 4 else 0.0
            times.append(t)
            etas.append(h + b)
    return np.array(times), np.array(etas)


def sample_1d_snapshots(sim_dir, x_phys, origin):
    """
    Sample η at physical position x_phys from all 1D snapshots.
    CSV x-column = cell-centre offset from 0; physical = csv_x + origin.
    Returns (times, etas).
    """
    files = sorted(glob.glob(os.path.join(sim_dir, 'solution_*.csv')),
                   key=lambda f: int(os.path.basename(f).replace('solution_','').replace('.csv','')))
    times, etas = [], []
    for fpath in files:
        sim_time = None
        best_dist = None
        best_eta  = None
        with open(fpath) as f:
            for line in f:
                line = line.strip()
                if line.startswith('# sim_time='):
                    sim_time = float(line.split('=')[1])
                elif line.startswith('#') or line.startswith('x'):
                    continue
                else:
                    parts = line.split(',')
                    csv_x = float(parts[0])
                    phys_x = csv_x + origin
                    dist = abs(phys_x - x_phys)
                    h = float(parts[2])
                    b = float(parts[3])
                    if best_dist is None or dist < best_dist:
                        best_dist = dist
                        best_eta  = h + b
        if sim_time is not None and best_eta is not None:
            times.append(sim_time)
            etas.append(best_eta)
    return np.array(times), np.array(etas)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--sim2d',     required=True)
    p.add_argument('--sim1d',     required=True)
    p.add_argument('--output',    required=True)
    p.add_argument('--stations',  nargs='+', default=['r15','r25','r35'])
    p.add_argument('--x1d',       nargs='+', type=float, default=[15.0, 25.0, 35.0])
    p.add_argument('--origin1d',  type=float, default=0.0)
    p.add_argument('--origin2d',  type=float, default=-50.0)
    args = p.parse_args()

    station_names = args.stations
    x1d_positions = args.x1d
    assert len(station_names) == len(x1d_positions), \
        "--stations and --x1d must have the same length"

    n = len(station_names)
    fig, axes = plt.subplots(1, n, figsize=(5 * n, 4), sharey=False)
    if n == 1:
        axes = [axes]

    for ax, name, x1d in zip(axes, station_names, x1d_positions):
        # --- 2D station ---
        st_path = os.path.join(args.sim2d, 'stations', f'{name}.csv')
        if os.path.exists(st_path):
            t2, eta2 = read_station_csv(st_path)
            ax.plot(t2, eta2, label='2D circular', color='tab:blue', linewidth=1.5)
        else:
            print(f"Warning: 2D station file not found: {st_path}")

        # --- 1D snapshot sampling ---
        t1, eta1 = sample_1d_snapshots(args.sim1d, x1d, args.origin1d)
        ax.plot(t1, eta1, 'o--', label='1D dam break', color='tab:orange',
                linewidth=1.5, markersize=5)

        ax.set_title(f'Station {name}  (x = {x1d:.0f} m)')
        ax.set_xlabel('t  [s]')
        ax.set_ylabel('η = h + b  [m]')
        ax.legend(fontsize=8)
        ax.grid(True, alpha=0.3)

    fig.suptitle('1D vs 2D: water surface η at radial stations', fontsize=12)
    fig.tight_layout()
    os.makedirs(os.path.dirname(args.output) or '.', exist_ok=True)
    fig.savefig(args.output, dpi=150, bbox_inches='tight')
    print(f"Saved → {args.output}")


if __name__ == '__main__':
    main()
