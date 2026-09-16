#!/usr/bin/env python3
import json
import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parents[1]
contract = json.loads((root / "contracts/dashboard-layout-v1.json").read_text())
source = (root / "tui/src/dashboard_layout.c").read_text()
frontend = (root / "web/src/dashboard/dashboardLayout.ts").read_text()

if contract.get("columns") != 12 or contract.get("max_y") != 200 or len(contract.get("tiles", [])) != 7:
    sys.exit("invalid canonical dashboard layout contract")
for tile in contract["tiles"]:
    constraint = '{{"{}", {}U, {}U, {}U, {}U}}'.format(
        tile["id"], tile["min_width"], tile["max_width"], tile["min_height"], tile["max_height"])
    default = tile["default"]
    default_fragment = '{{"{}", {}U, {}U, {}U, {}U}}'.format(
        tile["id"], default["x"], default["y"], default["width"], default["height"])
    if constraint not in source or default_fragment not in source:
        sys.exit(f"C contract drift: {tile['id']}")
if "dashboard-layout-v1.json" not in frontend:
    sys.exit("frontend does not consume canonical dashboard layout contract")
print("PASS dashboard layout contract C/TypeScript parity")
