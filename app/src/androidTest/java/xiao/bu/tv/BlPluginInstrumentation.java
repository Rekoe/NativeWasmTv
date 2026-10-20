package xiao.bu.tv;

import android.app.*;
import android.content.*;
import android.os.*;
import android.view.*;
import android.widget.*;
import android.graphics.*;
import java.io.*;
import java.lang.reflect.*;
import java.util.concurrent.*;
import org.json.*;

/** Real host + downloaded JAR + deterministic API/MP4 fixtures. Compatible with API 14. */
public final class BlPluginInstrumentation extends Instrumentation {
    private Bundle args;private volatile MainActivity activity;private JarChannelRuntime runtime;private Object plugin;private View root;
    public void onCreate(Bundle args){super.onCreate(args);this.args=args;start();}
    public void callActivityOnResume(Activity a){super.callActivityOnResume(a);if(a instanceof MainActivity){a.getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON|WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED|WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD|WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON);activity=(MainActivity)a;}}
    private static Object field(Object o,String name)throws Exception{Class<?> c=o.getClass();while(c!=null){try{Field f=c.getDeclaredField(name);f.setAccessible(true);return f.get(o);}catch(NoSuchFieldException ignored){c=c.getSuperclass();}}throw new NoSuchFieldException(name);}
    private static void check(boolean condition,String message){if(!condition)throw new AssertionError(message);}
    private String route()throws Exception{return String.valueOf(field(field(plugin,"route"),"kind"));}
    private void waitRoute(String kind)throws Exception{long end=SystemClock.elapsedRealtime()+15000;while(SystemClock.elapsedRealtime()<end){if(route().equals(kind))return;SystemClock.sleep(100);}throw new AssertionError("Expected route "+kind+", got "+route());}
    private void waitItems()throws Exception{long end=SystemClock.elapsedRealtime()+30000;while(SystemClock.elapsedRealtime()<end){if(((java.util.List<?>)field(plugin,"items")).size()>0)return;SystemClock.sleep(100);}throw new AssertionError("No cards loaded");}
    private static Button findButton(View node,String title){Button result=null;if(node instanceof Button&&((Button)node).getText().toString().equals(title))result=(Button)node;if(node instanceof ViewGroup){ViewGroup group=(ViewGroup)node;for(int i=0;i<group.getChildCount();i++){Button found=findButton(group.getChildAt(i),title);if(found!=null)result=found;}}return result;}
    private static EditText findEdit(View node){if(node instanceof EditText)return (EditText)node;if(node instanceof ViewGroup){ViewGroup g=(ViewGroup)node;for(int i=0;i<g.getChildCount();i++){EditText e=findEdit(g.getChildAt(i));if(e!=null)return e;}}return null;}
    private static boolean containsText(View node,String text){if(node instanceof TextView&&((TextView)node).getText().toString().contains(text))return true;if(node instanceof ViewGroup){ViewGroup g=(ViewGroup)node;for(int i=0;i<g.getChildCount();i++)if(containsText(g.getChildAt(i),text))return true;}return false;}
    private void click(String title)throws Exception{
        final Button[] target=new Button[1];long end=SystemClock.elapsedRealtime()+15000;
        do{runOnMainSync(new Runnable(){public void run(){target[0]=findButton(root,title);}});if(target[0]!=null&&target[0].isShown())break;SystemClock.sleep(100);}while(SystemClock.elapsedRealtime()<end);
        check(target[0]!=null&&target[0].isShown(),"Button missing: "+title);
        int[] point=new int[2];runOnMainSync(new Runnable(){public void run(){target[0].getLocationOnScreen(point);}});float x=point[0]+target[0].getWidth()/2f,y=point[1]+target[0].getHeight()/2f;
        long now=SystemClock.uptimeMillis();MotionEvent down=MotionEvent.obtain(now,now,MotionEvent.ACTION_DOWN,x,y,0),up=MotionEvent.obtain(now,now+60,MotionEvent.ACTION_UP,x,y,0);sendPointerSync(down);SystemClock.sleep(60);sendPointerSync(up);down.recycle();up.recycle();SystemClock.sleep(150);
    }
    private void key(int code){sendKeyDownUpSync(code);SystemClock.sleep(200);}
    private void screenshot(final String name)throws Exception{runOnMainSync(new Runnable(){public void run(){Bitmap bitmap=Bitmap.createBitmap(root.getWidth(),root.getHeight(),Bitmap.Config.RGB_565);try{root.draw(new Canvas(bitmap));try(FileOutputStream out=new FileOutputStream(new File(activity.getExternalFilesDir(null),name))){bitmap.compress(Bitmap.CompressFormat.PNG,100,out);}}catch(Exception e){throw new RuntimeException(e);}finally{bitmap.recycle();}}});}
    private void trustFixture()throws Exception{
        String path=args.getString("cert");if(path==null)return;
        java.security.cert.Certificate cert;try(InputStream in=new FileInputStream(path)){cert=java.security.cert.CertificateFactory.getInstance("X.509").generateCertificate(in);}
        java.security.KeyStore keys=java.security.KeyStore.getInstance(java.security.KeyStore.getDefaultType());keys.load(null);keys.setCertificateEntry("fixture",cert);
        javax.net.ssl.TrustManagerFactory managers=javax.net.ssl.TrustManagerFactory.getInstance(javax.net.ssl.TrustManagerFactory.getDefaultAlgorithm());managers.init(keys);
        javax.net.ssl.X509TrustManager trust=(javax.net.ssl.X509TrustManager)managers.getTrustManagers()[0];javax.net.ssl.SSLContext tls=javax.net.ssl.SSLContext.getInstance("TLS");tls.init(null,new javax.net.ssl.TrustManager[]{trust},null);
        okhttp3.OkHttpClient client=NetworkClient.sharedClient().newBuilder().sslSocketFactory(tls.getSocketFactory(),trust).hostnameVerifier(new javax.net.ssl.HostnameVerifier(){public boolean verify(String host,javax.net.ssl.SSLSession session){return host.equals(args.getString("fixtureHost"));}}).build();
        for(String name:new String[]{"client","factory"}){Field f=NetworkClient.class.getDeclaredField(name);f.setAccessible(true);f.set(null,name.equals("client")?client:new okhttp3.OkUrlFactory(client));}GithubProxy.setEnabled(false);
    }
    public void onStart(){Bundle result=new Bundle();int code=0;long start=SystemClock.elapsedRealtime();
        try{
            NetworkClient.initialize(getTargetContext());trustFixture();getTargetContext().getSharedPreferences("ntv-bl",0).edit().putBoolean("systemPlayer","true".equals(args.getString("system"))).apply();
            getTargetContext().startActivity(new Intent(getTargetContext(),MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));long end=SystemClock.elapsedRealtime()+30000;while(activity==null&&SystemClock.elapsedRealtime()<end)SystemClock.sleep(50);check(activity!=null,"Activity did not resume");SystemClock.sleep(2500);
            runOnMainSync(new Runnable(){public void run(){try{
                Channel channel=new Channel("1","BL 单 View","",args.getString("source"),null,null);ChannelCatalog.GROUPS=new ChannelCatalog.Group[]{new ChannelCatalog.Group("视频",ChannelCatalog.SOURCE_CUSTOM,new Channel[]{channel})};
                for(String name:new String[]{"currentGroupIndex","currentChannelIndex","currentSourceIndex"}){Field f=MainActivity.class.getDeclaredField(name);f.setAccessible(true);f.setInt(activity,0);}Method launch=MainActivity.class.getDeclaredMethod("startChannel",int.class);launch.setAccessible(true);launch.invoke(activity,0);
            }catch(Exception e){throw new RuntimeException(e);}}});
            runtime=(JarChannelRuntime)field(activity,"jarChannelRuntime");end=SystemClock.elapsedRealtime()+60000;while(SystemClock.elapsedRealtime()<end){plugin=field(runtime,"plugin");if(plugin!=null)break;SystemClock.sleep(100);}check(plugin!=null,"JAR plugin did not load");root=(View)field(runtime,"view");waitItems();
            end=SystemClock.elapsedRealtime()+15000;while((root.getWidth()==0||root.getHeight()==0||!root.isShown())&&SystemClock.elapsedRealtime()<end)SystemClock.sleep(100);check(root.getWidth()>0&&root.getHeight()>0&&root.isShown(),"Native root did not become visible");
            check(field(runtime,"browser")==null,"Native channel created a WebView");check(field(field(plugin,"api"),"client")==NetworkClient.sharedClient(),"Plugin did not reuse host OkHttp");screenshot("bl-home.png");
            result.putLong("firstCardsMs",SystemClock.elapsedRealtime()-start);result.putInt("sdk",Build.VERSION.SDK_INT);
            Bundle focusStatus=new Bundle();View initialFocus=root.findFocus();focusStatus.putString("stream","BL_FOCUS "+(initialFocus==null?"null":initialFocus.getClass().getName()+":"+initialFocus.getContentDescription())+"\n");sendStatus(0,focusStatus);
            key(KeyEvent.KEYCODE_DPAD_RIGHT);key(KeyEvent.KEYCODE_DPAD_CENTER);waitRoute("detail");click("播放");waitRoute("player");
            end=SystemClock.elapsedRealtime()+30000;Object player=null;while(SystemClock.elapsedRealtime()<end){player=field(plugin,"player");if(player!=null&&"playing".equals(field(player,"state")))break;SystemClock.sleep(100);}check(player!=null&&"playing".equals(field(player,"state")),"Player did not prepare");
            final Object runningPlayer=player;Method position=player.getClass().getDeclaredMethod("position");position.setAccessible(true);SystemClock.sleep(1500);long p1=((Number)position.invoke(player)).longValue();SystemClock.sleep(800);long p2=((Number)position.invoke(player)).longValue();check(p2>p1,"Playback clock did not advance");
            Object engine=field(player,"engine");result.putString("engine",engine.getClass().getName());result.putLong("playbackMs",p2);
            Bundle playing=new Bundle();playing.putString("stream","BL_PLAYING engine="+engine.getClass().getName()+" position="+p2+"\n");sendStatus(0,playing);
            key(KeyEvent.KEYCODE_MEDIA_PAUSE);check("paused".equals(field(player,"state")),"Remote pause failed");key(KeyEvent.KEYCODE_MEDIA_PLAY);check("playing".equals(field(player,"state")),"Remote play failed");
            click("画质");check(field(plugin,"modal")!=null,"Quality panel missing");key(KeyEvent.KEYCODE_BACK);check(field(plugin,"modal")==null&&field(plugin,"player")==runningPlayer,"BACK did not dismiss only the in-View panel");
            if(!"true".equals(args.getString("real"))){
                long beforeSeek=((Number)position.invoke(player)).longValue();key(KeyEvent.KEYCODE_MEDIA_FAST_FORWARD);end=SystemClock.elapsedRealtime()+8000;while(SystemClock.elapsedRealtime()<end&&((Number)position.invoke(player)).longValue()<beforeSeek+5000)SystemClock.sleep(100);check(((Number)position.invoke(player)).longValue()>=beforeSeek+5000,"Remote fast forward failed");key(KeyEvent.KEYCODE_MEDIA_REWIND);SystemClock.sleep(800);
                click("画质");key(KeyEvent.KEYCODE_DPAD_CENTER);end=SystemClock.elapsedRealtime()+15000;
                while(SystemClock.elapsedRealtime()<end&&(field(player,"engine")==engine||!"playing".equals(field(player,"state"))))SystemClock.sleep(100);
                check(field(player,"engine")!=engine&&"playing".equals(field(player,"state")),"Quality switch did not replace and start the decoder");
                Object beforePart=field(player,"engine");click("选集");key(KeyEvent.KEYCODE_DPAD_DOWN);key(KeyEvent.KEYCODE_DPAD_CENTER);end=SystemClock.elapsedRealtime()+15000;
                while(SystemClock.elapsedRealtime()<end&&((Number)field(plugin,"cid")).longValue()!=3000)SystemClock.sleep(100);check(((Number)field(plugin,"cid")).longValue()==3000,"Part selection failed");
                end=SystemClock.elapsedRealtime()+15000;while(SystemClock.elapsedRealtime()<end&&(field(player,"engine")==beforePart||!"playing".equals(field(player,"state"))))SystemClock.sleep(100);check(field(player,"engine")!=null&&field(player,"engine")!=beforePart&&"playing".equals(field(player,"state")),"New part did not prepare");
                click("字幕");end=SystemClock.elapsedRealtime()+10000;while(SystemClock.elapsedRealtime()<end&&field(plugin,"modal")==null)SystemClock.sleep(100);key(KeyEvent.KEYCODE_DPAD_DOWN);key(KeyEvent.KEYCODE_DPAD_CENTER);
                end=SystemClock.elapsedRealtime()+10000;while(SystemClock.elapsedRealtime()<end&&((JSONArray)field(player,"captions")).length()==0)SystemClock.sleep(100);check(((JSONArray)field(player,"captions")).length()>0,"Subtitle did not load");
                runOnMainSync(new Runnable(){public void run(){((xiao.bu.tv.jar.ChannelPlugin.NativePage)plugin).setActive(false);}});SystemClock.sleep(500);check(!((tv.danmaku.ijk.media.player.IMediaPlayer)field(player,"engine")).isPlaying(),"Background did not pause");
                runOnMainSync(new Runnable(){public void run(){((xiao.bu.tv.jar.ChannelPlugin.NativePage)plugin).setActive(true);}});SystemClock.sleep(500);check(((tv.danmaku.ijk.media.player.IMediaPlayer)field(player,"engine")).isPlaying(),"Foreground did not resume");
            }
            key(KeyEvent.KEYCODE_BACK);waitRoute("detail");key(KeyEvent.KEYCODE_BACK);waitRoute("home");check(field(runningPlayer,"engine")==null,"Player was not released on return");waitItems();
            if(!"true".equals(args.getString("real"))){Object proxy=field(plugin,"proxy");Method url=proxy.getClass().getDeclaredMethod("url",String.class);url.setAccessible(true);String upstream=UriOrigin(args.getString("source"))+"/video.mp4",local=(String)url.invoke(proxy,upstream);okhttp3.OkHttpClient client=NetworkClient.sharedClient();
                try(okhttp3.Response r=client.newCall(new okhttp3.Request.Builder().url(local).header("Range","bytes=0-31").build()).execute()){check(r.code()==206&&r.body().bytes().length==32&&r.header("Content-Range").startsWith("bytes 0-31/"),"Media proxy Range failed");}
                try(okhttp3.Response r=client.newCall(new okhttp3.Request.Builder().url(local).head().build()).execute()){check(r.code()==200&&Long.parseLong(r.header("Content-Length"))>0,"Media proxy HEAD failed");}
                Method playlist=proxy.getClass().getDeclaredMethod("playlist",String.class,String.class);playlist.setAccessible(true);String rewritten=(String)playlist.invoke(proxy,"#EXTM3U\n#EXT-X-KEY:METHOD=AES-128,URI=\"key.bin\"\n#EXT-X-MAP:URI=\"init.mp4\"\nsegment.ts\n#EXT-X-ENDLIST\n",upstream);check(rewritten.contains("URI=\"http://127.0.0.1:")&&!rewritten.contains("key.bin")&&!rewritten.contains("segment.ts")&&!rewritten.contains("init.mp4"),"HLS URI rewrite failed");
            }
            click("搜索");waitRoute("search");runOnMainSync(new Runnable(){public void run(){EditText input=findEdit(root);check(input!=null,"Search input missing");input.setText("true".equals(args.getString("real"))?"哔哩哔哩":"BL test");}});click("搜索");waitRoute("results");
            try{waitItems();result.putString("search","cards");}catch(AssertionError noCards){if(!"true".equals(args.getString("real")))throw noCards;if(containsText(root,"要求验证"))result.putString("search","site_verification_required");else if(containsText(root,"暂无内容"))result.putString("search","no_public_results");else throw noCards;}screenshot("bl-search.png");
            click("我的");waitRoute("mine");key(KeyEvent.KEYCODE_DPAD_CENTER);waitRoute("history");waitItems();screenshot("bl-history.png");
            result.putLong("javaHeapBytes",Runtime.getRuntime().totalMemory()-Runtime.getRuntime().freeMemory());result.putLong("nativeHeapBytes",Debug.getNativeHeapAllocatedSize());
            key(KeyEvent.KEYCODE_BACK);waitRoute("mine");for(int n=0;n<4;n++)key(KeyEvent.KEYCODE_DPAD_DOWN);key(KeyEvent.KEYCODE_DPAD_CENTER);waitRoute("login");end=SystemClock.elapsedRealtime()+15000;while(!containsText(root,"扫码")&&SystemClock.elapsedRealtime()<end)SystemClock.sleep(100);SystemClock.sleep(3000);check(containsText(root,"等待扫码")||containsText(root,"使用哔哩"),"QR encoding or polling failed");screenshot("bl-login.png");
            key(KeyEvent.KEYCODE_BACK);waitRoute("mine");key(KeyEvent.KEYCODE_BACK);SystemClock.sleep(100);key(KeyEvent.KEYCODE_BACK);check(!runtime.visible(),"Root double BACK did not close the native channel");check(!activity.isFinishing(),"Plugin navigation closed the host Activity");
            check(((java.util.concurrent.ThreadPoolExecutor)field(plugin,"jobs")).isShutdown(),"Worker executor leaked after dispose");check(((java.util.Set<?>)field(field(plugin,"api"),"pending")).isEmpty(),"HTTP calls remained after dispose");
            check(((java.util.concurrent.ThreadPoolExecutor)field(plugin,"jobs")).awaitTermination(5,TimeUnit.SECONDS),"API worker threads did not terminate");
            check(((java.util.concurrent.ThreadPoolExecutor)field(field(plugin,"images"),"workers")).awaitTermination(5,TimeUnit.SECONDS),"Image workers did not terminate");
            check(((java.util.concurrent.ThreadPoolExecutor)field(field(plugin,"proxy"),"workers")).awaitTermination(5,TimeUnit.SECONDS),"Media proxy workers did not terminate");check(((java.net.ServerSocket)field(field(plugin,"proxy"),"server")).isClosed(),"Proxy server port remained open");
            result.putString("stream","PASS: native JAR, shared host OkHttp, remote navigation, video decode/time advance, pause/resume, panel BACK, playback release, search, local history and root exit; "+result.toString());code=-1;
        }catch(Throwable error){result.putString("stream",android.util.Log.getStackTraceString(error));try{if(root!=null&&root.getWidth()>0&&root.getHeight()>0)screenshot("bl-error.png");}catch(Exception ignored){}}
        finally{if(activity!=null)runOnMainSync(new Runnable(){public void run(){activity.finish();}});}finish(code,result);
    }
    private static String UriOrigin(String source){return android.net.Uri.parse(source).getQueryParameter("apiOrigin");}
}
