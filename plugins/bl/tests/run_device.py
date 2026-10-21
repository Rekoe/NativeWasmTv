"""Run BlPluginInstrumentation on an explicitly selected device and test package."""
import argparse,pathlib,shlex,socket,subprocess,time,urllib.parse,threading
p=argparse.ArgumentParser();p.add_argument('--adb',default='adb');p.add_argument('--serial',required=True)
p.add_argument('--package',required=True,help='Separate test host package, e.g. xiao.bu.tv.bltest')
p.add_argument('--host-apk',required=True);p.add_argument('--test-apk',required=True)
p.add_argument('--real',action='store_true',help='Use official Bilibili APIs (JAR still comes from local HTTPS fixture)')
p.add_argument('--system',action='store_true',help='Exercise the API-14 platform player adapter')
p.add_argument('--dash',action='store_true',help='Exercise separate DASH video/audio merge on API 16+')
p.add_argument('--smoke',action='store_true',help='Initial playback, navigation and live danmaku; skip decoder-switch stress tests')
p.add_argument('--jar-url',help='Optional published HTTPS JAR instead of the local HTTPS fixture')
a=p.parse_args();root=pathlib.Path(__file__).resolve().parents[3];adb=[a.adb,'-s',a.serial]
def run(*args,**kw):return subprocess.run(adb+list(args),check=True,**kw)
for apk in [a.host_apk,a.test_apk]:run('install','-r',apk)
run('shell','input','keyevent','224')
subprocess.run(adb+['shell','wm','dismiss-keyguard'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.connect(('1.1.1.1',80));host=s.getsockname()[0];s.close()
sha=(root/'plugins/bl/dist/bl.sha256').read_text().strip();params={'sha256':sha}
if not a.real:
    for key in ['apiOrigin','passportOrigin','liveOrigin']:params[key]='http://'+host+':39626'
source=(a.jar_url or 'https://'+host+':39627/bl.jar')+'?'+urllib.parse.urlencode(params)
command='am instrument -w -e source '+shlex.quote(source)
if a.real:command+=' -e real true'
if a.system:command+=' -e system true'
if a.dash:command+=' -e dash true'
if a.smoke:command+=' -e smoke true'
command+=' '+a.package+'.test/xiao.bu.tv.BlPluginInstrumentation'
out=root/'output/bl/tests'/(a.serial+('-real' if a.real else '-fixture')+('-system' if a.system else '-dash' if a.dash else '-ijk')+('-smoke' if a.smoke else ''));out.mkdir(parents=True,exist_ok=True)
for name in ['bl-playing','bl-controls','bl-controls-screen','bl-home','bl-search','bl-history','bl-login','bl-settings','bl-episodes','bl-danmaku-settings','bl-live','bl-live-screen','bl-error']:(out/(name+'.png')).unlink(missing_ok=True)
process=subprocess.Popen(adb+['shell',command],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding='utf-8',errors='replace')
lines=[]
def collect():
    for line in process.stdout:
        lines.append(line)
        if 'BL_PLAYING engine=' in line:
            try:
                remote='/sdcard/Android/data/'+a.package+'/files/bl-playing.png'
                subprocess.run(adb+['shell','screencap','-p',remote],check=True,timeout=20,stdout=subprocess.DEVNULL)
                subprocess.run(adb+['pull',remote,str(out/'bl-playing.png')],check=True,timeout=20,stdout=subprocess.DEVNULL)
            except subprocess.SubprocessError:pass
        if 'BL_CAPTURE bl-controls.png' in line:
            remote='/sdcard/Android/data/'+a.package+'/files/bl-controls-screen.png'
            subprocess.run(adb+['shell','screencap','-p',remote],timeout=20,stdout=subprocess.DEVNULL)
            subprocess.run(adb+['pull',remote,str(out/'bl-controls-screen.png')],timeout=20,stdout=subprocess.DEVNULL)
        if 'BL_CAPTURE bl-live.png' in line:
            remote='/sdcard/Android/data/'+a.package+'/files/bl-live-screen.png'
            subprocess.run(adb+['shell','screencap','-p',remote],timeout=20,stdout=subprocess.DEVNULL)
            subprocess.run(adb+['pull',remote,str(out/'bl-live-screen.png')],timeout=20,stdout=subprocess.DEVNULL)
reader=threading.Thread(target=collect,daemon=True);reader.start()
time.sleep(2);run('shell','am','start','-W','-n',a.package+'/xiao.bu.tv.MainActivity')
subprocess.run(adb+['shell','wm','dismiss-keyguard'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
try:process.wait(timeout=240)
except subprocess.TimeoutExpired:process.kill();raise
reader.join(25);text=''.join(lines);print(text)
(out/'result.txt').write_text(text,encoding='utf-8')
images=['bl-home.png','bl-search.png','bl-history.png','bl-login.png','bl-settings.png','bl-live.png']+([] if a.real or a.smoke else ['bl-episodes.png','bl-danmaku-settings.png','bl-controls.png']) if 'PASS:' in text else ['bl-error.png']
for name in images:subprocess.run(adb+['pull','/sdcard/Android/data/'+a.package+'/files/'+name,str(out/name)])
raise SystemExit(0 if 'PASS:' in text else 1)
