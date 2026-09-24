project('shady', 'c', version: '0.1.0',
  default_options: ['buildtype=debugoptimized', 'c_std=c11', 'warning_level=2'])

# wlroots has an unstable API: this source targets the inspected 0.20 series.
wlroots = dependency('wlroots-0.20', version: ['>=0.20.2', '<0.21'])
egl = dependency('egl')
glesv2 = dependency('glesv2')
wayland_client = dependency('wayland-client')
wayland_protocols = dependency('wayland-protocols')
wayland_scanner = find_program('wayland-scanner')
protocol_dir = wayland_protocols.get_variable(pkgconfig: 'pkgdatadir')
pointer_constraints_xml = join_paths(protocol_dir, 'unstable', 'pointer-constraints', 'pointer-constraints-unstable-v1.xml')
pointer_constraints_header = custom_target('pointer-constraints-client-header',
  input: pointer_constraints_xml,
  output: 'pointer-constraints-unstable-v1-client-protocol.h',
  command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'])
pointer_constraints_code = custom_target('pointer-constraints-private-code',
  input: pointer_constraints_xml,
  output: 'pointer-constraints-unstable-v1-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'])

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
close_animation_enabled = get_option('close_animation').enabled()
scene_effects_enabled = get_option('scene_effects').enabled()
if physics_enabled
  shady_sources += 'src/modules/physics/physics.c'
endif
if fps_enabled
  shady_sources += ['src/modules/fps/fps.c', pointer_constraints_header, pointer_constraints_code]
endif
if window_motion_enabled
  shady_sources += 'src/modules/window_motion/window_motion.c'
endif
if close_animation_enabled
  shady_sources += 'src/modules/close_animation/close_animation.c'
endif
if scene_effects_enabled
  shady_sources += 'src/modules/scene_effects/scene_effects.c'
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
    '-DSHADY_HAS_CLOSE_ANIMATION=@0@'.format(close_animation_enabled ? 1 : 0),
    '-DSHADY_HAS_SCENE_EFFECTS=@0@'.format(scene_effects_enabled ? 1 : 0),
  ],
  include_directories: include_directories('src'),
  dependencies: [
    wlroots,
    dependency('wayland-server'),
    wayland_client,
    dependency('xkbcommon'),
    egl,
    glesv2,
    meson.get_compiler('c').find_library('m', required: true),
  ],
  install: true)
