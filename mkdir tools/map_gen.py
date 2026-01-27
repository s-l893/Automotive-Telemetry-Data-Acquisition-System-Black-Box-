import pandas as pd
import folium
import os

def generate_map(csv_file):
    """Generate an interactive map from black box CSV data."""
    
    print(f"\n{'='*60}")
    print(f"Black Box Data Visualizer")
    print(f"{'='*60}\n")
    
    # Check file exists
    abs_path = os.path.abspath(csv_file)
    print(f"Looking for: {abs_path}")
    
    if not os.path.exists(csv_file):
        print(f"❌ File not found!\n")
        return
    
    print(f"✓ File found!\n")
    
    # Load CSV
    try:
        df = pd.read_csv(csv_file)
        print(f"✓ Loaded {len(df)} rows")
        print(f"✓ Columns: {list(df.columns)}\n")
    except Exception as e:
        print(f"❌ Error reading CSV: {e}\n")
        return
    
    # Clean GPS data
    df_clean = df[(df['Lat'] != 0) & (df['Lon'] != 0)].copy()
    print(f"✓ Valid GPS points: {len(df_clean)}/{len(df)}")
    
    if df_clean.empty:
        print(f"❌ No valid GPS coordinates!\n")
        return
    
    # Create map
    center = [df_clean['Lat'].iloc[0], df_clean['Lon'].iloc[0]]
    print(f"✓ Map centered at: {center}\n")
    
    m = folium.Map(location=center, zoom_start=15, tiles='OpenStreetMap')
    
# Color gradient based on RPM (green → yellow → red)
    colormap = cm.LinearColormap(colors=['green', 'yellow', 'red'])

    # Draw colored segments
    for i in range(len(points) - 1):
        hex_color = colormap.rgb_hex_str(color_value)
        folium.PolyLine(locations=[points[i], points[i+1]], 
                    color=hex_color).add_to(m)
    
    # Mark VTEC events (RPM > 5150)
    vtec_events = df_clean[df_clean['RPM'] > 5150]
    print(f"✓ VTEC events found: {len(vtec_events)}")
    
    for _, row in vtec_events.iterrows():
        folium.CircleMarker(
            location=[row['Lat'], row['Lon']],
            radius=4,
            color='red',
            fill=True,
            fillColor='red',
            fillOpacity=0.7,
            popup=f"<b>VTEC!</b><br>RPM: {row['RPM']:.0f}<br>Speed: {row['Spd']:.1f} km/h"
        ).add_to(m)
    
    # Save map
    output = os.path.join(os.path.dirname(csv_file), 
                          os.path.basename(csv_file).replace('.csv', '_map.html'))
    m.save(output)
    
    print(f"\n✓ SUCCESS! Map saved to:")
    print(f"  {os.path.abspath(output)}\n")
    print(f"Open this file in your browser to view the map.\n")

if __name__ == "__main__":
    # Path relative to tools/ directory
    csv_file = os.path.join("..", "data", "robust_drive.csv")
    generate_map(csv_file)