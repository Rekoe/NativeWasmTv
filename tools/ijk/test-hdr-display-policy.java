package xiao.bu.tv;

/** Host checks for capability combinations not available on the attached SDR phones. */
final class HdrDisplayPolicyTest {
    static void require(boolean condition, String reason) {
        if (!condition) throw new AssertionError(reason);
    }
    public static void main(String[] args) {
        require(!HdrDisplayPolicy.passthrough(1, 6, null, 30), "Unknown old display must map PQ");
        require(!HdrDisplayPolicy.passthrough(1, 6, new int[0], 30), "SDR screen must map PQ");
        require(!HdrDisplayPolicy.passthrough(1, 6, new int[]{1}, 30), "DV alone does not guarantee HDR10");
        require(HdrDisplayPolicy.passthrough(1, 6, new int[]{2}, 30), "HDR10 display preserves PQ");
        require(HdrDisplayPolicy.passthrough(1, 6, new int[]{4}, 30), "HDR10+ display supports base PQ");
        require(!HdrDisplayPolicy.passthrough(1, 7, new int[]{2}, 30), "HDR10 alone does not guarantee HLG");
        require(!HdrDisplayPolicy.passthrough(1, 6, new int[]{3}, 30), "HLG alone does not guarantee PQ");
        require(HdrDisplayPolicy.passthrough(1, 7, new int[]{3}, 30), "HLG display preserves HLG");
        for (int mode : new int[]{0,2,3})
            require(!HdrDisplayPolicy.passthrough(mode,6,new int[]{2,3,4}, 30), "Explicit SDR modes must never passthrough");
        require(!HdrDisplayPolicy.decoderToneMapping(3,35), "Hardware/soft display must use GPU even on API35");
        require(HdrDisplayPolicy.decoderToneMapping(1,31), "Automatic SDR fallback can use decoder tone mapping");
        require(!HdrDisplayPolicy.decoderToneMapping(1,25), "Old SDR fallback needs GPU mapping");
        require("hardware_sdr".equals(HdrMode.sanitize("hardware_sdr")), "New preference survives reload");
        require(HdrMode.nativeValue("hardware_sdr")==3, "Native option selects new mode");
        require(HdrMode.HARDWARE.equals(HdrMode.sanitize("invalid")), "Preserve hardware default");
        for(int sdk : new int[]{19,25,26,30,31,35}) {
            int mode=HdrMode.nativeValue(HdrMode.DEFAULT);
            require(mode==1, "Default hardware preference uses automatic capability routing");
            require(HdrDisplayPolicy.passthrough(mode,7,new int[]{3},sdk)==(sdk>=26), "Automatic HLG route needs supported screen and API26");
            require(HdrDisplayPolicy.passthrough(mode,6,new int[]{2},sdk)==(sdk>=26), "Automatic PQ route needs supported screen and API26");
            require(HdrDisplayPolicy.decoderToneMapping(mode,sdk)==(sdk>=31), "Modern SDR fallback can request decoder mapping");
            require(!HdrDisplayPolicy.passthrough(mode,6,new int[0],sdk), "SDR screen never gets HDR passthrough");
        }
        require(HdrMode.nativeValue("passthrough")==1, "Legacy HDR preference keeps the same output policy");
        require(HdrMode.DEFAULT.equals(HdrMode.sanitize("passthrough")), "Duplicate HDR preference migrates to auto");
        require(HdrMode.nativeValue(HdrMode.sanitize("passthrough"))==HdrMode.nativeValue("passthrough"),
                "Preference migration preserves playback output");
        require(HdrMode.nativeValue("mapping")==0, "SDR mapping remains distinct from forced GPU mapping");
        require(HdrDisplayPolicy.decoderToneMapping(0,31), "SDR mapping uses decoder conversion on modern Android");
        require(!HdrDisplayPolicy.decoderToneMapping(0,30), "SDR mapping uses GPU conversion on older Android");
        require(!HdrDisplayPolicy.passthrough(1,7,new int[]{3},25), "Android 7 HDR path is unverified");
        for(int sdk:new int[]{14,19,25,28,29,30,31,37}) {
            require(HdrDisplayPolicy.rawYuv(sdk,"OpenGL ES 3.2","GL_EXT_YUV_target GL_OES_EGL_image_external")== (sdk>=29),"Raw YUV version gate");
            require(!HdrDisplayPolicy.rawYuv(sdk,"OpenGL ES 2.0","GL_EXT_YUV_target"),"ES2 is a compatibility RGB path");
            require(!HdrDisplayPolicy.rawYuv(sdk,"OpenGL ES 3.2","GL_EXT_YUV_target_fake"),"Extension match must be exact");
        }
        require(!HdrDisplayPolicy.rawYuv(31,null,null),"Unknown GPU capabilities keep compatibility path");
        for(int standard:new int[]{1,2,4,6})for(int range:new int[]{1,2})for(int depth:new int[]{8,10,12}){
            float[] c=HdrDisplayPolicy.yuvColor(standard,range,depth);float shift=1<<(depth-8),max=(1<<depth)-1;
            for(int value:new int[]{0,1}){
                float y=range==1?value:(16+219*value)*shift/max;
                for(int channel=0;channel<3;channel++)require(Math.abs(c[channel]*(y-c[9])-value)<.00001,"Digital black/white range mismatch");
            }
            float kr=standard==6?.2627f:standard==1?.2126f:.299f,kb=standard==6?.0593f:standard==1?.0722f:.114f;
            float r=.8f,g=.3f,b=.1f,y=kr*r+(1-kr-kb)*g+kb*b,u=(b-y)/(2*(1-kb)),v=(r-y)/(2*(1-kr));
            float yy=range==1?y:y*219*shift/max,uu=range==1?u:u*224*shift/max,vv=range==1?v:v*224*shift/max;
            float[] expected={r,g,b};for(int i=0;i<3;i++)require(Math.abs(c[i]*yy+c[3+i]*uu+c[6+i]*vv-expected[i])<.00001,"YUV colour round trip");
        }
        System.out.println("HDR API14-37 display/codec/GPU policy and 24 YUV matrix/range/depth combinations passed");
    }
}
