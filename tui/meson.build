cc = meson.get_compiler('c')

sqlite3_dep = dependency('sqlite3', required: true)
uuid_dep = dependency('uuid', required: true)

ncursesw_dep = dependency('ncursesw', required: false)
if not ncursesw_dep.found()
  ncursesw_dep = cc.find_library('ncursesw', required: true)
endif

utf8proc_dep = dependency('libutf8proc', required: false)
if not utf8proc_dep.found()
  utf8proc_dep = dependency('utf8proc', required: true)
endif

m_dep = cc.find_library('m', required: true)

trainlog_include = include_directories('include')

strict_c_args = [
  '-D_POSIX_C_SOURCE=200809L',
  '-Wconversion',
  '-Wformat=2',
  '-Wshadow',
]

trainlog_core_sources = files(
  'src/bodyviz.c',
  'src/catalog.c',
  'src/database.c',
  'src/duration.c',
  'src/id.c',
  'src/timeutil.c',
)

trainlog_core = static_library(
  'trainlog_core',
  trainlog_core_sources,
  include_directories: trainlog_include,
  dependencies: [
    sqlite3_dep,
    uuid_dep,
    utf8proc_dep,
    m_dep,
  ],
  c_args: strict_c_args,
)

trainlog_core_dep = declare_dependency(
  link_with: trainlog_core,
  include_directories: trainlog_include,
  dependencies: [
    sqlite3_dep,
    uuid_dep,
    utf8proc_dep,
    m_dep,
  ],
)

trainlog_tui_sources = files(
  'src/main.c',
  'src/theme.c',
  'src/tui.c',
)

trainlog_exe = executable(
  'trainlog',
  trainlog_tui_sources,
  dependencies: [
    trainlog_core_dep,
    ncursesw_dep,
  ],
  c_args: strict_c_args,
  install: true,
)

test_database = executable(
  'test_database',
  'tests/test_database.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test_catalog = executable(
  'test_catalog',
  'tests/test_catalog.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test(
  'database',
  test_database,
)

test(
  'catalog',
  test_catalog,
)

test_session_detail = executable(
  'test_session_detail',
  'tests/test_session_detail.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test(
  'session_detail',
  test_session_detail,
)

test_duration = executable(
  'test_duration',
  'tests/test_duration.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test_body_metrics = executable(
  'test_body_metrics',
  'tests/test_body_metrics.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test('duration', test_duration)
test('body_metrics', test_body_metrics)

test_bodyviz = executable(
  'test_bodyviz',
  'tests/test_bodyviz.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test('bodyviz', test_bodyviz)
