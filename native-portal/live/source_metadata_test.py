#!/usr/bin/env python3
"""Original compositor read/release calls and client inner ref stores, CPU only."""
import ctypes as C,unittest
import source_provider_test as S

COMP={0x15fee4,0x15fee8,0x160164,0x160168,0x160184,0x160188,0x160380,0x160384,0x1605c4,0x1605c8}
CLIENT={0x32750c,0x327e54,0x3280a8,0x32813c,0x328140,0x327640,0x3276c4,0x3276c8,
        0x327e18,0x327f2c,0x327f30,0x327f70,0x327f74,0x3274d8,0x32758c,0x327590,0x3275a0,0x3275a4}
class Native(S.Native):
    def __init__(self,dll):
        super().__init__(dll);self.guards={};self.metadata=0xb00000000;self.meta_error=0;self.release_error=0
        self.put(self.cb+0x60f138,self.cb+0x3280a8);self.put(self.cb+0x60f140,self.cb+0x327640)
        self.put(0x90069000,0x9006a000);self.put(0x9006a000,0x90067000);self.put(0x9006a010,0x90068000)
    def step(self,pc):
        if pc in COMP:self.observed(0,pc)
        if pc-self.cb in CLIENT:self.observed(1,pc-self.cb)
        if pc==self.cb+0x258500:self.guards[self.x(0)]=self.x(1)
        elif pc==self.cb+0x258578:self.x(0,self.header[self.guards[self.x(0)]])
        elif pc==self.cb+0x258508:pass
        elif pc==self.cb+0x368488:self.x(0,0x90069000)
        elif pc in [self.cb+0x327728,self.cb+0x327780]:self.x(0,0x9006b000) # explicit mapping/data boundary
        elif pc==0x90067000:
            self.metadata+=1;self.put(self.x(1),self.metadata);self.x(0,self.meta_error)
        elif pc==0x90068000:self.x(0,self.release_error)
        elif pc==self.cb+0x32a950:
            queue=self.x(0);value=self.x(1);node=self.alloc(0x40)
            self.put(node+0x20,self.get(value));self.put(node+0x28,self.get(value+8,4),4)
            self.put(queue+0x10,node);self.put(queue+0x28,1);self.x(0,node)
        elif pc==self.cb+0x8fd30:
            self.put(self.x(1)+8,0);self.x(0,0x9006c000) # original tree erasure boundary
        else:return super().step(pc)
        self.m.reg_write(S.A.UC_ARM64_REG_PC,self.x(30))
    def segment(self,start,end):
        self.m.emu_start(start,end,count=20000)
        if self.error:raise self.error
        assert self.m.reg_read(S.A.UC_ARM64_REG_PC)==end
    def read_cycle(self):
        for h in self.header.values():
            self.put(h+8,16,4);self.put(h+12,128,4);self.put(h+0x14,0,4)
            self.put(h+0x68,1,4);self.put(h+0x6c,1,4)
        assert self.dll.source_begin(self.fixture,0), 'native Update observation scope missing'
        sp=self.stack-0x360;self.m.reg_write(S.A.UC_ARM64_REG_SP,sp);self.x(19,self.camera)
        self.x(0,self.manager);self.x(5,self.get(self.manager))
        self.segment(0x15fec8,0x15fee8);self.observed(0,0x15fee8) # actual layout acquire argument assembly/BLR/result
        if not self.x(0):self.segment(0x160150,0x160168)
        self.segment(0x160168,0x160188);self.observed(0,0x160188) # actual data acquire
        if not self.x(0):
            self.segment(0x16036c,0x160384);self.observed(0,0x160384)
        return bool(self.dll.source_finish(self.fixture,1))

class MetadataTests(unittest.TestCase):
    def setUp(self):self.n=Native(DLL);assert self.n.run()
    def tearDown(self):self.n.close()
    def test_original_read_release_records_handles(self):
        n=self.n;self.assertTrue(n.read_cycle(),n.rejected)
        for q in range(2):
            self.assertEqual(n.value(q,8),1);self.assertEqual(n.value(q,9),1)
            self.assertNotEqual(n.value(q,10),0);self.assertNotEqual(n.value(q,11),0)
            self.assertEqual(n.value(q,12),1);self.assertEqual(n.value(q,13),1)
    def test_missing_ref_store_does_not_certify_return(self):
        self.n.omit={(1,0x327f74)};self.assertFalse(self.n.read_cycle())
    def test_missing_metadata_return_does_not_certify_return(self):
        self.n.omit={(1,0x3275a4)};self.assertFalse(self.n.read_cycle())
    def test_metadata_acquire_error_is_preserved(self):
        n=self.n;n.meta_error=9;self.assertTrue(n.read_cycle())
        for q in range(2):self.assertEqual(n.value(q,14),7);self.assertEqual(n.value(q,9),0)
    def test_native_ignored_metadata_result_is_not_rewritten(self):
        n=self.n;n.release_error=7;self.assertTrue(n.read_cycle())
        for q in range(2):self.assertEqual(n.value(q,15),7)
    def test_missing_inner_entry_refuses(self):
        self.n.omit={(1,0x327e18)};self.assertFalse(self.n.read_cycle())
    def test_missing_manager_entry_refuses(self):
        self.n.omit={(1,0x3280a8)};self.assertFalse(self.n.read_cycle())
    def test_header_id_mismatch_refuses(self):
        self.n.inject[(1,0x327e54)]=lambda s:s.x.__setitem__(0,s.x[0]+8)
        self.assertFalse(self.n.read_cycle())
    def test_wrong_metadata_return_handle_refuses(self):
        self.n.inject[(0,0x160164)]=lambda s:s.x.__setitem__(2,s.x[2]+1)
        self.assertFalse(self.n.read_cycle())
    def test_wrong_ref_store_value_refuses(self):
        self.n.inject[(1,0x32758c)]=lambda s:s.x.__setitem__(0,s.x[0]+1)
        self.assertFalse(self.n.read_cycle())
    def test_wrong_original_queue_refuses(self):
        self.n.inject[(0,0x15fee4)]=lambda s:s.x.__setitem__(1,s.x[1]+1)
        self.assertFalse(self.n.read_cycle())

if __name__=='__main__':
    S.compile_binding();DLL=C.CDLL(str(S.BUILD/'binding.so'))
    DLL.source_fixture.argtypes=[S.Read,C.c_void_p];DLL.source_fixture.restype=C.c_void_p
    DLL.source_delete.argtypes=[C.c_void_p];DLL.source_begin.argtypes=[C.c_void_p,C.c_int]
    DLL.source_event.argtypes=[C.c_void_p,C.c_int,C.c_uint64,C.POINTER(S.Saved)]
    DLL.source_finish.argtypes=[C.c_void_p,C.c_uint64]
    DLL.source_value.argtypes=[C.c_void_p,C.c_uint,C.c_uint];DLL.source_value.restype=C.c_uint64
    unittest.main()
