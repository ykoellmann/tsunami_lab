#!/usr/bin/env pvpython
from paraview.simple import *
import sys
import xml.etree.ElementTree as ET
import glob
import os

# --- Konfiguration ---
INPUT_DIR    = sys.argv[1]
NAME         = sys.argv[2]
STATIONS_XML = sys.argv[3] if len(sys.argv) > 3 else None

OUTPUT_FILE = os.path.join("simulations/visualizations", NAME + ".avi")
os.makedirs("simulations/visualizations", exist_ok=True)

# --- Dateien einsammeln ---
files = sorted(
    glob.glob(os.path.join(INPUT_DIR, "solution_*.csv")),
    key=lambda f: int(os.path.basename(f).replace("solution_", "").replace(".csv", ""))
)
if not files:
    print(f"[Fehler] Keine solution_*.csv Dateien in '{INPUT_DIR}' gefunden.")
    sys.exit(1)
print(f"[1/5] {len(files)} Zeitschritte gefunden in '{INPUT_DIR}'")

# --- CSV laden ---
csv = CSVReader(FileName=files)
csv.HaveHeaders = 1
csv.CommentCharacters = '#'
print(f"[2/5] CSV geladen")

# --- Pipeline aufbauen ---
calc = Calculator(Input=csv)
calc.Function = 'height + bathymetry'
calc.ResultArrayName = 'eta'

t2p = TableToPoints(Input=calc)
t2p.XColumn = 'x'
t2p.YColumn = 'y'
t2p.ZColumn = 'eta'

delaunay = Delaunay2D(Input=t2p)
display = Show(delaunay)
ColorBy(display, ('POINTS', 'eta'))
print(f"[3/5] Pipeline aufgebaut (CSV → Calculator → TableToPoints → Delaunay2D)")

# --- Kamera automatisch positionieren ---
delaunay.UpdatePipeline()
bounds = delaunay.GetDataInformation().GetBounds()
x_center = (bounds[0] + bounds[1]) / 2
y_center = (bounds[2] + bounds[3]) / 2
z_center = (bounds[4] + bounds[5]) / 2
y_size   = bounds[3] - bounds[2]

view = GetActiveViewOrCreate('RenderView')
view.CameraFocalPoint = [x_center, y_center, z_center]
view.CameraPosition   = [x_center, y_center - y_size * 1.5, z_center + y_size]
view.CameraViewUp     = [0, 0, 1]
ResetCamera()

# --- Stationen einblenden (optional) ---
if STATIONS_XML is not None:
    tree = ET.parse(STATIONS_XML)
    root = tree.getroot()
    stations = root.findall('station')
    for station in stations:
        name = station.get('name')
        x    = float(station.get('x'))
        y    = float(station.get('y'))
        sphere = Sphere(Center=[x, y, 0], Radius=0.5)
        sphere_display = Show(sphere)
        sphere_display.DiffuseColor = [1, 0, 0]
        text = Text()
        text.Text = name
        text_display = Show(text)
        text_display.Position = [x, y]
    print(f"[4/5] {len(stations)} Station(en) eingeblendet aus '{STATIONS_XML}'")
else:
    print(f"[4/5] Keine Stationen angegeben")

# --- Animation speichern ---
Render()
scene = GetAnimationScene()
scene.UpdateAnimationUsingDataTimeSteps()
print(f"[5/5] Speichere Animation nach '{OUTPUT_FILE}' ({len(files)} Frames, 5 FPS)...")
SaveAnimation(OUTPUT_FILE, FrameRate=5, ImageResolution=[1920, 1080])
print(f"      Fertig.")