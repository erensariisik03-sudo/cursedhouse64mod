#!/usr/bin/env python3
"""Exercise the real PowerShell collector with an isolated fake ADB executable."""
import argparse,json,os,subprocess,tempfile,unittest,zipfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--pwsh',required=True);args=p.parse_args()
SCRIPT=Path(__file__).resolve().parents[1]/'tools/Collect-GmmDiagnostics.ps1'
FAKE='''#!/usr/bin/env python3
import json,os,sys,time
with open(os.environ['GMM_TEST_CALLS'],'a') as f:f.write(json.dumps(sys.argv[1:])+'\\n')
a=sys.argv[1:];mode=os.environ['GMM_TEST_MODE']
if a==['devices']:
 print('List of devices attached')
 print('TEST:5555\\tdevice' if mode!='offline' else 'TEST:5555\\tunauthorized')
 if mode=='multi':print('SECOND\\tdevice')
 sys.exit(0)
assert a[:2]==['-s','TEST:5555'],a
a=a[2:]
if a[0]=='logcat' and '-d' not in a:
 print('I multiplayermod: GMM_DIAG build=gas-god-ammo-arm64',flush=True)
 print('I multiplayermod: GMM_DIAG gas entry=victim-rpc blocked=1',flush=True)
 if mode=='logger_fail':sys.exit(1)
 if mode=='logger_drop':time.sleep(0.7);sys.exit(2)
 time.sleep(60)
elif a[:4]==['shell','dumpsys','activity','exit-info']:
 print('exit-info unsupported on this test device');sys.exit(1)
else:print('snapshot evidence')
'''
class CollectorTest(unittest.TestCase):
 def invoke(self,mode='normal',extra=()):
  temp=tempfile.TemporaryDirectory(prefix='gmm diagnostics ');self.addCleanup(temp.cleanup)
  root=Path(temp.name);fake=root/'fake-adb';fake.write_text(FAKE);fake.chmod(0o700)
  calls=root/'calls.jsonl';env=dict(os.environ,GMM_TEST_CALLS=str(calls),GMM_TEST_MODE=mode)
  result=subprocess.run([args.pwsh,'-NoLogo','-NoProfile','-File',str(SCRIPT),'-Seconds','1','-AdbPath',str(fake),'-OutputDirectory',str(root/'logs with spaces'),*extra],env=env,text=True,capture_output=True,timeout=30)
  entries=[json.loads(x) for x in calls.read_text().splitlines()] if calls.exists() else []
  archives=list((root/'logs with spaces').glob('*.zip'))
  for call in entries:
   self.assertNotIn('-c',call);self.assertNotIn('force-stop',call);self.assertNotIn('kill-server',call)
  return result,entries,archives
 def test_capture_and_zip(self):
  r,c,z=self.invoke();self.assertEqual(r.returncode,0,r.stderr+r.stdout);self.assertEqual(len(z),1)
  with zipfile.ZipFile(z[0]) as archive:
   names=archive.namelist();self.assertIn('logcat.txt',names);self.assertIn('crash-buffer.txt',names)
   self.assertIn('GMM_DIAG gas',archive.read('diagnostics.txt').decode('utf-8-sig'))
   self.assertIn('ADB exit code: 1',archive.read('after-exit-info.txt').decode('utf-8-sig'))
   self.assertIn('before-memory.txt',names);self.assertIn('after-memory.txt',names)
  self.assertTrue(any('-T' in call for call in c))
 def test_multiple_devices_require_selection(self):
  r,c,z=self.invoke('multi');self.assertNotEqual(r.returncode,0);self.assertFalse(z);self.assertEqual(c,[['devices']])
 def test_explicit_serial(self):
  r,c,z=self.invoke('multi',('-Serial','TEST:5555'));self.assertEqual(r.returncode,0,r.stderr+r.stdout);self.assertEqual(len(z),1)
 def test_unauthorized_device(self):
  r,c,z=self.invoke('offline');self.assertNotEqual(r.returncode,0);self.assertFalse(z);self.assertEqual(c,[['devices']])
 def test_failed_logger_still_archives(self):
  r,c,z=self.invoke('logger_fail');self.assertNotEqual(r.returncode,0);self.assertEqual(len(z),1)
  with zipfile.ZipFile(z[0]) as archive:self.assertIn('after-exit-info.txt',archive.namelist())
 def test_disconnected_logger_preserves_evidence(self):
  r,c,z=self.invoke('logger_drop');self.assertEqual(r.returncode,0,r.stderr+r.stdout);self.assertEqual(len(z),1)
  with zipfile.ZipFile(z[0]) as archive:self.assertIn('GMM_DIAG gas',archive.read('diagnostics.txt').decode('utf-8-sig'))
 def test_invalid_package(self):
  r,c,z=self.invoke(extra=('-PackageName','bad;command'));self.assertNotEqual(r.returncode,0);self.assertFalse(c);self.assertFalse(z)
unittest.main(argv=['diagnostics_test.py'],verbosity=2)
