#!/usr/bin/env bash
set -euo pipefail

ROOT="$(
  CDPATH= cd -- "$(dirname -- "$0")/.." &&
  pwd
)"

SYNC_ONCE="$ROOT/build/tui/trainlog-sync-once"
DAEMON="$ROOT/tools/trainlog_syncd.py"

if [[ ! -x "$SYNC_ONCE" ]]; then
  echo "missing executable: $SYNC_ONCE" >&2
  echo "run meson compile -C build first" >&2
  exit 1
fi

mkdir -p \
  "$HOME/.local/bin" \
  "$HOME/.config/systemd/user"

ln -sfn \
  "$SYNC_ONCE" \
  "$HOME/.local/bin/trainlog-sync-once"

ln -sfn \
  "$DAEMON" \
  "$HOME/.local/bin/trainlog-syncd"

UNIT="$HOME/.config/systemd/user/trainlog-syncd.service"

cat > "$UNIT" <<EOF
[Unit]
Description=Trainlog Android MTP synchronization agent

[Service]
Type=simple
ExecStart=/usr/bin/python3 "$DAEMON" --sync-once "$SYNC_ONCE" --interval 3
Restart=on-failure
RestartSec=2

[Install]
WantedBy=default.target
EOF

systemctl --user daemon-reload
systemctl --user enable --now trainlog-syncd.service

echo "TRAINLOG_SYNCD_INSTALL=PASS"
systemctl --user --no-pager --full status trainlog-syncd.service || true
