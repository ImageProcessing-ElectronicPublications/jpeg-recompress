project(
  'jpeg-recompress',
  'c',
  license : 'MIT',
  default_options : ['buildtype=release', 'prefix=/usr'],
  version : '2.6.4'
)

cc = meson.get_compiler('c')
libm_dep = cc.find_library('m', required: true)

libiqa = dependency('iqa')
libjpeg = dependency('libjpeg')
libsmallfry = dependency('smallfry')
libwebp = dependency('libwebp')

libjmetrics = static_library(
  'jmetrics',
  sources: [
    'src/jmetrics.c',
  ],
  install: false,
)

jpeg_compare = executable(
  'jpeg-compare',
  sources: [
    'src/jpeg-compare.c',
  ],
  dependencies: [libiqa, libm_dep, libsmallfry, libjpeg],
  link_with: [libjmetrics],
  install: true,
  install_dir: get_option('bindir'),
)

jpeg_hash = executable(
  'jpeg-hash',
  sources: [
    'src/jpeg-hash.c',
  ],
  dependencies: [libjpeg],
  link_with: [libjmetrics],
  install: true,
  install_dir: get_option('bindir'),
)

jpeg_recompress = executable(
  'jpeg-recompress',
  sources: [
    'src/jpeg-recompress.c',
  ],
  dependencies: [libiqa, libm_dep, libsmallfry, libjpeg],
  link_with: [libjmetrics],
  install: true,
  install_dir: get_option('bindir'),
)

jpeg_zfpoint = executable(
  'jpeg-zfpoint',
  sources: [
    'src/jpeg-zfpoint.c',
  ],
  dependencies: [libm_dep, libsmallfry, libjpeg],
  link_with: [libjmetrics],
  install: true,
  install_dir: get_option('bindir'),
)

webp_compress = executable(
  'webp-compress',
  sources: [
    'src/webp-compress.c',
  ],
  dependencies: [libiqa, libm_dep, libsmallfry, libjpeg, libwebp],
  link_with: [libjmetrics],
  install: true,
  install_dir: get_option('bindir'),
)

install_man(
  [
    'man/jpeg-compare.1',
    'man/jpeg-hash.1',
    'man/jpeg-recompress.1',
    'man/jpeg-zfpoint.1',
    'man/webp-compress.1',
  ],
  install_dir: get_option('mandir') / 'man1',
)

install_data(
  sources: [
    'CHANGELOG',
    'LICENSE',
    'README.md',
  ],
  install_dir: get_option('datadir') / 'doc' / meson.project_name(),
)
