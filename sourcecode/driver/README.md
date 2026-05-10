# robarm_driver — Project State

## What this is
A temporary Python web driver for the robotic arm firmware (STM32, USB CDC, microrl-based CLI).
Hosts a local Flask server on `localhost:5000` with an HTML/JS UI.

## Files
```
robarm_driver/
├── server.py          # Flask backend
├── static/
│   └── index.html     # frontend (all-in-one HTML/CSS/JS)
└── config.json        # auto-generated on first run; stores motor names + constants
```

## How to run
```bash
pip install flask pyserial
python server.py
# open http://127.0.0.1:5000
```

## Motor IDs
`M0A, M0B, M1A, M1B, M2A, M2B` — matches firmware exactly.

## Firmware commands used
- `getpos` — no args, returns 6 lines (one per motor), parsed into positions
- `setpos <motor> <pos> <speed>` — closed-loop move
- `stoppos <motor>` — stop position control
- `setmotorparam <motor> flipdir <val>`
- `setmotorparam <motor> pos_start <val>`
- `setmotorparam <motor> pos_end <val>`
- arbitrary CLI passthrough via command console

## Persistent storage
`config.json` in the same folder as `server.py`. Stores per-motor:
- `name` — display name (UI only)
- `flipdir` — pushed to firmware on apply
- `pos_start` — pushed to firmware on apply
- `pos_end` — pushed to firmware on apply

## UI layout
- Left sidebar: serial connect panel, read frequency input, command console (scrolling log)
- Right area: 6 motor cards in a single column

## Per motor card (top to bottom)
1. Motor ID (fixed) + editable name + Apply button
2. Constants row: flipdir, pos_start, pos_end (inputs)
3. Target position slider (0–4095) + number input (synced)
4. Speed slider (1–255) + number input (synced)
5. Position bar: green bar = current pos, yellow tick = target pos
6. Status row: current pos, target pos, delta (Δ)
7. Bottom row: "set pos" button (sends setpos), "stop" button (sends stoppos)

## Read frequency
- Number input in sidebar, default 100ms, floor 50ms
- Frontend polls `/api/poll_interval` every iteration to respect live changes

## Known limitations / TODOs
- Motor names are stored Python-side only (firmware has no name concept yet — planned firmware update)
- `getpos` with no args returns all 6 positions; reply is parsed line by line
- Serial read after write uses a 50ms sleep + drain loop (simple, not interrupt-driven)
- No authentication — localhost only, single user

## Status
v2 — tested against screenshots, three fixes applied:

1. **getpos parser updated** — firmware now returns `M0A=2004,M0B=1558,...` on one labeled CSV line. Parser handles this format, falls back to old 6-line plain-number format.
2. **microrl escape sequences stripped** — `clean_reply()` in server.py strips ANSI escape codes and control chars server-side. Frontend also has `cleanReply()` as a second pass for anything that slips through.
3. **Stop All button** — added to header bar (always visible, red). Sends `stop` command (stops all 6 motors + clears PWM).

Next steps:
- Test against real arm
- Tune serial read timing if replies are still garbled

v3 — four fixes applied:

1. **Parsing bug fixed** — `parse_positions()` now uses `re.finditer(r'(M[012][AB])=(\d+)', reply)` to extract key=val pairs from anywhere in the reply string. Immune to the firmware echoing the command back (`getpos M0A=...`) or appending `IRin >` after the data.
2. **Autoreconnect** — on page load, if not connected, frontend checks `/api/last_port` and attempts reconnect to the previously used port. `last_port`/`last_baud` saved server-side on each successful connect (survives page refresh, not server restart).
3. **Console line splitting** — `logReply()` splits firmware replies on `\n` before logging, so `OK` and `IRin >` each appear on their own line.
4. **Get motor params button** — "get params" button in the poll panel calls `/api/fetch_motor_params`, which sends `getmotorparam <motor> <param>` for all 6 motors × 3 params (flipdir, pos_start, pos_end) and updates the GUI inputs live. Results also logged to console.

v4 — two fixes:
 
1. **getmotorparam parsing fixed** — was using `re.search(r'\d+', reply)` which finds the *first* digit, which could be the motor index in the echoed command (e.g. `M0A` → 0, `M1B` → 1, `M2A` → 2). Switched to `re.findall(r'\d+', reply)[-1]` to grab the last integer, which is always the actual param value before `IRin >`.
2. **Button renamed** — "get params" → "get motor params" to match the CLI command name.

v5 - human changed
1. the driver now calls setmotorparams for each motor at the start of a successful connection once -this'll set the motors parameters once
2. we now save the changed motor speed to the config.json file as well, and load it up next time