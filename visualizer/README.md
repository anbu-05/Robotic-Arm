# Robotic Arm Workspace Visualizer

Simple 2D visualizer for a three-link robotic arm:

- shoulder link
- elbow link
- wrist link

The arm uses millimeters for link lengths.

## Setup

Create and activate a virtual environment if you do not already have one:

```bash
python3 -m venv .venv
source .venv/bin/activate
```

Install dependencies:

```bash
pip install -r requirements.txt
```

## Run

Use the Qt Matplotlib backend:

```bash
MPLBACKEND=QtAgg python3 arm_workspace.py
```

If Matplotlib complains about its config directory, run:

```bash
MPLCONFIGDIR=.matplotlib-cache MPLBACKEND=QtAgg python3 arm_workspace.py
```

## Controls

- Drag the elbow, wrist, or hand point to move the arm.
- Use the `shoulder mm`, `elbow mm`, and `wrist mm` boxes to change link lengths.
- Click `apply` after editing link lengths.
- Use the angle sliders to directly set shoulder, elbow, and wrist angles.
- Use the limit sliders to set min/max angle limits for each joint.
- Use the lock checkboxes to keep a joint angle fixed while dragging or using sliders.

## Main Settings

The main options are at the top of `arm_workspace.py`:

```python
LINK_LENGTHS_MM = [120.0, 100.0, 70.0]
START_ANGLES_DEGREES = [35.0, 35.0, -25.0]

SHOW_WORKSPACE = True
SHOW_JOINT_LABELS = True
SHOW_END_POSITION = True
SHOW_LENGTH_BOXES = True
SHOW_ANGLE_SLIDERS = True
SHOW_JOINT_LOCKS = True
SHOW_LIMIT_SLIDERS = True
SHOW_LIMIT_ARCS = True
```

Joint limits are also set near the top:

```python
START_JOINT_LIMITS_DEGREES = [
    [-180.0, 180.0],
    [-180.0, 180.0],
    [-180.0, 180.0],
]
```

## Notes

- The shoulder/base is fixed at `(0, 0)`.
- Orange arcs show the current visual joint limits.
- The workspace circle shows the maximum reach from the shoulder.
- The movement solver is simple and meant for visualization, not precise robot control.
