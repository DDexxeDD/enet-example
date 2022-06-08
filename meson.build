project('enet test', 'c')

cc = meson.get_compiler('c')

library_directory = meson.current_source_dir() + '/libs'

unified_dependencies = [
  cc.find_library('pthread'),
]
sokol_dependencies = [
  cc.find_library('GL'),
  cc.find_library('m'),
  cc.find_library('dl'),
  cc.find_library('X11'),
  cc.find_library('Xi'),
  cc.find_library('Xcursor'),
  cc.find_library('pthread')
]

includes = [include_directories('.'),
  include_directories('libs'),
  include_directories('libs/nuklear'),
  include_directories('libs/sokol'),
]

demo_sources = [
  'source/main.c',
  'source/server.c',
  'source/client.c',
  'source/implementations.c',
  'source/gui.c',
  'source/frame_limiter.c',
  'source/packet.c'
]

enet_sources = ['libs/enet/callbacks.c',
  'libs/enet/compress.c',
  'libs/enet/host.c',
  'libs/enet/list.c',
  'libs/enet/packet.c',
  'libs/enet/peer.c',
  'libs/enet/protocol.c',
  'libs/enet/unix.c',
  'libs/enet/win32.c']

unified_binary = executable ('enet_test',
  demo_sources,
  enet_sources,
  include_directories : includes,
  dependencies : [unified_dependencies, sokol_dependencies])
