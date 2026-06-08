import argparse
import os
import sys
from pathlib import Path

import branca.colormap as cm
import folium
import numpy as np
import pandas as pd

# Windows consoles often use cp1252; avoid UnicodeEncodeError on emoji/unicode prints.
if hasattr(sys.stdout, "reconfigure"):
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except (OSError, ValueError):
        pass

# openstreetmap.org tiles often return 403 "Access blocked" when the saved HTML is opened
# via file:// (Referer / tile usage policy). Carto basemaps are OSM-derived and usually work.
_BASEMAP_TILES = "https://{s}.basemaps.cartocdn.com/light_all/{z}/{x}/{y}.png"
_BASEMAP_ATTR = (
    '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> '
    'contributors &copy; <a href="https://carto.com/attributions">CARTO</a>'
)


def _haversine_m(lat1, lon1, lat2, lon2):
    """Great-circle distance in meters (vectorized for scalars or arrays)."""
    r = 6371000.0
    p1 = np.radians(lat1)
    p2 = np.radians(lat2)
    dphi = np.radians(lat2 - lat1)
    dl = np.radians(lon2 - lon1)
    a = np.sin(dphi / 2) ** 2 + np.cos(p1) * np.cos(p2) * np.sin(dl / 2) ** 2
    return 2 * r * np.arcsin(np.sqrt(np.clip(a, 0.0, 1.0)))


def _fix_common_lon_sign_bug(lat, lon):
    """
    Many UART/GPS stacks drop the west sign; e.g. -81.x becomes +81.x while lat stays ~43.
    If lat looks like US/Canada and lon is positive in a band where west longitudes occur, flip.
    """
    lat = np.asarray(lat, dtype=float)
    lon = np.asarray(lon, dtype=float)
    likely_west_typo = (lat >= 20.0) & (lat <= 60.0) & (lon > 50.0) & (lon < 100.0)
    lon = np.where(likely_west_typo, -np.abs(lon), lon)
    return lon


def _gps_valid_mask(lat, lon):
    """Finite, in-range, not null-island; exclude lon==0 (common 'no fix' in logs)."""
    lat = np.asarray(lat, dtype=float)
    lon = np.asarray(lon, dtype=float)
    ok = np.isfinite(lat) & np.isfinite(lon)
    ok &= (lat >= -90.0) & (lat <= 90.0) & (lon >= -180.0) & (lon <= 180.0)
    ok &= ~((np.abs(lat) < 0.05) & (np.abs(lon) < 0.05))
    ok &= np.abs(lon) > 1e-5
    ok &= np.abs(lat) > 0.05
    return ok


def _drop_spatial_outliers(df, lat_col="Lat", lon_col="Lon", max_km=150.0):
    """Remove points farther than max_km from the coordinate median (bad fixes / spikes)."""
    if df.empty:
        return df
    med_lat = df[lat_col].median()
    med_lon = df[lon_col].median()
    d = _haversine_m(med_lat, med_lon, df[lat_col].to_numpy(), df[lon_col].to_numpy())
    return df.loc[d <= max_km * 1000.0].copy()


def _time_delta_seconds(t):
    """Infer per-row sampling interval from Time column (ms vs s heuristics)."""
    t = np.asarray(t, dtype=float)
    dt = np.diff(t)
    ok = np.isfinite(dt) & (dt > 0)
    if not ok.any():
        return None
    med = float(np.median(dt[ok]))
    if med >= 200.0:
        return dt / 1000.0  # milliseconds -> seconds
    if med >= 5.0:
        return dt  # already seconds (coarse logs)
    return dt


def _remove_gps_by_implied_speed(
    df,
    time_col="Time",
    lat_col="Lat",
    lon_col="Lon",
    max_mps=75.0,
    max_iter=5000,
):
    """
    Remove points that imply impossible travel speed vs the previous sample.
    Handles one-way 'teleport' glitches (duplicate coords then a huge jump).
    """
    out = df.reset_index(drop=True)
    removed = 0
    for _ in range(max_iter):
        n = len(out)
        if n < 2:
            break
        t = pd.to_numeric(out[time_col], errors="coerce").to_numpy(dtype=float)
        lat = out[lat_col].to_numpy(dtype=float)
        lon = out[lon_col].to_numpy(dtype=float)
        dt_s = _time_delta_seconds(t)
        if dt_s is None or len(dt_s) != n - 1:
            break
        dt_s = np.where(np.isfinite(dt_s) & (dt_s > 0), dt_s, 0.5)
        dist = np.array(
            [
                float(_haversine_m(lat[i], lon[i], lat[i + 1], lon[i + 1]))
                for i in range(n - 1)
            ]
        )
        speed = dist / np.maximum(dt_s, 0.05)
        bad = speed > max_mps
        if not bad.any():
            break
        k = int(np.argmax(bad)) + 1
        out = out.drop(index=k).reset_index(drop=True)
        removed += 1
    return out, removed


def _remove_interior_gps_spikes(
    df,
    lat_col="Lat",
    lon_col="Lon",
    min_leg_m=220.0,
    max_bridge_m=800.0,
    max_passes=8,
):
    """
    Drop interior points that form an impossible 'tent pole': both legs to neighbors are
    long, but neighbors are close to each other (bad fix / coordinate glitch).
    Repeats until stable so double-spikes are cleaned too.
    """
    if len(df) < 3:
        return df.copy(), 0

    total_removed = 0
    out = df.reset_index(drop=True)
    for _ in range(max_passes):
        lat = out[lat_col].to_numpy(dtype=float)
        lon = out[lon_col].to_numpy(dtype=float)
        n = len(out)
        if n < 3:
            break
        drop = np.zeros(n, dtype=bool)
        for i in range(1, n - 1):
            d_ab = float(_haversine_m(lat[i - 1], lon[i - 1], lat[i], lon[i]))
            d_bc = float(_haversine_m(lat[i], lon[i], lat[i + 1], lon[i + 1]))
            d_ac = float(_haversine_m(lat[i - 1], lon[i - 1], lat[i + 1], lon[i + 1]))
            if d_ab > min_leg_m and d_bc > min_leg_m and d_ac < max_bridge_m:
                drop[i] = True
        if not drop.any():
            break
        removed = int(drop.sum())
        total_removed += removed
        out = out.loc[~drop].reset_index(drop=True)
    return out, total_removed


def _merge_colored_segments(points, hex_colors):
    """
    Merge consecutive route edges that share the same color.
    Edge i connects points[i] -> points[i+1] and uses hex_colors[i].
    Returns list of (list of [lat, lon], color_hex).
    """
    n = len(points)
    if n < 2 or len(hex_colors) < n - 1:
        return []
    runs = []
    i = 0
    while i < n - 1:
        c = hex_colors[i]
        chain = [points[i], points[i + 1]]
        j = i + 1
        while j < n - 1 and hex_colors[j] == c:
            chain.append(points[j + 1])
            j += 1
        runs.append((chain, c))
        i = j
    return runs


def generate_heatmap(csv_file_path, data_channel="RPM", out_path=None):
    """
    Build an interactive map: route colored by a telemetry channel (heatmap-style).

    Parameters
    ----------
    csv_file_path : str
        Path to CSV log.
    data_channel : str
        Column to visualize (e.g. RPM, Spd, Ax, Ay, Throttle).
    out_path : str, optional
        Output HTML path; default is next to the CSV: <stem>_<channel>_heatmap.html
    """
    csv_file_path = str(csv_file_path)
    print(f"\n{'=' * 70}")
    print("Automotive Black Box - Telemetry heatmap (route coloring)")
    print(f"{'=' * 70}\n")

    try:
        df = pd.read_csv(csv_file_path)
        print(f"Loaded: {csv_file_path}")
        print(f"Rows: {len(df)}")
        print(f"Columns: {', '.join(df.columns)}\n")
    except Exception as e:
        print(f"Error reading file: {e}\n")
        return

    required = {"Lat", "Lon"}
    if not required.issubset(df.columns):
        print(f"Missing required columns {required}. Found: {list(df.columns)}\n")
        return

    for col in ("Lat", "Lon"):
        df[col] = pd.to_numeric(df[col], errors="coerce")

    df["Lon"] = _fix_common_lon_sign_bug(df["Lat"].to_numpy(), df["Lon"].to_numpy())

    mask = _gps_valid_mask(df["Lat"].to_numpy(), df["Lon"].to_numpy())
    df_valid = df.loc[mask].copy()
    dropped_gps = int((~mask).sum())

    if df_valid.empty:
        print("No valid GPS rows after bounds / fix checks.\n")
        return

    df_valid = _drop_spatial_outliers(df_valid)
    if df_valid.empty:
        print("All points removed as spatial outliers; check GPS data.\n")
        return

    if "Time" in df_valid.columns:
        df_valid["Time"] = pd.to_numeric(df_valid["Time"], errors="coerce")
        df_valid = df_valid.sort_values("Time", kind="mergesort").reset_index(drop=True)

    speed_removed = 0
    if "Time" in df_valid.columns:
        df_valid, speed_removed = _remove_gps_by_implied_speed(df_valid)
        if df_valid.empty:
            print("All GPS rows removed after implied-speed cleanup.\n")
            return

    df_valid, spike_removed = _remove_interior_gps_spikes(df_valid)
    if df_valid.empty:
        print("All GPS rows removed after spike cleanup.\n")
        return

    print(f"GPS OK: {len(df_valid)} / {len(df)} rows ({dropped_gps} dropped before outlier filter)")
    if speed_removed:
        print(f"   Removed {speed_removed} point(s) (implied speed > 75 m/s vs prior sample)")
    if spike_removed:
        print(f"   Removed {spike_removed} GPS spike point(s) (tent-pole bad fixes)")

    if data_channel not in df_valid.columns:
        print(f"Column '{data_channel}' not found.")
        print(f"Available: {', '.join(df_valid.columns)}\n")
        return

    df_valid[data_channel] = pd.to_numeric(df_valid[data_channel], errors="coerce")
    df_valid = df_valid.dropna(subset=[data_channel])
    if df_valid.empty:
        print(f"No numeric values for '{data_channel}'.\n")
        return

    # Telemetry summary (only columns that exist)
    def stat_line(name, fmt_min, fmt_max, fmt_avg):
        if name not in df_valid.columns:
            return
        s = pd.to_numeric(df_valid[name], errors="coerce").dropna()
        if s.empty:
            return
        print(f"   {name}:  {fmt_min.format(s.min())} - {fmt_max.format(s.max())} (avg: {fmt_avg.format(s.mean())})")

    def stat_line_rpm_nonzero(fmt_min, fmt_max, fmt_avg):
        if "RPM" not in df_valid.columns:
            return
        s = pd.to_numeric(df_valid["RPM"], errors="coerce").dropna()
        s_nz = s[s != 0]
        if s_nz.empty:
            print("   RPM:  (no non-zero samples; idle/off during log)")
            return
        print(
            f"   RPM:  {fmt_min.format(s_nz.min())} - {fmt_max.format(s_nz.max())} "
            f"(avg excl. 0: {fmt_avg.format(s_nz.mean())})"
        )

    print("\nTelemetry summary:")
    stat_line_rpm_nonzero("{:.0f}", "{:.0f}", "{:.0f}")
    stat_line("Spd", "{:.1f}", "{:.1f}", "{:.1f}")
    stat_line("Ax", "{:.2f}", "{:.2f}", "{:.2f}")
    stat_line("Ay", "{:.2f}", "{:.2f}", "{:.2f}")
    if "VTEC" in df_valid.columns:
        v = pd.to_numeric(df_valid["VTEC"], errors="coerce").fillna(0)
        print(f"   VTEC:  {int((v == 1).sum())} samples with VTEC=1")
    print()

    center = [float(df_valid["Lat"].iloc[0]), float(df_valid["Lon"].iloc[0])]
    m = folium.Map(
        location=center,
        zoom_start=16,
        tiles=_BASEMAP_TILES,
        attr=_BASEMAP_ATTR,
        max_zoom=20,
    )
    print(f"Map center: [{center[0]:.6f}, {center[1]:.6f}]")

    channel_configs = {
        "RPM": {
            "colors": ["#00ff00", "#ffff00", "#ff0000"],
            "caption": "Engine RPM",
            "vmin": 0,
            "vmax": 7000,
        },
        "Spd": {
            "colors": ["#0000ff", "#00ffff", "#ffff00", "#ff0000"],
            "caption": "Speed (km/h)",
            "vmin": 0,
            "vmax": 150,
        },
        "Ax": {
            "colors": ["#00ff00", "#ffff00", "#ff6600", "#ff0000"],
            "caption": "Longitudinal G",
            "vmin": -1.0,
            "vmax": 1.0,
        },
        "Ay": {
            "colors": ["#0080ff", "#00ff80", "#ffff00", "#ff0080"],
            "caption": "Lateral G",
            "vmin": -1.5,
            "vmax": 1.5,
        },
        "Throttle": {
            "colors": ["#404040", "#00ccff", "#ffcc00", "#ff3300"],
            "caption": "Throttle",
            "vmin": 0.0,
            "vmax": 100.0,
        },
    }

    config = channel_configs.get(
        data_channel,
        {
            "colors": ["#0000ff", "#00ff00", "#ffff00", "#ff0000"],
            "caption": data_channel,
            "vmin": None,
            "vmax": None,
        },
    )

    vmin = config["vmin"]
    vmax = config["vmax"]
    series = df_valid[data_channel].astype(float)
    if vmin is None:
        vmin = float(series.min())
    if vmax is None:
        vmax = float(series.max())
    if data_channel == "RPM" and vmax is not None:
        peak = float(np.nanmax(series.to_numpy(dtype=float)))
        vmax = max(float(vmax), peak * 1.02, 7000.0)
    if vmax <= vmin:
        vmax = vmin + 1e-6

    colormap = cm.LinearColormap(
        colors=config["colors"],
        vmin=vmin,
        vmax=vmax,
        caption=config["caption"],
    )
    colormap.add_to(m)

    # Color each segment by max(endpoint telemetry) so a brief peak (e.g. max RPM) paints
    # the visible edge, not only the sample at the start of the segment.
    arr = series.to_numpy(dtype=float)
    if len(arr) >= 2:
        edge_vals = np.maximum(arr[:-1], arr[1:])
    else:
        edge_vals = np.array([], dtype=float)
    vals = np.clip(edge_vals, vmin, vmax)
    hex_colors = [colormap.rgb_hex_str(float(v)) for v in vals]
    points = df_valid[["Lat", "Lon"]].astype(float).values.tolist()

    runs = _merge_colored_segments(points, hex_colors)
    for seg, color in runs:
        folium.PolyLine(
            locations=seg,
            color=color,
            weight=5,
            opacity=0.88,
        ).add_to(m)

    print(f"Drew {len(runs)} colored segments ({len(points) - 1} original edges) for '{data_channel}'")

    if "VTEC" in df_valid.columns:
        vtec = pd.to_numeric(df_valid["VTEC"], errors="coerce").fillna(0) == 1
        vtec_rows = df_valid.loc[vtec]
        marker_count = 0
        for i, (_, row) in enumerate(vtec_rows.iterrows()):
            if i % 10 != 0:
                continue
            rpm = row["RPM"] if "RPM" in row.index and pd.notna(row.get("RPM")) else float("nan")
            spd = row["Spd"] if "Spd" in row.index and pd.notna(row.get("Spd")) else float("nan")
            axv = row["Ax"] if "Ax" in row.index and pd.notna(row.get("Ax")) else float("nan")
            popup_lines = ["<b>VTEC</b>"]
            if np.isfinite(rpm):
                popup_lines.append(f"RPM: <b>{rpm:.0f}</b>")
            if np.isfinite(spd):
                popup_lines.append(f"Speed: <b>{spd:.1f} km/h</b>")
            if np.isfinite(axv):
                popup_lines.append(f"Ax: <b>{axv:.2f}g</b>")
            folium.CircleMarker(
                location=[float(row["Lat"]), float(row["Lon"])],
                radius=5,
                color="#ff0000",
                fill=True,
                fillColor="#ff6600",
                fillOpacity=0.8,
                popup="<br>".join(popup_lines),
                tooltip="VTEC ON",
            ).add_to(m)
            marker_count += 1
        print(f"VTEC markers (every 10th event): {marker_count}")

    folium.Marker(
        location=[float(df_valid["Lat"].iloc[0]), float(df_valid["Lon"].iloc[0])],
        popup="<b>START</b>",
        icon=folium.Icon(color="green"),
    ).add_to(m)
    folium.Marker(
        location=[float(df_valid["Lat"].iloc[-1]), float(df_valid["Lon"].iloc[-1])],
        popup="<b>FINISH</b>",
        icon=folium.Icon(color="red"),
    ).add_to(m)

    src = Path(csv_file_path)
    if out_path:
        output_name = Path(out_path)
    else:
        output_name = src.parent / f"{src.stem}_{data_channel}_heatmap.html"
    output_name = output_name.resolve()
    m.save(str(output_name))

    print(f"\nSaved: {output_name}")
    print(f"{'=' * 70}\n")


def main():
    parser = argparse.ArgumentParser(description="Generate telemetry-colored map from CSV log.")
    parser.add_argument(
        "csv",
        nargs="?",
        default=None,
        help="Path to CSV (default: bundled sample path or DATA env)",
    )
    parser.add_argument(
        "--channel",
        "-c",
        default="RPM",
        help="Column to color by (default RPM)",
    )
    parser.add_argument(
        "--out",
        "-o",
        default=None,
        help="Output HTML path",
    )
    args = parser.parse_args()

    default_data = (
        Path(__file__).resolve().parent.parent
        / "data"
        / "03291927.CSV"
    )
    data_path = args.csv or os.environ.get("BLACKBOX_CSV") or str(default_data)

    if not os.path.isfile(data_path):
        print(f"File not found: {data_path}")
        sys.exit(1)
    generate_heatmap(data_path, data_channel=args.channel, out_path=args.out)


if __name__ == "__main__":
    main()
