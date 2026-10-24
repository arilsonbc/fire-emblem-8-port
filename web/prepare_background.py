from pathlib import Path
import re,hashlib,json
root=Path(__file__).resolve().parent.parent
rom=(root/'baserom.gba').read_bytes()
assert hashlib.sha1(rom).hexdigest()=='c25b145e37456171ada4b0d440bf88a19f4d509f'
symbols=(root/'fireemblem8.map').read_text()
base=int(re.search(r'(0x[0-9a-f]+)\s+gConvoBackgroundData\b',symbols)[1],16)-0x8000000
def unpack(a):
 p=a-0x8000000;assert rom[p]==16
 size=int.from_bytes(rom[p+1:p+4],'little');p+=4;out=bytearray()
 while len(out)<size:
  flags=rom[p];p+=1
  for bit in range(7,-1,-1):
   if len(out)>=size:break
   if flags&(1<<bit):
    a,b=rom[p:p+2];p+=2;distance=((a&15)<<8|b)+1;assert distance<=len(out)
    for _ in range((a>>4)+3):
     if len(out)<size:out.append(out[-distance])
   else:out.append(rom[p]);p+=1
 return bytes(out)
parts=[];rows=[];manifest=[]
for ident in [0,2,4,5,12,14,16,22,24,26,28,29,30,32,33,34,35,36,38,40,41]:
 pointers=[int.from_bytes(rom[base+ident*12+i:base+ident*12+i+4],'little') for i in [0,4,8]]
 gfx=unpack(pointers[0]);p=pointers[1]-0x8000000;w,h=rom[p]+1,rom[p+1]+1
 assert 30<=w<=32 and 20<=h<=32
 tsa=rom[p+2:p+2+w*h*2];pal=rom[pointers[2]-0x8000000:pointers[2]-0x8000000+256]
 for label,data,word in [('gfx',gfx,False),('tsa',tsa,True),('pal',pal,True)]:
  values=[int.from_bytes(data[i:i+2],'little') for i in range(0,len(data),2)] if word else list(data)
  parts.append('static const '+('u16' if word else 'u8')+' bg_'+label+str(ident)+'[]={'+','.join(map(str,values))+'};')
 rows.append('{'+','.join(map(str,[ident,w,h,len(gfx)]))+',bg_gfx'+str(ident)+',bg_tsa'+str(ident)+',bg_pal'+str(ident)+'}')
 manifest.append({'id':ident,'width':w,'height':h,'gfxBytes':len(gfx),'pointers':pointers,'sha256':hashlib.sha256(gfx+tsa+pal).hexdigest()})
parts.append('static const struct BackgroundAsset backgrounds[]={'+','.join(rows)+'};')
(root/'web/generated/background_assets.h').write_text('\n'.join(parts))
(root/'web/generated/background_assets.json').write_text(json.dumps(manifest,indent=2))
print(f'Backgrounds: {len(manifest)} original conversation backgrounds, bounded 4bpp/TSA/palette resources')
