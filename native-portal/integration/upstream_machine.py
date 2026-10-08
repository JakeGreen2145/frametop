#!/usr/bin/env python3
"""Compiled Adapter + Bridge + emitted islands + pinned native instruction path.
Only allocator, memcpy, barrier, enqueue/pipeline/queue operations are CPU fakes.
"""
import ctypes as C
import pathlib,sys,struct,unittest
R=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(R/'arena'))
import test_machine as a
from unicorn import UcError
lib=C.CDLL(str(R/'render/build/upstream-machine.so'))
class Descriptor(C.Structure):_fields_=[(k,C.c_uint64) for k in ('buffer','offset','range')]
RESOLVE=C.CFUNCTYPE(C.c_bool,C.c_void_p,C.POINTER(Descriptor))
for name,args,result in [
 ('new',[C.c_void_p,C.POINTER(a.Memory),RESOLVE,*([C.c_uint64]*4)],C.c_void_p),
 ('delete',[C.c_void_p],None),('reserve',[C.c_void_p,C.c_uint],C.c_uint64),
 ('dispatch',[C.c_void_p,C.POINTER(a.Saved),C.c_uint,C.c_uint64,C.c_uint64],None),
 ('enqueue',[C.c_void_p,C.c_uint64],C.c_bool),('finish',[C.c_void_p],C.c_bool),
 ('off',[C.c_void_p],None),('expire',[C.c_void_p],None),('generation',[C.c_void_p],None),
 ('pending',[C.c_void_p],C.c_bool),('enqueues',[C.c_void_p],C.c_uint),
 ('record',[C.c_void_p,C.c_uint,C.POINTER(Descriptor)],None)]:
 f=getattr(lib,'upstream_'+name);f.argtypes=args;f.restype=result
class Integrated(a.Native):
 def __init__(self):
  self.adapter=None;self.descriptor_calls=0
  self.host_written:tuple[int,int]|None=None
  self.host_copy:tuple[int,int]|None=None
  super().__init__(False)
  a.lib.arena_delete(self.owner)
  def write(_,address,src,n):
   # Record the exact host write for this dispatch, not a numeric address class.
   C.memmove(address,src,n);self.host_written=(address,n);return True
  self.host_write=a.WRITE(write)
  self.memory=a.Memory(None,self.callbacks[0],self.host_write,self.callbacks[2])
  self.owner=a.lib.arena_new(C.byref(self.memory))
  assert a.lib.arena_arm(self.owner,C.byref(self.scope),True)
  self.initialize()
  # Synthetic native command identity is distinct from the wrapper and persists.
  command_wrapper=self.rq(self.R+0x1380)
  self.wq(command_wrapper+8,self.R+0x29000)
  self.wu(self.wrap+0x18,0xffffffff)
  # Two structurally valid current passes, populated by synthetic native setup.
  self.wq(self.R+0x15a8,self.rq(self.R+0x15a0)+2*0x98)
  identity=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
  projection=[1,0,0,0,0,1,0,0,0,0,-1,-.1,0,0,-1,0]
  self.uc.mem_write(self.K+0x100,struct.pack('<16f',*identity))
  self.uc.mem_write(self.K+0x140,struct.pack('<16f',*projection))
  def resolve(_,out):
   # Execute the pinned native resolver while preserving the suspended hook.
   # Its classifier goes through the same Adapter/Bridge island callback.
   ctx=self.uc.context_save()
   try:
    dest=self.R+0x1a000
    self.run(0x2988a8,[self.R,dest,self.wrap])
    values=struct.unpack('<QQQ',bytes(self.uc.mem_read(dest,24)))
    out[0]=Descriptor(*values);self.descriptor_calls+=1
    return True
   finally:self.uc.context_restore(ctx)
  self.resolve=RESOLVE(resolve)
  self.adapter=lib.upstream_new(self.owner,C.byref(self.memory),self.resolve,self.R,self.wrap,self.K,self.stockwrap)
 def hook(self,uc,pc,size,user):
  if pc==0x4be00 and self.host_copy is not None:
   # This libc boundary was already synthetic. Keep the native source pointer
   # unchanged, but read its registered host allocation rather than guest RAM.
   # The same integer may name a live native object in Unicorn; never map or
   # overwrite that object to mirror host staging. Consume provenance once.
   dst,src,n=[self.x(i) for i in range(3)]
   assert (src,n)==self.host_copy, 'native memcpy differs from owned host staging'
   payload=C.string_at(src,n)
   uc.mem_write(dst,payload);self.host_copy=None
   self.events.append({'kind':'memcpy_fake','dst':dst,'bytes':n})
   self.x(0,dst);self.ret();return
  if pc==0x680000 and self.adapter:
   ctx=self.x(0);site=self.x(1);s=a.Saved.from_buffer_copy(bytes(uc.mem_read(ctx,800)))
   if site in (0,6):self.host_written=None;self.host_copy=None # upload/reset
   lr=self.x(30);lib.upstream_dispatch(self.adapter,C.byref(s),site,self.lifetime,self.thread)
   if site==0 and s.pad==1:
    assert self.host_written==(s.x[2],592), 'replacement has no matching host write'
    self.host_copy=self.host_written
   uc.mem_write(ctx,bytes(s));self.calls.append((site,s.pad))
   for i in range(31):self.x(i,0)
   for i in range(32):uc.reg_write(a.ar.UC_ARM64_REG_Q0+i,0)
   for reg in (a.ar.UC_ARM64_REG_NZCV,a.ar.UC_ARM64_REG_FPCR,a.ar.UC_ARM64_REG_FPSR):uc.reg_write(reg,0)
   uc.reg_write(a.ar.UC_ARM64_REG_PC,lr);return
  super().hook(uc,pc,size,user)
 def close(self):
  if self.adapter:lib.upstream_delete(self.adapter);self.adapter=None
  super().close()
 def reserve(self,eye):return lib.upstream_reserve(self.adapter,eye)
 def enqueue(self,ticket):return lib.upstream_enqueue(self.adapter,ticket)
 def record(self,i):
  d=Descriptor();lib.upstream_record(self.adapter,i,C.byref(d));return d.buffer,d.offset,d.range
 def copies(self):return [e['bytes'] for e in self.events if e['kind']=='memcpy_fake']
class Tests(unittest.TestCase):
 def fixture(self):
  p=Integrated();self.addCleanup(p.close);return p
 def test_two_replacements_no_late_upload_both_orders(self):
  for order in ((0,1),(1,0)):
   p=self.fixture()
   for eye in order:
    t=p.reserve(eye);self.assertTrue(t);p.upstream();self.assertTrue(p.enqueue(t))
   self.assertEqual(p.copies(),[592,592]);self.assertEqual(p.descriptor_calls,4)
   self.assertEqual(p.record(0),(p.rq(p.A+0xb0),0,592))
   self.assertEqual(p.record(1),(p.rq(p.A+0xb0),640,592))
   self.assertTrue(lib.upstream_finish(p.adapter));self.assertFalse(lib.upstream_pending(p.adapter))
   self.assertEqual(a.lib.arena_actual(p.owner),1280)
   p.run(0x2990c8,[p.R,p.src,0x80000-1152,p.R+0x19000,0x1000,0x800])
   self.assertEqual(p.ru(p.R+0x1418),0x80080)
   self.assertFalse(any(e['kind']=='native_diagnostic_fake' for e in p.events))
 def test_unowned_stock_once(self):
  p=self.fixture();p.upstream();self.assertEqual(p.copies(),[528])
  t=p.reserve(0);self.assertTrue(t);p.upstream();self.assertTrue(p.enqueue(t))
  self.assertEqual(p.copies(),[528,592]);self.assertEqual(p.record(0)[1],576)
 def test_off_and_expiry_at_each_boundary(self):
  for revoke in (lib.upstream_off,lib.upstream_expire):
   for boundary in (0,1,2):
    p=self.fixture();t=p.reserve(0);self.assertTrue(t)
    if boundary==0:revoke(p.adapter);p.upstream()
    elif boundary==1:
     p.x(27,p.K);p.x(7,0x2993f8)
     p.run(0x161d64,[p.R,p.stockwrap,p.K+0xc0],stop=0x161d68)
     revoke(p.adapter);p.run(0x161d68,stop=0x161d6c)
    else:p.upstream();revoke(p.adapter)
    self.assertFalse(p.enqueue(t));self.assertEqual(lib.upstream_enqueues(p.adapter),0)
    self.assertEqual(p.copies(),[] if boundary==0 else [592])
    if boundary:self.assertTrue(lib.upstream_pending(p.adapter))
 def test_partial_copy_keeps_reservation_and_debt(self):
  p=self.fixture();t=p.reserve(0);self.assertTrue(t)
  p.x(27,p.K);p.x(7,0x2993f8)
  p.run(0x161d64,[p.R,p.stockwrap,p.K+0xc0],stop=0x29912c)
  self.assertEqual(a.lib.arena_actual(p.owner),640);self.assertEqual(p.ru(p.R+0x1418),0)
  self.assertTrue(lib.upstream_pending(p.adapter));self.assertFalse(p.enqueue(t))
  p.reset();self.assertEqual(a.lib.arena_actual(p.owner),0);self.assertTrue(lib.upstream_pending(p.adapter))
 def test_generation_and_duplicate_descriptor_refuse(self):
  p=self.fixture();t=p.reserve(0);p.upstream();lib.upstream_generation(p.adapter)
  self.assertFalse(p.enqueue(t));self.assertEqual(p.copies(),[592])
  p=self.fixture();t=p.reserve(0);p.upstream();p.upstream()
  self.assertFalse(p.enqueue(t));self.assertEqual(p.copies(),[592])
 def test_wrong_thread_or_lifetime_preserves_unsupported_trap(self):
  for field in ('thread','lifetime'):
   p=self.fixture();self.assertTrue(p.reserve(0));setattr(p,field,getattr(p,field)+1)
   with self.assertRaises(UcError):p.upstream()
   self.assertEqual(p.copies(),[])
 def test_later_native_constants_not_early_snapshot(self):
  p=self.fixture();t=p.reserve(0);p.uc.mem_write(p.K+0xc0,b'final528')
  p.upstream();self.assertTrue(p.enqueue(t))
  self.assertEqual(bytes(p.uc.mem_read(p.mapping,528)),bytes(p.uc.mem_read(p.K+0xc0,528)))
if __name__=='__main__':unittest.main(verbosity=2)
