#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Execute delivered graph boundary windows through actual selected islands."""
import gc
import json
from pathlib import Path
import struct
import unittest
import stock_observer_machine as S
from unicorn import UC_HOOK_CODE
import unicorn.arm64_const as A

class GraphMachine(unittest.TestCase):
    def test_noop_collapse_and_last_use_boundaries(self):
        m=S.Machine();rows=[]
        for base in (0,0x100000000,0x7000000000):
            for mode in ('optimizer-zero','optimizer-nonzero','normal-ret','mode-bypass'):
                results=[]
                for installed in (False,True):
                    vals=S.values(base);vals[0]=S.DATA+0x2000;vals[30]=base+0x790100
                    u=S.emulator(base,vals);callback=base+0x780000;events=[]
                    if mode.startswith('optimizer'):
                        start=0x29ce00;site=start;stop=base+0x790100 if mode.endswith('zero') and not mode.endswith('nonzero') else base+0x29ce34
                        u.mem_write(base+start,m.at['compositor'](start,0x38));u.mem_write(vals[0]+0x15d0,bytes([int(mode=='optimizer-nonzero')]))
                    elif mode=='normal-ret':
                        start=0x2a2488;site=0x2a2498;stop=base+0x790100
                        u.mem_write(base+start,m.at['compositor'](start,0x14));u.reg_write(A.UC_ARM64_REG_SP,S.SP-0xd0)
                        for off,value in ((0x20,vals[29]),(0x28,vals[30]),(0x30,vals[19]),(0x38,vals[20]),(0x40,vals[21]),(0x48,vals[22])):S.putq(u,S.SP-0xd0+off,value)
                    else:
                        start=0x2a1e18;site=0x2a2564;stop=base+0x790100
                        u.mem_write(base+start,m.at['compositor'](start,4));u.mem_write(base+site,m.at['compositor'](site,4));u.reg_write(A.UC_ARM64_REG_X1,1)
                    if installed:m.install(u,'compositor',site,base,callback)
                    def hook(uc,address,size,data):
                        if address==callback:events.append(m.callback(uc))
                        elif address==stop:uc.emu_stop()
                    h=u.hook_add(UC_HOOK_CODE,hook);u.emu_start(base+start,base+0x7ffffc,count=1000)
                    self.assertEqual(u.reg_read(A.UC_ARM64_REG_PC),stop)
                    if installed:
                        self.assertEqual([e[1] for e in events],[site])
                        saved=struct.unpack('<100Q',events[0][2]);self.assertEqual((saved[31],saved[30]),(S.SP,vals[30]))
                    results.append(([u.reg_read(reg) for reg in S.REGS],bytes(u.mem_read(vals[0],0x2000))))
                    u.hook_del(h);del u;gc.collect()
                self.assertEqual(results[0],results[1]);rows.append(dict(base=hex(base),mode=mode,site=hex(site),passed=True))
        (S.L.B/'graph-native-boundaries.json').write_text(json.dumps(dict(scope='native pinned instruction windows, modeled objects, actual loader-selected emitter and host dispatcher; pre-RET is not native completion',cases=rows),indent=2)+'\n')
        print('PASS native optimizer zero/nonzero and restored normal/mode-bypass boundaries:',len(rows))
if __name__=='__main__':unittest.main()
