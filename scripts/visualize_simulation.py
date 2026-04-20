#!/usr/bin/python3
"""
Visualize wave propagation simulation output from tsunami_lab.

Reads solution_*.csv files and plots height and momentum_x over x
for each time step, either as a static multi-panel figure or an animation.
"""

import argparse
import glob
import sys
import os
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.animation as animation


def load_solutions(directory: str = ".") -> list[tuple[int, pd.DataFrame]]:
    pattern = os.path.join(directory, "solution_*.csv")
    files = sorted(glob.glob(pattern), key=lambda f: int(f.split("solution_")[1].split(".csv")[0]))
    if not files:
        print(f"No solution_*.csv files found in '{directory}'.")
        sys.exit(1)
    return [(int(f.split("solution_")[1].split(".csv")[0]), pd.read_csv(f)) for f in files]


def plot_static(solutions: list[tuple[int, pd.DataFrame]], output: str = "solution_static.png", title: str = "Tsunami Lab — Wave Propagation") -> None:
    n = len(solutions)
    fig, axes = plt.subplots(2, n, figsize=(4 * n, 6), sharey="row", layout="constrained")
    if n == 1:
        axes = [[axes[0]], [axes[1]]]

    for col, (idx, df) in enumerate(solutions):
        ax_h = axes[0][col]
        ax_m = axes[1][col]

        ax_h.plot(df["x"], df["height"], color="skyblue", linewidth=1.5)
        ax_h.set_title(f"Step {idx}", fontsize=10)
        ax_h.set_xlabel("x")
        if col == 0:
            ax_h.set_ylabel("Height h")
        ax_h.grid(True, linestyle="--", alpha=0.5)

        ax_m.plot(df["x"], df["momentum_x"], color="coral", linewidth=1.5)
        ax_m.set_xlabel("x")
        if col == 0:
            ax_m.set_ylabel("Momentum hu")
        ax_m.grid(True, linestyle="--", alpha=0.5)

    fig.suptitle(title, fontsize=13, fontweight="bold")
    plt.savefig(output, dpi=150)
    print(f"Saved static plot to '{output}'.")
    plt.show()


def plot_animation(solutions: list[tuple[int, pd.DataFrame]], output: str = "solution_animation.gif", title_prefix: str = "Tsunami Lab — Wave Propagation", duration: float = 10.0) -> None:
    fig, (ax_h, ax_m) = plt.subplots(2, 1, figsize=(8, 6))

    all_h = pd.concat([df["height"] for _, df in solutions])
    all_m = pd.concat([df["momentum_x"] for _, df in solutions])
    h_min, h_max = all_h.min(), all_h.max()
    m_min, m_max = all_m.min(), all_m.max()
    h_pad = (h_max - h_min) * 0.05 or 0.5
    m_pad = (m_max - m_min) * 0.05 or 0.5

    line_h, = ax_h.plot([], [], color="skyblue", linewidth=1.5)
    ax_h.set_xlim(solutions[0][1]["x"].min(), solutions[0][1]["x"].max())
    ax_h.set_ylim(h_min - h_pad, h_max + h_pad)
    ax_h.set_ylabel("Height h")
    ax_h.grid(True, linestyle="--", alpha=0.5)

    line_m, = ax_m.plot([], [], color="coral", linewidth=1.5)
    ax_m.set_xlim(solutions[0][1]["x"].min(), solutions[0][1]["x"].max())
    ax_m.set_ylim(m_min - m_pad, m_max + m_pad)
    ax_m.set_ylabel("Momentum hu")
    ax_m.set_xlabel("x")
    ax_m.grid(True, linestyle="--", alpha=0.5)

    title = fig.suptitle("", fontsize=12, fontweight="bold")

    def init():
        line_h.set_data([], [])
        line_m.set_data([], [])
        return line_h, line_m

    def update(frame):
        idx, df = solutions[frame]
        line_h.set_data(df["x"], df["height"])
        line_m.set_data(df["x"], df["momentum_x"])
        title.set_text(f"{title_prefix}  |  Output step {idx}")
        return line_h, line_m, title

    fps = max(1, round(len(solutions) / duration))
    ani = animation.FuncAnimation(
        fig, update, frames=len(solutions),
        init_func=init, interval=1000 // fps, blit=False, repeat=True
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
