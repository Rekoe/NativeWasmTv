/* Real AVC SPS fixtures: Annex B, avcC, sample framing and malformed headers. */
#include <stdio.h>
#include <stdlib.h>
#include "ntv_h264_color.h"
static void require(int ok,const char *reason)
{if(!ok){fprintf(stderr,"FAIL: %s\n",reason);exit(1);}}
int main(int argc,char **argv)
{
    require(argc==6,"fixture depth primaries transfer matrix");
    FILE *file=fopen(argv[1],"rb");require(file!=NULL,"open fixture");
    uint8_t *data=malloc(4*1024*1024);require(data!=NULL,"buffer");
    unsigned size=fread(data,1,4*1024*1024,file);fclose(file);
    NtvHevcColor color={0};
    require(ntv_h264_annexb_color(data,size,&color),"Annex B SPS");
    printf("actual depth=%d primaries=%d transfer=%d matrix=%d\n",color.depth,color.primaries,color.transfer,color.matrix);
    require(color.depth==atoi(argv[2]) && color.primaries==atoi(argv[3])
            && color.transfer==atoi(argv[4]) && color.matrix==atoi(argv[5]),"actual source fields");
    unsigned start=0,end=0;
    for(unsigned i=0;i+4<size;i++)if(!data[i]&&!data[i+1]&&data[i+2]==1&&(data[i+3]&31)==7) {
        start=i+3;end=start+1;
        while(end+2<size&&(data[end]||data[end+1]||data[end+2]!=1))end++;
        if(end+2>=size)end=size;
        while(end>start+1&&!data[end-1])end--;
        break;
    }
    unsigned length=end-start;require(length>0&&length<=4096,"bounded SPS");
    uint8_t avcc[4104]={1,100,0,40,255,225};avcc[6]=length>>8;avcc[7]=length;
    memcpy(avcc+8,data+start,length);NtvHevcColor boxed={0};
    require(ntv_h264_extradata_color(avcc,8+length,&boxed)&&memcmp(&boxed,&color,sizeof(color))==0,"avcC equals Annex B");
    for(unsigned i=0;i<8+length;i++)require(!ntv_h264_extradata_color(avcc,i,&boxed),"truncated avcC");
    uint8_t sample[4100];
    for(unsigned n=1;n<=4;n++) {
        if(n==1&&length>255)continue;
        avcc[4]=n-1;unsigned pos=0;
        for(unsigned j=n;j;j--)sample[pos++]=length>>((j-1)*8);
        memcpy(sample+pos,data+start,length);pos+=length;
        require(ntv_h264_packet_color(sample,pos,avcc,8+length,&boxed)&&memcmp(&boxed,&color,sizeof(color))==0,"length-prefixed SPS");
        for(unsigned i=0;i<pos;i++)require(!ntv_h264_packet_color(sample,i,avcc,8+length,&boxed),"truncated sample");
        memset(sample,255,n);require(!ntv_h264_packet_color(sample,pos,avcc,8+length,&boxed),"invalid NAL length");
    }
    uint8_t malformed[4096];unsigned seed=12345;
    for(unsigned i=0;i<10000;i++) {
        unsigned count=1+i%4095;malformed[0]=0x67;
        for(unsigned j=1;j<count;j++){seed=seed*1664525U+1013904223U;malformed[j]=seed>>24;}
        ntv_h264_sps_color(malformed,count,&boxed);
    }
    printf("PASS depth=%d primaries=%d transfer=%d matrix=%d range=%d; framing/truncation/fuzz\n",
            color.depth,color.primaries,color.transfer,color.matrix,color.range);
    free(data);return 0;
}
