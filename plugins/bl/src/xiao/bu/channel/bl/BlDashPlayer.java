package xiao.bu.channel.bl;

import android.content.Context;
import android.net.Uri;
import android.view.*;
import java.io.*;
import java.util.*;
import okhttp3.*;
import com.google.android.exoplayer2.*;
import com.google.android.exoplayer2.source.*;
import com.google.android.exoplayer2.trackselection.DefaultTrackSelector;
import com.google.android.exoplayer2.upstream.*;
import com.google.android.exoplayer2.video.VideoListener;
import tv.danmaku.ijk.media.player.AbstractMediaPlayer;
import tv.danmaku.ijk.media.player.MediaInfo;
import tv.danmaku.ijk.media.player.misc.ITrackInfo;

/** API-16+ DASH merge; neither the UI module nor another HTTP client is packaged. */
final class BlDashPlayer extends AbstractMediaPlayer {
    private final SimpleExoPlayer player;private final BlApi api;private final String audio;private String source="";private boolean notified,released;private int width,height;
    private final Set<Call> downloads=Collections.newSetFromMap(new java.util.concurrent.ConcurrentHashMap<Call,Boolean>());
    BlDashPlayer(Context context,BlApi api,String audio){this.api=api;this.audio=audio;
        DefaultLoadControl load=new DefaultLoadControl(new DefaultAllocator(true,65536),3000,10000,300,800,8*1024*1024,false);
        player=ExoPlayerFactory.newSimpleInstance(context,new DefaultTrackSelector(),load);
        player.addListener(new Player.DefaultEventListener(){public void onPlayerStateChanged(boolean ready,int state){if(released)return;if(state==Player.STATE_READY&&!notified){notified=true;notifyOnPrepared();}if(state==Player.STATE_ENDED)notifyOnCompletion();}public void onPlayerError(ExoPlaybackException error){if(!released)notifyOnError(-1004,error.type);}});
        player.addVideoListener(new VideoListener(){public void onVideoSizeChanged(int w,int h,int rotation,float ratio){width=w;height=h;notifyOnVideoSizeChanged(w,h,Math.round(ratio*1000),1000);}public void onRenderedFirstFrame(){notifyOnInfo(3,0);}});
    }
    public void setDisplay(SurfaceHolder holder){player.setVideoSurfaceHolder(holder);}public void setSurface(Surface surface){player.setVideoSurface(surface);}
    public void setDataSource(String url){source=url;}public String getDataSource(){return source;}
    public void prepareAsync(){DataSource.Factory factory=new DataSource.Factory(){public DataSource createDataSource(){return new SharedSource(api,downloads);}};ExtractorMediaSource video=new ExtractorMediaSource.Factory(factory).createMediaSource(Uri.parse(source));MediaSource merged=audio.isEmpty()?video:new MergingMediaSource(video,new ExtractorMediaSource.Factory(factory).createMediaSource(Uri.parse(audio)));player.prepare(merged);}
    public void start(){player.setPlayWhenReady(true);}public void stop(){player.stop();}public void pause(){player.setPlayWhenReady(false);}public boolean isPlaying(){return player.getPlayWhenReady()&&player.getPlaybackState()==Player.STATE_READY;}
    public void seekTo(long at){player.seekTo(at);}public long getCurrentPosition(){return player.getCurrentPosition();}public long getDuration(){long value=player.getDuration();return value==C.TIME_UNSET?0:value;}
    public void release(){released=true;resetListeners();for(Call call:downloads)call.cancel();player.release();downloads.clear();}public void reset(){notified=false;player.stop(true);resetListeners();}
    void speed(float value){player.setPlaybackParameters(new PlaybackParameters(value));}
    public void setVolume(float left,float right){player.setVolume((left+right)/2);}public void setAudioStreamType(int type){}public void setKeepInBackground(boolean value){}public void setScreenOnWhilePlaying(boolean value){}public void setWakeMode(Context c,int mode){}
    public void setLooping(boolean value){player.setRepeatMode(value?Player.REPEAT_MODE_ONE:Player.REPEAT_MODE_OFF);}public boolean isLooping(){return player.getRepeatMode()==Player.REPEAT_MODE_ONE;}
    public int getVideoWidth(){return width;}public int getVideoHeight(){return height;}public int getVideoSarNum(){return 1;}public int getVideoSarDen(){return 1;}public int getAudioSessionId(){return 0;}public MediaInfo getMediaInfo(){return new MediaInfo();}public ITrackInfo[] getTrackInfo(){return new ITrackInfo[0];}public void setLogEnabled(boolean value){}public boolean isPlayable(){return true;}
    public void setDataSource(Context c,Uri uri){setDataSource(uri.toString());}public void setDataSource(Context c,Uri uri,Map<String,String> headers){setDataSource(uri.toString());}public void setDataSource(FileDescriptor descriptor)throws IOException{throw new IOException("DASH 仅接受网络媒体地址");}
    private static final class SharedSource extends BaseDataSource {
        final BlApi api;final Set<Call> downloads;Response response;InputStream stream;Uri uri;boolean opened;long remaining;
        SharedSource(BlApi api,Set<Call> downloads){super(true);this.api=api;this.downloads=downloads;}
        public long open(DataSpec spec)throws IOException{transferInitializing(spec);Request.Builder request=api.request(spec.uri.toString());if(spec.position>0||spec.length!=C.LENGTH_UNSET)request.header("Range","bytes="+spec.position+"-"+(spec.length==C.LENGTH_UNSET?"":spec.position+spec.length-1));response=api.execute(request.build(),downloads);if(!response.isSuccessful()||response.body()==null)throw new IOException("DASH HTTP "+response.code());uri=Uri.parse(response.request().url().toString());stream=response.body().byteStream();if(response.code()==200&&spec.position>0){long skip=spec.position;byte[] bytes=new byte[8192];while(skip>0){int read=stream.read(bytes,0,(int)Math.min(bytes.length,skip));if(read<0)throw new EOFException();skip-=read;}}remaining=spec.length!=C.LENGTH_UNSET?spec.length:response.body().contentLength();if(response.code()==200&&remaining>=0&&spec.length==C.LENGTH_UNSET)remaining=Math.max(0,remaining-spec.position);opened=true;transferStarted(spec);return remaining;}
        public int read(byte[] buffer,int at,int count)throws IOException{if(count==0)return 0;if(remaining==0)return C.RESULT_END_OF_INPUT;int read=stream.read(buffer,at,remaining<0?count:(int)Math.min(count,remaining));if(read<0)return C.RESULT_END_OF_INPUT;if(remaining>0)remaining-=read;bytesTransferred(read);return read;}
        public Uri getUri(){return uri;}public Map<String,List<String>> getResponseHeaders(){return response==null?Collections.<String,List<String>>emptyMap():response.headers().toMultimap();}
        public void close(){if(response!=null)response.close();response=null;stream=null;uri=null;if(opened){opened=false;transferEnded();}}
    }
}
