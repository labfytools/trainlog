#!/usr/bin/env bash
# Install the private Trainlog automatic synchronization user services.
set -euo pipefail

ROOT="$(
  CDPATH= cd -- "$(dirname -- "$0")/.." &&
  pwd
)"

SYNC_ONCE="$ROOT/build/tui/trainlog-sync-once"
MTP_ADAPTER="$ROOT/build/tui/trainlog-generation-mtp-adapter"
BT_ADAPTER="$ROOT/tools/trainlog_generation_bt_adapter.py"
BT_AGENT="$ROOT/tools/trainlog_bt_agent.py"
DAEMON="$ROOT/tools/trainlog_syncd.py"
GEN_CONFIG="${TRAINLOG_SYNC_GENERATION_CONFIG:-$HOME/.config/trainlog/sync-generation-config-v1.json}"

for executable in "$SYNC_ONCE" "$MTP_ADAPTER" "$BT_ADAPTER" "$BT_AGENT" "$DAEMON"; do
  if [[ ! -x "$executable" ]]; then
    echo "missing executable: $executable" >&2
    echo "run meson compile -C build first and keep Trainlog tools executable" >&2
    exit 1
  fi
done

mkdir -p   "$HOME/.local/bin"   "$HOME/.config/systemd/user"

# WHY: private pre-orchestrator rollouts installed this exact drop-in to call
# syncd without its now-required bounded runtime paths. Leaving it in place
# silently overrides the canonical unit below and creates an endless restart
# loop. CONTRACT: remove only the known obsolete Trainlog-owned override;
# preserve every unrecognized administrator customization. INVARIANT: the
# effective ExecStart always owns sync-once plus both transport adapters.
LEGACY_ROLLOUT="$HOME/.config/systemd/user/trainlog-syncd.service.d/rollout.conf"
if [[ -f "$LEGACY_ROLLOUT" ]] &&
   grep -Fxq 'ExecStart=%h/.local/bin/trainlog-syncd --interval 3' "$LEGACY_ROLLOUT"; then
  rm -f -- "$LEGACY_ROLLOUT"
fi

ln -sfn "$SYNC_ONCE" "$HOME/.local/bin/trainlog-sync-once"
ln -sfn "$DAEMON" "$HOME/.local/bin/trainlog-syncd"
ln -sfn "$BT_AGENT" "$HOME/.local/bin/trainlog-btd"

cat > "$HOME/.config/systemd/user/trainlog-syncd.service" <<EOF
[Unit]
Description=Trainlog automatic synchronization agent

[Service]
Type=simple
Environment=TRAINLOG_SYNC_BT_ADAPTER=$BT_ADAPTER
Environment=TRAINLOG_SYNC_MTP_ADAPTER=$MTP_ADAPTER
ExecStart=/usr/bin/python3 "$DAEMON" --sync-once "$SYNC_ONCE" --bt-adapter "$BT_ADAPTER" --mtp-adapter "$MTP_ADAPTER" --interval 3
Restart=on-failure
RestartSec=2

[Install]
WantedBy=default.target
EOF

cat > "$HOME/.config/systemd/user/trainlog-btd.service" <<EOF
[Unit]
Description=Trainlog Bluetooth RFCOMM agent
ConditionPathExists=$GEN_CONFIG

[Service]
Type=simple
ExecCondition=/usr/bin/grep -Eq '"bluetooth_enabled"[[:space:]]*:[[:space:]]*true' "$GEN_CONFIG"
ExecStart=/usr/bin/python3 "$BT_AGENT" --config "$GEN_CONFIG"
Restart=on-failure
RestartSec=2

[Install]
WantedBy=default.target
EOF

systemctl --user daemon-reload
systemctl --user enable --now trainlog-syncd.service
systemctl --user enable trainlog-btd.service
if [[ -f "$GEN_CONFIG" ]] && grep -Eq '"bluetooth_enabled"[[:space:]]*:[[:space:]]*true' "$GEN_CONFIG"; then
  systemctl --user restart trainlog-btd.service
fi

echo "TRAINLOG_SYNCD_INSTALL=PASS"
systemctl --user --no-pager --full status trainlog-syncd.service || true
systemctl --user --no-pager --full status trainlog-btd.service || true
