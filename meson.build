project(
  'randfs',
  'c',
  meson_version : '>= 1.3.0',
  version : '0.1',
  default_options : ['warning_level=3'],
)

dependencies = [
]

cc = meson.get_compiler('c')
math_dep = cc.find_library('m', required : true)

mkfs = executable(
  'mkfs',
  'tools/mkfs.c',
  dependencies : math_dep,
  install : true,
)

image_dump = executable(
  'image_dump',
  'tools/image_dump.c',
  install : true,
)

