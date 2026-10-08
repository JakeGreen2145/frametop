#!/usr/bin/env python3
"""Offline pinned instructions. External memcpy/resource barriers are synthetic."""
import ast, ctypes as C, importlib.util, json, os, pathlib, struct, sys, unittest
R=pathlib.Path(__file__).resolve().parent
WORK=pathlib.Path('/home/jakeg/.hermes/work')
import hashlib
for name,digest in json.loads((R/'manifest.json').read_text())['fixture_sha256'].items():
    assert hashlib.sha256((WORK/name).read_bytes()).hexdigest()==digest, 'offline fixture hash mismatch: '+name
sys.dont_write_bytecode=True
sys.path.insert(0,str(WORK/'frame-native-transaction-feasibility'))
from explore import b, img
# Load only audited class definitions, never the evidence writer / top-level probes.
tree=ast.parse((WORK/'frame-native-transaction-feasibility/probes.py').read_text())
ns={'b':b,'img':img,'struct':struct}
from unicorn import UC_HOOK_CODE
import unicorn.arm64_const as ar
ns.update(UC_HOOK_CODE=UC_HOOK_CODE,ar=ar)
exec(compile(ast.Module(body=[n for n in tree.body if isinstance(n,ast.ClassDef)],type_ignores=[]),'<audited Probe class>','exec'),ns)
P=ns['P']
class OriginalRegression(unittest.TestCase):
    def test_full_original_budget_with_two_substitutions(self):
        p=P(); p.wu(p.A+0xb8,0x80080)
        p.upload(); p.upload()
        p.run(0x2990c8,[p.R,p.src,0x80000-1152,p.R+0x19000,0x1000,0x800])
        self.assertFalse(any(e['kind']=='native_diagnostic_fake' for e in p.events), 'old fixed comparison enters unsafe log-and-copy path')
    def test_expanded_ubo_classifier(self):
        p=P();p.wu(p.wrap+0x18,0x80000)
        out=p.R+0x19000;p.run(0x2988a8,[p.R,out,p.wrap])
        self.assertEqual(p.rq(out),0xaabb0001,'old classifier silently selects own buffer')
# Running --red executes these requirements against the original ELF.
if '--red' in sys.argv:
    sys.argv.remove('--red')
    unittest.main(verbosity=2)

READ=C.CFUNCTYPE(C.c_bool,C.c_void_p,C.c_uint64,C.c_void_p,C.c_size_t)
WRITE=READ
COHERENT=C.CFUNCTYPE(C.c_bool,C.c_void_p,C.c_uint64)
class Memory(C.Structure): _fields_=[('user',C.c_void_p),('read',READ),('write',WRITE),('coherent',COHERENT)]
class Scope(C.Structure): _fields_=[(k,C.c_uint64) for k in ('renderer','lifetime','thread','allocator','base')]
class Owned(C.Structure): _fields_=[(k,C.c_uint64) for k in ('camera','stock_wrapper','wrapper','staging','ticket')]+[('portal',C.c_ubyte*64)]
class Descriptor(C.Structure): _fields_=[(k,C.c_uint64) for k in ('buffer','offset','range','ticket')]
class Saved(C.Structure): _fields_=[('x',C.c_uint64*31),('sp',C.c_uint64),('q',C.c_ubyte*512),('nzcv',C.c_uint64),('fpcr',C.c_uint64),('fpsr',C.c_uint64),('pad',C.c_uint64)]
lib=C.CDLL(str(R/'build/test_bridge.so'))
for name,args,result in [
 ('new',[C.POINTER(Memory)],C.c_void_p),('delete',[C.c_void_p],None),
 ('arm',[C.c_void_p,C.POINTER(Scope),C.c_bool],C.c_bool),
 ('offer',[C.c_void_p,C.POINTER(Owned)],C.c_bool),
 ('dispatch',[C.c_void_p,C.POINTER(Saved),C.c_uint,C.c_uint64,C.c_uint64],None),
 ('descriptor',[C.c_void_p,C.POINTER(Descriptor)],C.c_bool),
 ('actual',[C.c_void_p],C.c_uint64),('ready',[C.c_void_p],C.c_bool)]:
 fn=getattr(lib,'arena_'+name);fn.argtypes=args;fn.restype=result
EMITTED=json.loads((R/'build/emitted.json').read_text())
class Native(P):
 def __init__(self,initialize=True):
  self.owner=None;self.calls=[];self.callback_errors=[];self.thread=2;self.lifetime=1
  super().__init__()
  def read(_,address,out,n):
   try:C.memmove(out,bytes(self.uc.mem_read(address,n)),n);return True
   except Exception as e:self.callback_errors.append(str(e));return False
  def write(_,address,src,n):
   try:self.uc.mem_write(address,C.string_at(src,n));return True
   except Exception as e:self.callback_errors.append(str(e));return False
  self.callbacks=(READ(read),WRITE(write),COHERENT(lambda _,r:r==self.A))
  self.memory=Memory(None,*self.callbacks);self.owner=lib.arena_new(C.byref(self.memory))
  self.scope=Scope(self.R,1,2,self.R+0x25000,0)
  assert lib.arena_arm(self.owner,C.byref(self.scope),True)
  for case in EMITTED:
   if case['base']:continue
   self.uc.mem_write(case['island'],bytes.fromhex(case['code']))
   self.uc.mem_write(case['offset'],struct.pack('<I',case['branch']))
  self.K=self.R+0x160000;self.staging=self.R+0x170000
  self.stockwrap=self.R+0x11000;self.stockresource=self.R+0x13100
  self.wq(self.stockwrap+8,self.stockresource);self.wu(self.stockresource+0xb8,528)
  self.wq(self.stockresource+0xb0,0xaabb0003);self.wu(self.stockwrap+0x18,0xffffffff)
  self.wq(self.K+8,self.R);self.wq(self.K+0x2d0,self.stockwrap)
  self.uc.mem_write(self.K+0xc0,bytes(i%251 for i in range(528)))
  if initialize:self.initialize()
 def run(self,*args,**kwargs):
  # BRK is expected in refusal cases; leave assertion reporting to unittest.
  return b.Probe.run(self,*args,**kwargs)
 def initialize(self):
  out=self.R+0x26000;self.x(19,self.R);self.x(8,out)
  self.run(0x29a14c,[self.scope.allocator,0x80000],stop=0x29a154)
  self.A=self.rq(out);assert self.ru(self.A+0xb8)==0x80080
  self.wq(self.R+0x1390,self.A)
  self.x(24,self.R);self.run(0x7fdbc,[1],stop=0x7fdc0)
  assert lib.arena_ready(self.owner)
 def close(self):
  if self.owner:lib.arena_delete(self.owner);self.owner=None
 def hook(self,uc,pc,size,user):
  if pc==0x680000:
   ctx=self.x(0);site=self.x(1);s=Saved.from_buffer_copy(bytes(uc.mem_read(ctx,800)))
   lr=self.x(30);lib.arena_dispatch(self.owner,C.byref(s),site,self.lifetime,self.thread)
   uc.mem_write(ctx,bytes(s));self.calls.append((site,s.pad))
   # Hostile external callback: production island must preserve complete state.
   for i in range(31):self.x(i,0)
   for i in range(32):uc.reg_write(ar.UC_ARM64_REG_Q0+i,0)
   for reg in (ar.UC_ARM64_REG_NZCV,ar.UC_ARM64_REG_FPCR,ar.UC_ARM64_REG_FPSR):uc.reg_write(reg,0)
   uc.reg_write(ar.UC_ARM64_REG_PC,lr);return
  super().hook(uc,pc,size,user)
 def offer(self,ticket):
  o=Owned(self.K,self.stockwrap,self.wrap,self.staging,ticket,(C.c_ubyte*64)(*([0xa5]*64)))
  assert lib.arena_offer(self.owner,C.byref(o));return o
 def upstream(self):
  self.x(27,self.K);self.x(7,0x2993f8)
  self.run(0x161d64,[self.R,self.stockwrap,self.K+0xc0],stop=0x161d6c)
 def descriptor(self):
  d=Descriptor();assert lib.arena_descriptor(self.owner,C.byref(d));return (d.buffer,d.offset,d.range,d.ticket)
 def reset(self):
  self.x(22,self.R);self.run(0x2987d4,stop=0x2987d8)
class NativeTests(unittest.TestCase):
 def fixture(self,initialize=True):
  p=Native(initialize);self.addCleanup(p.close);return p
 def test_full_stock_budget_two_upstream_replacements(self):
  p=self.fixture();p.offer(1);p.upstream();left=p.descriptor();p.offer(2);p.upstream();right=p.descriptor()
  self.assertEqual(left[1:],(0,592,1));self.assertEqual(right[1:],(640,592,2))
  self.assertEqual(bytes(p.uc.mem_read(p.mapping,528)),bytes(p.uc.mem_read(p.K+0xc0,528)))
  self.assertEqual(bytes(p.uc.mem_read(p.mapping+528,64)),bytes([0xa5]*64))
  p.run(0x2990c8,[p.R,p.src,0x80000-1152,p.R+0x19000,0x1000,0x800])
  self.assertEqual(p.ru(p.R+0x1418),0x80080);self.assertEqual(lib.arena_actual(p.owner),0x80080)
  self.assertEqual([e['bytes'] for e in p.events if e['kind']=='memcpy_fake'],[592,592,0x80000-1152])
  self.assertEqual(len(p.barriers),3)
  self.assertFalse(any(e['kind']=='native_diagnostic_fake' for e in p.events))
  self.assertFalse(p.callback_errors)
 def test_third_owned_stops_only_owned_then_stock_fills_budget(self):
  p=self.fixture()
  for n in (1,2):p.offer(n);p.upstream()
  p.offer(3);p.upstream();d=Descriptor();self.assertFalse(lib.arena_descriptor(p.owner,C.byref(d)))
  self.assertEqual(lib.arena_actual(p.owner),1280)
  p.run(0x2993f8,[p.R,p.stockwrap,p.src]);self.assertEqual(p.ru(p.R+0x1418),1856)
  self.assertEqual([e['bytes'] for e in p.events if e['kind']=='memcpy_fake'],[592,592,528])
 def test_unowned_forward_once_and_no_repeat_fallback(self):
  p=self.fixture();p.upstream();self.assertEqual(p.ru(p.R+0x1418),576)
  p.offer(1);p.upstream();p.upstream()
  self.assertEqual([e['bytes'] for e in p.events if e['kind']=='memcpy_fake'],[528,592])
  d=Descriptor();self.assertFalse(lib.arena_descriptor(p.owner,C.byref(d)), 'duplicate skipped invocation must not revive a prior descriptor')
 def test_reset_only_at_native_cursor_reset(self):
  p=self.fixture();p.offer(1);p.upstream();p.offer(2);p.upstream()
  p.reset();self.assertEqual(p.ru(p.R+0x1418),0);self.assertEqual(lib.arena_actual(p.owner),0)
  p.offer(3);p.upstream();self.assertEqual(p.descriptor()[1],0)
 def test_both_classifiers_high_offsets_and_sentinel(self):
  p=self.fixture();p.wu(p.own+0xb8,64);p.wu(p.wrap+0x18,0x80000)
  out=p.R+0x19000;p.run(0x2988a8,[p.R,out,p.wrap]);self.assertEqual(p.rq(out),p.rq(p.A+0xb0));self.assertEqual(p.rq(out+8),0x80000)
  p.wu(p.wrap+0x18,0xffffffff);p.run(0x2988a8,[p.R,out,p.wrap]);self.assertEqual(p.rq(out),0xaabb0002);self.assertEqual(p.rq(out+16),0xffffffffffffffff)
  packet=p.R+0x20000;p.wq(p.wrap+0x10,p.own);p.wq(packet+0x120,p.wrap)
  p.wu(packet+0x1bc,0x80000);p.wu(packet+0x1c0,64);p.wu(p.R+0x1418,0x80080)
  p.x(24,packet);p.x(21,p.R);p.run(0x29f354,stop=0x29f3ac)
  p.wu(packet+0x1bc,0xffffffff);p.run(0x29f354,stop=0x29f3f0)
 def test_geometry_predecessor_charges_stock(self):
  p=self.fixture();packet=p.R+0x20000;p.wq(p.wrap+0x10,p.own)
  p.wq(p.R+0x18000+0x50,packet);p.wq(p.R+0x18000+0x58,packet+0x2b0);p.wq(packet+0x120,p.wrap)
  p.run(0x299208,[p.R,p.wrap,p.src,64])
  self.assertEqual(p.ru(p.R+0x1418),64);self.assertEqual(lib.arena_actual(p.owner),64);self.assertEqual(p.ru(p.wrap+0x1c),64)
  p.run(0x2990c8,[p.R,p.src,0,p.R+0x19000,0x1000,0x800])
  original=P();original.wu(original.R+0x1418,64)
  original.run(0x2990c8,[original.R,original.src,0,original.R+0x19000,0x1000,0x800])
  self.assertEqual(p.ru(p.R+0x1418),original.ru(original.R+0x1418));self.assertEqual(lib.arena_actual(p.owner),64)
 def test_unsupported_overflow_traps_before_copy(self):
  from unicorn import UcError
  p=self.fixture();p.run(0x2990c8,[p.R,p.src,0x80000,p.R+0x19000,0x1000,0x800]);count=len(p.events)
  with self.assertRaises(UcError):p.run(0x2990c8,[p.R,p.src,64,p.R+0x19000,0x1000,0x800])
  self.assertEqual(len(p.events),count);self.assertFalse(lib.arena_ready(p.owner))
 def test_wrong_alignment_and_lifetime_never_copy(self):
  from unicorn import UcError
  for wrong in ('alignment','lifetime','thread'):
   p=self.fixture();p.offer(1)
   if wrong=='alignment':p.wq(p.R+0x328,32)
   elif wrong=='lifetime':p.lifetime=2
   else:p.thread=3
   with self.assertRaises(UcError):p.upstream()
   self.assertFalse(any(e['kind']=='memcpy_fake' for e in p.events))
 def test_late_attach_keeps_stock_unchanged(self):
  p=self.fixture(False);p.upstream();self.assertEqual(p.ru(p.R+0x1418),576);self.assertFalse(lib.arena_ready(p.owner))
 def test_initialization_rejects_foreign_allocation_resource(self):
  p=self.fixture(False);out=p.R+0x26000;p.x(19,p.R);p.x(8,out)
  p.run(0x29a14c,[p.scope.allocator,0x80000],stop=0x29a154)
  # An enlarged lookalike resource is not the returned native allocation.
  p.wu(p.A+0xb8,0x80080)
  p.x(24,p.R);p.run(0x7fdbc,[1],stop=0x7fdc0)
  self.assertFalse(lib.arena_ready(p.owner))
 def test_wrong_stock_wrapper_size_never_reads_592_from_camera(self):
  p=self.fixture();p.offer(1);p.wu(p.stockresource+0xb8,592);p.upstream()
  self.assertFalse(any(e['kind']=='memcpy_fake' for e in p.events))
  self.assertEqual(lib.arena_actual(p.owner),0)
 def test_partial_native_write_retains_charge(self):
  p=self.fixture();p.offer(1)
  p.x(27,p.K);p.x(7,0x2993f8)
  p.run(0x161d64,[p.R,p.stockwrap,p.K+0xc0],stop=0x29912c)
  self.assertEqual(lib.arena_actual(p.owner),640)
  self.assertEqual(p.ru(p.R+0x1418),0)
  d=Descriptor();self.assertFalse(lib.arena_descriptor(p.owner,C.byref(d)))
  p.reset();self.assertEqual(lib.arena_actual(p.owner),0)
 def test_native_initialization_failures_refuse_capability(self):
  for fault in ('capacity','map','alignment','result','coherence'):
   p=self.fixture(False);out=p.R+0x26000;p.x(19,p.R);p.x(8,out)
   p.run(0x29a14c,[p.scope.allocator,0x80000],stop=0x29a154)
   allocated=p.rq(out);p.wq(p.R+0x1390,allocated)
   if fault!='coherence':p.A=allocated
   if fault=='capacity':p.wu(allocated+0xb8,0x80000)
   if fault=='map':p.wq(p.R+0x1410,0)
   if fault=='alignment':p.wq(p.R+0x328,32)
   p.x(24,p.R);p.run(0x7fdbc,[0 if fault=='result' else 1],stop=0x7fdc0)
   self.assertFalse(lib.arena_ready(p.owner))
 def test_hash_and_predecessor_audit(self):
  import prepare
  with self.assertRaises(ValueError):prepare.verify_hash(b'not the pinned ELF')
  manifest=json.loads((R/'manifest.json').read_text())
  self.assertEqual(prepare.audit(prepare.image()),manifest['direct_predecessors'])

if __name__=='__main__':
 suite=unittest.defaultTestLoader.loadTestsFromTestCase(NativeTests)
 result=unittest.TextTestRunner(verbosity=2).run(suite)
 sys.exit(not result.wasSuccessful())
