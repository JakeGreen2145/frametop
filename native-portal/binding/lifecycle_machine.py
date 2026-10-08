#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Pinned native reset/disposal plus actual shared-emitter islands and C++ ledger."""
import base64, ctypes as C, hashlib, importlib.util, json, pathlib, struct, sys, unittest
sys.path[:0]=['/home/jakeg/.hermes/work/frame-native-reuse/camera-analysis/pylib','/home/jakeg/.hermes/work/frame-native-product-design/pylib']
from unicorn import UC_HOOK_CODE
import unicorn.arm64_const as ar
ROOT=pathlib.Path(__file__).resolve().parent
WORK=pathlib.Path('/home/jakeg/.hermes/work')
sys.dont_write_bytecode=True
analyzer=WORK/'frame-native-live-render-analysis/analyze.py'
assert hashlib.sha256(analyzer.read_bytes()).hexdigest()=='4db7b7f49b1225cadbd60107e54c68bf07a87ada4d725dcec7a8eda18e386cd2'
spec=importlib.util.spec_from_file_location('native_probe',analyzer)
b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)
b.ELF=WORK/'frame-native-reuse/camera-analysis/vrcompositor';img=b.Image()
assert hashlib.sha256(b.ELF.read_bytes()).hexdigest()=='9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca4be0784bf6c265'
assert img.reloc[0x59f918]==0x300b20
READ=C.CFUNCTYPE(C.c_bool,C.c_void_p,C.c_uint64,C.c_void_p,C.c_size_t)
class Memory(C.Structure):_fields_=[('user',C.c_void_p),('read',READ)]
class Capture(C.Structure):_fields_=[('x',C.c_uint64*31),('sp',C.c_uint64),('q',C.c_ubyte*512),('nzcv',C.c_uint64),('fpcr',C.c_uint64),('fpsr',C.c_uint64),('pad',C.c_uint64)]
lib=C.CDLL(str(ROOT/'build/lifecycle-machine.so'))
for name,args,result in [('new',[C.POINTER(Memory),C.c_uint32,C.c_uint32],C.c_void_p),('delete',[C.c_void_p],None),('capture',[C.c_void_p,C.c_uint,C.POINTER(Capture),C.c_uint64,C.c_uint64],C.c_bool),('queue',[C.c_void_p],C.c_bool),('wait',[C.c_void_p],C.c_bool),('state',[C.c_void_p],C.c_uint)]:
 fn=getattr(lib,'life_'+name);fn.argtypes=args;fn.restype=result
EMITTED=json.loads((ROOT/'build/lifecycle-emitted.json').read_text())
class Native(b.Probe):
 def __init__(self,install=True):
  super().__init__(img);self.deleted=[];self.calls=[];self.callbacks=[];self.handle=None
  self.C=self.R+0x4000;self.reset_result=0;self.begin_result=0;self.vk_calls=[]
  self.wq(self.C+0x738,0xf00d)
  self.read=READ(lambda _,p,out,n:self.read_into(p,out,n));self.memory=Memory(None,self.read)
  self.handle=lib.life_new(C.byref(self.memory),57,125)
  if install:
   for case in EMITTED:
    self.uc.mem_write(case['island'],bytes.fromhex(case['code']))
    self.uc.mem_write(case['offset'],struct.pack('<I',case['branch']))
  for got,target in [(0x5aeb60,self.STOP+0x500),(0x5ae3e0,self.STOP+0x600)]:self.wq(self.rq(got),target)
  # Owned pair has actual full-length FTP inline labels; stock is interleaved.
  pas=self.R+0x18000;packet=self.R+0x20000
  self.wq(self.R+0x15a0,pas);self.wq(self.R+0x15a8,pas+0x98);self.wq(self.R+0x15b0,pas+0x98)
  self.wq(self.R+0x15b8,self.R+0x19000);self.wq(self.R+0x15c0,self.R+0x19000);self.wq(self.R+0x15c8,self.R+0x19100)
  self.wq(pas+0x50,packet);self.wq(pas+0x58,packet+3*0x2b0);self.wq(pas+0x60,packet+3*0x2b0)
  for i,ticket in enumerate([1,0,2]):
   addr=packet+i*0x2b0
   label=(b'FTP:'+base64.urlsafe_b64encode(struct.pack('>Q',ticket)).rstrip(b'=')) if ticket else b'stock'
   self.wq(addr+8,0x999 if ticket else 0x123)
   self.wq(addr+0x160,addr+0x170);self.wq(addr+0x168,len(label));self.uc.mem_write(addr+0x170,label+b'\0')
 def read_into(self,p,out,n):
  try:C.memmove(out,bytes(self.uc.mem_read(p,n)),n);return True
  except Exception:return False
 def close(self):
  if self.handle:lib.life_delete(self.handle);self.handle=None
 def hook(self,uc,pc,size,user):
  if pc==0x680000:
   ctx=self.x(0);site=self.x(1);s=Capture.from_buffer_copy(bytes(uc.mem_read(ctx,800)));lr=self.x(30)
   ok=lib.life_capture(self.handle,site,C.byref(s),1,7);self.callbacks.append((site,ok,s))
   # Capture is read-only. Deliberately destroy volatile AND preserved registers
   # here to test the emitted whole-context save/restore, not Python preservation.
   for i in range(31):self.x(i,0)
   for i in range(32):uc.reg_write(ar.UC_ARM64_REG_Q0+i,0)
   for reg in (ar.UC_ARM64_REG_NZCV,ar.UC_ARM64_REG_FPCR,ar.UC_ARM64_REG_FPSR):uc.reg_write(reg,0)
   uc.reg_write(ar.UC_ARM64_REG_PC,lr);return
  if pc in (self.STOP+0x500,self.STOP+0x600):
   reset=pc==self.STOP+0x500
   assert self.x(0)==0xf00d
   if reset:assert self.x(1)==0
   else:assert self.ru(self.x(1))==42 and self.ru(self.x(1)+16)==0
   self.vk_calls.append('reset' if reset else 'begin')
   self.x(0,(self.reset_result if reset else self.begin_result)&0xffffffff)
   uc.reg_write(ar.UC_ARM64_REG_PC,self.x(30));return
  if pc==0x4bef0:
   self.deleted.append(self.x(0));uc.reg_write(ar.UC_ARM64_REG_PC,self.x(30));return
  if pc==0x1744b0:
   self.calls.append('diagnostic');uc.reg_write(ar.UC_ARM64_REG_PC,self.x(30));return
  super().hook(uc,pc,size,user)
 def queue(self):
  self.run(0x3018b0,[0x1234,self.C],stop=0x3018b4)
  assert lib.life_queue(self.handle)
 def wait(self):assert lib.life_wait(self.handle)
 def current(self):
  self.x(19,self.R);self.x(23,self.R+0x1400)
  self.run(0x2a2230,stop=0x2a2324)
 def retained(self):
  self.x(22,self.R);self.run(0x2987d4,stop=0x298844)
 def reset(self):
  for lo,hi in [(0x770,0x778),(0x428,0x430),(0x4f0,0x4f8)]:self.wq(self.C+lo,self.R+lo);self.wq(self.C+hi,self.R+lo+8)
  # Actual relocated vtable destination, not a fixture-selected approximation.
  out=self.run(self.rq(0x59f918),[self.C])
  assert all(self.rq(self.C+lo)==self.rq(self.C+hi) for lo,hi in [(0x770,0x778),(0x428,0x430),(0x4f0,0x4f8)])
  return out
class InstructionTests(unittest.TestCase):
 def fixture(self):
  p=Native();self.addCleanup(p.close);return p
 def test_actual_swap_disposal_then_reset(self):
  for result,begin in [(0,0),(-4,0),(0,-4),(2,0)]:
   p=self.fixture();p.queue();p.current()
   self.assertEqual(lib.life_state(p.handle),1)
   self.assertEqual(p.deleted,[], 'current empty-retained route MOVES packets, does not free')
   p.retained();self.assertEqual(p.deleted,[p.R+0x20000])
   self.assertEqual(lib.life_state(p.handle),3)
   p.wait();p.reset_result=result;p.begin_result=begin
   self.assertEqual(p.reset(),int(result==0 and begin==0))
   expected=3 if result else (31 if not begin else 23)
   self.assertEqual(lib.life_state(p.handle),expected)
   self.assertEqual(p.vk_calls,['reset'] if result else ['reset','begin'])
   print('native reset raw=%d begin=%d lists empty in all cases, state=%d'%(result,begin,expected))
 def test_nonempty_retained_native_append(self):
  for capacity in (1,3):
   p=self.fixture();old=p.R+0x19000;stock=p.R+0x24000
   p.wq(p.R+0x15c0,old+0x98);p.wq(p.R+0x15c8,old+capacity*0x98)
   p.wq(old+0x50,stock);p.wq(old+0x58,stock+0x2b0);p.wq(old+0x60,stock+0x2b0)
   p.wq(stock+8,0x123);p.wq(stock+0x160,stock+0x170)
   p.queue();p.current();self.assertEqual(lib.life_state(p.handle),1)
   p.retained();p.wait();p.reset();self.assertEqual(lib.life_state(p.handle),31)
   self.assertIn(stock,p.deleted)
   if capacity==1:self.assertIn(old,p.deleted)
 def test_reset_before_completion_retains_unsupported_debt(self):
  p=self.fixture();p.queue();p.current();p.retained();p.reset()
  self.assertTrue(lib.life_state(p.handle)&32)
  self.assertFalse(lib.life_state(p.handle)&16)
  self.assertEqual(p.vk_calls,['reset','begin'],'observer never suppresses native mixed work')
 def test_delayed_disposal_and_failed_begin(self):
  p=self.fixture();p.queue();p.wait();p.begin_result=-4
  # Current mutable R already names another C before the old command resets.
  p.wq(p.R+0x1380,p.R+0x3000);p.wq(p.R+0x3008,p.R+0x7000)
  p.reset()
  self.assertEqual(lib.life_state(p.handle),4)
  p.current();p.retained();self.assertEqual(lib.life_state(p.handle),23)
 def test_repeat_common_queue_not_suppressed(self):
  p=self.fixture();p.queue();p.current();p.retained();p.wait();p.reset()
  p.run(0x3018b0,[0x1234,p.C],stop=0x3018b4)
  self.assertTrue(lib.life_state(p.handle)&32);self.assertFalse(lib.life_state(p.handle)&16)
  self.assertEqual(p.uc.reg_read(ar.UC_ARM64_REG_SP),p.STACK+0x10000-0x550)
 def test_full_register_preservation_vs_original(self):
  # Displaced instructions, including ADRP and both conditional result routes.
  for case in EMITTED:
   for raw in ([0,0xffffffff] if case['id'] in (5,7) else [0]):
    hooked=self.fixture();original=Native(False);self.addCleanup(original.close)
    for p in (hooked,original):
     for i in range(31):p.x(i,0x100+i)
     p.x(0,raw)
     for i in range(32):p.uc.reg_write(ar.UC_ARM64_REG_Q0+i,(i+1)*0x100000000000000010000000000000001)
     for reg,val in [(ar.UC_ARM64_REG_NZCV,0xa0000000),(ar.UC_ARM64_REG_FPCR,0x400000),(ar.UC_ARM64_REG_FPSR,0x10)]:p.uc.reg_write(reg,val)
     p.uc.reg_write(ar.UC_ARM64_REG_SP,p.STACK+0x10000)
     p.x(19,p.R);p.x(22,p.R)
    pc=case['offset'];target=pc+4
    if case['id']==5 and raw:target=0x300c68
    if case['id']==7 and raw:target=0x300a48
    original.uc.emu_start(pc,target,count=1)
    hooked.uc.emu_start(pc,target,count=300)
    self.assertEqual(hooked.uc.reg_read(ar.UC_ARM64_REG_PC),target)
    for i in range(31):self.assertEqual(hooked.x(i),original.x(i),(case['id'],i))
    for i in range(32):self.assertEqual(hooked.uc.reg_read(ar.UC_ARM64_REG_Q0+i),original.uc.reg_read(ar.UC_ARM64_REG_Q0+i))
    for reg in (ar.UC_ARM64_REG_SP,ar.UC_ARM64_REG_NZCV,ar.UC_ARM64_REG_FPCR,ar.UC_ARM64_REG_FPSR):self.assertEqual(hooked.uc.reg_read(reg),original.uc.reg_read(reg))
if __name__=='__main__':unittest.main(verbosity=2)
