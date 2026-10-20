package xiao.bu.channel.bl;

import android.os.*;
import android.view.*;
import android.widget.*;
import android.media.AudioManager;
import java.util.*;
import org.json.*;
import tv.danmaku.ijk.media.player.*;

/** Single SurfaceView player using the host's IJK libraries; API 14/15 uses an API-14 platform adapter. */
final class BlPlayer extends FrameLayout implements SurfaceHolder.Callback,BlDanmaku.Clock {
    interface Actions{void quality();void episodes();void comments();void subtitle();void complete();void failure(String error);}
    private final BlUi ui;private final BlMediaProxy proxy;private final Actions actions;
    private final SurfaceView surface;private final LinearLayout controls;private final TextView time,status,subtitle;
    private final Button toggle;private final SeekBar seek;
    final BlDanmaku danmaku;
    private IMediaPlayer engine;
    private JSONArray streams=new JSONArray();
    private int segment,width,height;private long segmentStart,position,seekWhenReady;
    private final boolean systemPlayer;
    private boolean prepared,closed,active=true,resumeAfterBackground,seeking,hardware=true,hardwareFailed;
    private String source="";
    private final Handler main=new Handler(Looper.getMainLooper());
    private JSONArray captions=new JSONArray();
    volatile String state="idle";
    private final Runnable hide=new Runnable(){public void run(){controls.setVisibility(GONE);}};
    private final Runnable tick=new Runnable(){public void run(){if(closed||!active)return;long at=position(),length=duration();time.setText(BlUi.time(at)+" / "+BlUi.time(length));if(!seeking&&length>0)seek.setProgress((int)Math.min(1000,at*1000/length));toggle.setText(running()?"暂停":"播放");updateSubtitle(at);main.postDelayed(this,500);}};
    BlPlayer(BlUi ui,BlMediaProxy proxy,String title,boolean hardware,boolean systemPlayer,Actions actions){
        super(ui.context);this.ui=ui;this.proxy=proxy;this.actions=actions;this.hardware=hardware;this.systemPlayer=systemPlayer||Build.VERSION.SDK_INT<16;setBackgroundColor(0xff000000);setContentDescription("BL播放器");
        surface=new SurfaceView(ui.context);surface.setZOrderMediaOverlay(true);surface.getHolder().addCallback(this);surface.getHolder().setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);addView(surface,new FrameLayout.LayoutParams(-1,-1,Gravity.CENTER));
        danmaku=new BlDanmaku(ui,this);addView(danmaku,new FrameLayout.LayoutParams(-1,-1));
        TextView heading=ui.text(title,23,BlUi.TEXT);heading.setPadding(ui.px(24),ui.px(18),ui.px(24),ui.px(18));heading.setBackgroundColor(0x77000000);addView(heading,new FrameLayout.LayoutParams(-1,ui.px(70),Gravity.TOP));
        status=ui.text("正在准备播放…",21,BlUi.TEXT);status.setGravity(Gravity.CENTER);addView(status,new FrameLayout.LayoutParams(-1,ui.px(80),Gravity.CENTER));
        subtitle=ui.text("",24,0xffffffff);subtitle.setGravity(Gravity.CENTER);subtitle.setShadowLayer(3,1,1,0xff000000);FrameLayout.LayoutParams sub=new FrameLayout.LayoutParams(-1,ui.px(90),Gravity.BOTTOM);sub.bottomMargin=ui.px(120);addView(subtitle,sub);
        controls=ui.vertical();controls.setPadding(ui.px(22),ui.px(10),ui.px(22),ui.px(16));controls.setBackgroundColor(0xdd181818);
        LinearLayout progress=ui.horizontal();time=ui.text("00:00 / 00:00",16,BlUi.TEXT);progress.addView(time,new LinearLayout.LayoutParams(ui.px(170),ui.px(34)));seek=new SeekBar(ui.context);seek.setMax(1000);seek.setContentDescription("播放进度");progress.addView(seek,new LinearLayout.LayoutParams(0,ui.px(40),1));controls.addView(progress);
        LinearLayout buttons=ui.horizontal();toggle=ui.button("暂停");buttons.addView(toggle);toggle.setOnClickListener(new OnClickListener(){public void onClick(View v){toggle();}});
        add(buttons,"快退",new Runnable(){public void run(){seekTo(position()-10000);}});add(buttons,"快进",new Runnable(){public void run(){seekTo(position()+10000);}});
        add(buttons,"选集",new Runnable(){public void run(){actions.episodes();}});add(buttons,"画质",new Runnable(){public void run(){actions.quality();}});
        add(buttons,"弹幕",new Runnable(){boolean shown=true;public void run(){shown=!shown;danmaku.enabled(shown);}});
        add(buttons,"字幕",new Runnable(){public void run(){actions.subtitle();}});add(buttons,"评论",new Runnable(){public void run(){actions.comments();}});
        controls.addView(buttons);addView(controls,new FrameLayout.LayoutParams(-1,-2,Gravity.BOTTOM));
        seek.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){public void onStartTrackingTouch(SeekBar s){seeking=true;main.removeCallbacks(hide);}public void onStopTrackingTouch(SeekBar s){seeking=false;seekTo(duration()*s.getProgress()/1000);showControls();}public void onProgressChanged(SeekBar s,int p,boolean user){if(user)time.setText(BlUi.time(duration()*p/1000)+" / "+BlUi.time(duration()));}});
        setOnTouchListener(new OnTouchListener(){public boolean onTouch(View v,MotionEvent e){if(e.getAction()==MotionEvent.ACTION_UP){showControls();return true;}return true;}});
    }
    private void add(LinearLayout row,String label,final Runnable action){ui.gap(row,8);Button button=ui.button(label);row.addView(button);button.setOnClickListener(new OnClickListener(){public void onClick(View v){showControls();action.run();}});}
    void play(JSONArray source,long at){if(closed)return;releaseEngine();streams=source;segment=-1;seekTo(Math.max(0,at));}
    void setSubtitle(JSONArray value){captions=value;}
    private void updateSubtitle(long at){String text="";for(int i=0;i<captions.length();i++){JSONObject c=captions.optJSONObject(i);if(c!=null&&c.optDouble("from")*1000<=at&&c.optDouble("to")*1000>at){text=c.optString("content");break;}}subtitle.setText(text);}
    long duration(){long total=0;for(int i=0;i<streams.length();i++){JSONObject d=streams.optJSONObject(i);if(d!=null)total+=d.optLong("length");}if(total==0&&engine!=null&&prepared)try{total=engine.getDuration();}catch(Exception ignored){}return total;}
    public long position(){if(engine!=null&&prepared)try{return segmentStart+engine.getCurrentPosition();}catch(Exception ignored){}return position;}
    public boolean running(){if(engine==null||!prepared||!active||closed)return false;try{return engine.isPlaying();}catch(Exception e){return false;}}
    void seekTo(long target){
        if(streams.length()==0)return;long total=duration();if(total>0)target=Math.min(target,total-1);target=Math.max(0,target);int index=0;long start=0;
        for(int i=0;i<streams.length()-1;i++){long length=streams.optJSONObject(i).optLong("length");if(length<=0||target<start+length)break;start+=length;index++;}
        position=target;if(index==segment&&engine!=null&&prepared){try{engine.seekTo(target-start);}catch(Exception ignored){}return;}
        segment=index;segmentStart=start;seekWhenReady=target-start;source=streams.optJSONObject(index).optString("url");start();
    }
    private void start(){
        if(closed||source.isEmpty()||!surface.getHolder().getSurface().isValid())return;
        releaseEngine();state="preparing";status.setText("正在加载…");status.setVisibility(VISIBLE);
        try{
            IMediaPlayer p;
            if(!systemPlayer){IjkMediaPlayer.loadLibrariesOnce(null);IjkMediaPlayer ijk=new IjkMediaPlayer();ijk.setOption(IjkMediaPlayer.OPT_CATEGORY_PLAYER,"mediacodec",hardware&&!hardwareFailed?1:0);ijk.setOption(IjkMediaPlayer.OPT_CATEGORY_PLAYER,"max-buffer-size",2*1024*1024);ijk.setOption(IjkMediaPlayer.OPT_CATEGORY_PLAYER,"max_cached_duration",3000);ijk.setOption(IjkMediaPlayer.OPT_CATEGORY_PLAYER,"start-on-prepared",0);p=ijk;}
            else p=new BlSystemPlayer();
            final IMediaPlayer current=p;engine=p;p.setAudioStreamType(AudioManager.STREAM_MUSIC);p.setDisplay(surface.getHolder());
            p.setOnPreparedListener(new IMediaPlayer.OnPreparedListener(){public void onPrepared(IMediaPlayer mp){if(closed||engine!=mp)return;prepared=true;state="playing";status.setVisibility(GONE);if(seekWhenReady>0)mp.seekTo(seekWhenReady);if(active)mp.start();else resumeAfterBackground=true;main.removeCallbacks(tick);main.post(tick);danmaku.start();showControls();}});
            p.setOnVideoSizeChangedListener(new IMediaPlayer.OnVideoSizeChangedListener(){public void onVideoSizeChanged(IMediaPlayer mp,int w,int h,int sarN,int sarD){if(engine!=mp||w<=0||h<=0)return;width=w;height=h;fit();}});
            p.setOnCompletionListener(new IMediaPlayer.OnCompletionListener(){public void onCompletion(IMediaPlayer mp){if(engine!=mp||closed)return;if(segment+1<streams.length()){segmentStart+=streams.optJSONObject(segment).optLong("length");segment++;source=streams.optJSONObject(segment).optString("url");seekWhenReady=0;start();}else{state="completed";actions.complete();}}});
            p.setOnErrorListener(new IMediaPlayer.OnErrorListener(){public boolean onError(IMediaPlayer mp,int what,int extra){if(engine!=mp||closed)return true;if(!systemPlayer&&hardware&&!hardwareFailed){hardwareFailed=true;seekWhenReady=Math.max(0,position()-segmentStart);start();return true;}state="error";status.setText("播放失败 ("+what+"/"+extra+")");status.setVisibility(VISIBLE);actions.failure("播放失败，请切换画质或稍后重试");return true;}});
            p.setDataSource(proxy.url(source));p.prepareAsync();
        }catch(Exception e){state="error";status.setText("播放失败："+e.getMessage());actions.failure("无法打开播放地址");}catch(LinkageError e){state="error";status.setText("宿主缺少兼容的 IJK 组件");actions.failure("请安装与设备架构匹配的完整宿主 APK");}
    }
    private void fit(){if(width<=0||height<=0||getWidth()<=0||getHeight()<=0)return;float ratio=Math.min(getWidth()/(float)width,getHeight()/(float)height);FrameLayout.LayoutParams p=new FrameLayout.LayoutParams(Math.max(1,(int)(width*ratio)),Math.max(1,(int)(height*ratio)),Gravity.CENTER);surface.setLayoutParams(p);}
    protected void onSizeChanged(int w,int h,int oldw,int oldh){super.onSizeChanged(w,h,oldw,oldh);fit();}
    private void releaseEngine(){proxy.clear();if(engine!=null){IMediaPlayer old=engine;engine=null;prepared=false;try{old.setOnPreparedListener(null);old.setOnCompletionListener(null);old.setOnErrorListener(null);old.setOnVideoSizeChangedListener(null);old.setDisplay(null);old.release();}catch(Exception ignored){}}}
    void toggle(){if(engine==null||!prepared)return;try{if(engine.isPlaying()){engine.pause();state="paused";}else{engine.start();state="playing";danmaku.start();}}catch(Exception ignored){}showControls();}
    void showControls(){if(closed)return;controls.setVisibility(VISIBLE);main.removeCallbacks(hide);main.postDelayed(hide,6000);}
    boolean key(KeyEvent e){int code=e.getKeyCode();
        if(code==KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE||code==KeyEvent.KEYCODE_MEDIA_PLAY||code==KeyEvent.KEYCODE_MEDIA_PAUSE){if(e.getAction()==KeyEvent.ACTION_DOWN&&e.getRepeatCount()==0){if(code==KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE||code==KeyEvent.KEYCODE_MEDIA_PLAY&&!running()||code==KeyEvent.KEYCODE_MEDIA_PAUSE&&running())toggle();}return true;}
        if(code==KeyEvent.KEYCODE_MEDIA_NEXT||code==KeyEvent.KEYCODE_MEDIA_PREVIOUS){if(e.getAction()==KeyEvent.ACTION_DOWN&&e.getRepeatCount()==0)actions.episodes();return true;}
        if(code==KeyEvent.KEYCODE_MEDIA_FAST_FORWARD||code==KeyEvent.KEYCODE_MEDIA_REWIND){if(e.getAction()==KeyEvent.ACTION_DOWN)seekTo(position()+(code==KeyEvent.KEYCODE_MEDIA_REWIND?-10000:10000));return true;}
        if(code==KeyEvent.KEYCODE_MEDIA_STOP){if(e.getAction()==KeyEvent.ACTION_DOWN){if(running())toggle();seekTo(0);}return true;}
        if(controls.getVisibility()!=VISIBLE){if(code==KeyEvent.KEYCODE_DPAD_LEFT||code==KeyEvent.KEYCODE_DPAD_RIGHT){if(e.getAction()==KeyEvent.ACTION_DOWN)seekTo(position()+(code==KeyEvent.KEYCODE_DPAD_LEFT?-10000:10000));return true;}if(code==KeyEvent.KEYCODE_DPAD_CENTER||code==KeyEvent.KEYCODE_ENTER||code==KeyEvent.KEYCODE_DPAD_DOWN||code==KeyEvent.KEYCODE_DPAD_UP){if(e.getAction()==KeyEvent.ACTION_DOWN){showControls();toggle.requestFocusFromTouch();}return true;}}
        if(e.getAction()==KeyEvent.ACTION_DOWN){main.removeCallbacks(hide);main.postDelayed(hide,6000);}return false;
    }
    private boolean runningBeforePause(){try{return engine!=null&&prepared&&engine.isPlaying();}catch(Exception e){return false;}}
    void active(boolean value){active=value;danmaku.active(value);main.removeCallbacks(tick);if(!value){resumeAfterBackground=runningBeforePause();if(resumeAfterBackground)try{engine.pause();}catch(Exception ignored){}main.removeCallbacks(hide);}else if(engine!=null&&prepared){if(resumeAfterBackground)try{engine.start();}catch(Exception ignored){}resumeAfterBackground=false;main.post(tick);danmaku.start();}}
    public void surfaceCreated(SurfaceHolder h){start();}
    public void surfaceChanged(SurfaceHolder h,int format,int w,int height){}
    public void surfaceDestroyed(SurfaceHolder h){position=position();seekWhenReady=Math.max(0,position-segmentStart);releaseEngine();}
    void close(){closed=true;main.removeCallbacksAndMessages(null);danmaku.close();releaseEngine();}
}
