# Robotic Arm Visualizer

Current status:
- A first simulator file has been created: `arm_workspace.py`.
- A human-facing `README.md` has been created.
- Goal is a simple 2D robotic arm visualizer for a shoulder, elbow, and wrist.
- Link lengths should be configurable so joint spacing can match the physical arm idea.
- Current version is a three-link 2D arm that can be moved by dragging the elbow, wrist, or hand.
- Angle sliders are available again as an option controlled by `SHOW_ANGLE_SLIDERS`.
- Joint locks are available with checkboxes controlled by `SHOW_JOINT_LOCKS`.
- Joint limits are visible as orange arcs and editable with range sliders.
- Latest run produced this warning: `FigureCanvasAgg is non-interactive, and thus cannot be shown`.
- This means Matplotlib is using the non-interactive `Agg` backend, so the slider window cannot open.
- Running `MPLBACKEND=QtAgg python3 arm_workspace.py` failed because no Qt binding was installed.
- `PyQt6` has been added to `requirements.txt` so `QtAgg` has a GUI backend available.
- `PyQt6` was installed successfully into `.venv`.

Approach notes:
- Prefer a very small Python + Matplotlib implementation first.
- Useful existing reference: PythonRobotics has a two-joint planar arm example.
- Avoid full physics or heavy robotics frameworks unless the simple version is not enough.
- PythonRobotics is best treated as reference/example code, not as a heavy dependency.
- The useful pieces are forward kinematics, inverse kinematics, and Matplotlib interaction.
- The current local code uses simple forward kinematics for three links.
- Link lengths are hardcoded at the top of `arm_workspace.py` in `LINK_LENGTHS_MM`.
- Link lengths are shown and editable while the program is running using millimeter text boxes.
- The shoulder/base is fixed at `(0, 0)`.
- Dragging the arm uses a simple angle-based CCD inverse-kinematics solver.
- Angle sliders stay synced after dragging the arm.
- Locked joints keep their current angle during dragging and slider changes.
- Joint limit sliders clamp the angle sliders and the drag solver.

Open decisions:
- Whether joint limits are needed.
- Whether this should only show reachable workspace or also animate arm poses.
- Whether the wrist should represent only a third link angle or also a target hand orientation.
- Whether to use TkAgg, QtAgg, or a save-to-image fallback for environments without a GUI backend.
- Whether dragging the elbow/wrist should preserve orientation differently.
- Whether joint limits should be saved between runs.

Run notes:
- Install dependencies inside the active venv with `pip install -r requirements.txt`.
- Launch with `MPLBACKEND=QtAgg python3 arm_workspace.py`.
- If Matplotlib says `/home/anbu/.config/matplotlib` is not writable, set `MPLCONFIGDIR=.matplotlib-cache` before running.
- Human usage instructions live in `README.md`.
