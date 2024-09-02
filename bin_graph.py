import pathlib
import numpy as np
import matplotlib.pyplot as plt


def process(frame):
    for k in frame:
        m, i, r = [seg[1:] for seg in k.split("-")]
        frame[k]["m"] = m
        frame[k]["i"] = i
        frame[k]["r"] = r
        frame[k]["pct-gc"] = frame[k]["gc-time"] / frame[k]["total-time"]
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
            ax.bar(x + offset, data[r], width, label=f"r= {rs[r]}")

        # Adding labels and title
        ax.set_xlabel("Number of Iterations")
        ax.set_ylabel(measurement)
        ax.set_title(title)
        ax.set_xticks(x)
        ax.set_xticklabels(Is)
        ax.legend()
        # Display the plot
        fig.savefig(f"{outputFolder}/{title}.pdf")

    mkChart(makeNormalisedData("total-time").T, "total-time", "Total time (h/p)")
    mkChart(makeNormalisedData("pct-gc").T, "pct-gc", "proportion of time spent on gc")
    mkChart(makeNormalisedData("L2D_CACHE_REFILL").T, "cache-refils", "")
    mkChart(makeNormalisedData("rss-kb").T, "rss-kb", "")
