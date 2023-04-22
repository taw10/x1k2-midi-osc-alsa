project('x1k2-midi-osc', ['c'],
        version: '0.1.0',
        license: 'GPL3+',
        default_options: ['buildtype=debugoptimized'])

# Dependencies
lo_dep = dependency('liblo', required: true)
alsa_dep = dependency('alsa', required: true)

executable('x1k2-midi-osc',
           ['x1k2-midi-osc.c'],
           dependencies: [lo_dep, alsa_dep],
           install: true)
