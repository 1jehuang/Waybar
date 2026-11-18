waybar-waveform(5)

# NAME

waybar - native measured-amplitude waveform

# DESCRIPTION

The *waveform/<name>* module reads newline-delimited JSON from a continuous
*exec* command. It does not capture audio. The producer supplies measured,
chronological amplitude history, oldest first. The native GTK drawing area
renders an antialiased, filled symmetric envelope using the CSS foreground
color, interpolating changed active history on the GTK frame clock at up to
60 frames per second. Animation stops after each sample interval. Unchanged,
paused and off frames do not start animation timers.

# CONFIGURATION

*exec*: ++
	typeof: string ++
	Required continuous command producing one JSON object per line. As with
	custom modules, WAYBAR_OUTPUT_NAME identifies the bar output. Commands
	are run through /bin/sh. The producer must remain running while paused.

*width*: ++
	typeof: integer ++
	default: 180 ++
	Active width in logical pixels, bounded to 24..2048.

*height*: ++
	typeof: integer ++
	default: 24 ++
	Height and compact noncapturing width, bounded to 8..256.

The standard AModule *on-click*, mouse release, double/triple click, scrolling,
*on-update*, tooltip and cursor options are supported. No automatic execution
or producer restart occurs on click. No audio device is opened by this module.

# JSON CONTRACT

```
{"levels":[0.0,0.2,0.7,0.1],"capturing":true,"state":"listening","class":"listening","tooltip":"Listening","sequence":42,"sample_interval_ms":50}
```

*levels*, *capturing*, and *state* are required. Levels contain up to 128 finite
numbers, clamped to 0..1. They are rolling measured history, not frequencies
or synthetic oscillator values. *sequence* is an optional unsigned 64-bit
sample counter. Increment it for each newly measured sample, not each status
message. *sample_interval_ms* defaults to 50 and is bounded to 16..1000.
*class* is one optional CSS class string. *tooltip* is optional plain text,
not Pango markup.

State *off* hides the module. Other states with *capturing* false show a static
compact pause affordance, or an error mark for state *error*, retaining the
normal click target. EOF or malformed JSON shows the error affordance.
A subsequent valid record recovers automatically while the command is running.

Records are limited to 16384 bytes excluding newline. Oversized records are
discarded through the next newline. At most 64 KiB are read per main-loop
dispatch, with only the newest complete valid frame retained. There is no
unbounded sample queue. The command is not automatically restarted on EOF.

# STYLE

The drawing area is named *#waveform-<name>*, for example *#waveform-jarvis*.
It has the *module* class, the configured instance ID class, the current
*state* class, and the producer's optional *class*. Set CSS *color* to choose
the waveform and compact affordance color. Silence remains genuinely flat.

# EXAMPLE

```
"waveform/jarvis": {
    "exec": "jarvis-waveform-stream",
    "width": 180,
    "height": 24,
    "on-click": "jarvis-toggle"
}
```
