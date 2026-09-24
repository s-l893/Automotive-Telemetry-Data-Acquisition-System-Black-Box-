#!/usr/bin/env python3
"""
Black Box V2 - session log -> route heat map (real road map, GPS-colored line).

Usage:
    python black_box_heatmap.py LOG_000.CSV -o session_map.html
    python black_box_heatmap.py LOG_000.CSV -o session_map.html --max-speed-kmh 180

Output is a self-contained HTML file (Leaflet + real map tiles via folium)
with a layer toggle to switch the track's color channel between RPM,
lateral G (accel_y), and longitudinal G (accel_x). Open it in a normal
browser on a machine with internet access - the tiles load live from Esri,
so this only works run locally (not in a sandboxed/offline viewer), and it
needs internet access each time you open it, same as any other online map.

Assumes accel_y = lateral, accel_x = longitudinal (typical mounting).
Pass --swap-axes if your IMU is rotated 90 degrees on the board.
"""
import argparse
import math
import sys

import pandas as pd
import folium
import branca.colormap as cm


def haversine_km(lat1, lon1, lat2, lon2):
    r = 6371.0
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dphi = math.radians(lat2 - lat1)
    dlmb = math.radians(lon2 - lon1)
    a = math.sin(dphi / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dlmb / 2) ** 2
    return 2 * r * math.asin(math.sqrt(a))


def load_log(path):
    """Robust to leading-space column names (' id', ' lat', ...) and a
    trailing 'END OF SESSION' footer line."""
    df = pd.read_csv(path, skipinitialspace=True, engine="python")
    df.columns = [c.strip() for c in df.columns]
    df["timestamp"] = pd.to_numeric(df["timestamp"], errors="coerce")
    df = df.dropna(subset=["timestamp"]).reset_index(drop=True)
    df["timestamp"] = df["timestamp"].astype(int)
    for col in ("lat", "long", "speed", "RPM", "accel_x", "accel_y", "accel_z"):
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors="coerce")
    return df


def filter_track(df, max_speed_kmh=220.0):
    """Keep only rows with a real GPS fix, collapse them down to one point
    per actual GPS update (the NEO-M8N only reports a new fix roughly once a
    second, so most rows in the CSV just repeat the last fix while CAN/IMU
    keep logging every 10ms - averaging RPM/accel over each fix's interval
    gives a truer value than picking a single row), then drop a fix if it
    implies an impossible speed from the previous *kept* fix. This catches
    an isolated bad GPS reading (jumps away and, since it's rejected, the
    next real fix is compared against the last good one, catching the jump
    back too) without penalizing a route that legitimately covers a couple
    of km, which a simple distance-from-center check would."""
    d = df[(df.lat.notna()) & (df.long.notna()) & (df.lat != 0) & (df.long != 0)]
    d = d.reset_index(drop=True)
    if d.empty:
        return d

    fix_id = ((d.lat != d.lat.shift()) | (d.long != d.long.shift())).cumsum()
    agg = {"timestamp": "first", "lat": "first", "long": "first"}
    for col in ("RPM", "accel_x", "accel_y", "accel_z"):
        if col in d.columns:
            agg[col] = "mean"
    grouped = d.groupby(fix_id).agg(agg).reset_index(drop=True)

    keep = []
    prev = None
    for _, row in grouped.iterrows():
        if prev is not None:
            dt_h = max(row.timestamp - prev.timestamp, 1) / 3600000.0
            dist = haversine_km(row.lat, row.long, prev.lat, prev.long)
            if dist / dt_h > max_speed_kmh:
                continue
        keep.append(row)
        prev = row
    return pd.DataFrame(keep).reset_index(drop=True)


def build_map(df, out_path, decimate=1, swap_axes=False):
    d = df.iloc[::decimate].reset_index(drop=True)
    if len(d) < 2:
        raise SystemExit("Not enough valid GPS points after filtering to draw a track.")

    lat_col, lon_col_metric = ("accel_x", "accel_y") if swap_axes else ("accel_y", "accel_x")

    channels = {
        "RPM": ("RPM", "RdYlGn_11", True),          # low->high, green->red (reversed below)
        "Lateral G (raw)": (lat_col, "coolwarm", False),
        "Longitudinal G (raw)": (lon_col_metric, "coolwarm", False),
    }

    center = [d.lat.median(), d.long.median()]
    # tile.openstreetmap.org 403s file:// requests (no Referer header) under
    # its anti-hotlinking policy, and CartoDB's tiles now require an API
    # key. Esri's World Street Map tiles need neither, so this works when
    # the HTML is just double-clicked, no local server or key required.
    m = folium.Map(location=center, zoom_start=15, tiles=None)
    folium.TileLayer(
        tiles="https://server.arcgisonline.com/ArcGIS/rest/services/World_Street_Map/MapServer/tile/{z}/{y}/{x}",
        attr="Esri, HERE, Garmin, FAO, NOAA, USGS, © OpenStreetMap contributors",
        name="Esri World Street Map",
    ).add_to(m)

    for label, (col, palette, _reversed) in channels.items():
        vals = d[col]
        vmin, vmax = float(vals.min()), float(vals.max())
        colormap = cm.LinearColormap(
            colors=["#2f6dd6", "#3fd0c9", "#f4d35e", "#f4743b", "#e8412c"],
            vmin=vmin, vmax=vmax, caption=label,
        )
        fg = folium.FeatureGroup(name=label, show=(label == "RPM"))
        for i in range(len(d) - 1):
            p1 = (d.lat[i], d.long[i])
            p2 = (d.lat[i + 1], d.long[i + 1])
            v = (vals[i] + vals[i + 1]) / 2.0
            folium.PolyLine(
                [p1, p2], color=colormap(v), weight=4, opacity=0.85,
                tooltip=f"{label}: {v:.0f}",
            ).add_to(fg)
        fg.add_to(m)
        colormap.add_to(m)

    folium.LayerControl(collapsed=False).add_to(m)
    m.save(out_path)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv_path")
    ap.add_argument("-o", "--out", default="session_map.html")
    ap.add_argument("--decimate", type=int, default=1, help="keep every Nth GPS fix (default 1 = all fixes; fixes are already ~1/sec, so this rarely needs to change)")
    ap.add_argument("--max-speed-kmh", type=float, default=220.0, help="drop a GPS fix implying more than this speed from the previous kept fix")
    ap.add_argument("--swap-axes", action="store_true", help="use accel_x as lateral / accel_y as longitudinal instead of the default")
    args = ap.parse_args()

    df = load_log(args.csv_path)
    print(f"Loaded {len(df)} rows.")
    track = filter_track(df, max_speed_kmh=args.max_speed_kmh)
    n_raw_valid = len(df[(df.lat != 0) & (df.long != 0)])
    print(f"{n_raw_valid} rows had a GPS fix, collapsing to {len(track)} distinct fix points "
          f"(most rows just repeat the last ~1Hz fix while CAN/IMU keep logging every 10ms).")
    build_map(track, args.out, decimate=args.decimate, swap_axes=args.swap_axes)
    print(f"Wrote {args.out} - open it locally in a browser (needs internet for map tiles).")


if __name__ == "__main__":
    main()
