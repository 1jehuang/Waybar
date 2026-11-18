waybar-waveform(5)

# NAME

waybar - native measured-amplitude waveform

# DESCRIPTION

The *waveform/<name>* module reads newline-delimited JSON from a continuous
*exec* command. It does not capture audio. The producer supplies measured,
chronological amplitude history, oldest first. The native GTK drawing area
renders seven antialiased rounded vertical pills using the CSS foreground
color. Each pill represents one chronological RMS bin from the latest up to
28 measured samples, oldest on the left. Changed measured heights interpolate
on the GTK frame clock at up to 60 frames per second. No synthetic motion or
oscillator is generated. Animation stops after each sample interval. Unchanged,
paused and off frames do not start animation timers.

# CONFIGURATION

*exec*: ++
	typeof: string ++
	Required continuous command producing one JSON object per line. As with
	custom modules, WAYBAR_OUTPUT_NAME identifies the bar output. Commands
	are run through /bin/sh. The producer must remain running while paused.

*fps*: ++
	typeof: integer ++
	default: 60 ++
	Maximum animation redraw rate, clamped to 1..60 frames per second. Lower
	values trade smoothness for fewer redraws. GTK frame-clock availability
	may reduce the actual frame rate further.

*width*: ++
	typeof: integer ++
	default: 86 ++
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

The last valid record remains authoritative until another record or EOF.
The producer/helper owns capture freshness and must emit off or paused when
its measured audio source expires. The module does not start a freshness timer.

Records are limited to 16384 bytes excluding newline. Oversized records are
discarded through the next newline. At most 64 KiB are read per main-loop
dispatch, with only the newest complete valid frame retained. There is no
unbounded sample queue. The command is not automatically restarted on EOF.

# STYLE

The drawing area is named *#waveform-<name>*, for example *#waveform-jarvis*.
It has the *module* class, the configured instance ID class, the current
*state* class, and the producer's optional *class*. Set CSS *color* to choose
the pills and compact affordance color. Quiet or empty capture history leaves
seven static two-pixel-high capsules visible. The minimum height is a capture
affordance, not generated audio amplitude.

# EXAMPLE

```
"waveform/jarvis": {
    "exec": "jarvis-waveform-stream",
    "width": 86,
    "height": 24,
    "on-click": "jarvis-toggle"
}
```
