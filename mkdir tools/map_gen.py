import pandas as pd
import folium
from folium import plugins
import os

def generate_map(csv_file):
    # 1. Load the data using your specific headers
    # Format: Time,Ax,Ay,Az,Lat,Lon,Spd,RPM,Throttle,VTEC
    try:
        df = pd.read_csv(csv_file)
    except Exception as e:
        print(f"Error reading file: {e}")
        return

    # 2. Clean data: Remove invalid GPS points (0.0, 0.0)
    df = df[(df['Lat'] != 0) & (df['Lon'] != 0)]
    
    if df.empty:
        print("No valid GPS coordinates found in file.")
        return

    # 3. Initialize map centered on London, ON
    start_point = [df['Lat'].iloc[0], df['Lon'].iloc[0]]
    m = folium.Map(location=start_point, zoom_start=15, tiles='OpenStreetMap')

    # 4. Draw the driving path
    path = df[['Lat', 'Lon']].values.tolist()
    folium.PolyLine(path, color="blue", weight=5, opacity=0.7, tooltip="Drive Path").add_to(m)

    # 5. Highlight "Performance" events (e.g., High RPM or VTEC)
    for _, row in df.iterrows():
        if row['RPM'] > 5000: # VTEC engagement threshold for your V6
            folium.CircleMarker(
                location=[row['Lat'], row['Lon']],
                radius=4,
                color='red',
                fill=True,
                popup=f"RPM: {row['RPM']} | VTEC: {row['VTEC']}"
            ).add_to(m)

    # 6. Save output
    output_name = csv_file.replace('.csv', '_map.html')
    m.save(output_name)
    print(f"Success! Map saved as {output_name}")

if __name__ == "__main__":
    # Point this to a sample file from your SD card
    generate_map('01271530.csv')