cc = meson.get_compiler('c')

sqlite3_dep = dependency('sqlite3', required: true)
uuid_dep = dependency('uuid', required: true)
udev_dep = dependency('libudev', required: true)
mtp_dep = dependency('libmtp', required: true)

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
  'src/usb.c',
  'src/mtp.c',
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
    udev_dep,
    mtp_dep,
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
    udev_dep,
    mtp_dep,
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

test_exercise_performance = executable(
  'test_exercise_performance',
  'tests/test_exercise_performance.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test(
  'exercise_performance',
  test_exercise_performance,
)

test_session_type_schema = executable(
  'test_session_type_schema',
  'tests/test_session_type_schema.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test(
  'session_type_schema',
  test_session_type_schema,
)

test_session_edit = executable(
  'test_session_edit',
  'tests/test_session_edit.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test(
  'session_edit',
  test_session_edit,
)

test_body_observation_edit = executable(
  'test_body_observation_edit',
  'tests/test_body_observation_edit.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test(
  'body_observation_edit',
  test_body_observation_edit,
)

trainlog_usb_probe = executable(
  'trainlog-usb-probe',
  'tools/usb_probe.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test_usb = executable(
  'test_usb',
  'tests/test_usb.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test(
  'usb',
  test_usb,
)

trainlog_mtp_probe = executable(
  'trainlog-mtp-probe',
  'tools/mtp_probe.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test_mtp = executable(
  'test_mtp',
  'tests/test_mtp.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test(
  'mtp',
  test_mtp,
)

trainlog_mtp_exchange_probe = executable(
  'trainlog-mtp-exchange-probe',
  'tools/mtp_exchange_probe.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

trainlog_mtp_roundtrip_probe = executable(
  'trainlog-mtp-roundtrip-probe',
  'tools/mtp_roundtrip_probe.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test_exercise_profile_schema = executable(
  'test_exercise_profile_schema',
  'tests/test_exercise_profile_schema.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test(
  'exercise_profile_schema',
  test_exercise_profile_schema,
)

test_continuous_session = executable(
  'test_continuous_session',
  'tests/test_continuous_session.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test(
  'continuous_session',
  test_continuous_session,
)

test_continuous_detail = executable(
  'test_continuous_detail',
  'tests/test_continuous_detail.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test(
  'continuous_detail',
  test_continuous_detail,
)

trainlog_mtp_mobile_export_probe = executable(
  'trainlog-mtp-mobile-export-probe',
  'tools/mtp_mobile_export_probe.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

