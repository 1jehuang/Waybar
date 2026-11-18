test_inc = include_directories('../include')

test_dep = [
    catch2,
    fmt,
    gtkmm,
    jsoncpp,
    spdlog,
]

test_src = files(
    'main.cpp',
    'config.cpp',
    '../src/config.cpp',
)

waybar_test = executable(
    'waybar_test',
    test_src,
    dependencies: test_dep,
    include_directories: test_inc,
)

test(
    'waybar',
    waybar_test,
    workdir: meson.project_source_root(),
)

subdir('utils')
subdir('hyprland')

xvfb_run = find_program('xvfb-run', required: false)
if xvfb_run.found()
    waveform_gtk_test = executable(
        'waveform_gtk_test',
        'waveform_gtk.cpp',
        '../src/modules/waveform.cpp',
        '../src/AModule.cpp',
        dependencies: [fmt, gtkmm, jsoncpp, spdlog],
        include_directories: test_inc,
    )
    test('waveform-gtk', xvfb_run,
         args: ['-a', waveform_gtk_test],
         env: ['GDK_BACKEND=x11'],
         timeout: 15)
endif
