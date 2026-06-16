# KNOWN BUGS:

1. ~~listparam doesnt really print anything. when using microsoft serial monitor.~~
2. ~~serious bug: different parameters are getting affected sometimes. like for example i set spike threshold, smoothk gets set to zero. there's some instability with the USB-CDC to microrl~~
3. ~~the filtered values are staying at 0 even though raw values are updating.~~ ~~spike rejection is causing filtered values to stay at 0. spike rejection is necessary tho -if you look at the filtered values and think about it.~~

# CLI Commands

* `setmotor <motor> <pwm> <dir>`
  Set motor PWM and direction

* `stop`
  Stop all motors

* `stop motor`
  Stop specific motor

* `setpos <motor> <position> <speed>`
  Move motor to target position using closed-loop control

* `getpos <motor>`
  Returns current position(s) (ADC-based). If no motor specified, prints all 6 positions (M0A, M0B, M1A, M1B, M2A, M2B), one per line

* `stoppos <motor>`
  Disable position control for motor

* `setmotorparam <motor> <parameter> <value>`
  Set motor-specific parameter (flipdir, pos_start, pos_end)

* `getmotorparam <motor> <parameter>`
  Get motor-specific parameter value

* `setparam <param> <value>`
  Update runtime parameters

* `getparam <param>`
  Read parameter value

* `listparams`
  List all parameters

* `couple <master motor> <slave motor> <inverse>`
  Couple two motors together for synchronized motion

* `decouple <master motor>`
  Remove motor coupling

* `listcoupled`
  Display all active motor couplings

Params (dynamic):

* spike_threshold
* adc_filter (0/1)
* var_samples
* var_to_check
* range_samples
* range_to_check

---

# Features

* 6 motor control (PWM + direction)
* ADC (6 channels, DMA, continuous)
* Runtime-configurable parameters (table-driven)
* USB CDC CLI (microrl)
* Command parsing via argc/argv
* Ctrl+C → emergency stop
* Non-blocking main loop
* Real-time tuning (no reflashing)
* Extensible parameter system (no execute() changes needed)
* Closed-loop position control (per motor)
* Direction correction via software (flipdir)
* Position limiting
* Motor coupling (master/slave synchronized control)
* Coupling inversion support
* Coupling inspection via CLI

---

---

# ADC Filtering Details

<!If this system evolves, consider adding a timing section (loop frequency, ADC sampling rate, effective filter bandwidth) to better understand control responsiveness.

>

Pipeline per channel:

1. **Initialization guard**
   First sample is directly assigned to avoid spike rejection locking to zero.

2. **Spike rejection**
   Rejects sudden jumps larger than `spike_threshold` by clamping to previous value. (note, spike rejection is turned off for now because it is causing issues during initialization)

3. **Exponential smoothing**
   `filtered = (prev * smooth_k + raw) / (smooth_k + 1)`
   Controls noise vs responsiveness tradeoff.

4. **Deadband**
   Ignores small changes below `deadband` to eliminate jitter and stabilize output.

5. **Rate limiting (optional)**
   Limits maximum change per loop using `max_step` to smooth motion.

Outputs:

* `adc_filtered[]` → filtered ADC values
* `motors[].pos` → control input

---

# Position Control

Basic closed-loop control using ADC position feedback.

Behavior:

* Motor moves toward `target_pos`
* Speed limited by `target_pwm`
* Stops when within small error band
* Continuously corrects drift

Internal logic:

* Error = target - current
* Direction based on sign of error
* PWM proportional to error (P control)
* Clamped to max speed
* Target position clamped to valid motor range

Notes:

* Depends heavily on ADC stability
* Requires tuning of gain (`pos_kp`) and deadband
* Too high gain → oscillation
* Too low gain → slow response

---

# All Parameters (What They Do)

<!In future updates, include valid ranges and typical values for each parameter (e.g., smooth_k: 1–15). This avoids trial-and-error tuning during debugging.

>

## ADC Filtering Parameters

* **adc_filter (0/1)**
  Master enable for filtering.
  0 → raw ADC values
  1 → filtered pipeline enabled

* **spike_threshold**
  Max allowed jump between consecutive samples.
  If exceeded, value is clamped to previous.
  Too low → blocks real motion
  Too high → spikes pass through

* **spike_start_delay**
  amount of time spike rejection is ignored -this fixes the problem where the filtering cannot even start because of spike rejection.

* **smooth_k**
  Exponential smoothing strength.
  Formula: `filtered = (prev * k + raw) / (k + 1)`
  Higher → smoother but slower response

* **deadband**
  Minimum change required to update output.
  Suppresses small fluctuations completely

* **enable_rate_limit (0/1)**
  Enables step limiting per loop iteration

* **max_step**
  Maximum allowed change per loop when rate limiting is enabled

---

## ADC Debug Parameters

* **var_samples**
  Number of samples used for variation (diff-based noise measurement)

* **var_to_check**
  Channel index used for variation measurement

* **range_samples**
  Number of samples used for range (min-max window)

* **range_to_check**
  Channel index used for range measurement

---

## Internal Buffers (for reference)

* **AD_RES_BUFFER[]**
  Raw ADC DMA buffer (continuous updates)

* **adc_filtered[]**
  Final filtered output per channel

* **adc_variation[]**
  Average step difference (noise metric)

* **adc_range[]**
  Min-max range over a sample window

* **motors[].pos**
  Final value used by control system

---

# Presets (Recommended Starting Points)

<!You may want to add a CLI command mapping for presets (e.g., preset balanced) and document it here for faster switching during runtime.

>

**1. Debug (see real signal)**

* spike_threshold = 100
* smooth_k = 1
* deadband = 0
* enable_rate_limit = 0

Behavior:

* Almost raw ADC
* Useful for diagnosing noise and wiring

---

**2. Balanced (default working mode)**

* spike_threshold = 40
* smooth_k = 6
* deadband = 8
* enable_rate_limit = 0

Behavior:

* Good stability
* Acceptable responsiveness
* Suitable for most control tasks

---

**3. Stable (minimal jitter)**

* spike_threshold = 60
* smooth_k = 10
* deadband = 12
* enable_rate_limit = 0

Behavior:

* Very low visible noise
* Output feels steady
* Slight lag

---

**4. Control Smooth (for servo motion)**

* spike_threshold = 40
* smooth_k = 6
* deadband = 6
* enable_rate_limit = 1
* max_step = 10–20

Behavior:

* Smooth transitions
* No sudden jumps
* Best for driving actuators directly

---

# Tuning Strategy

1. Start with **Balanced preset**
2. Increase **deadband** until jitter disappears
3. Increase **smooth_k** if still noisy
4. Adjust **spike_threshold** to avoid blocking real motion
5. Enable **rate limiting** only if motion feels jerky

Rule of thumb:

* Deadband kills jitter
* Smoothing reduces noise energy
* Spike threshold removes outliers
* Rate limit controls motion speed

---

<!Future entries could include measured results (before/after noise values, range reductions, etc.) to track quantitative improvements over time. >

# Progress Log (CLI + Params)

* Added microrl and wired USB RX → char stream into CLI
* Implemented execute() with command-based parsing (setmotor, stop)
* Added SIGINT (Ctrl+C) for emergency stop with main-loop handling
* Introduced runtime parameters (replaced macros with variables)
* Implemented setparam for live tuning
* Refactored params to table-driven lookup (no execute() edits needed)
* Added getparam and listparams for visibility
* Cleaned print() to restore proper CLI echo/output
* Reworked USB RX to use ring buffer (decoupled ISR from processing)
* Moved microrl_insert_char() into main loop (fixes RX–TX overlap risk)
* Switched print() to blocking retry (temporary USB stability fix)
* Fixed USB RX handler bug (nested function issue)
* Hooked CDC_Receive_FS → USB_CDC_RxHandler
* Verified microrl receives input via ring buffer
* Observed listparams issue persists (likely TX-side or monitor-related)
* Added position control (setpos, getpos, stoppos)
* Added software direction correction (flipdir control via CLI)
* Renamed MotorState.flip_dir to flipdir and replaced setdirflip/getdirflip with setmotorparam/getmotorparam commands
* Modified getpos command to optionally print all motor positions when no argument is provided

---

# Progress Log (ADC Filtering)

* Implemented spike rejection + exponential smoothing
* Identified mismatch between "variation" and actual signal range
* Added range measurement (min/max window-based)
* Introduced configurable smoothing (`smooth_k`)
* Added deadband to eliminate jitter and reduce apparent range
* Added optional rate limiter for controlled transitions
* Fixed initialization bug (filter stuck at 0 due to spike rejection)
* Verified stable filtered output under noisy wiring conditions
* Enabled runtime tuning of all filter parameters via CLI

---

# Progress Log (others - human made)

- implemented position limiting. "The position_control() function has been updated to clamp the target position to the valid range"


- added motor coupling:

  > you can couple two motors using `couple <master motor> <slave motor> <inverse>`. the inverse tells whether the motors need to spin in opposite directions. 

  > the slave motor's position_control flag gets cleared, and a new flag is_slave is set to high, a new flag am_i_coupled_inversely (1 when it's coupled in the opposite direction) is set according to the <inverse> argument

  > the master gets a new flag slave_index, which equals -1 for no slaves

  > when two motors are coupled, both the motors use the master's pos reading. for each motor, if is_slave is high the motor gets skipped. then, position_control() checks whether it has a slave, if it does, it first computes the direction and pwm of the master first, and then sets the slaves direction (the same as master if am_i_coupled_inversely is 0, and opposite if am_i_coupled_inversely is 1) and pwm (the pwm is the same as the master).

  > we'll also have a decouple `decouple <master motor>` command to decouples the master and the slave.

  Updated main.c with motor coupling support:

  Extended MotorState:
    - is_slave
    - am_i_coupled_inversely
    - slave_index

  Updated position_control():
    - skips slave motors
    - computes master first
    - mirrors master direction/pwm to slave
    - keeps slave pos synced to master position

  Added commands in execute():
    - `couple <master> <slave> <inverse>`
    - `decouple <master>`

  Ensured initialization:
    - slave_index = -1 after memset(motors, 0, ...) in main()
    - same reset in sigint()

  Added safer handling for master stop / stoppos so coupled slave PWM is also cleared.

  Added `listcoupled` command. It displays all coupled motor pairs with their direction mode in machine-readable format like getpos:

  ```
  No couplings: \n (empty line)
  With couplings: M1A=M2B:same,M2A=M0A:inverse\n
  ```