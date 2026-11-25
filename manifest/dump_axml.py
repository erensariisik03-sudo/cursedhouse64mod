import struct,sys
d=open(sys.argv[1],'rb').read()
assert struct.unpack_from('<HHI',d,0)==(3,8,len(d)),'bad header/size'
t,hs,sz,cnt,sc,fl,ss,sty=struct.unpack_from('<HHIIIIII',d,8)
def gs(i):
    p=8+ss+struct.unpack_from('<I',d,8+hs+4*i)[0]; n=struct.unpack_from('<H',d,p)[0]
    return d[p+2:p+2+2*n].decode('utf-16le')
p=8+sz; rt,_,rsz=struct.unpack_from('<HHI',d,p); p+=rsz
depth=0; show=False; tags=0
while p<len(d):
    ty,h,s=struct.unpack_from('<HHI',d,p); assert s>=8 and p+s<=len(d)
    if ty==0x102:
        ns,name,_,_,ac=struct.unpack_from('<IIHHH',d,p+16)
        nm=gs(name); tags+=1
        if nm=='queries': show=True
        if show:
            at=[]
            for k in range(ac):
                a=p+36+20*k; an,av,_,_,ty2,dat=struct.unpack_from('<IIHBBI',d,a+4)
                at.append((gs(an),gs(av) if av!=0xffffffff else dat))
            print('  '*depth+'<'+nm,at)
        depth+=1
    elif ty==0x103:
        depth-=1; nm=gs(struct.unpack_from('<I',d,p+20)[0])
        if show and nm=='queries': show=False
    p+=s
assert p==len(d) and depth==0; print('structure OK, tags',tags,'strings',cnt)
