#!/usr/bin/env python3
"""
Produce an animated GIF of the 2D circular dam break from solution_*.csv files.

Usage:
    python3 scripts/visualize_2d_animation.py <sim_dir> <output.gif> [origin_x=-50 origin_y=-50]

The CSV x/y columns are cell-centre offsets from 0 (no origin included).
Pass origin_x / origin_y to shift to physical coordinates for axis labels.
"""
import sys
import os
import glob
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.animation as animation

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

def build_grid(data, origin_x, origin_y):
    xs = sorted(set(k[0] for k in data))
    ys = sorted(set(k[1] for k in data))
    nx, ny = len(xs), len(ys)
    xi = {v: i for i, v in enumerate(xs)}
    yi = {v: i for i, v in enumerate(ys)}
    eta = np.zeros((ny, nx))
    for (x, y), (h, b) in data.items():
        eta[yi[y], xi[x]] = h + b
    # convert to physical coordinates for axis labels
    xs_phys = np.array(xs) + origin_x
    ys_phys = np.array(ys) + origin_y
    return xs_phys, ys_phys, eta

def main():
    if len(sys.argv) < 3:
        print("usage: visualize_2d_animation.py <sim_dir> <output.gif> [origin_x=-50] [origin_y=-50]")
        sys.exit(1)
    sim_dir  = sys.argv[1]
    out_path = sys.argv[2]
    origin_x = float(sys.argv[3]) if len(sys.argv) > 3 else -50.0
    origin_y = float(sys.argv[4]) if len(sys.argv) > 4 else -50.0

    files = sorted(glob.glob(os.path.join(sim_dir, 'solution_*.csv')),
                   key=lambda f: int(os.path.basename(f).replace('solution_','').replace('.csv','')))
    if not files:
        print(f"No solution_*.csv files in {sim_dir}")
        sys.exit(1)
    print(f"Loading {len(files)} frames...")

    frames = []
    for fpath in files:
        t, data = load_frame(fpath)
        xs, ys, eta = build_grid(data, origin_x, origin_y)
        frames.append((t, xs, ys, eta))

    vmin = min(f[3].min() for f in frames)
    vmax = max(f[3].max() for f in frames)

    fig, ax = plt.subplots(figsize=(7, 6))
    t0, xs, ys, eta0 = frames[0]
    im = ax.pcolormesh(xs, ys, eta0, shading='auto', cmap='viridis', vmin=vmin, vmax=vmax)
    cb = fig.colorbar(im, ax=ax, label='η = h + b  [m]')
    ax.set_xlabel('x  [m]')
    ax.set_ylabel('y  [m]')
    title = ax.set_title(f't = {t0:.2f} s')
    ax.set_aspect('equal')

    def update(i):
        t, xs, ys, eta = frames[i]
        im.set_array(eta.ravel())
        title.set_text(f't = {t:.2f} s')
        return im, title

    ani = animation.FuncAnimation(fig, update, frames=len(frames), interval=400, blit=False)
    ani.save(out_path, writer='pillow', dpi=100)
    print(f"Saved → {out_path}")

if __name__ == '__main__':
    main()
