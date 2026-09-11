cc = meson.get_compiler('c')

sqlite3_dep = dependency('sqlite3', required: true)
uuid_dep = dependency('uuid', required: true)
udev_dep = dependency('libudev', required: true)
mtp_dep = dependency('libmtp', required: true)

notcurses_dep = dependency('notcurses-core', required: true)

utf8proc_dep = dependency('libutf8proc', required: false)
if not utf8proc_dep.found()
  utf8proc_dep = dependency('utf8proc', required: true)
endif

m_dep = cc.find_library('m', required: true)

trainlog_include = include_directories('include')

equipment_catalog_generated = custom_target(
  'equipment_catalog_generated',
  input: meson.project_source_root() / 'catalog/equipment-v1.json',
  output: 'equipment_catalog_generated.c',
  command: [find_program('python3'), meson.project_source_root() / 'tools/generate_equipment_catalog.py', '@INPUT@', '@OUTPUT@'],
)

body_zone_catalog_generated = custom_target(
  'body_zone_catalog_generated',
  input: meson.project_source_root() / 'catalog/body-zones-v1.json',
  output: 'body_zone_catalog_generated.c',
  command: [find_program('python3'), meson.project_source_root() / 'tools/generate_body_zone_catalog.py', '@INPUT@', '@OUTPUT@'],
)

training_knowledge_generated = custom_target(
  'training_knowledge_generated',
  input: [
    meson.project_source_root() / 'catalog/body-zones-v1.json',
    meson.project_source_root() / 'catalog/equipment-v1.json',
    meson.project_source_root() / 'catalog/science-references-v1.json',
    meson.project_source_root() / 'catalog/muscles-v1.json',
    meson.project_source_root() / 'catalog/joint-actions-v1.json',
    meson.project_source_root() / 'catalog/movement-patterns-v1.json',
    meson.project_source_root() / 'catalog/exercise-knowledge-v1.json',
    meson.project_source_root() / 'catalog/equipment-knowledge-v1.json',
    meson.project_source_root() / 'catalog/training-knowledge-audit-v1.json',
  ],
  output: 'training_knowledge_generated.c',
  depend_files: [
    meson.project_source_root() / 'tools/generate_training_knowledge.py',
    meson.project_source_root() / 'tools/validate_training_knowledge.py',
  ],
  command: [find_program('python3'), meson.project_source_root() / 'tools/generate_training_knowledge.py',
            meson.project_source_root() / 'catalog', '@OUTPUT@'],
)

session_generation_policy_generated = custom_target(
  'session_generation_policy_generated',
  input: meson.project_source_root() / 'catalog/session-generation-policy-v1.json',
  output: 'session_generation_policy_generated.c',
  depend_files: [
    meson.project_source_root() / 'tools/generate_session_generation_policy.py',
    meson.project_source_root() / 'tools/validate_session_generation_policy.py',
    meson.project_source_root() / 'catalog/body-zones-v1.json',
    meson.project_source_root() / 'catalog/movement-patterns-v1.json',
    meson.project_source_root() / 'catalog/exercise-knowledge-v1.json',
    meson.project_source_root() / 'catalog/equipment-knowledge-v1.json',
    meson.project_source_root() / 'catalog/science-references-v1.json',
  ],
  command: [find_program('python3'), meson.project_source_root() / 'tools/generate_session_generation_policy.py',
            '@INPUT@', meson.project_source_root() / 'catalog', '@OUTPUT@'],
)

strict_c_args = [
  '-D_POSIX_C_SOURCE=200809L',
  '-Wconversion',
  '-Wformat=2',
  '-Wshadow',
]

trainlog_core_sources = files(
  'src/bodyviz.c',
  'src/body_analytics.c',
  'src/catalog.c',
  'src/database.c',
  'src/duration.c',
  'src/id.c',
  'src/timeutil.c',
  'src/training_context.c',
  'src/usb.c',
  'src/mtp.c',
  'src/reps.c',
  'src/session_generation.c',
  'src/measured_max.c',
  'src/sync.c',
  'src/sync_history.c',
  'src/timestamp.c',
)
trainlog_core_sources += equipment_catalog_generated
trainlog_core_sources += body_zone_catalog_generated
trainlog_core_sources += training_knowledge_generated
trainlog_core_sources += session_generation_policy_generated

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
  'src/app_shell.c',
  'src/sync_screen_action.c',
  'src/terminal.c',
  'src/theme.c',
  'src/tui.c',
)

trainlog_exe = executable(
  'trainlog',
  trainlog_tui_sources,
  dependencies: [
    trainlog_core_dep,
    notcurses_dep,
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

test_equipment_catalog = executable(
  'test_equipment_catalog',
  'tests/test_equipment_catalog.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)
test('equipment_catalog', test_equipment_catalog)

test_body_zones = executable(
  'test_body_zones',
  'tests/test_body_zones.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)
test('body_zones', test_body_zones)

test_training_knowledge = executable(
  'test_training_knowledge',
  'tests/test_training_knowledge.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)
test('training_knowledge', test_training_knowledge)

test_session_generation = executable(
  'test_session_generation',
  ['tests/test_session_generation.c', custom_target(
    'session_generation_fixtures_generated',
    input: meson.project_source_root() / 'tests/fixtures/session-generation-v1.json',
    output: 'session_generation_fixtures_generated.c',
    command: [find_program('python3'), meson.project_source_root() / 'tools/generate_session_generation_fixtures.py', '@INPUT@', '@OUTPUT@'],
  )],
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)
test('session_generation', test_session_generation)

test(
  'session_generation_policy_validation',
  find_program('python3'),
  args: [meson.project_source_root() / 'tools/validate_session_generation_policy.py',
         meson.project_source_root() / 'catalog/session-generation-policy-v1.json',
         meson.project_source_root() / 'catalog'],
)

test_training_context = executable(
  'test_training_context',
  'tests/test_training_context.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)
test('training_context', test_training_context)

test_custom_equipment = executable(
  'test_custom_equipment',
  'tests/test_custom_equipment.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)
test('custom_equipment', test_custom_equipment)

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

test_max_results = executable(
  'test_max_results',
  'tests/test_max_results.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test('max_results', test_max_results)

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

test_reps = executable(
  'test_reps',
  'tests/test_reps.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test(
  'reps',
  test_reps,
)

test_variable_sets = executable(
  'test_variable_sets',
  'tests/test_variable_sets.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test(
  'variable_sets',
  test_variable_sets,
)

test_schema_v5_migration = executable(
  'test_schema_v5_migration',
  'tests/test_schema_v5_migration.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test(
  'schema_v5_migration',
  test_schema_v5_migration,
)

test_schema_v9_migration = executable(
  'test_schema_v9_migration',
  'tests/test_schema_v9_migration.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test('schema_v9_migration', test_schema_v9_migration)

test_schema_v7_migration = executable(
  'test_schema_v7_migration',
  'tests/test_schema_v7_migration.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test(
  'schema_v7_migration',
  test_schema_v7_migration,
)

python3_trainlog_tests = find_program('python3')

test(
  'timestamp_validation',
  python3_trainlog_tests,
  args: [meson.project_source_root() / 'tests/test_timestamp_validation.py'],
)

test(
  'mobile_import_variable_sets',
  python3_trainlog_tests,
  args: [
    meson.project_source_root() / 'tests/test_mobile_import_variable_sets.py',
  ],
)

test(
  'mobile_import_multi_occurrence',
  python3_trainlog_tests,
  args: [meson.project_source_root() / 'tests/test_mobile_import_multi_occurrence.py'],
)

test(
  'max_sync',
  python3_trainlog_tests,
  args: [meson.project_source_root() / 'tests/test_max_sync.py'],
)

test(
  'session_exchange_v3',
  python3_trainlog_tests,
  args: [meson.project_source_root() / 'tests/test_session_exchange_v3.py'],
)

test(
  'equipment_associations_exchange',
  python3_trainlog_tests,
  args: [meson.project_source_root() / 'tests/test_equipment_associations_exchange.py'],
)

test(
  'equipment_definitions_exchange',
  python3_trainlog_tests,
  args: [meson.project_source_root() / 'tests/test_equipment_definitions_exchange.py'],
)

test(
  'exercise_reconciliation',
  python3_trainlog_tests,
  args: [meson.project_source_root() / 'tests/test_exercise_reconciliation.py'],
)

test(
  'body_zone_sync',
  python3_trainlog_tests,
  args: [meson.project_source_root() / 'tests/test_body_zone_sync.py'],
)

test(
  'body_zone_catalog_validation',
  python3_trainlog_tests,
  args: [meson.project_source_root() / 'tests/test_body_zone_catalog.py'],
)

test(
  'exercise_alias_import_validation',
  python3_trainlog_tests,
  args: [meson.project_source_root() / 'tests/test_exercise_alias_import.py'],
)

test_sync_direction = executable(
  'test_sync_direction',
  'tests/test_sync_direction.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test('sync_direction', test_sync_direction)

test_sync_body_zone_wiring = executable(
  'test_sync_body_zone_wiring',
  'tests/test_sync_body_zone_wiring.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test('sync_body_zone_wiring', test_sync_body_zone_wiring)

test_sync_history = executable(
  'test_sync_history',
  'tests/test_sync_history.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test('sync_history', test_sync_history)

test_sync_screen_action = executable(
  'test_sync_screen_action',
  'tests/test_sync_screen_action.c',
  'src/sync_screen_action.c',
  dependencies: [trainlog_core_dep, notcurses_dep],
  c_args: strict_c_args,
)

test('sync_screen_action', test_sync_screen_action)

trainlog_sync_once = executable(
  'trainlog-sync-once',
  'tools/sync_once.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test_measured_max = executable(
  'test_measured_max',
  'tests/test_measured_max.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test(
  'measured_max',
  test_measured_max,
)

test_body_analytics = executable(
  'test_body_analytics',
  'tests/test_body_analytics.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test(
  'body_analytics',
  test_body_analytics,
)

test_terminal_input = executable(
  'test_terminal_input',
  'tests/test_terminal_input.c',
  'src/terminal.c',
  include_directories: trainlog_include,
  dependencies: [notcurses_dep, utf8proc_dep],
  c_args: strict_c_args,
)

test(
  'terminal_input_event_type_policy',
  test_terminal_input,
)

test_tui_workflows = executable(
  'test_tui_workflows',
  'tests/test_tui_workflows.c',
  'src/app_shell.c',
  'src/theme.c',
  'src/sync_screen_action.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test('tui_workflows', test_tui_workflows)

test_app_shell = executable(
  'test_app_shell',
  'tests/test_app_shell.c',
  'src/app_shell.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test('app_shell', test_app_shell)
