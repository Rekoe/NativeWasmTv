package xiao.bu.tv;

/** Host checks for capability combinations not available on the attached SDR phones. */
final class HdrDisplayPolicyTest {
    static void require(boolean condition, String reason) {
        if (!condition) throw new AssertionError(reason);
    }
    public static void main(String[] args) {
        require(!HdrDisplayPolicy.passthrough(1, 6, null), "Unknown old display must map PQ");
        require(!HdrDisplayPolicy.passthrough(1, 6, new int[0]), "SDR screen must map PQ");
        require(!HdrDisplayPolicy.passthrough(1, 6, new int[]{1}), "DV alone does not guarantee HDR10");
        require(HdrDisplayPolicy.passthrough(1, 6, new int[]{2}), "HDR10 display preserves PQ");
        require(HdrDisplayPolicy.passthrough(1, 6, new int[]{4}), "HDR10+ display supports base PQ");
        require(!HdrDisplayPolicy.passthrough(1, 7, new int[]{2}), "HDR10 alone does not guarantee HLG");
        require(!HdrDisplayPolicy.passthrough(1, 6, new int[]{3}), "HLG alone does not guarantee PQ");
        require(HdrDisplayPolicy.passthrough(1, 7, new int[]{3}), "HLG display preserves HLG");
        for (int mode : new int[]{0,2,3})
            require(!HdrDisplayPolicy.passthrough(mode,6,new int[]{2,3,4}), "Explicit SDR modes must never passthrough");
        require(!HdrDisplayPolicy.decoderToneMapping(3,35), "Hardware/soft display must use GPU even on API35");
        require(HdrDisplayPolicy.decoderToneMapping(1,31), "Automatic SDR fallback can use decoder tone mapping");
        require(!HdrDisplayPolicy.decoderToneMapping(1,25), "Old SDR fallback needs GPU mapping");
        require("hardware_sdr".equals(HdrMode.sanitize("hardware_sdr")), "New preference survives reload");
        require(HdrMode.nativeValue("hardware_sdr")==3, "Native option selects new mode");
        require(HdrMode.HARDWARE.equals(HdrMode.sanitize("invalid")), "Preserve hardware default");
        System.out.println("HDR display compatibility policy passed");
    }
}
