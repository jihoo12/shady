project('shady', 'c', version: '0.1.0',
  default_options: ['buildtype=debugoptimized', 'c_std=c11', 'warning_level=2'])

# wlroots has an unstable API: this source targets the inspected 0.20 series.
wlroots = dependency('wlroots-0.20', version: ['>=0.20.2', '<0.21'])
executable('shady', 'src/shady.c',
  c_args: ['-DWLR_USE_UNSTABLE', '-D_POSIX_C_SOURCE=200809L'],
  dependencies: [wlroots, dependency('wayland-server'), dependency('xkbcommon')],
  install: true)
