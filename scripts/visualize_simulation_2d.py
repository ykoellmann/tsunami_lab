#!/usr/bin/env pvpython

from paraview.simple import *
import sys
import xml.etree.ElementTree as ET
import glob
import os

INPUT_DIR    = sys.argv[1]
NAME         = sys.argv[2]
STATIONS_XML = sys.argv[3] if len(sys.argv) > 3 else None

OUTPUT_FILE = os.path.join("visualizations", NAME + ".avi")
os.makedirs("visualizations", exist_ok=True)

# Alle CSVs sortiert einsammeln
files = sorted(
    glob.glob(os.path.join(INPUT_DIR, "solution_*.csv")),
    key=lambda f: int(os.path.basename(f).replace("solution_", "").replace(".csv", ""))
)

if not files:
    print(f"Keine solution_*.csv Dateien in '{INPUT_DIR}' gefunden.")
    sys.exit(1)

csv = CSVReader(FileName=files)  # komplette Liste übergeben
csv.HaveHeaders = 1
csv.SkipInitialLines = 1

# η = height + bathymetry
calc = Calculator(Input=csv)
calc.Function = 'height + bathymetry'
calc.ResultArrayName = 'eta'

# Punkte aufspannen
t2p = TableToPoints(Input=calc)
t2p.XColumn = 'x'
t2p.YColumn = 'y'
t2p.ZColumn = 'eta'

# Fläche
delaunay = Delaunay2D(Input=t2p)
display = Show(delaunay)
ColorBy(display, ('POINTS', 'eta'))

# --- Stationen einblenden (optional) ---
if STATIONS_XML is not None:
    tree = ET.parse(STATIONS_XML)
    root = tree.getroot()

    for station in root.findall('station'):
        name = station.get('name')
        x    = float(station.get('x'))
        y    = float(station.get('y'))

        # Punkt bei Z=0, wird auf Oberfläche projiziert
        sphere = Sphere(Center=[x, y, 0], Radius=0.5)
        sphere_display = Show(sphere)
        sphere_display.DiffuseColor = [1, 0, 0]  # rot

        # Label
        text = Text()
        text.Text = name
        text_display = Show(text)
        text_display.Position = [x, y]

# Render & Export
GetActiveViewOrCreate('RenderView')
ResetCamera()
Render()

SaveAnimation(OUTPUT_FILE, FrameRate=24, ImageResolution=[1920, 1080])