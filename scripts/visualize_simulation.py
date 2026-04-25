#!/usr/bin/env python3
"""
Visualize wave propagation simulation output from tsunami_lab.

Reads solution_*.csv files and plots height and momentum_x over x
for each time step, either as a static multi-panel figure or an animation.

When a 'bathymetry' column is present the top panel shows a side-view
cross-section with ground (brown) and water (blue) fills.  The y-axis
starts at the minimum bathymetry value so the full seabed is visible.
Without bathymetry the top panel shows plain water height (y starts at 0).
"""

import argparse
import glob
import sys
import os
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import matplotlib.animation as animation


def load_solutions(directory: str = ".") -> list[tuple[int, pd.DataFrame, float | None]]:
    pattern = os.path.join(directory, "solution_*.csv")
    files = sorted(glob.glob(pattern), key=lambda f: int(f.split("solution_")[1].split(".csv")[0]))
    if not files:
        print(f"No solution_*.csv files found in '{directory}'.")
        sys.exit(1)
    result = []
    for f in files:
        idx = int(f.split("solution_")[1].split(".csv")[0])
        with open(f) as fh:
            first = fh.readline()
        sim_time = float(first.split("=")[1]) if first.startswith("# sim_time=") else None
        df = pd.read_csv(f, comment="#")
        result.append((idx, df, sim_time))
    return result


def _has_bathymetry(solutions: list[tuple[int, pd.DataFrame, float | None]]) -> bool:
    return "bathymetry" in solutions[0][1].columns


def _draw_cross_section(ax, df: pd.DataFrame, bathy_floor: float, first_col: bool) -> tuple:
    """Fill ground + water on ax. Returns (fill_ground, fill_water, line_surface, line_bathy)."""
    bathy   = df["bathymetry"]
    surface = bathy + df["height"]

    fill_ground = ax.fill_between(df["x"], bathy_floor, bathy, color="#8B6914", alpha=0.9)
    fill_water  = ax.fill_between(df["x"], bathy, surface, color="steelblue", alpha=0.65)
    line_surface, = ax.plot(df["x"], surface, color="steelblue", linewidth=1.4)
    line_bathy,   = ax.plot(df["x"], bathy,   color="#4a3008",   linewidth=1.0)

    if first_col:
        ax.legend(
            handles=[
                mpatches.Patch(color="#8B6914", alpha=0.9,  label="ground"),
                mpatches.Patch(color="steelblue", alpha=0.65, label="water"),
            ],
            fontsize=8, loc="upper right",
        )
    return fill_ground, fill_water, line_surface, line_bathy


def plot_static(solutions: list[tuple[int, pd.DataFrame, float | None]], output: str = "solution_static.png", title: str = "Tsunami Lab — Wave Propagation") -> None:
    has_bathy = _has_bathymetry(solutions)
    n = len(solutions)

    fig, axes = plt.subplots(2, n, figsize=(4 * n, 8), sharey="row", layout="constrained")
    if n == 1:
        axes = [[axes[0]], [axes[1]]]

    if has_bathy:
        all_bathy = pd.concat([df["bathymetry"] for _, df, _ in solutions])
        all_h     = pd.concat([df["height"]     for _, df, _ in solutions])
        bathy_floor  = all_bathy.min() - (all_bathy.max() - all_bathy.min()) * 0.05
        surface_max  = (all_bathy + all_h).max()
        cs_pad       = (surface_max - bathy_floor) * 0.05 or 0.1

    for col, (idx, df, sim_time) in enumerate(solutions):
        ax_top = axes[0][col]
        ax_m   = axes[1][col]
        time_str = f"  (t = {sim_time:.2f} s)" if sim_time is not None else ""
        ax_top.set_title(f"Step {idx}{time_str}", fontsize=10)

        if has_bathy:
            _draw_cross_section(ax_top, df, bathy_floor, col == 0)
            ax_top.set_ylim(bathy_floor, surface_max + cs_pad)
            if col == 0:
                ax_top.set_ylabel("Elevation (m)")
        else:
            ax_top.plot(df["x"], df["height"], color="skyblue", linewidth=1.5)
            ax_top.set_ylim(bottom=0)
            if col == 0:
                ax_top.set_ylabel("Height h")

        ax_top.set_xlabel("x")
        ax_top.grid(True, linestyle="--", alpha=0.4)

        ax_m.plot(df["x"], df["momentum_x"], color="coral", linewidth=1.5)
        ax_m.set_xlabel("x")
        if col == 0:
            ax_m.set_ylabel("Momentum hu")
        ax_m.grid(True, linestyle="--", alpha=0.5)

    fig.suptitle(title, fontsize=13, fontweight="bold")
    plt.savefig(output, dpi=150)
    print(f"Saved static plot to '{output}'.")
    plt.show()


def plot_animation(solutions: list[tuple[int, pd.DataFrame, float | None]], output: str = "solution_animation.gif", title_prefix: str = "Tsunami Lab — Wave Propagation", duration: float = 10.0) -> None:
    has_bathy = _has_bathymetry(solutions)
    fig, (ax_top, ax_m) = plt.subplots(2, 1, figsize=(8, 8))

    x     = solutions[0][1]["x"]
    all_h = pd.concat([df["height"]      for _, df, _ in solutions])
    all_m = pd.concat([df["momentum_x"]  for _, df, _ in solutions])
    h_max = all_h.max()
    m_min, m_max = all_m.min(), all_m.max()
    m_pad = (m_max - m_min) * 0.05 or 0.5

    fill_ground = fill_water = line_surface = line_bathy = line_h = None

    if has_bathy:
        all_bathy   = pd.concat([df["bathymetry"] for _, df, _ in solutions])
        bathy_floor = all_bathy.min() - (all_bathy.max() - all_bathy.min()) * 0.05
        surface_max = (all_bathy + all_h).max()
        cs_pad      = (surface_max - bathy_floor) * 0.05 or 0.1

        ax_top.set_xlim(x.min(), x.max())
        ax_top.set_ylim(bathy_floor, surface_max + cs_pad)
        ax_top.set_ylabel("Elevation (m)")
        ax_top.grid(True, linestyle="--", alpha=0.4)

        _b0 = solutions[0][1]["bathymetry"]
        _s0 = _b0 + solutions[0][1]["height"]
        fill_ground, fill_water, line_surface, line_bathy = _draw_cross_section(ax_top, solutions[0][1], bathy_floor, True)
    else:
        h_pad  = h_max * 0.05 or 0.5
        line_h, = ax_top.plot([], [], color="skyblue", linewidth=1.5)
        ax_top.set_xlim(x.min(), x.max())
        ax_top.set_ylim(0, h_max + h_pad)
        ax_top.set_ylabel("Height h")
        ax_top.grid(True, linestyle="--", alpha=0.5)

    line_m, = ax_m.plot([], [], color="coral", linewidth=1.5)
    ax_m.set_xlim(x.min(), x.max())
    ax_m.set_ylim(m_min - m_pad, m_max + m_pad)
    ax_m.set_ylabel("Momentum hu")
    ax_m.set_xlabel("x")
    ax_m.grid(True, linestyle="--", alpha=0.5)

    title = fig.suptitle("", fontsize=12, fontweight="bold")
    fig.tight_layout()

    def init():
        line_m.set_data([], [])
        if not has_bathy:
            line_h.set_data([], [])
        return ()

    def update(frame):
        nonlocal fill_ground, fill_water
        idx, df, sim_time = solutions[frame]
        line_m.set_data(df["x"], df["momentum_x"])
        time_str = f"  |  t = {sim_time:.2f} s" if sim_time is not None else ""
        title.set_text(f"{title_prefix}  |  Output step {idx}{time_str}")

        if has_bathy:
            if fill_ground is not None:
                fill_ground.remove()
            if fill_water is not None:
                fill_water.remove()
            bathy   = df["bathymetry"]
            surface = bathy + df["height"]
            fill_ground = ax_top.fill_between(df["x"], bathy_floor, bathy, color="#8B6914", alpha=0.9)
            fill_water  = ax_top.fill_between(df["x"], bathy, surface, color="steelblue", alpha=0.65)
            line_surface.set_data(df["x"], surface)
            line_bathy.set_data(df["x"], bathy)
        else:
            line_h.set_data(df["x"], df["height"])

        return ()

    fps = max(1, round(len(solutions) / duration))
    ani = animation.FuncAnimation(
        fig, update, frames=len(solutions),
        init_func=init, interval=1000 // fps, blit=False, repeat=True,
    )

    ani.save(output, writer="pillow", fps=fps)
    print(f"Saved animation to '{output}'.")
    plt.show()


def main():
    parser = argparse.ArgumentParser(description="Visualize tsunami simulation output.")
    parser.add_argument("directory", nargs="?", default=".", help="Directory containing solution_*.csv files.")
    parser.add_argument("-a", "--animate", action="store_true", help="Create an animation instead of a static plot.")
    parser.add_argument("-o", "--output", help="Output filename.")
    parser.add_argument("-n", "--name", default="Tsunami Lab — Wave Propagation", help="Title of the plot.")
    parser.add_argument("-d", "--duration", type=float, default=10.0, help="Animation duration in seconds (default: 10).")

    args = parser.parse_args()

    solutions = load_solutions(args.directory)
    print(f"Loaded {len(solutions)} solution file(s).")

    if args.animate:
        output = args.output if args.output else "solution_animation.gif"
        plot_animation(solutions, output=output, title_prefix=args.name, duration=args.duration)
    else:
        output = args.output if args.output else "solution_static.png"
        plot_static(solutions, output=output, title=args.name)


if __name__ == "__main__":
    main()
