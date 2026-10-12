"""Known YUV patches for HdrDecoderColorInstrumentation, FFmpeg/libx265 required.

Usage: python tools/ijk/generate-hdr-color-fixtures.py [output-directory]
Push colors-all.json and the MP4s to the manifest's app-owned fixture directory.
colors.json contains normal output; colors-partial.json isolates incomplete
output metadata; colors-all.json also checks complete, already-SDR output.
The reference uses quantized source YUV, BT.2100 HLG/PQ and the documented
291-nit SDR adaptation, before conversion to BT.709 display RGB.
"""
import pathlib, json, math, struct, subprocess, sys
root=pathlib.Path(sys.argv[1] if len(sys.argv)>1 else '.codex-tmp/hdr-output').resolve()
root.mkdir(parents=True,exist_ok=True)
colors=[(x,x,x) for x in [0,.0625,.125,.25,.5,.75,.875,1]]+[
    (.65,.30,.15),(.25,.65,.30),(.20,.25,.65),(.65,.65,.20),
    (.70853657,.26654934,.12982403),(.52480767,.74436791,.27190927),
    (.23091570,.11827064,.81335211),(.71215887,.28070973,.77276848)]
def reference(rgb,transfer,standard):
    if standard==1:return [round(max(0,min(1,c))*255) for c in rgb]
    scene=[c*c/3 if c<=.5 else (math.exp((c-.55991073)/.17883277)+.28466892)/12 for c in rgb]
    luma=max(sum(c*w for c,w in zip(scene,[.2627,.678,.0593])),1e-6)
    light=[2.91*c*luma**.03 for c in scene]
    peak=2.91
    if transfer==6:
        absolute=[10000*(max(c**(1/78.84375)-.8359375,0)/max(18.8515625-18.6875*c**(1/78.84375),1e-6))**(1/.1593017578125) for c in rgb]
        luma=max(sum(c*w for c,w in zip(absolute,[.2627,.678,.0593]))/1000,1e-6)
        light=[c*.00291*luma**(1.03/1.2-1) for c in absolute]
    if transfer==3:light=[c**2.4 for c in rgb];peak=1
    matrix=[[1.660491,-.587641,-.072850],[-.124550,1.132900,-.008349],[-.018151,-.100579,1.118730]]
    light=[max(0,sum(c*w for c,w in zip(light,row))) for row in matrix]
    signal=max(light)
    if transfer>5 and signal>.55:
        k=.55;p=peak;a=-k*k*(p-1)/(k*k-2*k+p);b=(k*k-2*k*p+p)/(p-1)
        mapped=min((b+k)**2/(b-a)*(signal+a)/(signal+b),1)
        light=[c*mapped/signal for c in light]
    return [round(max(0,min(1,c))**(1/2.4)*255) for c in light]
manifest=[]
for name,depth,full,transfer,standard in [('hlg8-limited',8,False,7,6),('hlg8-full',8,True,7,6),
        ('hlg10-limited',10,False,7,6),('pq8-limited',8,False,6,6),('pq10-limited',10,False,6,6),
        ('wide-sdr8-limited',8,False,3,6),('sdr8-limited',8,False,3,1)]:
    kr,kb=(.2627,.0593) if standard==6 else (.2126,.0722)
    maximum=(1<<depth)-1;scale=1<<(depth-8)
    yo,ys,uvs=(0,maximum,maximum) if full else (16*scale,219*scale,224*scale)
    uvo=128*scale
    patches=[];expected=[]
    for r,g,b in colors:
        y=kr*r+(1-kr-kb)*g+kb*b;u=(b-y)/(2*(1-kb));v=(r-y)/(2*(1-kr))
        yy,uu,vv=[round(yo+ys*y),round(uvo+uvs*u),round(uvo+uvs*v)]
        patches.append((yy,uu,vv))
        y=(yy-yo)/ys;u=(uu-uvo)/uvs;v=(vv-uvo)/uvs
        r=y+2*(1-kr)*v;b=y+2*(1-kb)*u;g=(y-kr*r-kb*b)/(1-kr-kb)
        expected.append(reference([max(0,min(1,c)) for c in [r,g,b]],transfer,standard))
    data=bytearray()
    for component,w,h in [(0,640,360),(1,320,180),(2,320,180)]:
        for y in range(h):
            for x in range(w):
                value=patches[(y//(h//2))*8+x//(w//8)][component]
                data.extend(struct.pack('<H',value) if depth==10 else bytes([value]))
    raw=root/(name+'.yuv');raw.write_bytes(data)
    pix='yuv420p10le' if depth==10 else 'yuv420p'
    params='lossless=1:repeat-headers=1:keyint=25:colorprim='+('9' if standard==6 else '1')+':transfer='+('18' if transfer==7 else '16' if transfer==6 else '1')+':colormatrix='+('9' if standard==6 else '1')
    params+=':range='+('full' if full else 'limited')
    command=['ffmpeg','-hide_banner','-loglevel','error','-stream_loop','-1','-f','rawvideo','-pixel_format',pix,
        '-video_size','640x360','-framerate','25','-i',str(raw),'-t','2','-c:v','libx265','-preset','ultrafast',
        '-x265-params',params,'-y',str(root/(name+'.mp4'))]
    with open(root/(name+'-encode.log'),'w') as log:subprocess.run(command,stdout=log,stderr=log,check=True)
    manifest.append(dict(name=name,file='/sdcard/Android/data/xiao.bu.tv/files/hdr-output/'+name+'.mp4',
        standard=standard,transfer=transfer,range=1 if full else 2,expected=expected))
(root/'colors.json').write_text(json.dumps(manifest,indent=2))
partial=[]
for transfer in [0,3,1]:
    row=dict(manifest[0]);row.update(name='HLG-partial-output-transfer-'+str(transfer),reportedTransfer=transfer)
    partial.append(row)
all_cases=manifest+partial
for index in [0,2,4,5]:
    row=dict(manifest[index]);row.update(name=row['name']+'-unknown-standard-sdr-tag',reportedTransfer=3,reportedStandard=0)
    all_cases.append(row)
for index in [5,6]:
    row=dict(manifest[index]);row.update(name=row['name']+'-complete-sdr-output',bridgeTransfer=7)
    all_cases.append(row)
(root/'colors-partial.json').write_text(json.dumps(partial,indent=2))
(root/'colors-all.json').write_text(json.dumps(all_cases,indent=2))
