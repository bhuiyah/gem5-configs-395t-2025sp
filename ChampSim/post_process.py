#!/usr/bin/env python3
import os
import glob
import re
import matplotlib.pyplot as plt

def parse_arc_file(filepath):
    """
    Parse a large log file line-by-line to extract:
      p_history = [ ... ]
      b1_history = [ ... ]
      b2_history = [ ... ]
    Returns three lists of floats: (p_data, b1_data, b2_data).
    """
    p_line = None
    b1_line = None
    b2_line = None

    with open(filepath, 'r') as f:
        for line in f:
            line = line.strip()
            if line.startswith("p_history ="):
                p_line = line
            elif line.startswith("b1_history ="):
                b1_line = line
            elif line.startswith("b2_history ="):
                b2_line = line

    def parse_list(line_str):
        if not line_str:
            return []
        start = line_str.find('[')
        end = line_str.rfind(']')
        if start == -1 or end == -1:
            return []
        content = line_str[start+1:end]
        return [float(x.strip()) for x in content.split(',') if x.strip()]

    p_data  = parse_list(p_line)
    b1_data = parse_list(b1_line)
    b2_data = parse_list(b2_line)
    return p_data, b1_data, b2_data

def main():
    # Define experiment subdirectories and their descriptive title labels.
    subdirs = ["og", "bdouble", "bhalf", "pcache", "phalf"]
    subdir_titles = {
        "og": "faithful ARC",
        "bdouble": "ARC with ghost list size clamped to 2×NUM_WAY",
        "bhalf": "ARC with ghost list size clamped to ½×NUM_WAY",
        "pcache": "ARC with p initialized to NUM_WAY",
        "phalf": "ARC with p initialized to NUM_WAY/2"
    }
    
    # Define which metrics to plot for each subdir.
    subdir_plot_options = {
        "og":       ["p", "b1", "b2"],
        "bdouble":  ["b1", "b2"],
        "bhalf":    ["b1", "b2"],
        "pcache":   ["p"],
        "phalf":    ["p"]
    }

    # The workloads to parse.
    workloads = ["astar", "bfs", "cc", "mcf", "omnetpp", "soplex", "sphinx", "xalancbmk"]

    data_dir = "spec_runs_data"  # Top-level directory for experiment data

    # Store data as: arc_data[(subdir, workload)] = (p_list, b1_list, b2_list)
    arc_data = {}
    total_files = 0
    print("Starting data collection...")
    for sd in subdirs:
        print(f"Processing subdirectory: {sd}")
        for w in workloads:
            pattern = os.path.join(data_dir, sd, f"arc_policy_selector_log_{w}*.txt")
            files_found = glob.glob(pattern)
            if not files_found:
                print(f"  WARNING: No file found for workload '{w}' in subdir '{sd}'.")
                continue
            filepath = files_found[0]
            print(f"  Parsing workload '{w}' from: {filepath}")
            p_list, b1_list, b2_list = parse_arc_file(filepath)
            arc_data[(sd, w)] = (p_list, b1_list, b2_list)
            total_files += 1
    print(f"Data collection complete. Parsed {total_files} files.\n")

    # Function to plot a single metric for one experiment subdir.
    def plot_metric_for_subdir(subdir, metric, out_filename):
        plt.figure(figsize=(10,6))
        line_style = dict(linewidth=1.5, alpha=0.8)
        has_data = False
        for w in workloads:
            key = (subdir, w)
            if key in arc_data:
                p_list, b1_list, b2_list = arc_data[key]
                if metric == 'p':
                    data = p_list
                elif metric == 'b1':
                    data = b1_list
                elif metric == 'b2':
                    data = b2_list
                else:
                    data = []
                if data:
                    plt.plot(data, label=w, **line_style)
                    has_data = True
        if not has_data:
            plt.close()
            print(f"  No data to plot for metric '{metric}' in subdir '{subdir}'.")
            return
        title_label = subdir_titles.get(subdir, subdir.upper())
        metric_title = {"p": "p_history", "b1": "B1 History", "b2": "B2 History"}
        plt.title(f"{title_label} - ARC {metric_title.get(metric, metric)}")
        plt.xlabel("Access Number")
        if metric == 'p':
            plt.ylabel("p (average across sets)")
        elif metric == 'b1':
            plt.ylabel("B1 size (average across sets)")
        elif metric == 'b2':
            plt.ylabel("B2 size (average across sets)")
        plt.legend(loc="upper right")
        plt.grid(True)
        plt.savefig(out_filename, dpi=150)
        plt.close()
        print(f"  Saved plot: {out_filename}")

    # Now, produce separate graphs for each subdir and each metric
    print("Starting plotting...")
    for sd in subdirs:
        metrics_to_plot = subdir_plot_options.get(sd, [])
        for metric in metrics_to_plot:
            out_file = f"arc_{metric}_history_{sd}.png"
            print(f"Plotting {metric} for {sd}...")
            plot_metric_for_subdir(sd, metric, out_file)
    print("Done! Generated separate PNG files for each experiment and metric.")

if __name__ == "__main__":
    main()
