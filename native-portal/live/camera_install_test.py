#!/usr/bin/env python3
"""Check the production manifest, emitter and link inventory, not donor helpers."""
import importlib.util
import json
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'live'))
import prepare
import build
REQUIRED=[0x8dc1c,0x8b020,0x15e1c8,0x15df70,0x162360,0x6d6e0,0x6d6f0,
          0x1366b0,0x136118,0x13a100,0x15d730,0x15d198,0x15d248,
          0x15f9e0,0x15fba8,0x15f938,0x15faf8,0x15fcc0]
manifest=json.loads((ROOT/'live/runtime-sites.json').read_text())
rows=prepare.merge(prepare.image(prepare.ELF),prepare.core_specs()+manifest['sites'])
by_pc={s['pc']:s for s in rows}
for pc in REQUIRED:
    assert pc in by_pc, ('uninstalled camera boundary',hex(pc))
    if pc!=0x8dc1c:assert by_pc[pc]['emitter']=='return-function',hex(pc)
for src in ['live/camera_owner.cpp','binding/camera.cpp','control/camera_gate.cpp','control/owner_coordinator.cpp']:
    assert src in build.inventory()+manifest['sources'],('missing production link',src)
print('production camera manifest and link inventory PASS')
