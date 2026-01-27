import pandas as pd
import numpy as np
import os

def generate_realistic_drive():
    """
    Generate a realistic driving scenario around Western University, London ON.
    Simulates a spirited drive with VTEC engagement, cornering, and acceleration.
    """
    
    print("\n" + "="*70)
    print("🏁 Generating Realistic Test Data")
    print("="*70 + "\n")
    
    # ========================================================================
    # ROUTE: Realistic Loop Around Western University
    # ========================================================================
    # Route follows actual roads: Western Rd → Sarnia Rd → Richmond St loop
    
    num_points = 500  # ~4 minutes of driving at 2Hz sampling
    
    # Define waypoints along actual roads (manually traced from map)
    waypoints = [
        # Western Road (heading south)
        (43.0095, -81.2735),
        (43.0090, -81.2738),
        (43.0085, -81.2741),
        (43.0080, -81.2744),
        (43.0075, -81.2747),
        (43.0070, -81.2750),
        
        # Turn right onto Sarnia Road (heading west)
        (43.0068, -81.2753),
        (43.0066, -81.2758),
        (43.0064, -81.2765),
        (43.0062, -81.2773),
        (43.0060, -81.2780),
        
        # Continue on Sarnia (accelerating)
        (43.0058, -81.2788),
        (43.0056, -81.2795),
        (43.0054, -81.2802),
        (43.0052, -81.2808),
        (43.0050, -81.2815),
        
        # Turn left onto Richmond (heading south)
        (43.0047, -81.2817),
        (43.0043, -81.2818),
        (43.0038, -81.2819),
        (43.0033, -81.2820),
        (43.0028, -81.2821),
        
        # Continue south on Richmond
        (43.0023, -81.2822),
        (43.0018, -81.2822),
        (43.0013, -81.2822),
        (43.0008, -81.2822),
        (43.0003, -81.2822),
        
        # Turn around / loop back (right turn)
        (43.0000, -81.2820),
        (42.9998, -81.2816),
        (42.9997, -81.2811),
        (42.9996, -81.2805),
        
        # Head back north on Proudfoot Lane
        (42.9997, -81.2799),
        (42.9999, -81.2793),
        (43.0002, -81.2787),
        (43.0006, -81.2781),
        (43.0010, -81.2775),
        
        # Turn right back onto Western Road
        (43.0015, -81.2770),
        (43.0020, -81.2765),
        (43.0025, -81.2760),
        (43.0030, -81.2755),
        (43.0035, -81.2750),
        (43.0040, -81.2746),
        (43.0045, -81.2742),
        
        # Return to start
        (43.0050, -81.2739),
        (43.0055, -81.2737),
        (43.0060, -81.2736),
        (43.0065, -81.2735),
        (43.0070, -81.2735),
        (43.0075, -81.2735),
        (43.0080, -81.2735),
        (43.0085, -81.2735),
        (43.0090, -81.2735),
        (43.0095, -81.2735),  # Back to start
    ]
    
    # Interpolate between waypoints to get smooth path
    waypoint_lats = [w[0] for w in waypoints]
    waypoint_lons = [w[1] for w in waypoints]
    
    # Create interpolated path with num_points
    waypoint_indices = np.linspace(0, len(waypoints) - 1, num_points)
    lats = np.interp(waypoint_indices, range(len(waypoints)), waypoint_lats)
    lons = np.interp(waypoint_indices, range(len(waypoints)), waypoint_lons)
    
    # Add realistic GPS noise (±1 meter, smaller than before)
    lats += np.random.normal(0, 0.00001, num_points)
    lons += np.random.normal(0, 0.00001, num_points)
    
    print(f"✓ Generated {num_points} GPS coordinates")
    print(f"  Start: [{lats[0]:.6f}, {lons[0]:.6f}]")
    print(f"  End:   [{lats[-1]:.6f}, {lons[-1]:.6f}]\n")
    
    # ========================================================================
    # TELEMETRY SIMULATION
    # ========================================================================
    
    # RPM Profile (realistic Honda J35 V6 behavior)
    rpm = []
    
    # Segment 1: Gradual acceleration (1st → 2nd → 3rd gear)
    rpm.extend(np.linspace(1500, 3500, 50))      # Cruising in 1st
    rpm.extend(np.linspace(3500, 6500, 50))      # Pull to redline in 1st
    rpm.extend(np.linspace(3000, 5800, 50))      # Shift to 2nd, pull again
    
    # Segment 2: Corner (braking, downshift)
    rpm.extend(np.linspace(5800, 3500, 50))      # Braking
    rpm.extend(np.linspace(3500, 4000, 50))      # Corner apex
    
    # Segment 3: VTEC PULL! (Full throttle acceleration)
    rpm.extend(np.linspace(4000, 5200, 30))      # Building boost
    rpm.extend(np.linspace(5200, 6800, 70))      # VTEC KICKS IN YO!
    rpm.extend(np.linspace(6800, 4500, 20))      # Shift to next gear
    rpm.extend(np.linspace(4500, 6500, 30))      # Pull again
    
    # Segment 4: Coasting back
    rpm.extend(np.linspace(6500, 2500, 50))      # Engine braking
    rpm.extend(np.linspace(2500, 1500, 50))      # Idle/coast
    
    rpm = np.array(rpm)
    
    # Add realistic RPM fluctuations (±50 RPM)
    rpm += np.random.normal(0, 50, num_points)
    rpm = np.clip(rpm, 800, 7000).astype(int)
    
    print(f"✓ RPM simulation complete")
    print(f"  Min: {rpm.min()} | Max: {rpm.max()} | Avg: {rpm.mean():.0f}\n")
    
    # ========================================================================
    # VTEC Engagement (>5150 RPM for Honda J35)
    # ========================================================================
    vtec = (rpm > 5150).astype(int)
    vtec_count = np.sum(np.diff(vtec) == 1)  # Count rising edges
    
    print(f"✓ VTEC engagements: {vtec_count} times")
    
    # ========================================================================
    # Speed (derived from RPM and gear ratios)
    # ========================================================================
    # Simplified: speed ≈ RPM / 70 + base speed
    speed = (rpm / 70) + np.random.normal(20, 5, num_points)
    speed = np.clip(speed, 0, 140).astype(int)
    
    print(f"✓ Speed range: {speed.min()} - {speed.max()} km/h\n")
    
    # ========================================================================
    # G-Forces (Ax = longitudinal, Ay = lateral)
    # ========================================================================
    
    # Longitudinal Gs (acceleration/braking)
    ax = np.zeros(num_points)
    
    # Acceleration zones (positive Gs)
    ax[0:150] = np.linspace(0.2, 0.6, 150)       # Accelerating
    ax[250:350] = np.linspace(0.4, 0.8, 100)     # VTEC pull (high Gs!)
    
    # Braking zones (negative Gs)
    ax[150:200] = np.linspace(0.6, -0.7, 50)     # Hard braking for corner
    ax[350:400] = np.linspace(0.5, -0.4, 50)     # Engine braking
    
    # Add noise
    ax += np.random.normal(0, 0.05, num_points)
    ax = np.clip(ax, -1.0, 1.0)
    
    # Lateral Gs (cornering)
    ay = np.zeros(num_points)
    
    # Corner 1 (right turn)
    ay[150:200] = np.linspace(0, 0.6, 50)        # Right lateral load
    ay[200:250] = np.linspace(0.6, 0, 50)        # Exit
    
    # Corner 2 (left turn)
    ay[400:450] = np.linspace(0, -0.5, 50)       # Left lateral load
    ay[450:500] = np.linspace(-0.5, 0, 50)       # Straighten out
    
    # Add noise
    ay += np.random.normal(0, 0.03, num_points)
    ay = np.clip(ay, -1.0, 1.0)
    
    print(f"✓ G-force simulation:")
    print(f"  Ax: {ax.min():.2f}g to {ax.max():.2f}g (avg: {ax.mean():.2f}g)")
    print(f"  Ay: {ay.min():.2f}g to {ay.max():.2f}g (avg: {ay.mean():.2f}g)\n")
    
    # ========================================================================
    # Az (Vertical) - mostly 1g with bumps
    # ========================================================================
    az = np.ones(num_points) + np.random.normal(0, 0.05, num_points)
    
    # ========================================================================
    # Throttle Position (0-100%)
    # ========================================================================
    throttle = np.zeros(num_points)
    throttle[0:150] = np.linspace(30, 80, 150)      # Moderate acceleration
    throttle[150:200] = np.linspace(80, 0, 50)      # Lift off
    throttle[250:350] = np.linspace(50, 100, 100)   # WOT (Wide Open Throttle!)
    throttle[350:400] = np.linspace(100, 20, 50)    # Coast
    throttle[400:] = np.linspace(20, 10, 100)       # Idle
    
    throttle += np.random.normal(0, 5, num_points)
    throttle = np.clip(throttle, 0, 100)
    
    # ========================================================================
    # Time (500ms intervals = 2Hz sampling rate)
    # ========================================================================
    time_ms = np.arange(0, num_points * 500, 500)
    
    # ========================================================================
    # BUILD DATAFRAME
    # ========================================================================
    df = pd.DataFrame({
        'Time': time_ms,
        'Ax': np.round(ax, 2),
        'Ay': np.round(ay, 2),
        'Az': np.round(az, 2),
        'Lat': np.round(lats, 6),
        'Lon': np.round(lons, 6),
        'Spd': speed,
        'RPM': rpm,
        'Throttle': np.round(throttle, 1),
        'VTEC': vtec
    })
    
    # ========================================================================
    # SAVE TO FILE
    # ========================================================================
    base_dir = os.path.dirname(os.path.abspath(__file__))
    output_path = os.path.join(base_dir, '..', 'data', 'robust_drive.csv')
    
    # Create data directory if it doesn't exist
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    
    df.to_csv(output_path, index=False)
    
    print(f"✅ Data saved to: {output_path}")
    print(f"   Total duration: {time_ms[-1]/1000:.1f} seconds")
    print(f"   File size: {os.path.getsize(output_path) / 1024:.1f} KB\n")
    
    # ========================================================================
    # PREVIEW
    # ========================================================================
    print("📊 Preview of first 5 rows:")
    print(df.head().to_string(index=False))
    print(f"\n{'='*70}\n")
    
    return output_path


if __name__ == "__main__":
    output_file = generate_realistic_drive()
    print(f"✓ Ready to visualize!")
    print(f"  Run: python map_gen.py")
    print(f"  Or update map_gen.py to use: '{output_file}'\n")