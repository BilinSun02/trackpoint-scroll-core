project('trackpoint-scroll-core', 'c',
  version: '0.1.0',
  default_options: ['c_std=c11', 'warning_level=3'])

inc = include_directories('include')
m_dep = meson.get_compiler('c').find_library('m', required: false)

core = static_library(
  'trackpoint-scroll-core',
  ['src/engine.c', 'src/profiles.c'],
  include_directories: inc,
  dependencies: m_dep,
  install: false)

core_dep = declare_dependency(
  link_with: core,
  include_directories: inc,
  dependencies: m_dep)

if not meson.is_subproject()
  engine_test = executable(
    'test-engine', 'tests/test_engine.c',
    dependencies: core_dep)
  profile_test = executable(
    'test-profiles', 'tests/test_profiles.c',
    dependencies: core_dep)

  test('engine', engine_test)
  test('profiles', profile_test)
endif
