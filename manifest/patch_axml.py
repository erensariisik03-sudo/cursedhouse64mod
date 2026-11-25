import struct,sys
src,dst=sys.argv[1],sys.argv[2]
d=bytearray(open(src,'rb').read())
assert struct.unpack_from('<HH',d,0)==(3,8)
# string pool
t,hs,sz,cnt,sc,flags,ss,sty=struct.unpack_from('<HHIIIIII',d,8)
assert t==1 and not (flags&0x100) and sc==0 and sty==0, (t,flags,sc,sty)
pool=8; pool_end=pool+sz
offs=[struct.unpack_from('<I',d,pool+hs+4*i)[0] for i in range(cnt)]
def getstr(i):
    p=pool+ss+offs[i]; n=struct.unpack_from('<H',d,p)[0]; p+=2
    if n&0x8000: n=((n&0x7fff)<<16)|struct.unpack_from('<H',d,p)[0]; p+=2
    return d[p:p+2*n].decode('utf-16le')
S=[getstr(i) for i in range(cnt)]
NEW="android.intent.action.TTS_SERVICE"
assert NEW not in S
# resource map
rm=pool_end; rt,rhs,rsz=struct.unpack_from('<HHI',d,rm); assert rt==0x180
resids=list(struct.unpack_from('<%dI'%((rsz-8)//4),d,rm+8))
def idx(s,res=None):
    c=[i for i,x in enumerate(S) if x==s and (res is None or (i<len(resids) and resids[i]==res))]
    assert c,(s); return c[0]
i_name=idx('name',0x01010003); i_intent=idx('intent'); i_action=idx('action'); i_queries=idx('queries'); i_ns=idx('http://schemas.android.com/apk/res/android')
# new string appended
raw=struct.pack('<H',len(NEW))+NEW.encode('utf-16le')+b'\0\0'
while len(raw)%4: raw+=b'\0'
data_len=pool_end-(pool+ss)           # existing strings area incl. padding
new_off=data_len
new_idx=cnt
newpool=bytearray(d[pool:pool_end])
# insert offset entry after offsets array
ins=pool+hs+4*cnt-pool
newpool[ins:ins]=struct.pack('<I',new_off)
newpool+=raw
struct.pack_into('<IIII',newpool,8,cnt+1,0,flags,ss+4)   # stringCount, styleCount, flags, stringsStart
struct.pack_into('<I',newpool,4,len(newpool))
rest=bytearray(d[pool_end:])
# find queries END_TAG (direct child of manifest) in rest
p=0; end_at=None; depth=0
while p<len(rest):
    ty,h,s=struct.unpack_from('<HHI',rest,p)
    if ty==0x102:
        name=struct.unpack_from('<I',rest,p+20)[0]
        if name==i_queries: q_depth=depth
        depth+=1
    elif ty==0x103:
        depth-=1
        name=struct.unpack_from('<I',rest,p+20)[0]
        if name==i_queries: end_at=p
    p+=s
assert end_at is not None
def start(name,attrs):
    b=struct.pack('<HHIII',0x102,16,36+20*len(attrs),1,0xffffffff)
    b+=struct.pack('<IIHHHHHH',0xffffffff,name,0x14,0x14,len(attrs),0,0,0)
    for a in attrs: b+=a
    return b
def end(name): return struct.pack('<HHIII',0x103,16,24,1,0xffffffff)+struct.pack('<II',0xffffffff,name)
attr=struct.pack('<III',i_ns,i_name,new_idx)+struct.pack('<HBBI',8,0,3,new_idx)
block=start(i_intent,[])+start(i_action,[attr])+end(i_action)+end(i_intent)
rest[end_at:end_at]=block
out=bytearray(d[:8])+newpool+d[pool_end:pool_end]+rest
# d[pool_end:pool_end] empty; rest already includes resmap and nodes
struct.pack_into('<I',out,4,len(out))
open(dst,'wb').write(out)
print('ok',len(d),'->',len(out),'new string idx',new_idx)
