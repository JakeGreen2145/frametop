#!/usr/bin/env python3
"""Test-only host/guest address alias regression. Executes the native upload path."""
import ctypes as C
import unittest
from unittest.mock import patch
import upstream_machine as m


class NativeObjectsAtHostAddress(m.Integrated):
    # Keep ordinary native objects separate while deliberately placing another
    # guest allocation at each real Adapter staging address, independent of ASLR.
    R=0x20000000000
    STACK=R+0x200000
    STOP=R+0x300000

    def hook(self,uc,pc,size,user):
        if pc==0x4be00 and getattr(self,'before_copy',None):
            self.before_copy()
        super().hook(uc,pc,size,user)


class TransportTests(unittest.TestCase):
    def test_native_memcpy_must_match_registered_host_write(self):
        for register in (1,2):
            with self.subTest(register=register):
                p=NativeObjectsAtHostAddress()
                self.addCleanup(p.close)
                self.assertTrue(p.reserve(0))
                p.before_copy=lambda: p.x(register,p.x(register)+1)
                with self.assertRaisesRegex(AssertionError,'native memcpy differs from owned host staging'):
                    p.upstream()
                self.assertEqual(p.copies(),[])

    def test_reset_discards_abandoned_host_source(self):
        p=NativeObjectsAtHostAddress()
        self.addCleanup(p.close)
        self.assertTrue(p.reserve(0))
        p.x(27,p.K);p.x(7,0x2993f8)
        p.run(0x161d64,[p.R,p.stockwrap,p.K+0xc0],stop=0x29912c)
        assert p.host_copy is not None
        source,n=p.host_copy
        p.reset()
        # Reset cannot leave an address registered as host memory forever.
        for page in range(source&~4095,(source+n+4095)&~4095,4096):
            p.uc.mem_map(page,4096)
        guest=b'G'*n
        p.uc.mem_write(source,guest)
        p.run(0x4be00,[p.src,source,n])
        self.assertEqual(bytes(p.uc.mem_read(p.src,n)),guest)
        self.assertTrue(m.lib.upstream_pending(p.adapter))

    def test_real_staging_alias_preserves_guest_objects_and_exact_upload(self):
        for order in ((0,1),(1,0)):
            with self.subTest(order=order):
                p=NativeObjectsAtHostAddress()
                self.addCleanup(p.close)
                dispatch=m.lib.upstream_dispatch
                sources=[]
                guest_pages={}

                def alias_after_dispatch(adapter,saved,site,life,thread):
                    dispatch(adapter,saved,site,life,thread)
                    s=saved._obj
                    if site!=0 or s.pad!=1:
                        return
                    source=s.x[2]
                    sources.append((source,C.string_at(source,592)))
                    for page in range(source&~4095,(source+592+4095)&~4095,4096):
                        if page not in guest_pages:
                            self.assertFalse(any(lo<=page<=hi for lo,hi,_ in p.uc.mem_regions()))
                            p.uc.mem_map(page,4096)
                            sentinel=bytes((i*7+53)%256 for i in range(4096))
                            p.uc.mem_write(page,sentinel)
                            guest_pages[page]=sentinel

                with patch.object(m.lib,'upstream_dispatch',alias_after_dispatch):
                    for index,eye in enumerate(order):
                        ticket=p.reserve(eye)
                        self.assertTrue(ticket)
                        def before_copy():
                            source,payload=sources[-1]
                            self.assertEqual((p.x(0),p.x(1),p.x(2)),(p.mapping+index*640,source,592))
                            self.assertEqual(payload[:528],bytes(p.uc.mem_read(p.K+0xc0,528)))
                        p.before_copy=before_copy
                        p.upstream()
                        p.before_copy=None
                        source,payload=sources[-1]
                        self.assertTrue(p.enqueue(ticket))
                        self.assertEqual(bytes(p.uc.mem_read(p.mapping+index*640,592)),payload)
                        for page,sentinel in guest_pages.items():
                            self.assertEqual(bytes(p.uc.mem_read(page,4096)),sentinel)
                        # The same integer is now an ordinary guest memcpy source.
                        # Ownership must be invocation-scoped, not address-scoped.
                        guest=bytes(p.uc.mem_read(source,592))
                        p.run(0x4be00,[p.src,source,592])
                        self.assertEqual(bytes(p.uc.mem_read(p.src,592)),guest)
                self.assertEqual(p.descriptor_calls,4)
                self.assertEqual(p.record(0),(p.rq(p.A+0xb0),0,592))
                self.assertEqual(p.record(1),(p.rq(p.A+0xb0),640,592))
                self.assertEqual(p.copies(),[592,592,592,592])
                self.assertTrue(m.lib.upstream_finish(p.adapter))
                self.assertFalse(m.lib.upstream_pending(p.adapter))
                self.assertFalse(p.callback_errors)


if __name__=='__main__':
    unittest.main(verbosity=2)
