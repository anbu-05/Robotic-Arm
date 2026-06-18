import json
import math
import os

import matplotlib.pyplot as plt
from matplotlib.patches import Arc
from matplotlib.widgets import Button, CheckButtons, RangeSlider, Slider, TextBox


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
SHOW_SAVE_BUTTON = True
AUTO_LOAD_CONFIG = True
SAVE_IMAGE_WITH_CONFIG = True

ANGLE_MIN_DEGREES = -180.0
ANGLE_MAX_DEGREES = 180.0
CONFIG_FILE_NAME = "arm_config.json"
IMAGE_FILE_NAME = "arm_workspace.png"
START_JOINT_LIMITS_DEGREES = [
    [-180.0, 180.0],
    [-180.0, 180.0],
    [-180.0, 180.0],
]
START_JOINT_LOCKS = [False, False, False]

LINK_COLOR = "tab:blue"
JOINT_COLOR = "black"
ACTIVE_JOINT_COLOR = "tab:red"
WORKSPACE_COLOR = "lightgray"
LIMIT_COLOR = "tab:orange"

DRAG_PICK_RADIUS_MM = 25.0
IK_ITERATIONS = 20


def get_joint_positions(link_lengths, angles_degrees):
    x_positions = [0.0]
    y_positions = [0.0]

    x = 0.0
    y = 0.0
    total_angle = 0.0

    for index in range(len(link_lengths)):
        total_angle += math.radians(angles_degrees[index])
        x += link_lengths[index] * math.cos(total_angle)
        y += link_lengths[index] * math.sin(total_angle)
        x_positions.append(x)
        y_positions.append(y)

    return make_points(x_positions, y_positions)


def get_point_positions(link_lengths, angles_degrees, point_count):
    return get_joint_positions(link_lengths[:point_count], angles_degrees[:point_count])


def make_points(x_positions, y_positions):
    points = []
    for index in range(len(x_positions)):
        points.append([x_positions[index], y_positions[index]])
    return points


def normalize_angle_degrees(angle):
    while angle > 180.0:
        angle -= 360.0
    while angle < -180.0:
        angle += 360.0
    return angle


def clamp(value, low, high):
    if value < low:
        return low
    if value > high:
        return high
    return value


def clamp_angle(angle, joint_limits):
    return clamp(normalize_angle_degrees(angle), joint_limits[0], joint_limits[1])


def clamp_angles(angles, joint_limits):
    new_angles = []
    for index in range(len(angles)):
        new_angles.append(clamp_angle(angles[index], joint_limits[index]))
    return new_angles


def get_angles_from_points(points):
    angles = []
    previous_absolute_angle = 0.0

    for index in range(len(points) - 1):
        dx = points[index + 1][0] - points[index][0]
        dy = points[index + 1][1] - points[index][1]
        absolute_angle = math.degrees(math.atan2(dy, dx))
        angles.append(normalize_angle_degrees(absolute_angle - previous_absolute_angle))
        previous_absolute_angle = absolute_angle

    return angles


def get_absolute_angles(angles_degrees):
    absolute_angles = []
    total_angle = 0.0

    for angle in angles_degrees:
        total_angle += angle
        absolute_angles.append(total_angle)

    return absolute_angles


def get_distance(point_a, point_b):
    dx = point_a[0] - point_b[0]
    dy = point_a[1] - point_b[1]
    return math.hypot(dx, dy)


def solve_angles_with_ccd(link_lengths, angles, target, active_point_index, joint_locks, joint_limits):
    new_angles = angles.copy()
    highest_joint_index = active_point_index - 1

    for _ in range(IK_ITERATIONS):
        for joint_index in range(highest_joint_index, -1, -1):
            if joint_locks[joint_index]:
                continue

            points = get_point_positions(link_lengths, new_angles, active_point_index)
            joint = points[joint_index]
            end = points[active_point_index]

            current_angle = math.atan2(end[1] - joint[1], end[0] - joint[0])
            target_angle = math.atan2(target[1] - joint[1], target[0] - joint[0])
            change_degrees = math.degrees(target_angle - current_angle)

            new_angles[joint_index] += change_degrees
            new_angles[joint_index] = clamp_angle(new_angles[joint_index], joint_limits[joint_index])

    return new_angles


def draw_workspace(axis, link_lengths):
    max_reach = sum(link_lengths)
    min_reach = max(0.0, max(link_lengths) - (sum(link_lengths) - max(link_lengths)))

    outer_circle = plt.Circle((0.0, 0.0), max_reach, fill=False, color=WORKSPACE_COLOR)
    axis.add_patch(outer_circle)

    if min_reach > 0.0:
        inner_circle = plt.Circle((0.0, 0.0), min_reach, fill=False, color=WORKSPACE_COLOR, linestyle="--")
        axis.add_patch(inner_circle)


def draw_limit_arcs(axis, points, angles, link_lengths, joint_limits):
    if not SHOW_LIMIT_ARCS:
        return

    absolute_angles = get_absolute_angles(angles)

    for index in range(3):
        parent_angle = 0.0
        if index > 0:
            parent_angle = absolute_angles[index - 1]

        start_angle = parent_angle + joint_limits[index][0]
        end_angle = parent_angle + joint_limits[index][1]
        radius = min(link_lengths[index] * 0.35, 40.0)

        arc = Arc(
            points[index],
            radius * 2.0,
            radius * 2.0,
            theta1=start_angle,
            theta2=end_angle,
            color=LIMIT_COLOR,
            linewidth=2,
            linestyle="--",
        )
        axis.add_patch(arc)


def draw_arm(axis, points, link_lengths, angles, joint_limits, active_joint_index):
    axis.clear()
    axis.set_aspect("equal", adjustable="box")
    axis.grid(True)
    axis.set_title("3 Link 2D Robot Arm")
    axis.set_xlabel("X (mm)")
    axis.set_ylabel("Y (mm)")

    reach = sum(link_lengths)
    axis.set_xlim(-reach - 40.0, reach + 40.0)
    axis.set_ylim(-reach - 40.0, reach + 40.0)

    if SHOW_WORKSPACE:
        draw_workspace(axis, link_lengths)

    draw_limit_arcs(axis, points, angles, link_lengths, joint_limits)

    x_positions = []
    y_positions = []
    for point in points:
        x_positions.append(point[0])
        y_positions.append(point[1])

    axis.plot(x_positions, y_positions, color=LINK_COLOR, linewidth=4)
    axis.scatter(x_positions, y_positions, color=JOINT_COLOR, s=50, zorder=3)

    if active_joint_index is not None:
        axis.scatter(
            [x_positions[active_joint_index]],
            [y_positions[active_joint_index]],
            color=ACTIVE_JOINT_COLOR,
            s=90,
            zorder=4,
        )

    if SHOW_JOINT_LABELS:
        labels = ["shoulder", "elbow", "wrist", "hand"]
        for index in range(len(labels)):
            axis.text(x_positions[index] + 5.0, y_positions[index] + 5.0, labels[index])

    if SHOW_END_POSITION:
        text = f"hand: x={x_positions[-1]:.1f} mm, y={y_positions[-1]:.1f} mm"
        axis.text(0.02, 0.96, text, transform=axis.transAxes)


def find_nearest_draggable_joint(points, mouse_point):
    nearest_index = None
    nearest_distance = None

    for index in range(1, len(points)):
        distance = get_distance(points[index], mouse_point)
        if nearest_distance is None or distance < nearest_distance:
            nearest_distance = distance
            nearest_index = index

    if nearest_distance is not None and nearest_distance <= DRAG_PICK_RADIUS_MM:
        return nearest_index

    return None


def apply_new_lengths(link_lengths, text_boxes):
    new_lengths = []

    for text_box in text_boxes:
        try:
            new_length = float(text_box.text)
        except ValueError:
            return link_lengths

        if new_length <= 0.0:
            return link_lengths

        new_lengths.append(new_length)

    return new_lengths


def save_config(link_lengths, joint_angles, joint_limits, joint_locks):
    config = {
        "link_lengths_mm": link_lengths,
        "joint_angles_degrees": joint_angles,
        "joint_limits_degrees": joint_limits,
        "joint_locks": joint_locks,
    }

    with open(CONFIG_FILE_NAME, "w", encoding="utf-8") as config_file:
        json.dump(config, config_file, indent=2)


def load_config(link_lengths, joint_angles, joint_limits, joint_locks):
    if not AUTO_LOAD_CONFIG:
        return link_lengths, joint_angles, joint_limits, joint_locks
    if not os.path.exists(CONFIG_FILE_NAME):
        return link_lengths, joint_angles, joint_limits, joint_locks

    try:
        with open(CONFIG_FILE_NAME, "r", encoding="utf-8") as config_file:
            config = json.load(config_file)
    except (OSError, json.JSONDecodeError):
        return link_lengths, joint_angles, joint_limits, joint_locks

    loaded_lengths = config.get("link_lengths_mm", link_lengths)
    loaded_angles = config.get("joint_angles_degrees", joint_angles)
    loaded_limits = config.get("joint_limits_degrees", joint_limits)
    loaded_locks = config.get("joint_locks", joint_locks)

    if len(loaded_lengths) != 3 or len(loaded_angles) != 3:
        return link_lengths, joint_angles, joint_limits, joint_locks
    if len(loaded_limits) != 3 or len(loaded_locks) != 3:
        return link_lengths, joint_angles, joint_limits, joint_locks

    loaded_angles = clamp_angles(loaded_angles, loaded_limits)
    return loaded_lengths, loaded_angles, loaded_limits, loaded_locks


def main():
    link_lengths = LINK_LENGTHS_MM.copy()
    joint_limits = []
    for joint_limit in START_JOINT_LIMITS_DEGREES:
        joint_limits.append(joint_limit.copy())

    joint_angles = clamp_angles(START_ANGLES_DEGREES.copy(), joint_limits)
    joint_locks = START_JOINT_LOCKS.copy()
    link_lengths, joint_angles, joint_limits, joint_locks = load_config(
        link_lengths,
        joint_angles,
        joint_limits,
        joint_locks,
    )
    points = get_joint_positions(link_lengths, joint_angles)
    active_joint_index = None
    updating_sliders = False

    figure, axis = plt.subplots()
    text_boxes = []
    angle_sliders = []
    limit_sliders = []

    bottom_space = 0.10
    if SHOW_LENGTH_BOXES:
        bottom_space += 0.12
    if SHOW_ANGLE_SLIDERS:
        bottom_space += 0.18
    if SHOW_LIMIT_SLIDERS:
        bottom_space += 0.18
    plt.subplots_adjust(bottom=bottom_space)

    def redraw():
        nonlocal points
        points = get_joint_positions(link_lengths, joint_angles)
        draw_arm(axis, points, link_lengths, joint_angles, joint_limits, active_joint_index)
        figure.canvas.draw_idle()

    def update_angle_sliders():
        nonlocal updating_sliders
        if not SHOW_ANGLE_SLIDERS:
            return

        updating_sliders = True
        for index in range(3):
            angle_sliders[index].set_val(joint_angles[index])
        updating_sliders = False

    if SHOW_LENGTH_BOXES:
        labels = ["shoulder mm", "elbow mm", "wrist mm"]

        for index in range(3):
            box_axis = figure.add_axes([0.18 + index * 0.22, 0.08, 0.14, 0.05])
            text_box = TextBox(box_axis, labels[index], initial=str(link_lengths[index]))
            text_boxes.append(text_box)

        button_axis = figure.add_axes([0.74, 0.08, 0.16, 0.05])
        apply_button = Button(button_axis, "apply")

        def on_apply(_):
            nonlocal points, link_lengths
            link_lengths = apply_new_lengths(link_lengths, text_boxes)
            redraw()

        apply_button.on_clicked(on_apply)

    if SHOW_ANGLE_SLIDERS:
        slider_names = ["shoulder deg", "elbow deg", "wrist deg"]

        for index in range(3):
            slider_axis = figure.add_axes([0.20, 0.22 + index * 0.05, 0.55, 0.03])
            slider = Slider(
                slider_axis,
                slider_names[index],
                ANGLE_MIN_DEGREES,
                ANGLE_MAX_DEGREES,
                valinit=joint_angles[index],
            )
            angle_sliders.append(slider)

        def on_slider_change(_):
            nonlocal joint_angles
            if updating_sliders:
                return

            for index in range(3):
                if not joint_locks[index]:
                    joint_angles[index] = clamp_angle(angle_sliders[index].val, joint_limits[index])

            update_angle_sliders()
            redraw()

        for slider in angle_sliders:
            slider.on_changed(on_slider_change)

    if SHOW_LIMIT_SLIDERS:
        slider_names = ["shoulder limit", "elbow limit", "wrist limit"]

        for index in range(3):
            slider_axis = figure.add_axes([0.20, 0.40 + index * 0.05, 0.55, 0.03])
            slider = RangeSlider(
                slider_axis,
                slider_names[index],
                ANGLE_MIN_DEGREES,
                ANGLE_MAX_DEGREES,
                valinit=(joint_limits[index][0], joint_limits[index][1]),
            )
            limit_sliders.append(slider)

        def on_limit_change(_):
            nonlocal joint_angles
            for index in range(3):
                low, high = limit_sliders[index].val
                joint_limits[index] = [low, high]
            joint_angles = clamp_angles(joint_angles, joint_limits)
            update_angle_sliders()
            redraw()

        for slider in limit_sliders:
            slider.on_changed(on_limit_change)

    if SHOW_JOINT_LOCKS:
        lock_labels = ["lock shoulder", "lock elbow", "lock wrist"]
        lock_axis = figure.add_axes([0.80, 0.76, 0.17, 0.14])
        lock_buttons = CheckButtons(lock_axis, lock_labels, joint_locks)

        def on_lock_change(label):
            index = lock_labels.index(label)
            joint_locks[index] = not joint_locks[index]
            redraw()

        lock_buttons.on_clicked(on_lock_change)

    if SHOW_SAVE_BUTTON:
        save_axis = figure.add_axes([0.80, 0.68, 0.17, 0.05])
        save_button = Button(save_axis, "save config")

        def on_save(_):
            save_config(link_lengths, joint_angles, joint_limits, joint_locks)
            if SAVE_IMAGE_WITH_CONFIG:
                figure.savefig(IMAGE_FILE_NAME)

        save_button.on_clicked(on_save)

    def on_press(event):
        nonlocal active_joint_index
        if event.inaxes != axis or event.xdata is None or event.ydata is None:
            return

        mouse_point = [event.xdata, event.ydata]
        active_joint_index = find_nearest_draggable_joint(points, mouse_point)
        redraw()

    def on_release(_):
        nonlocal active_joint_index
        active_joint_index = None
        redraw()

    def on_move(event):
        nonlocal joint_angles
        if active_joint_index is None:
            return
        if event.inaxes != axis or event.xdata is None or event.ydata is None:
            return

        target = [event.xdata, event.ydata]
        joint_angles = solve_angles_with_ccd(
            link_lengths,
            joint_angles,
            target,
            active_joint_index,
            joint_locks,
            joint_limits,
        )
        update_angle_sliders()
        redraw()

    figure.canvas.mpl_connect("button_press_event", on_press)
    figure.canvas.mpl_connect("button_release_event", on_release)
    figure.canvas.mpl_connect("motion_notify_event", on_move)

    redraw()
    plt.show()


if __name__ == "__main__":
    main()
