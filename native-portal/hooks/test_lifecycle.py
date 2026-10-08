#!/usr/bin/env python3
"""Actual production callback fixtures, no native runtime or VR calls."""
import pathlib
import subprocess
import tempfile
import unittest
import decode

R = pathlib.Path(__file__).resolve().parent

class LifecycleTests(unittest.TestCase):
    def test_callback_sampling_and_metadata(self):
        with tempfile.TemporaryDirectory(dir=R/'build', prefix='lifecycle-') as d:
            p = pathlib.Path(d)
            source = (R/'observer.c').read_text().split('static int guards(', 1)[0]
            for text in ('#include <openssl/evp.h>\n', '#include "identity.h"\n',
                         '#if !defined(__aarch64__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__\n#error This observer is little-endian AArch64 only.\n#endif\n'):
                source = source.replace(text, '', 1)
            (p/'observer-under-test.c').write_text(source)
            subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function',
                            '-Wno-unused-variable','-Wno-misleading-indentation', '-I'+str(p),
                            '-I'+str(R),str(R/'test_lifecycle.c'),'-pthread','-Wl,--wrap=clock_gettime','-o',str(p/'test')], check=True)
            result = subprocess.run([str(p/'test'),str(p/'events.bin')], capture_output=True, text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            with (p/'events.bin').open('rb') as f:
                events = list(decode.records(f))
            self.assertTrue(all(e['schema']==2 for e in events))
            enqueue = next(e for e in events if e.get('event')=='camera_enqueue')
            self.assertEqual(next(c for c in enqueue['chunks'] if c['tag']==21)['hex'],'4000000001000000')
            self.assertEqual([e['eye'] for e in events if e.get('event')=='camera_enqueue'],[0,1])
            self.assertTrue(all(e['flags'] & 32 for e in events if e.get('event') in ('acquire_enter','acquire_return_true')))
            new_sites=set(range(25,len(decode.NAMES)))
            self.assertEqual(new_sites,{e['site'] for e in events if e['site'] in new_sites})
            cache = next(e for e in events if e.get('event')=='cache_poststore')
            evidence = next(c for c in cache['chunks'] if c['tag']==41).get('cache_join')
            self.assertEqual(evidence,dict(before=0,target=1,after=1,status='committed-change'))
            wait = next(e for e in events if e.get('event')=='wait_return')
            entry = next(c for c in wait['chunks'] if c['tag']==34)['entry']
            self.assertEqual(entry['serial'],0x1122334455667788)
            self.assertEqual(entry['subserial'],3)
            self.assertEqual(wait['x'][0],'0x1')
            post = next(e for e in events if e.get('event')=='acquire_return_true')
            self.assertEqual(next(c for c in post['chunks'] if c['tag']==12)['hex'], '42'*40)
            backend = next(e for e in events if e.get('event')=='backend_enter')
            name = next(c for c in backend['chunks'] if c['tag']==14)
            self.assertEqual(name['hex'], b'Cam: Room View'.hex())
            policy = next(e for e in events if e.get('event')=='policy_feedback')
            evidence = next(c for c in policy['chunks'] if c['tag']==30)['policy_loads']
            self.assertEqual(evidence['style']['value'],2)
            self.assertTrue(evidence['style']['conflicting'])
            self.assertIsNone(evidence['ordinary_request']['value'])
            self.assertEqual(events[-1]['counters']['critical_budget_drops'],1)
            print(result.stdout,end='')
            long_run=subprocess.run([str(p/'test'),str(p/'long.bin'),'120s'],capture_output=True,text=True)
            self.assertEqual(long_run.returncode,0,long_run.stdout+long_run.stderr)
            with (p/'long.bin').open('rb') as f:
                long_events=list(decode.records(f))
            release=next(e for e in long_events if e.get('event')=='release_return_slow')
            self.assertEqual(release['frame_context'],9001)
            self.assertEqual(long_events[-1]['control'],'drained')
            self.assertEqual(long_events[-1]['monotonic_ns'],120000000001)
            frames={e['frame_context'] for e in long_events if e.get('event')=='outer_enter'}
            for frame in frames:
                self.assertEqual([e['eye'] for e in long_events if e.get('event')=='camera_enqueue' and e['frame_context']==frame],[0,1])
            self.assertLessEqual((p/'long.bin').stat().st_size,long_events[-1]['sampling']['max_file_bytes'])
            print(long_run.stdout,end='')
            print('synthetic trace:',len(long_events),'records,',len(frames),'complete sampled stereo frames,',(p/'long.bin').stat().st_size,'bytes')

if __name__=='__main__': unittest.main(verbosity=2)
