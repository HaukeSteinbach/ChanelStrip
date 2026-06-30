Steinbach Chanel Strip  v1.0.1

A channel strip audio plugin (VST3 / AU) built with JUCE.
Developed by Hauke Steinbach.

mail@haukesteinbach.de
www.haukesteinbach.de

------------------------------------------------------------------------

FEATURES

EQ
- HPF at 40 Hz (on/off)
- Low Shelf at 100 Hz, +/-12 dB  --  right-click: Sub Left
- Mid Bell at 1 kHz, +/-12 dB  --  Q scales subtly with gain (Q: 1.0 @ 0 dB → 0.75 @ ±12 dB)
- High Shelf at 10 kHz, +/-12 dB
- All EQ parameters smoothed (50 ms ramp) to prevent clicks

Sub Left (right-click the LOW knob)
- Mirrors the left channel's sub content (< 77 Hz) to the right channel
- Right channel above 77 Hz remains unchanged
- 2nd-order Butterworth crossover at 77 Hz
- Filter runs continuously to prevent clicks on toggle

Preamp / Saturation
- Gain +/-24 dB with 2x oversampling to suppress aliasing
- Three saturation characters:
    Warm    -- Neve transformer (asymmetric tanh, 2nd + 3rd harmonics)
    Medium  -- Tube Class-A (hard-driven tanh + cubic blend, 3rd harmonics)
    Hot     -- Sinusoidal wavefolder (dense harmonic series)
- Character knob morphs between dry and saturated signal
- L/R Link: when off, each instance has independent analog L/R variation

Routing
- Pan with equal-power law, smoothed per sample
- Binaural Pan mode (right-click the pan knob to toggle):
    Woodworth Spherical Head Model
    ITD (Interaural Time Delay): max ~29 samples at 44.1 kHz
    ILD (Interaural Level Difference): equal-power pan
    All parameters smoothed (10 ms) to prevent artifacts

Output Clipper
- Hard clip at -4 dB
- Neve-style soft clip at -4 dB (linear below 70%, tanh above)

Analog Console Mode
- Groups up to 4 plugin instances
- Crosstalk, voltage sag, and morph modulation between instances
- Cross-plugin shared memory communication

------------------------------------------------------------------------

INSTALLATION (macOS)

Run: SteinbachChanelStrip-1.0.1.pkg

Installs to:
  VST3  ->  /Library/Audio/Plug-Ins/VST3/
  AU    ->  /Library/Audio/Plug-Ins/Components/

Admin password required.
If Gatekeeper shows a warning: right-click the .pkg and choose Open.

------------------------------------------------------------------------

PARAMETERS

  Parameter       Range           Default  Description
  --------------- --------------- -------- ----------------------------
  HPF 40 Hz       on / off        on       High-pass filter at 40 Hz
  Low (100 Hz)    -12 to +12 dB   0        Low shelf (right-click: Sub Left)
  Mid (1 kHz)     -12 to +12 dB   0        Mid bell, Q 1.0→0.75 with gain
  High (10 kHz)   -12 to +12 dB   0        High shelf
  Pan             -1 to +1        0        Stereo pan (right-click: binaural)
  Preamp Gain     -24 to +24 dB   0        Input gain before saturation
  Character       0 to 1          0        Dry/saturated blend
  Soft Clip       on / off        off      Output clipper mode
  Binaural Pan    on / off        off      ITD+ILD binaural panning
  Sub Left        on / off        off      Mirror L sub (< 77 Hz) to R channel
  Console Mode    on / off        off      Analog console group processing
  Console Group   1 - 4           1        Console instance group
  L/R Link        on / off        on       Link L/R analog variation

------------------------------------------------------------------------

LICENSE

Private / proprietary. All rights reserved. Hauke Steinbach 2026.
