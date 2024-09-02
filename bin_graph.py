import pathlib
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.patches import Patch

# colour palette sourced from here:
# https://gka.github.io/palettes/#/5|s|006eea,ff8457|ffffe0,ff005e,93003a|1|1
colours = ["#006eea", "#8872c6", "#ba77a2", "#e07d7e", "#ff8457"]


def process(frame):
    for k in frame:
        m, i, r = [seg[1:] for seg in k.split("-")]
        frame[k]["m"] = m
        frame[k]["i"] = i
        frame[k]["r"] = r
        frame[k]["pct-gc"] = frame[k]["gc-time"] / frame[k]["total-time"]
        frame[k]["pct-gc-mk"] = frame[k]["mark-time"] / frame[k]["gc-time"]
    return frame


def make_cluster_keys(frame, treeDepth):
    Is = sorted(set([frame[x]["i"] for x in frame]), key=int)
    rs = sorted(set([frame[x]["r"] for x in frame]), key=int)
    cluster_keys = [[f"m{treeDepth}-i{x}-r{r}" for r in rs] for x in Is]

    return (Is, rs, cluster_keys)


def makeGraphs(data, outputFolder, treeDepth):
    output = pathlib.Path(outputFolder).resolve()
    output.mkdir(mode=0o750, parents=True, exist_ok=True)

    hybrid = process(data["hybrid"]["binary_tree_profiling"])
    purecap = process(data["purecap"]["binary_tree_profiling"])

    Is, rs, cluster_keys = make_cluster_keys(hybrid, treeDepth)

    def makeNormalisedData(dataKey):
        return np.array(
            [
                [hybrid[k][dataKey] / purecap[k][dataKey] for k in row]
                for row in cluster_keys
            ]
        )

    # Your Data
    def mkChart(data, title, measurement):
        # Number of bars in each cluster
        num_bars = len(data[0])
        # Positions of the bars on the x-axis
        x = np.arange(num_bars)

        # The width of the bars
        width = 1 / (len(rs) + 1)

        # Create the plot
        fig, ax = plt.subplots()

        # Plotting the bars for each cluster
        offsets = [(i * width) - (width * (len(rs) - 1) / 2) for i in range(len(rs))]
        for r in range(len(rs)):
            offset = offsets[r]
            ax.bar(x + offset, data[r], width, label=f"r= {rs[r]}", color=colours[r])

        # Adding labels and title
        ax.set_xlabel("Number of Iterations")
        ax.set_ylabel(measurement)
        ax.set_title(title)
        ax.set_xticks(x)
        ax.set_xticklabels(Is)

        ax.legend()
        # Display the plot
        fig.savefig(f"{outputFolder}/{title}.pdf")

    def markTimeChart():
        hData = np.array(
            [[hybrid[key]["pct-gc-mk"] for key in row] for row in cluster_keys]
        ).T
        pData = np.array(
            [[purecap[key]["pct-gc-mk"] for key in row] for row in cluster_keys]
        ).T

        x = np.arange(len(hData[0]))
        num_bars = len(rs)
        width = 1 / (num_bars * 3 + 3)
        offsets = [(i * 3 * width) - 1 / 3 for i in range(len(rs))]

        fig, ax = plt.subplots()
        for i in range(len(rs)):
            offset = offsets[i]
            ax.bar(
                x + offset,
                hData[i],
                width,
                color=colours[i],
                label=f"r= {rs[i]}",
                edgecolor="black",
            )
            ax.bar(
                x + offset,
                1 - hData[i],
                width,
                bottom=hData[i],
                color="black",
                edgecolor="black",
            )

            ax.bar(
                x + offset + width,
                pData[i],
                width,
                hatch="..",
                color=colours[i],
                edgecolor="black",
            )
            ax.bar(
                x + offset + width,
                1 - pData[i],
                width,
                bottom=pData[i],
                color="black",
                edgecolor="black",
            )

        ax.set_xlabel("Number of Iterations")
        ax.set_ylabel("proportion of GC time spent marking")
        ax.set_title("% time spent Marking")
        ax.set_xticks(x)
        ax.set_xticklabels(Is)

        custom_objs = [Patch(facecolor=c, label="Color Patch") for c in colours] + [
            Patch(facecolor="none", edgecolor="black"),
            Patch(facecolor="none", edgecolor="black", hatch=".."),
        ]

        ax.legend(
            custom_objs,
            [f"r= {r}" for r in rs] + ["hybrid", "purecap"],
            bbox_to_anchor=(1, 1),
        )

        fig.savefig(f"{outputFolder}/mark-time.pdf")

    mkChart(makeNormalisedData("total-time").T, "total-time", "Total time (h/p)")
    mkChart(makeNormalisedData("pct-gc").T, "pct-gc", "proportion of time spent on gc")
    mkChart(makeNormalisedData("L2D_CACHE_REFILL").T, "cache-refils", "")
    mkChart(makeNormalisedData("rss-kb").T, "rss-kb", "")
    markTimeChart()
