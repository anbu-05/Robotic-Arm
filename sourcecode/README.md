# KNOWN BUGS:
1. listparam doesnt really print anything. when using microsoft serial monitor.
2. serious bug: different parameters are getting affected sometimes. like for example i set spike threshold, smoothk gets set to zero. there's some instability with the USB-CDC to microrl

# CLI Commands

* **setmotor \<motor> \<pwm> \<dir>**
  Set motor PWM and direction

* **stop**
  Stop all motors

* **stop \<motor>**
  Stop specific motor

* **setparam \<param> \<value>**
  Update runtime parameters

* **getparam \<param>**
  Read parameter value

* **listparams**
  List all parameters

Params (dynamic):

* spike_threshold
* adc_filter (0/1)
* var_samples
* var_to_check
* range_samples
* range_to_check
* smooth_k
* deadband
* max_step
* enable_rate_limit

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
* ADC filtering pipeline (spike rejection + smoothing + deadband + optional rate limit)

---

# ADC Filtering Details

Pipeline per channel:

1. **Initialization guard**
   First sample is directly assigned to avoid spike rejection locking to zero.

2. **Spike rejection**
   Rejects sudden jumps larger than `spike_threshold` by clamping to previous value.

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

# Filter Parameters (What They Do)

* **spike_threshold**
  Max allowed jump between samples.
  Lower → aggressive spike removal, may block real motion
  Higher → allows faster changes, less protection

* **smooth_k**
  Strength of exponential smoothing.
  Low (1–3) → fast, noisy
  Medium (4–8) → balanced
  High (8–15) → very smooth, more lag

* **deadband**
  Minimum change required to update output.
  Low (0–3) → responsive, jitter visible
  Medium (5–10) → stable, good for control
  High (10+) → very stable, may feel "sticky"

* **enable_rate_limit**
  Toggle for rate limiting (0/1)

* **max_step**
  Max change per loop when rate limit is enabled.
  Low → very smooth but slow response
  High → closer to raw behavior

---

# Presets (Recommended Starting Points)

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

# Progress Log (CLI + Params)

* Added microrl and wired USB RX → char stream into CLI
* Implemented execute() with command-based parsing (setmotor, stop)
* Added SIGINT (Ctrl+C) for emergency stop with main-loop handling
* Introduced runtime parameters (replaced macros with variables)
* Implemented setparam for live tuning
* Refactored params to table-driven lookup (no execute() edits needed)
* Added getparam and listparams for visibility
* Cleaned print() to restore proper CLI echo/output

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
