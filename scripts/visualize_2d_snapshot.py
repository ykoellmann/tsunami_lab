#!/usr/bin/env python3
"""
Render a single snapshot from a 2D simulation as a filled-contour figure.
Useful for showing the bathymetric obstacle run.

Usage:
    python3 scripts/visualize_2d_snapshot.py <solution_XXXX.csv> <output.png> [origin_x=-50] [origin_y=-50]
"""
import sys
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

def load_frame(path):
    data = {}
    sim_time = None
    with open(path) as f:
        for line in f:
            line = line.strip()
            if line.startswith('# sim_time='):
                sim_time = float(line.split('=')[1])
            elif line.startswith('#') or line.startswith('x'):
                continue
            else:
                parts = line.split(',')
                x = float(parts[0]); y = float(parts[1])
                h = float(parts[2]); b = float(parts[3])
                data[(x, y)] = (h, b)
    return sim_time, data

def main():
    if len(sys.argv) < 3:
        print("usage: visualize_2d_snapshot.py <solution.csv> <output.png> [origin_x=-50] [origin_y=-50]")
        sys.exit(1)
    csv_path = sys.argv[1]
    out_path = sys.argv[2]
    origin_x = float(sys.argv[3]) if len(sys.argv) > 3 else -50.0
    origin_y = float(sys.argv[4]) if len(sys.argv) > 4 else -50.0

    sim_time, data = load_frame(csv_path)

    xs = sorted(set(k[0] for k in data))
    ys = sorted(set(k[1] for k in data))
    xi = {v: i for i, v in enumerate(xs)}
    yi = {v: i for i, v in enumerate(ys)}
    nx, ny = len(xs), len(ys)

    eta = np.zeros((ny, nx))
    bath = np.zeros((ny, nx))
    for (x, y), (h, b) in data.items():
        eta[yi[y], xi[x]] = h + b
        bath[yi[y], xi[x]] = b

    xs_phys = np.array(xs) + origin_x
    ys_phys = np.array(ys) + origin_y

    fig, axes = plt.subplots(1, 2, figsize=(13, 5))

    im0 = axes[0].pcolormesh(xs_phys, ys_phys, eta, shading='auto', cmap='viridis')
    fig.colorbar(im0, ax=axes[0], label='η = h + b  [m]')
    axes[0].set_title(f'Water surface η  (t = {sim_time:.2f} s)')
    axes[0].set_xlabel('x  [m]'); axes[0].set_ylabel('y  [m]')
    axes[0].set_aspect('equal')

    im1 = axes[1].pcolormesh(xs_phys, ys_phys, bath, shading='auto', cmap='terrain')
    fig.colorbar(im1, ax=axes[1], label='b  [m]')
    axes[1].set_title('Bathymetry b')
    axes[1].set_xlabel('x  [m]'); axes[1].set_ylabel('y  [m]')
    axes[1].set_aspect('equal')

    fig.tight_layout()
    fig.savefig(out_path, dpi=150, bbox_inches='tight')
    print(f"Saved → {out_path}")

if __name__ == '__main__':
    main()
