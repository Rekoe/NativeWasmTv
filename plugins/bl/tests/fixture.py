import http.server,threading,ssl,json,urllib.parse,pathlib,socket,re,time,argparse
args=argparse.ArgumentParser();args.add_argument('--cert',required=True,type=pathlib.Path);args.add_argument('--key',required=True,type=pathlib.Path);args.add_argument('--video',required=True,type=pathlib.Path);options=args.parse_args()
ROOT=pathlib.Path(__file__).resolve().parents[3]
work=ROOT/'.codex-tmp/bl';work.mkdir(parents=True,exist_ok=True)
s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.connect(('1.1.1.1',80));host=s.getsockname()[0];s.close()
base=f'http://{host}:39626'
def video(i=0):return dict(bvid='BV1xx411c7mD' if i==0 else 'BV'+str(1000000000+i),aid=1000+i,cid=2000+i,title=f'BL 原生兼容测试 {i+1:02d}',pic=base+'/cover.png',duration=40,owner=dict(mid=123,name='Native View 测试'),stat=dict(view=123456+i,danmaku=17),desc='本地测试视频，验证单 View 界面、共享 OkHttp、遥控器和旧系统播放。',pages=[dict(cid=2000+i,page=1,part='第一集'),dict(cid=3000+i,page=2,part='第二集')])
class Handler(http.server.BaseHTTPRequestHandler):
 def log_message(self,fmt,*args):pass
 def do_HEAD(self):self.do_GET()
 def response(self,code,data,typ='application/json'):
  if not isinstance(data,bytes):data=json.dumps(data,ensure_ascii=False).encode()
  self.send_response(code);self.send_header('Content-Type',typ);self.send_header('Content-Length',len(data));self.end_headers()
  if self.command!='HEAD':self.wfile.write(data)
 def do_GET(self):
  try:self.handle_get()
  except (BrokenPipeError,ConnectionResetError):pass
 def handle_get(self):
  p=urllib.parse.urlsplit(self.path);q=urllib.parse.parse_qs(p.query);page=int(q.get('pn',q.get('page',q.get('fresh_idx',['1'])))[0]);offset=(page-1)*24
  with (work/'requests.jsonl').open('a',encoding='utf-8') as f:f.write(json.dumps({'time':time.time(),'method':self.command,'path':self.path},ensure_ascii=False)+'\n')
  if p.path=='/bl.jar':return self.response(200,(ROOT/'plugins/bl/dist/bl.jar').read_bytes(),'application/java-archive')
  if p.path=='/cover.png':return self.response(200,(ROOT/'plugins/bl/assets/background.png').read_bytes(),'image/png')
  if p.path=='/video.mp4':
   file=options.video;size=file.stat().st_size;a=0;b=size-1;match=re.match(r'bytes=(\d+)-(\d*)',self.headers.get('Range',''))
   if match:a=int(match[1]);b=min(size-1,int(match[2])) if match[2] else size-1
   if a>=size:return self.response(416,b'')
   self.send_response(206 if match else 200);self.send_header('Content-Type','video/mp4');self.send_header('Accept-Ranges','bytes');self.send_header('Content-Length',b-a+1)
   if match:self.send_header('Content-Range',f'bytes {a}-{b}/{size}')
   self.end_headers()
   if self.command!='HEAD':
    with file.open('rb') as f:
     f.seek(a);left=b-a+1
     while left:
      data=f.read(min(left,32768))
      if not data:break
      self.wfile.write(data);left-=len(data)
   return
  data={};code=0
  if p.path=='/x/frontend/finger/spi':data={'b_3':'native-device-fixture'}
  elif p.path=='/x/web-interface/nav':code=-101;data={'isLogin':False,'wbi_img':{'img_url':'https://i0.hdslb.com/bfs/wbi/'+'a'*32+'.png','sub_url':'https://i0.hdslb.com/bfs/wbi/'+'b'*32+'.png'}}
  elif p.path.endswith('/feed/rcmd'):data={'item':[video(i) for i in range(offset,min(offset+24,72))]}
  elif p.path=='/x/web-interface/popular':data={'list':[video(i) for i in range(offset,min(offset+24,72))]}
  elif p.path=='/x/web-interface/view':data=video(0)
  elif p.path.endswith('/search/type'):data={'result':[video(i+4) for i in range(12)]}
  elif p.path=='/x/web-interface/archive/related':return self.response(200,dict(code=0,data=[video(i+1) for i in range(8)]))
  elif p.path in ['/x/player/playurl','/pgc/player/web/playurl']:data={'quality':int(q.get('qn',['32'])[0]),'format':'mp4','accept_quality':[16,32,64],'durl':[{'url':base+'/video.mp4','length':40000,'size':(options.video).stat().st_size}]}
  elif p.path=='/x/v1/dm/list.so':return self.response(200,b'<i><d p="2,1,25,16777215">Native View danmaku</d><d p="5,1,25,123456">API14 + OkHttp</d></i>','text/xml')
  elif p.path=='/x/player/wbi/v2':data={'subtitle':{'subtitles':[{'lan':'zh-CN','lan_doc':'中文测试字幕','subtitle_url':base+'/subtitle.json'}]}}
  elif p.path=='/subtitle.json':return self.response(200,{'body':[{'from':0,'to':20,'content':'BL 字幕测试'}]})
  elif p.path=='/x/passport-login/web/qrcode/generate':data={'url':'https://passport.bilibili.com/qrcode/h5/login?qrcode_key=fixture','qrcode_key':'fixture'}
  elif p.path=='/x/passport-login/web/qrcode/poll':data={'code':86101,'message':'等待扫码'}
  elif p.path=='/x/v2/reply':data={'replies':[{'member':{'uname':'测试用户'},'content':{'message':'原生评论界面'},'like':12}]}
  else:code=-404;data={}
  self.response(200,dict(code=code,message='fixture',data=data))
 def do_POST(self):
  length=int(self.headers.get('Content-Length','0'));self.rfile.read(length);self.response(200,dict(code=0,data={}))
http_server=http.server.ThreadingHTTPServer(('0.0.0.0',39626),Handler)
https_server=http.server.ThreadingHTTPServer(('0.0.0.0',39627),Handler)
tls=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);tls.minimum_version=ssl.TLSVersion.TLSv1;tls.set_ciphers('ALL:@SECLEVEL=0');tls.load_cert_chain(options.cert,options.key);https_server.socket=tls.wrap_socket(https_server.socket,server_side=True)
threading.Thread(target=http_server.serve_forever,daemon=True).start();print(json.dumps({'host':host,'api':base,'jar':f'https://{host}:39627/bl.jar'}),flush=True);https_server.serve_forever()
