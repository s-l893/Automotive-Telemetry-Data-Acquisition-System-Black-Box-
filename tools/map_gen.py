import pandas as pd
import folium
import branca.colormap as cm
import numpy as np
import os

def generate_heatmap(csv_file_path, data_channel='RPM'):
    """
    Generate an interactive heatmap visualization from black box data.
    
    Parameters:
    -----------
    csv_file_path : str
        Path to CSV file
    data_channel : str
        Which metric to visualize: 'RPM', 'Spd', 'Ax', 'Ay' (default: 'RPM')
    """
    
    print(f"\n{'='*70}")
    print(f"🏁 Automotive Black Box - Telemetry Heatmap Generator")
    print(f"{'='*70}\n")
    
    # Load data
    try:
        df = pd.read_csv(csv_file_path)
        print(f"✓ Loaded: {csv_file_path}")
        print(f"✓ Total data points: {len(df)}")
        print(f"✓ Columns: {', '.join(df.columns)}\n")
    except Exception as e:
        print(f"❌ Error reading file: {e}\n")
        return
    
    # Filter valid GPS
    df_valid = df[(df['Lat'] != 0) & (df['Lon'] != 0)].copy()
    
    if df_valid.empty:
        print("❌ No valid GPS coordinates found!\n")
        return
    
    print(f"✓ Valid GPS points: {len(df_valid)}/{len(df)}")
    
    # Validate data channel
    if data_channel not in df_valid.columns:
        print(f"❌ Column '{data_channel}' not found!")
        print(f"Available: {', '.join(df_valid.columns)}\n")
        return
    
    # Calculate statistics
    stats = {
        'RPM': {'min': df_valid['RPM'].min(), 'max': df_valid['RPM'].max(), 'avg': df_valid['RPM'].mean()},
        'Spd': {'min': df_valid['Spd'].min(), 'max': df_valid['Spd'].max(), 'avg': df_valid['Spd'].mean()},
        'Ax': {'min': df_valid['Ax'].min(), 'max': df_valid['Ax'].max(), 'avg': df_valid['Ax'].mean()},
        'VTEC': df_valid['VTEC'].sum()
    }
    
    print(f"\n📊 Telemetry Summary:")
    print(f"   RPM:   {stats['RPM']['min']:.0f} - {stats['RPM']['max']:.0f} (avg: {stats['RPM']['avg']:.0f})")
    print(f"   Speed: {stats['Spd']['min']:.1f} - {stats['Spd']['max']:.1f} km/h (avg: {stats['Spd']['avg']:.1f})")
    print(f"   Ax:    {stats['Ax']['min']:.2f} - {stats['Ax']['max']:.2f}g (avg: {stats['Ax']['avg']:.2f})")
    print(f"   VTEC:  {stats['VTEC']} engagement events\n")
    
    # Create map
    center = [df_valid['Lat'].iloc[0], df_valid['Lon'].iloc[0]]
    
    m = folium.Map(
        location=center,
        zoom_start=16,
        tiles='OpenStreetMap'  # Changed from dark_matter
    )
    
    print(f"✓ Map centered at: [{center[0]:.6f}, {center[1]:.6f}]")
    
    # Create color gradient
    channel_configs = {
        'RPM': {
            'colors': ['#00ff00', '#ffff00', '#ff0000'],
            'caption': 'Engine RPM',
            'vmin': 0,
            'vmax': 7000
        },
        'Spd': {
            'colors': ['#0000ff', '#00ffff', '#ffff00', '#ff0000'],
            'caption': 'Speed (km/h)',
            'vmin': 0,
            'vmax': 150
        },
        'Ax': {
            'colors': ['#00ff00', '#ffff00', '#ff6600', '#ff0000'],
            'caption': 'Longitudinal G-Force',
            'vmin': -1.0,
            'vmax': 1.0
        }
    }
    
    config = channel_configs.get(data_channel, channel_configs['RPM'])
    
    colormap = cm.LinearColormap(
        colors=config['colors'],
        vmin=config.get('vmin', df_valid[data_channel].min()),
        vmax=config.get('vmax', df_valid[data_channel].max()),
        caption=config['caption']
    )
    
    colormap.add_to(m)
    
    # Draw colored path segments
    points = df_valid[['Lat', 'Lon']].values.tolist()
    colors = df_valid[data_channel].values
    
    for i in range(len(points) - 1):
        color_value = colors[i]
        hex_color = colormap.rgb_hex_str(color_value)
        
        folium.PolyLine(
            locations=[points[i], points[i+1]],
            color=hex_color,
            weight=6,
            opacity=0.9
        ).add_to(m)
    
    print(f"✓ Drew {len(points)-1} path segments colored by {data_channel}")
    
    # Add VTEC markers
    vtec_events = df_valid[df_valid['VTEC'] == 1]
    marker_count = 0
    
    for idx, row in vtec_events.iterrows():
        if idx % 10 == 0:
            folium.CircleMarker(
                location=[row['Lat'], row['Lon']],
                radius=5,
                color='#ff0000',
                fill=True,
                fillColor='#ff6600',
                fillOpacity=0.8,
                popup=f"""
                    <b>⚡ VTEC ENGAGED</b><br>
                    RPM: <b>{row['RPM']:.0f}</b><br>
                    Speed: <b>{row['Spd']:.1f} km/h</b><br>
                    Ax: <b>{row['Ax']:.2f}g</b>
                """,
                tooltip="VTEC ON"
            ).add_to(m)
            marker_count += 1
    
    print(f"✓ Added {marker_count} VTEC engagement markers")
    
    # Add start/end markers
    folium.Marker(
        location=[df_valid['Lat'].iloc[0], df_valid['Lon'].iloc[0]],
        popup="<b>🏁 START</b>",
        icon=folium.Icon(color='green', icon='play', prefix='fa')
    ).add_to(m)
    
    folium.Marker(
        location=[df_valid['Lat'].iloc[-1], df_valid['Lon'].iloc[-1]],
        popup="<b>🏁 FINISH</b>",
        icon=folium.Icon(color='red', icon='stop', prefix='fa')
    ).add_to(m)
    
    # Save output
    output_name = csv_file_path.replace('.csv', f'_{data_channel}_heatmap.html')
    m.save(output_name)
    
    print(f"\n✅ SUCCESS!")
    print(f"   Heatmap saved: {output_name}")
    print(f"   Open in browser to view\n")
    print(f"{'='*70}\n")


if __name__ == "__main__":
    base_dir = os.path.dirname(os.path.abspath(__file__))
    data_path = os.path.join(base_dir, '..', 'data', 'robust_drive.csv')
    
    if not os.path.exists(data_path):
        print(f"❌ File not found: {data_path}\n")
    else:
        generate_heatmap(data_path, data_channel='RPM')