#!/usr/bin/env python3
"""Generate a tileable blue noise texture using the void-and-cluster method.

Requires: numpy, scipy, Pillow

Usage:
    python3 generate_blue_noise.py [--size 64] [--output blue_noise_64.png]
"""

import argparse
import numpy as np
from PIL import Image


def make_blue_noise(size: int, seed: int = 42) -> np.ndarray:
    """Generate a blue noise texture using void-and-cluster.

    Args:
        size: Width and height of the square texture.
        seed: Random seed for reproducibility.

    Returns:
        A (size, size) uint8 array with values 0–255.
    """
    rng = np.random.default_rng(seed)
    n = size * size
    sigma = 1.5

    # Seed a sparse binary pattern using Mitchell's best-candidate algorithm
    binary = np.zeros((size, size), dtype=bool)
    num_initial = n // 10
    points: list[tuple[int, int]] = []

    for _ in range(num_initial):
        if not points:
            y, x = rng.integers(0, size), rng.integers(0, size)
        else:
            best_dist = -1
            best_pos = (0, 0)
            for _ in range(20):
                cy, cx = int(rng.integers(0, size)), int(rng.integers(0, size))
                min_d = float("inf")
                for py, px in points:
                    # Toroidal distance for seamless tiling
                    dy = min(abs(cy - py), size - abs(cy - py))
                    dx = min(abs(cx - px), size - abs(cx - px))
                    d = dy * dy + dx * dx
                    if d < min_d:
                        min_d = d
                if min_d > best_dist:
                    best_dist = min_d
                    best_pos = (cy, cx)
            y, x = best_pos
        points.append((y, x))
        binary[y, x] = True

    def filtered_energy(bp: np.ndarray) -> np.ndarray:
        """Compute gaussian-weighted neighbor density (toroidal)."""
        energy = np.zeros((size, size), dtype=float)
        for dy in range(-5, 6):
            for dx in range(-5, 6):
                w = np.exp(-(dy * dy + dx * dx) / (2 * sigma * sigma))
                energy += w * np.roll(
                    np.roll(bp.astype(float), -dy, axis=0), -dx, axis=1
                )
        return energy

    rank = np.zeros((size, size), dtype=int)

    # Phase 1: Remove tightest-cluster points, assign decreasing ranks
    temp = binary.copy()
    count = int(np.sum(temp))
    for i in range(count, 0, -1):
        energy = filtered_energy(temp)
        energy[~temp] = -np.inf
        idx = np.unravel_index(np.argmax(energy), (size, size))
        rank[idx] = i - 1
        temp[idx] = False

    # Phase 2: Fill largest voids, assign increasing ranks
    temp = binary.copy()
    for i in range(count, n):
        energy = filtered_energy(temp)
        energy[temp] = np.inf
        idx = np.unravel_index(np.argmin(energy), (size, size))
        rank[idx] = i
        temp[idx] = True
        if i % 500 == 0:
            print(f"  Progress: {i}/{n}")

    return (rank.astype(float) / (n - 1) * 255).astype(np.uint8)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate a blue noise texture.")
    parser.add_argument("--size", type=int, default=64, help="Texture size (default: 64)")
    parser.add_argument("--seed", type=int, default=42, help="Random seed (default: 42)")
    parser.add_argument("--output", type=str, default=None, help="Output path (default: blue_noise_<size>.png)")
    args = parser.parse_args()

    if args.output is None:
        args.output = f"blue_noise_{args.size}.png"

    print(f"Generating {args.size}x{args.size} blue noise texture...")
    noise = make_blue_noise(args.size, args.seed)

    img = Image.fromarray(noise, mode="L")
    img.save(args.output)
    print(f"Saved to {args.output}")
    print(f"  Min: {noise.min()}, Max: {noise.max()}, Mean: {noise.mean():.1f}")


if __name__ == "__main__":
    main()
