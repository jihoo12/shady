project('shady', 'c', version: '0.1.0',
  default_options: ['buildtype=debugoptimized', 'c_std=c11', 'warning_level=2'])

# wlroots has an unstable API: this source targets the inspected 0.20 series.
wlroots = dependency('wlroots-0.20', version: ['>=0.20.2', '<0.21'])
egl = dependency('egl')
glesv2 = dependency('glesv2')

shader_dir = join_paths(meson.project_source_root(), 'shaders')

shady_sources = [
  'src/main.c',
  'src/config.c',
  'src/input.c',
  'src/xdg.c',
  'src/output.c',
  'src/render/render.c',
  'src/render/gl_pipeline.c',
  'src/render/math3d.c',
  'src/render/pick3d.c',
]

physics_enabled = get_option('physics').enabled()
fps_enabled = get_option('fps').enabled()
window_motion_enabled = get_option('window_motion').enabled()
if physics_enabled
  shady_sources += 'src/modules/physics/physics.c'
endif
if fps_enabled
  shady_sources += 'src/modules/fps/fps.c'
endif
if window_motion_enabled
  shady_sources += 'src/modules/window_motion/window_motion.c'
endif

executable('shady',
  shady_sources,
  c_args: [
    '-DWLR_USE_UNSTABLE',
    '-D_POSIX_C_SOURCE=200809L',
    '-DSHADY_SHADER_DIR="@0@"'.format(shader_dir),
    '-DSHADY_HAS_PHYSICS=@0@'.format(physics_enabled ? 1 : 0),
    '-DSHADY_HAS_FPS=@0@'.format(fps_enabled ? 1 : 0),
    '-DSHADY_HAS_WINDOW_MOTION=@0@'.format(window_motion_enabled ? 1 : 0),
  ],
  include_directories: include_directories('src'),
  dependencies: [
    wlroots,
    dependency('wayland-server'),
    dependency('xkbcommon'),
    egl,
    glesv2,
    meson.get_compiler('c').find_library('m', required: true),
  ],
  install: true)
