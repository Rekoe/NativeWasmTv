package xiao.bu.channel.bl;

import android.graphics.*;
import android.view.View;
import android.util.Xml;
import org.xmlpull.v1.XmlPullParser;
import java.io.*;
import java.util.*;

/** Bounded Canvas overlay. No layout or bitmap allocation occurs on the animation path. */
final class BlDanmaku extends View {
    interface Clock{long position();boolean running();}
    static final class Line{long at;String text;int color;float width;}
    private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG),stroke=new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Clock clock;private final List<Line> visible=new ArrayList<>();
    private List<Line> lines=Collections.emptyList();private int next;private long previous;
    private boolean enabled=true,active=true;
    private final Runnable frame=new Runnable(){public void run(){if(enabled&&active&&clock.running()){invalidate();postDelayed(this,33);}}};
    BlDanmaku(BlUi ui,Clock clock){super(ui.context);this.clock=clock;paint.setTextSize(ui.px(24));stroke.set(paint);stroke.setStyle(Paint.Style.STROKE);stroke.setStrokeWidth(Math.max(1,ui.px(2)));stroke.setColor(0xcc000000);setFocusable(false);}
    static List<Line> parse(byte[] bytes)throws Exception{
        ArrayList<Line> result=new ArrayList<>();XmlPullParser parser=Xml.newPullParser();parser.setInput(new ByteArrayInputStream(bytes),"UTF-8");
        int event;while((event=parser.next())!=XmlPullParser.END_DOCUMENT&&result.size()<12000)if(event==XmlPullParser.START_TAG&&"d".equals(parser.getName())){
            String[] data=String.valueOf(parser.getAttributeValue(null,"p")).split(",");if(data.length<4)continue;
            try{Line l=new Line();l.at=(long)(Double.parseDouble(data[0])*1000);l.color=0xff000000|(Integer.parseInt(data[3])&0xffffff);l.text=parser.nextText();if(l.text.length()>100)l.text=l.text.substring(0,100);result.add(l);}catch(Exception ignored){}
        }
        Collections.sort(result,new Comparator<Line>(){public int compare(Line a,Line b){return a.at<b.at?-1:a.at==b.at?0:1;}});return result;
    }
    void setLines(List<Line> value){lines=value;for(Line l:lines)l.width=paint.measureText(l.text);previous=-1;visible.clear();start();}
    void enabled(boolean value){enabled=value;setVisibility(value?VISIBLE:GONE);start();}
    void active(boolean value){active=value;start();}
    void start(){removeCallbacks(frame);if(active&&enabled&&clock.running())post(frame);}
    protected void onDraw(Canvas c){
        super.onDraw(c);long now=clock.position();
        if(now<previous||previous<0||now-previous>3000){visible.clear();int low=0,high=lines.size();while(low<high){int mid=(low+high)/2;if(lines.get(mid).at<now)low=mid+1;else high=mid;}next=low;}
        previous=now;
        while(next<lines.size()&&lines.get(next).at<=now){Line l=lines.get(next++);if(visible.size()<8)visible.add(l);}
        for(Iterator<Line> i=visible.iterator();i.hasNext();){Line l=i.next();if(now-l.at>8000)i.remove();}
        float row=paint.getTextSize()*1.6f;
        for(int n=0;n<visible.size();n++){Line l=visible.get(n);float x=getWidth()-(getWidth()+l.width)*(now-l.at)/8000f,y=row*(n+1);paint.setColor(l.color);c.drawText(l.text,x,y,stroke);c.drawText(l.text,x,y,paint);}
    }
    void close(){active=false;removeCallbacks(frame);visible.clear();lines=Collections.emptyList();}
}
