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
        System.out.println("HDR display compatibility policy passed");
    }
}
