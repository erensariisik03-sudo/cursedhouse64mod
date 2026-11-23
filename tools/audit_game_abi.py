#!/usr/bin/env python3
"""Read-only audit of metadata symbols and required IL2CPP exports for the ARM64 target."""
import argparse,hashlib,json,re,struct,subprocess
from pathlib import Path
# Names/arity only. Field offsets and native RVAs below are evidence, never runtime configuration.
SPECS={
 ('','HandleFallDemage'):({'Update':0,'hnl':0,'hno':0,'hnm':0},['pv','rfe']),
 ('','Door'):({'ewl':0},['doorOpen','doorMoving','doorLocked']),
 ('','NoiseObjectFallsMulti'):({'ffp':0},['spawnObject']),('','NoiseObjectFallsMultiGrandpa'):({'fga':0},['spawnObject']),
 ('','FirstPersonController'):({'hjs':0},['pv','walkSpeed','crouchSpeed']),
 ('','DaysManager'):({},[]),
 ('','FootStepsController'):({'OnSteppedFloor':0},['pv']),
 ('UnityEngine','Physics'):({'Linecast':5},[]),('UnityEngine','RaycastHit'):({'get_collider':0},[]),
 ('','HandleLocalSeeTargetToggling'):({'hpg':2,'RPC_SyncToggleGrannySeeTarget':1},['pv','grannySeeTarget']),
 ('','EnemyEye'):({'eli':0,'GetPlayers':0,'CheckForPlayersInView':0},['pka','daysManager','grannyUpdateParseRate','seeRange']),('','EnemyEyeGrandpa'):({'dwz':0,'GetPlayers':0,'CheckForPlayersInView':0},['pdz','grandpaUpdateParseRate','seeRange']),
 ('','EnemyAIGrannyMulti'):({'eim':0,'ein':0},['pv','targetTransform','enemyEye']),
 ('','EnemyAIGrandpaMulti'):({'duo':0,'dup':0},['pv','targetTransform','enemyEye']),
 ('','SpiderMomAIController'):({'inb':0,'inf':0,'inh':1,'ims':0,'inc':0},['pv','rnx','target','eatingPlayer','isPlayerInView']),
 ('','HandlePlayerChestExplosion'):({'hup':2,'RPC_SyncPlayerDeathExplosion':2},['pv']),
 ('','playerController'):({'RPC_playerExplosionDeath':0,'RPC_playPlayerBurningOnDeath':0},['qsa']),
 ('Photon.Pun','PhotonMessageInfo'):({},['timeInt','Sender','photonView']),
 ('','fm.<RequestAsync>d__2'):({'MoveNext':0,'SetStateMachine':1},[]),
 ('','fi'):({'cvb':0,'cvd':0,'cvf':0,'cvz':0,'cwa':0,'cwb':0},['oly']),
 ('','et'):({'cqj':0},[]),('','ew'):({'crj':0,'crk':1,'crh':0,'cri':1},[]),
 ('','ey'):({'crr':0,'crt':0,'crv':0,'crs':1,'cru':1,'crw':1},['<ojw>k__BackingField','<ojx>k__BackingField','<ojy>k__BackingField']),
 ('','kt'):({'eql':0},[]),
 ('','CosmeticLibrary'):({},['skins','accessories','emotes']),
 ('','CosmeticLibrary.Entry'):({},['Id']),
 ('','LockerManager'):({'flh':0,'flj':0,'flm':1,'fln':1},['pyc','pyd']),
 ('','PhotonManager'):({'gna':1,'gnb':1},[]),
 ('','ChatManager'):({'SendMessage':0},['inputField','canSendMessage']),
 ('','TMPInputFieldTagFilter'):({},['qfy']),('','TMPTextTagFilter'):({},['qfz']),
 ('<PrivateImplementationDetails>{B6FBF95C-D343-4B5E-9090-977A04841BC0}','a'):({'dms':0,'dmt':0},[]),
 ('TMPro','TMP_InputField'):({'LateUpdate':0,'SendOnValueChanged':0,'ActivateInputFieldInternal':0,'OnDeselect':1,'OnDisable':0,'get_text':0,'set_text':1,'get_shouldHideSoftKeyboard':0,'set_shouldHideSoftKeyboard':1,'get_isFocused':0,'get_readOnly':0,'IsInteractable':0,'get_inputType':0,'DeactivateInputField':1,'SendOnSubmit':0,'get_characterLimit':0,'set_characterLimit':1},[]),
 ('UnityEngine.EventSystems','EventSystem'):({'Update':0,'get_current':0},[]),
 ('UnityEngine','Object'):({'op_Implicit':1,'FindObjectsByType':3},[]),
 ('UnityEngine','Behaviour'):({'get_isActiveAndEnabled':0},[]),
 ('UnityEngine','Resources'):({'FindObjectsOfTypeAll':1},[]),
 ('UnityEngine','Screen'):({'get_width':0,'get_height':0},[]),
 ('System','Array'):({'get_Length':0,'GetValueImpl':1},[]),
 ('System','Action'):({'Invoke':0},[]),
 ('System.Text.RegularExpressions','Regex'):({'.ctor':1},[]),
 ('Photon.Pun','PhotonNetwork'):({'get_IsConnectedAndReady':0,'get_LocalPlayer':0,'get_CurrentRoom':0,'get_IsMessageQueueRunning':0},[]),
 ('Photon.Realtime','Player'):({'get_CustomProperties':0},[]),
}
VOICE_SPECS={
 ('','VoiceChatManager'):({'gpy':0,'gqc':0},['recorder','punVoiceClient']),
 ('Photon.Voice.Unity','RemoteVoiceLink'):({'OnDecodedFrameFloatAction':1},['VoiceInfo']),
 ('Photon.Voice','VoiceInfo'):({'get_SamplingRate':0,'get_Channels':0},[]),
 ('Photon.Voice','FrameOut<T>'):({},['<Buf>k__BackingField','<EndOfStream>k__BackingField']),
 ('Photon.Voice.PUN','PhotonVoiceView'):({'get_SpeakerInUse':0},['photonView']),
 ('Photon.Voice.Unity','Speaker'):({'get_RemoteVoice':0},[]),
 ('Photon.Voice.Unity','VoiceConnection'):({'get_Client':0},[]),
 ('Photon.Realtime','LoadBalancingClient'):({'get_InRoom':0},[]),
 ('Photon.Realtime','RoomInfo'):({'get_CustomProperties':0},[]),
 ('ExitGames.Client.Photon','Hashtable'):({'get_Item':1},[]),
 ('Photon.Voice.Unity','Recorder'):({'StartRecording':0,'get_RecordingEnabled':0,'set_RecordingEnabled':1,'get_TransmitEnabled':0,'set_TransmitEnabled':1,'get_SourceType':0,'set_SourceType':1,'get_AudioClip':0,'set_AudioClip':1,'get_LoopAudioClip':0,'set_LoopAudioClip':1,'get_VoiceDetection':0,'set_VoiceDetection':1},['sourceType','recordingEnabled']),
 ('UnityEngine','AudioClip'):({'Create':6,'SetData':2},[]),
 ('UnityEngine','AudioSource'):({'set_clip':1,'set_loop':1,'set_spatialBlend':1,'Play':0,'Stop':0},[]),
 ('UnityEngine','GameObject'):({'.ctor':0,'AddComponent':1},[]),
 ('UnityEngine','Object'):({'Destroy':1},[]),
 ('Photon.Realtime','Player'):({'get_ActorNumber':0,'get_NickName':0},[]),
 ('Photon.Pun','PhotonView'):({'get_IsMine':0,'get_Owner':0},[])
}
for key,(methods,fields) in VOICE_SPECS.items():
 prior_methods,prior_fields=SPECS.setdefault(key,({},[]));prior_methods.update(methods);prior_fields.extend(f for f in fields if f not in prior_fields)
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def argc(text):
 if not text.strip():return 0
 n=1;depth=0
 for c in text:
  if c in '<([':depth+=1
  elif c in '>)]':depth-=1
  elif c==',' and depth==0:n+=1
 return n
def parse(p):
 out={};ns='';active=None;last_rva=None
 for line in p.read_text(encoding='utf-8-sig').splitlines():
  if line.startswith('// Namespace:'):ns=line.split(':',1)[1].strip()
  if line and not line[0].isspace() and '// TypeDefIndex:' in line:
   match=re.search(r'\b(?:class|struct|enum|interface)\s+(\S+)',line)
   active=(ns,match[1]) if match else None
   if active:out[active]={'methods':{},'fields':{},'parent':line.split(' : ',1)[1].split(' //',1)[0].split(',')[0] if ' : ' in line else None}
  if not active:continue
  rva=re.search(r'// RVA: (0x[0-9A-Fa-f]+|-1)',line)
  if rva:last_rva=rva[1]
  method=re.match(r'^\t.*?\s([\w.<>]+)\((.*)\).*?(?:\{.*\}|;)$',line)
  if method:
   out[active]['methods'].setdefault((method[1],argc(method[2])),[]).append(last_rva);last_rva=None
  field=re.match(r'^\t.*?\s(\S+);\s*//\s*(0x[0-9A-Fa-f]+)',line)
  if field:out[active]['fields'][field[1]]=field[2]
 return out
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--so',type=Path,required=True);ap.add_argument('--dump',type=Path,required=True);ap.add_argument('--metadata',type=Path);ap.add_argument('--nm',required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args()
 b=a.so.read_bytes();assert b[:4]==b'\x7fELF' and b[5]==1
 bits=64;machine=struct.unpack_from('<H',b,18)[0];assert b[4]==2 and machine==183, "Expected an ARM64 ELF"
 classes=parse(a.dump);failures=[];members=[]
 for (ns,name),(methods,fields) in SPECS.items():
  c=classes.get((ns,name))
  if c is None:failures.append('type '+ns+'.'+name);continue
  for method,count in methods.items():
   # Runtime FindMethod searches parents as well.
   current=c;matches=None
   for _ in range(12):
    matches=current['methods'].get((method,count))
    if matches:break
    parent=current.get('parent')
    if not parent:break
    current=classes.get((ns,parent)) or classes.get(('UnityEngine.UI',parent))
    if not current:break
   if not matches:failures.append(f'{ns}.{name}.{method}/{count}')
   members.append({'type':ns+'.'+name,'method':method,'arity':count,'rvas':matches})
  for field in fields:
   value=c['fields'].get(field)
   if value is None:failures.append(f'{ns}.{name}.{field}')
   members.append({'type':ns+'.'+name,'field':field,'offset':value})
 exports={line.split()[-1] for line in subprocess.check_output([a.nm,'-D','--defined-only',str(a.so)],text=True).splitlines() if line.strip()}
 source=Path(__file__).resolve().parents[1]/'src/il2cpp_runtime.cpp'
 api_names=sorted((set(re.findall(r'(?:LOAD|OPTIONAL)\((\w+)\)',source.read_text()))|set(re.findall(r'dlsym\(library, \"il2cpp_(\w+)\"',source.read_text())))-{'name'})
 missing=['il2cpp_'+n for n in api_names if 'il2cpp_'+n not in exports];failures+=missing
 report={'elf_bits':bits,'elf_machine':machine,'so_sha256':digest(a.so),'dump_sha256':digest(a.dump),'members':members,'required_exports':len(api_names),'missing_exports':missing,'failures':failures}
 if a.metadata:
  md=a.metadata.read_bytes();magic,version=struct.unpack_from('<II',md);assert magic==0xfab11baf
  report['metadata_version']=version;report['metadata_sha256']=digest(a.metadata)
 a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n')
 print(f'ARM{bits}: {len(members)} metadata targets, {len(api_names)} exports; failures={len(failures)}')
 for failure in failures:print('MISSING',failure)
 return bool(failures)
if __name__=='__main__':raise SystemExit(main())
