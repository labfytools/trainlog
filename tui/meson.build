cc = meson.get_compiler('c')
fs = import('fs')

sqlite3_dep = dependency('sqlite3', required: true)
uuid_dep = dependency('uuid', required: true)
udev_dep = dependency('libudev', required: true)
mtp_dep = dependency('libmtp', required: true)

notcurses_dep = dependency('notcurses-core', required: true)
microhttpd_dep = dependency('libmicrohttpd', required: true)
yyjson_dep = dependency('yyjson', required: true)
crypto_dep = dependency('libcrypto', required: true)

utf8proc_dep = dependency('libutf8proc', required: false)
if not utf8proc_dep.found()
  utf8proc_dep = dependency('utf8proc', required: true)
endif

m_dep = cc.find_library('m', required: true)

trainlog_include = include_directories('include')
trainlog_private_include = include_directories('src')

web_option = get_option('web')
node = find_program('node', required: web_option.enabled())
npm = find_program('npm', required: web_option.enabled())
web_dependencies_ready = fs.is_dir(meson.project_source_root() / 'web/node_modules')
if web_option.enabled() and not web_dependencies_ready
  error('Web enabled but web/node_modules is absent; run npm ci in web/')
endif
web_assets_enabled = not web_option.disabled() and node.found() and npm.found() and web_dependencies_ready
if web_option.auto() and not web_assets_enabled
  warning('Web frontend disabled in auto mode; install Node/npm and run npm ci in web/')
endif

if web_assets_enabled
  frontend_sources = files(
    '../web/package.json',
    '../web/package-lock.json',
    '../web/tsconfig.json',
    '../web/tsconfig.app.json',
    '../web/tsconfig.node.json',
    '../contracts/dashboard-layout-v1.json',
    '../web/vite.config.ts',
    '../web/index.html',
    '../web/src/main.tsx',
    '../web/src/vite-env.d.ts',
    '../web/src/api/health.ts',
    '../web/src/api/dashboard.ts',
    '../web/src/api/analysis.ts',
    '../web/src/api/heartRate.ts',
    '../web/src/api/heartRate.test.ts',
    '../web/src/api/sleepDiary.ts',
    '../web/src/api/analysis.test.ts',
    '../web/src/analysis/messages.ts',
    '../web/src/api/dashboardLayout.ts',
    '../web/src/api/preparedItems.ts',
    '../web/src/api/sync.ts',
    '../web/src/api/sync.test.ts',
    '../web/src/api/webPreferences.ts',
    '../web/src/app/App.tsx',
    '../web/src/app/routes.ts',
    '../web/src/app/useRoute.ts',
    '../web/src/components/EmptyState.tsx',
    '../web/src/components/DisplayPreferences.tsx',
    '../web/src/components/DeleteConfirmationDialog.tsx',
    '../web/src/components/Footer.tsx',
    '../web/src/components/Header.tsx',
    '../web/src/components/HeartRateTimelinePanel.tsx',
    '../web/src/components/HeartRateTimelinePanel.test.tsx',
    '../web/src/components/heartRateVariation.ts',
    '../web/src/components/heartRateVariation.test.ts',
    '../web/src/components/SyncControl.tsx',
    '../web/src/components/SyncControl.test.tsx',
    '../web/src/components/Tile.tsx',
    '../web/src/charts/ProgressionChart.tsx',
    '../web/src/charts/trainlogChartTheme.ts',
    '../web/src/dashboard/BodyZoneFigure.tsx',
    '../web/src/dashboard/DashboardGrid.tsx',
    '../web/src/dashboard/DashboardTiles.tsx',
    '../web/src/dashboard/dashboardFormat.ts',
    '../web/src/dashboard/dashboardLayout.ts',
    '../web/src/presentation/DatePreferences.tsx',
    '../web/src/presentation/LanguagePreferences.tsx',
    '../web/src/presentation/dateFormat.ts',
    '../web/src/routes/DashboardPage.tsx',
    '../web/src/routes/AnalysisPage.tsx',
    '../web/src/routes/SleepDiaryWorkspace.tsx',
    '../web/src/routes/sleepTimelineGeometry.ts',
    '../web/src/routes/sleepTimelineGeometry.test.ts',
    '../web/src/routes/sleepVisualProjection.ts',
    '../web/src/routes/sleepVisualProjection.test.ts',
    '../web/src/report/sleepDiaryPdf.ts',
    '../web/src/report/sleepDiaryPdf.test.ts',
    '../web/src/routes/AnalysisPage.test.tsx',
    '../web/src/routes/PlaceholderPage.tsx',
    '../web/src/routes/SessionsPage.tsx',
    '../web/src/api/sessions.ts',
    '../web/src/api/exercises.ts',
    '../web/src/api/equipment.ts',
    '../web/src/api/equipment.test.ts',
    '../web/src/api/exercises.test.ts',
    '../web/src/api/programs.ts',
    '../web/src/routes/ProgramsTab.tsx',
    '../web/src/routes/ProgramsCalendarPage.tsx',
    '../web/src/routes/ExercisesPage.tsx',
    '../web/src/routes/EquipmentPage.tsx',
    '../web/src/routes/SettingsPage.tsx',
    '../web/src/routes/ExercisesPage.test.tsx',
    '../web/src/theme/app.css',
    '../web/src/theme/tokens.css',
  )
  web_dist = meson.current_build_dir() / 'web-dist'
  web_frontend_build = custom_target(
    'web_frontend_build',
    input: meson.project_source_root() / 'web/package-lock.json',
    output: 'web-frontend.stamp',
    depend_files: frontend_sources,
    command: [
      find_program('python3'),
      meson.project_source_root() / 'tools/build_web_frontend.py',
      npm,
      meson.project_source_root() / 'web',
      web_dist,
      '@OUTPUT@',
    ],
  )
  web_assets_source = custom_target(
    'web_assets_generated',
    input: web_frontend_build,
    output: 'web_assets_generated.c',
    depend_files: meson.project_source_root() / 'tools/generate_web_assets.py',
    command: [
      find_program('python3'),
      meson.project_source_root() / 'tools/generate_web_assets.py',
      web_dist,
      '@OUTPUT@',
    ],
  )
  test(
    'web_frontend_typecheck',
    npm,
    args: ['run', 'typecheck'],
    workdir: meson.project_source_root() / 'web',
  )
  test(
    'web_frontend',
    npm,
    args: ['test'],
    workdir: meson.project_source_root() / 'web',
  )
else
  web_assets_source = files('src/web_assets_disabled.c')
endif

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

exercise_name_catalog_generated = custom_target(
  'exercise_name_catalog_generated',
  input: meson.project_source_root() / 'catalog/exercise-names-v1.json',
  output: 'exercise_name_catalog_generated.c',
  command: [find_program('python3'), meson.project_source_root() / 'tools/generate_exercise_name_catalog.py', '@INPUT@', '@OUTPUT@'],
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
    meson.project_source_root() / 'catalog/equipment-exercise-relations-v2.json',
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
  '-DTRAINLOG_TOOLS_DIR="@0@"'.format(meson.project_source_root() / 'tools'),
  '-Wconversion',
  '-Wformat=2',
  '-Wshadow',
  '-DTRAINLOG_VERSION="@0@"'.format(meson.project_version()),
]

trainlog_core_sources = files(
  'src/bodyviz.c',
  'src/body_analytics.c',
  'src/catalog.c',
  'src/database.c',
  'src/dashboard.c',
  'src/json_writer.c',
  'src/web_dashboard.c',
  'src/web_dashboard_json.c',
  'src/web_analysis.c',
  'src/web_heart_rate.c',
  'src/web_prepared_items.c',
  'src/web_prepared_items_json.c',
  'src/web_sessions.c',
  'src/web_session_deletions.c',
  'src/web_programs.c',
  'src/web_sleep.c',
  'src/web_exercises.c',
  'src/web_equipment.c',
  'src/duration.c',
  'src/id.c',
  'src/timeutil.c',
  'src/training_context.c',
  'src/usb.c',
  'src/mtp.c',
  'src/generation_mtp.c',
  'src/reps.c',
  'src/session_generation.c',
  'src/sleep_diary.c',
  'src/statistics.c',
  'src/measured_max.c',
  'src/sync.c',
  'src/sync_history.c',
  'src/timestamp.c',
)
trainlog_core_sources += equipment_catalog_generated
trainlog_core_sources += body_zone_catalog_generated
trainlog_core_sources += exercise_name_catalog_generated
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
    yyjson_dep,
    crypto_dep,
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
    yyjson_dep,
    crypto_dep,
  ],
)

executable(
  'sync-generation-fixture',
  'tests/sync_generation_fixture.c',
  dependencies: trainlog_core_dep,
)

trainlog_tui_sources = files(
  'src/cli.c',
  'src/main.c',
  'src/paths.c',
  'src/app_shell.c',
  'src/presentation.c',
  'src/chart.c',
  'src/sync_screen_action.c',
  'src/terminal.c',
  'src/theme.c',
  'src/tui.c',
  'src/dashboard_layout.c',
  'src/web_preferences.c',
  'src/web_server.c',
)
trainlog_tui_sources += web_assets_source

trainlog_exe = executable(
  'trainlog',
  trainlog_tui_sources,
  dependencies: [
    trainlog_core_dep,
    notcurses_dep,
    microhttpd_dep,
    yyjson_dep,
  ],
  include_directories: trainlog_private_include,
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

test_dashboard = executable(
  'test_dashboard',
  'tests/test_dashboard.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)
test('dashboard', test_dashboard)

test_web_dashboard = executable(
  'test_web_dashboard',
  'tests/test_web_dashboard.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)
test('web_dashboard', test_web_dashboard)

test_web_analysis = executable(
  'test_web_analysis',
  'tests/test_web_analysis.c',
  dependencies: trainlog_core_dep,
  include_directories: trainlog_private_include,
  c_args: strict_c_args,
)
test('web_analysis', test_web_analysis)

test_web_heart_rate = executable(
  'test_web_heart_rate',
  'tests/test_web_heart_rate.c',
  dependencies: trainlog_core_dep,
  include_directories: trainlog_private_include,
  c_args: strict_c_args,
)
test('web_heart_rate', test_web_heart_rate)

test_web_prepared_items = executable(
  'test_web_prepared_items',
  'tests/test_web_prepared_items.c',
  dependencies: trainlog_core_dep,
  include_directories: trainlog_private_include,
  c_args: strict_c_args,
)
test('web_prepared_items', test_web_prepared_items)

test_web_sessions = executable(
  'test_web_sessions',
  'tests/test_web_sessions.c',
  dependencies: trainlog_core_dep,
  include_directories: trainlog_private_include,
  c_args: strict_c_args,
)
test('web_sessions', test_web_sessions)

test_web_programs = executable(
  'test_web_programs',
  'tests/test_web_programs.c',
  dependencies: trainlog_core_dep,
  include_directories: trainlog_private_include,
  c_args: strict_c_args,
)
test('web_programs', test_web_programs)

test_web_exercises = executable(
  'test_web_exercises',
  'tests/test_web_exercises.c',
  dependencies: trainlog_core_dep,
  include_directories: trainlog_private_include,
  c_args: strict_c_args,
)
test('web_exercises', test_web_exercises)

test_web_session_deletions = executable(
  'test_web_session_deletions',
  'tests/test_web_session_deletions.c',
  dependencies: trainlog_core_dep,
  include_directories: trainlog_private_include,
  c_args: strict_c_args,
)
test('web_session_deletions', test_web_session_deletions)

test_dashboard_header = executable(
  'test_dashboard_header',
  'tests/test_dashboard_header.c',
  include_directories: trainlog_include,
  c_args: strict_c_args,
)
test('dashboard_header', test_dashboard_header)

test_cli = executable(
  'test_cli',
  ['tests/test_cli.c', 'src/cli.c'],
  include_directories: trainlog_include,
  c_args: strict_c_args,
)
test('cli', test_cli)

test_web_server = executable(
  'test_web_server',
  ['tests/test_web_server.c', 'src/web_server.c', 'src/dashboard_layout.c',
   'src/web_preferences.c', web_assets_source],
  dependencies: [trainlog_core_dep, microhttpd_dep, yyjson_dep],
  include_directories: trainlog_private_include,
  c_args: strict_c_args,
)
test('web_server', test_web_server, timeout: 30)

test_dashboard_layout = executable(
  'test_dashboard_layout',
  ['tests/test_dashboard_layout.c', 'src/dashboard_layout.c'],
  dependencies: yyjson_dep,
  include_directories: trainlog_include,
  c_args: strict_c_args,
)
test('dashboard_layout', test_dashboard_layout)
test('dashboard_layout_contract', find_program('python3'),
  args: [meson.project_source_root() / 'tools/validate_dashboard_layout_contract.py'])
test('cli_help', trainlog_exe, args: ['--help'])
test('cli_version', trainlog_exe, args: ['--version'])

test_web_headers = executable(
  'test_web_headers',
  'tests/test_web_headers.c',
  include_directories: trainlog_include,
  c_args: strict_c_args,
)
test('web_headers', test_web_headers)
test(
  'web_asset_generator',
  find_program('python3'),
  args: [
    meson.project_source_root() / 'tools/test_generate_web_assets.py',
    meson.project_source_root() / 'tools/generate_web_assets.py',
  ],
)

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

trainlog_generation_mtp_adapter = executable(
  'trainlog-generation-mtp-adapter',
  'tools/generation_mtp_adapter.c',
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

test_generation_mtp = executable(
  'test_generation_mtp',
  'tests/test_generation_mtp.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)
test('generation_mtp', test_generation_mtp)

test(
  'syncd_routing',
  find_program('python3'),
  args: [meson.project_source_root() / 'tests/test_syncd_routing.py'],
)

generation_mtp_adapter_double = executable(
  'generation-mtp-adapter-double',
  'tests/generation_mtp_adapter_double.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
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

test_schema_v26_migration = executable(
  'test_schema_v26_migration',
  'tests/test_schema_v26_migration.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test('schema_v26_migration', test_schema_v26_migration)

test_schema_v28_migration = executable(
  'test_schema_v28_migration',
  'tests/test_schema_v28_migration.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test('schema_v28_migration', test_schema_v28_migration)

test_sleep_diary = executable(
  'test_sleep_diary',
  'tests/test_sleep_diary.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)
test('sleep_diary', test_sleep_diary)

test_schema_v29_migration = executable(
  'test_schema_v29_migration',
  'tests/test_schema_v29_migration.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)
test('schema_v29_migration', test_schema_v29_migration)

test_schema_v30_migration = executable(
  'test_schema_v30_migration',
  'tests/test_schema_v30_migration.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)
test('schema_v30_migration', test_schema_v30_migration)

test_schema_v31_migration = executable(
  'test_schema_v31_migration',
  'tests/test_schema_v31_migration.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)
test('schema_v31_migration', test_schema_v31_migration)

test_schema_v32_migration = executable(
  'test_schema_v32_migration',
  'tests/test_schema_v32_migration.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)
test('schema_v32_migration', test_schema_v32_migration)

test_schema_v33_migration = executable(
  'test_schema_v33_migration',
  'tests/test_schema_v33_migration.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)
test('schema_v33_migration', test_schema_v33_migration)

test_schema_v34_migration = executable(
  'test_schema_v34_migration',
  'tests/test_schema_v34_migration.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)
test('schema_v34_migration', test_schema_v34_migration)

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
  'program_execution_exchange',
  python3_trainlog_tests,
  args: [meson.project_source_root() / 'tests/test_program_execution_exchange.py'],
)

test(
  'sync_gap_characterization',
  python3_trainlog_tests,
  args: [meson.project_source_root() / 'tests/test_sync_gap_characterization.py'],
)

test(
  'sync_isolated_harness',
  python3_trainlog_tests,
  args: [meson.project_source_root() / 'tests/test_validate_sync_isolated.py'],
)
test(
  'causal_delete_exchange',
  find_program('python3'),
  args: [meson.project_source_root() / 'tests/test_causal_delete_exchange.py'],
)
test(
  'sync_generation_exchange',
  find_program('python3'),
  args: [meson.project_source_root() / 'tests/test_sync_generation_exchange.py'],
  timeout: 120,
)
test(
  'sleep_diary_exchange',
  find_program('python3'),
  args: [meson.project_source_root() / 'tests/test_sleep_diary_exchange.py'],
)
test(
  'heart_rate_exchange',
  find_program('python3'),
  args: [meson.project_source_root() / 'tests/test_heart_rate_exchange.py'],
)
test(
  'session_timeline_exchange',
  find_program('python3'),
  args: [meson.project_source_root() / 'tests/test_session_timeline_exchange.py'],
)
test(
  'cardio_session_exchange',
  find_program('python3'),
  args: [meson.project_source_root() / 'tests/test_cardio_session_exchange.py'],
)
test(
  'cardio_calibration_exchange',
  find_program('python3'),
  args: [meson.project_source_root() / 'tests/test_cardio_calibration_exchange.py'],
)
test(
  'cardio_guidance_exchange',
  find_program('python3'),
  args: [meson.project_source_root() / 'tests/test_cardio_guidance_exchange.py'],
)
test(
  'session_preparation_export',
  find_program('python3'),
  args: [meson.project_source_root() / 'tests/test_session_preparation_export.py'],
)
test(
  'generation_archival',
  find_program('python3'),
  args: [meson.project_source_root() / 'tests/test_generation_archival.py'],
  timeout: 120,
)
test(
  'sync_orchestrator',
  find_program('python3'),
  args: [meson.project_source_root() / 'tests/test_sync_orchestrator.py'],
)
test(
  'sync_peer_worker',
  find_program('python3'),
  args: [meson.project_source_root() / 'tests/test_sync_peer_worker.py'],
)
test(
  'sync_drive_transport',
  find_program('python3'),
  args: [meson.project_source_root() / 'tests/test_sync_drive_transport.py'],
)
test(
  'sync_bluetooth_transport',
  find_program('python3'),
  args: [meson.project_source_root() / 'tests/test_sync_bluetooth_transport.py'],
)
test(
  'sync_deployment_tools',
  find_program('python3'),
  args: [meson.project_source_root() / 'tests/test_sync_deployment_tools.py'],
)
test(
  'sync_error_classification',
  find_program('python3'),
  args: [meson.project_source_root() / 'tests/test_sync_error_classification.py'],
)
test(
  'schema_v21_migration',
  executable('test_schema_v21_migration', 'tests/test_schema_v21_migration.c', dependencies: trainlog_core_dep),
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

test(
  'training_feedback_sync',
  python3_trainlog_tests,
  args: [meson.project_source_root() / 'tests/test_training_feedback_sync.py'],
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
  'src/presentation.c',
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
  'src/chart.c',
  'src/presentation.c',
  'src/theme.c',
  'src/sync_screen_action.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test('tui_workflows', test_tui_workflows)

test_chart = executable(
  'test_chart',
  'tests/test_chart.c',
  'src/chart.c',
  'src/presentation.c',
  include_directories: trainlog_include,
  dependencies: m_dep,
  c_args: strict_c_args,
)

test('chart', test_chart)

test_statistics = executable(
  'test_statistics',
  'tests/test_statistics.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test('statistics', test_statistics)

test_feedback_labels = executable(
  'test_feedback_labels',
  'tests/test_feedback_labels.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)
test('feedback_labels', test_feedback_labels)

test_feedback_correction = executable(
  'test_feedback_correction',
  'tests/test_feedback_correction.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)
test('feedback_correction', test_feedback_correction)

test(
  'ai_history_export',
  find_program('python3'),
  args: [meson.project_source_root() / 'tests/test_ai_history_export.py'],
)

test(
  'post_sync_ai_drive',
  find_program('python3'),
  args: [meson.project_source_root() / 'tests/test_post_sync_ai_drive.py'],
)

test(
  'ai_session_draft',
  find_program('python3'),
  args: [meson.project_source_root() / 'tests/test_ai_session_draft.py'],
)

test_schema_v16_migration = executable(
  'test_schema_v16_migration',
  'tests/test_schema_v16_migration.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)
test('schema_v16_migration', test_schema_v16_migration)

python3 = find_program('python3')
test(
  'profile_state_sync',
  python3,
  args: [meson.project_source_root() / 'tests/test_profile_state_sync.py'],
  workdir: meson.project_source_root(),
)

test_app_shell = executable(
  'test_app_shell',
  'tests/test_app_shell.c',
  'src/app_shell.c',
  'src/presentation.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test('app_shell', test_app_shell)

test_presentation = executable(
  'test_presentation',
  'tests/test_presentation.c',
  'src/presentation.c',
  'src/app_shell.c',
  'src/sync_screen_action.c',
  dependencies: trainlog_core_dep,
  c_args: strict_c_args,
)

test('presentation_i18n', test_presentation)

test(
  'presentation_catalog_completeness',
  python3,
  args: [meson.project_source_root() / 'tui/tests/test_presentation_catalog.py'],
  workdir: meson.project_source_root(),
)
