/* Real Annex B SPS fixtures, hvcC framing, truncation and malformed input. */
#include <stdio.h>
#include <stdlib.h>
#include "ntv_hevc_color.h"
static void require(int condition, const char *reason)
{
    if (!condition) { fprintf(stderr,"FAIL: %s\n",reason); exit(1); }
}
int main(int argc, char **argv)
{
    for (int decoded = 0; decoded < 32; ++decoded) {
        int valid = decoded == 1 || (decoded >= 4 && decoded <= 18);
        require(ntv_hevc_frame_transfer(decoded,18) == (valid ? decoded : 18),
                "invalid SEI must preserve HLG SPS; valid decoded transfer wins");
        require(ntv_hevc_frame_transfer(decoded,2) == decoded,
                "unknown SPS must not invent HDR");
    }
    uint8_t sei[] = {0x4e,1,147,1,18,0x80};
    require(ntv_hevc_sei_transfer(sei,sizeof(sei)) == 18,"valid HLG Alternative Transfer SEI");
    for (unsigned i = 0; i < sizeof(sei); ++i)
        require(!ntv_hevc_sei_transfer(sei,i),"truncated SEI rejected");
    sei[4] = 0; require(!ntv_hevc_sei_transfer(sei,sizeof(sei)),"reserved SEI ignored");
    sei[4] = 2; require(!ntv_hevc_sei_transfer(sei,sizeof(sei)),"unspecified SEI ignored");
    sei[4] = 16; require(ntv_hevc_sei_transfer(sei,sizeof(sei)) == 16,"valid PQ SEI preserved");
    sei[4] = 18;
    uint8_t annex_sei[] = {0,0,0,1,0x4e,1,147,1,18,0x80,0,0,0,1,2,1,0x80};
    NtvHevcColor prefix = {0};
    require(ntv_hevc_annexb_color(annex_sei,sizeof(annex_sei),&prefix)
            && prefix.preferred_transfer == 18,"four-byte Annex B delimiters exclude trailing zero");
    NtvHevcColor compatible = {8,9,14,9,1,1,0};
    require(ntv_hevc_stream_transfer(18,&compatible) == 18,"decoded HLG wins SDR-compatible VUI");
    require(ntv_hevc_stream_transfer(2,&compatible) == 14,"no HDR guess from SDR VUI");
    require(argc == 6 || argc == 8,"file depth primaries transfer matrix [width height]");
    FILE *file = fopen(argv[1],"rb");
    require(file != NULL,"open fixture");
    uint8_t *data = malloc(8 * 1024 * 1024);
    require(data != NULL,"allocate fixture");
    unsigned size = fread(data,1,8 * 1024 * 1024,file);
    fclose(file);
    NtvHevcColor color = {0};
    require(ntv_hevc_annexb_color(data,size,&color),"read real SPS");
    printf("depth=%d primaries=%d transfer=%d matrix=%d range=%d\n",
            color.depth,color.primaries,color.transfer,color.matrix,color.range);
    require(color.depth == atoi(argv[2]) && color.primaries == atoi(argv[3])
            && color.transfer == atoi(argv[4]) && color.matrix == atoi(argv[5]),"source values");
    if(argc==8)require(color.width==atoi(argv[6]) && color.height==atoi(argv[7]),"cropped SPS dimensions");
    unsigned start = 0, end = 0;
    for (unsigned i = 0; i + 5 < size; ++i)
        if (!data[i] && !data[i+1] && data[i+2] == 1 && ((data[i+3] >> 1) & 63) == 33) {
            start = i + 3; end = start + 2;
            while (end + 2 < size && !(!data[end] && !data[end+1] && data[end+2] == 1)) ++end;
            if (end + 2 >= size) end = size;
            break;
        }
    unsigned length = end - start;
    require(length > 0 && length <= 4096,"bounded SPS");
    uint8_t hvcc[4160] = {1}; hvcc[22] = 1; hvcc[23] = 33; hvcc[25] = 1;
    hvcc[26] = length >> 8; hvcc[27] = length;
    memcpy(hvcc + 28,data + start,length);
    NtvHevcColor boxed = {0};
    require(ntv_hevc_extradata_color(hvcc,28+length,&boxed),"hvcC SPS");
    NtvHevcColor sps = {0};
    require(ntv_hevc_sps_color(data+start,length,&sps),"isolated SPS");
    require(sps.scan == 1,"progressive source flag survives Annex B / hvcC");
    /* Re-encode the real SPS after changing only the two PTL source flags.
     * Both flags equal means mixed/unspecified, never guess progressive. */
    uint8_t rbsp[4096], changed[8192]; unsigned rbsp_size=0, zeros=0;
    for(unsigned i=start+2;i<end;i++) {
        if(zeros==2 && data[i]==3){zeros=0;continue;}
        rbsp[rbsp_size++]=data[i];zeros=data[i]==0?zeros+1:0;
    }
    require(rbsp_size>6,"PTL present");
    for(unsigned flags=0;flags<4;flags++) {
        rbsp[6]=(rbsp[6]&63)|(flags<<6);
        unsigned count=2;zeros=0;changed[0]=data[start];changed[1]=data[start+1];
        for(unsigned i=0;i<rbsp_size;i++) {
            if(zeros==2 && rbsp[i]<=3){changed[count++]=3;zeros=0;}
            changed[count++]=rbsp[i];zeros=rbsp[i]==0?zeros+1:0;
        }
        NtvHevcColor classified={0};
        require(ntv_hevc_sps_color(changed,count,&classified),"source flag variant parses");
        require(classified.scan==(flags==2?1:flags==1?2:0),"PTL scan flags; unknown retained");
        require(classified.depth==sps.depth && classified.transfer==sps.transfer,"scan changes preserve colour");
    }
    require(memcmp(&sps,&boxed,sizeof(sps)) == 0,"hvcC matches isolated SPS");
    for (unsigned i = 0; i < 28+length; ++i)
        require(!ntv_hevc_extradata_color(hvcc,i,&boxed),"truncated hvcC rejected");
    require(!ntv_hevc_sps_color(data+start,2,&boxed),"truncated SPS rejected");
    unsigned pos = 28 + length;
    hvcc[22] = 2; hvcc[pos++] = 39; hvcc[pos++] = 0; hvcc[pos++] = 1;
    hvcc[pos++] = 0; hvcc[pos++] = sizeof(sei); memcpy(hvcc+pos,sei,sizeof(sei)); pos += sizeof(sei);
    require(ntv_hevc_extradata_color(hvcc,pos,&boxed) && boxed.transfer == 18
            && boxed.preferred_transfer == 18,"hvcC valid SEI overrides SPS");
    for(unsigned i=28+length;i<pos;i++)require(!ntv_hevc_extradata_color(hvcc,i,&boxed),"truncated SEI array rejected");
    uint8_t headers[4128] = {0,0,1}; memcpy(headers+3,data+start,length);
    memcpy(headers+3+length,"\0\0\1",3); memcpy(headers+6+length,sei,sizeof(sei));
    require(ntv_hevc_annexb_color(headers,6+length+sizeof(sei),&boxed)
            && boxed.transfer == 18 && boxed.depth == sps.depth,"Annex B valid SEI overrides SPS");
    uint8_t sample[4128];
    for (unsigned n = 1; n <= 4; ++n) {
        hvcc[21] = n - 1; unsigned offset = 0;
        for (unsigned j = n; j; --j) sample[offset++] = sizeof(sei) >> ((j-1)*8);
        memcpy(sample+offset,sei,sizeof(sei)); offset += sizeof(sei);
        require(ntv_hevc_packet_color(sample,offset,hvcc,pos,&boxed)
                && boxed.preferred_transfer == 18 && !boxed.depth,"length-prefixed SEI-only packet");
        for (unsigned j = 0; j < offset; ++j)
            require(!ntv_hevc_packet_color(sample,j,hvcc,pos,&boxed),"truncated sample rejected");
        memset(sample,255,n);
        require(!ntv_hevc_packet_color(sample,offset,hvcc,pos,&boxed),"oversized sample length rejected");
    }
    sei[4] = 0; memcpy(headers+6+length,sei,sizeof(sei));
    require(ntv_hevc_annexb_color(headers,6+length+sizeof(sei),&boxed)
            && boxed.transfer == sps.transfer && !boxed.preferred_transfer,"Annex B invalid SEI retains SPS");
    uint8_t malformed[4096];
    memset(malformed,0,sizeof(malformed)); malformed[0] = 0x42; malformed[1] = 1;
    require(!ntv_hevc_sps_color(malformed,sizeof(malformed),&boxed),"unbounded Golomb rejected");
    memset(malformed,255,sizeof(malformed)); malformed[0] = 0x42; malformed[1] = 1;
    require(!ntv_hevc_sps_color(malformed,sizeof(malformed),&boxed),"invalid layers rejected");
    unsigned seed = 12345;
    for (unsigned i = 0; i < 10000; ++i) {
        unsigned count = 3 + i % 4093;
        for (unsigned j = 2; j < count; ++j) {
            seed = seed * 1664525U + 1013904223U; malformed[j] = seed >> 24;
        }
        ntv_hevc_sps_color(malformed,count,&boxed);
        malformed[0]=0x4e;malformed[1]=1;
        ntv_hevc_sei_transfer(malformed,count);
        malformed[0]=0x42;
    }
    free(data);
    puts("HEVC source colour tests passed");
    return 0;
}
