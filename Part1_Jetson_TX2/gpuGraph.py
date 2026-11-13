#!/usr/bin/env python3

import os
from collections import deque

import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

GPU_LOAD_FILE = "/sys/devices/gpu.0/load"

SAMPLES = 240
INTERVAL_MS = 250

gpu_values = deque([0.0] * SAMPLES, maxlen=SAMPLES)
time_values = np.linspace(60, 0, SAMPLES)

fig, ax = plt.subplots(figsize=(8, 4))
fig.subplots_adjust(bottom=0.18)
fig.set_facecolor("#F2F1F0")

if fig.canvas.manager is not None:
    fig.canvas.manager.set_window_title(
        "GPU Activity Monitor"
    )

line, = ax.plot(time_values, list(gpu_values))

ax.set_xlim(60, 0)
ax.set_ylim(0, 100)
ax.set_title("GPU History")
ax.set_ylabel("GPU Usage (%)")
ax.set_xlabel("Seconds")
ax.grid(True, linestyle="dotted")

def read_gpu_load():
    with open(GPU_LOAD_FILE, "r") as gpu_file:
        value = gpu_file.read().strip()

    return float(value) / 10.0

def update_graph(frame):
    try:
        gpu_load = read_gpu_load()
    except (OSError, ValueError) as error:
        print("Unable to read Jetson GPU:", error)
        gpu_load = 0.0

    gpu_load = max(0.0, min(100.0, gpu_load))
    gpu_values.append(gpu_load)

    line.set_ydata(list(gpu_values))

    for collection in list(ax.collections):
        collection.remove()

    ax.fill_between(
        time_values,
        0,
        list(gpu_values),
        color="cyan",
        alpha=0.50
    )

    return (line,)

if not os.path.exists(GPU_LOAD_FILE):
    print("GPU load file was not found.")
    print("Run this script on the Jetson TX2.")
else:
    animation = FuncAnimation(
        fig,
        update_graph,
        interval=INTERVAL_MS,
        blit=False,
        cache_frame_data=False
    )

    plt.show()
