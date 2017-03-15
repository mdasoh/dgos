#pragma once

void isr_save_fxsave(void);
void isr_restore_fxrstor(void);
void isr_save_xsave(void);
void isr_restore_xrstor(void);

// Exception handlers
extern void isr_entry_0(void);
extern void isr_entry_1(void);
extern void isr_entry_2(void);
extern void isr_entry_3(void);
extern void isr_entry_4(void);
extern void isr_entry_5(void);
extern void isr_entry_6(void);
extern void isr_entry_7(void);
extern void isr_entry_8(void);
extern void isr_entry_9(void);
extern void isr_entry_10(void);
extern void isr_entry_11(void);
extern void isr_entry_12(void);
extern void isr_entry_13(void);
extern void isr_entry_14(void);
extern void isr_entry_15(void);
extern void isr_entry_16(void);
extern void isr_entry_17(void);
extern void isr_entry_18(void);
extern void isr_entry_19(void);
extern void isr_entry_20(void);
extern void isr_entry_21(void);
extern void isr_entry_22(void);
extern void isr_entry_23(void);
extern void isr_entry_24(void);
extern void isr_entry_25(void);
extern void isr_entry_26(void);
extern void isr_entry_27(void);
extern void isr_entry_28(void);
extern void isr_entry_29(void);
extern void isr_entry_30(void);
extern void isr_entry_31(void);

// PIC IRQs
extern void isr_entry_32(void);
extern void isr_entry_33(void);
extern void isr_entry_34(void);
extern void isr_entry_35(void);
extern void isr_entry_36(void);
extern void isr_entry_37(void);
extern void isr_entry_38(void);
extern void isr_entry_39(void);
extern void isr_entry_40(void);
extern void isr_entry_41(void);
extern void isr_entry_42(void);
extern void isr_entry_43(void);
extern void isr_entry_44(void);
extern void isr_entry_45(void);
extern void isr_entry_46(void);
extern void isr_entry_47(void);

// APIC IRQs
extern void isr_entry_48(void);
extern void isr_entry_49(void);
extern void isr_entry_50(void);
extern void isr_entry_51(void);
extern void isr_entry_52(void);
extern void isr_entry_53(void);
extern void isr_entry_54(void);
extern void isr_entry_55(void);
extern void isr_entry_56(void);
extern void isr_entry_57(void);
extern void isr_entry_58(void);
extern void isr_entry_59(void);
extern void isr_entry_60(void);
extern void isr_entry_61(void);
extern void isr_entry_62(void);
extern void isr_entry_63(void);
extern void isr_entry_64(void);
extern void isr_entry_65(void);
extern void isr_entry_66(void);
extern void isr_entry_67(void);
extern void isr_entry_68(void);
extern void isr_entry_69(void);
extern void isr_entry_70(void);
extern void isr_entry_71(void);

// Software interrupts (see interrupts.h)
extern void isr_entry_72(void);
extern void isr_entry_73(void);
extern void isr_entry_74(void);
extern void isr_entry_75(void);
extern void isr_entry_76(void);
extern void isr_entry_77(void);
extern void isr_entry_78(void);
extern void isr_entry_79(void);
extern void isr_entry_80(void);
extern void isr_entry_81(void);
extern void isr_entry_82(void);
extern void isr_entry_83(void);
extern void isr_entry_84(void);
extern void isr_entry_85(void);
extern void isr_entry_86(void);
extern void isr_entry_87(void);
extern void isr_entry_88(void);
extern void isr_entry_89(void);
extern void isr_entry_90(void);
extern void isr_entry_91(void);
extern void isr_entry_92(void);
extern void isr_entry_93(void);
extern void isr_entry_94(void);
extern void isr_entry_95(void);
extern void isr_entry_96(void);
extern void isr_entry_97(void);
extern void isr_entry_98(void);
extern void isr_entry_99(void);
extern void isr_entry_100(void);
extern void isr_entry_101(void);
extern void isr_entry_102(void);
extern void isr_entry_103(void);
extern void isr_entry_104(void);
extern void isr_entry_105(void);
extern void isr_entry_106(void);
extern void isr_entry_107(void);
extern void isr_entry_108(void);
extern void isr_entry_109(void);
extern void isr_entry_110(void);
extern void isr_entry_111(void);
extern void isr_entry_112(void);
extern void isr_entry_113(void);
extern void isr_entry_114(void);
extern void isr_entry_115(void);
extern void isr_entry_116(void);
extern void isr_entry_117(void);
extern void isr_entry_118(void);
extern void isr_entry_119(void);
extern void isr_entry_120(void);
extern void isr_entry_121(void);
extern void isr_entry_122(void);
extern void isr_entry_123(void);
extern void isr_entry_124(void);
extern void isr_entry_125(void);
extern void isr_entry_126(void);
extern void isr_entry_127(void);
extern void isr_entry_128(void);
extern void isr_entry_129(void);
extern void isr_entry_130(void);
extern void isr_entry_131(void);
extern void isr_entry_132(void);
extern void isr_entry_133(void);
extern void isr_entry_134(void);
extern void isr_entry_135(void);
extern void isr_entry_136(void);
extern void isr_entry_137(void);
extern void isr_entry_138(void);
extern void isr_entry_139(void);
extern void isr_entry_140(void);
extern void isr_entry_141(void);
extern void isr_entry_142(void);
extern void isr_entry_143(void);
extern void isr_entry_144(void);
extern void isr_entry_145(void);
extern void isr_entry_146(void);
extern void isr_entry_147(void);
extern void isr_entry_148(void);
extern void isr_entry_149(void);
extern void isr_entry_150(void);
extern void isr_entry_151(void);
extern void isr_entry_152(void);
extern void isr_entry_153(void);
extern void isr_entry_154(void);
extern void isr_entry_155(void);
extern void isr_entry_156(void);
extern void isr_entry_157(void);
extern void isr_entry_158(void);
extern void isr_entry_159(void);
extern void isr_entry_160(void);
extern void isr_entry_161(void);
extern void isr_entry_162(void);
extern void isr_entry_163(void);
extern void isr_entry_164(void);
extern void isr_entry_165(void);
extern void isr_entry_166(void);
extern void isr_entry_167(void);
extern void isr_entry_168(void);
extern void isr_entry_169(void);
extern void isr_entry_170(void);
extern void isr_entry_171(void);
extern void isr_entry_172(void);
extern void isr_entry_173(void);
extern void isr_entry_174(void);
extern void isr_entry_175(void);
extern void isr_entry_176(void);
extern void isr_entry_177(void);
extern void isr_entry_178(void);
extern void isr_entry_179(void);
extern void isr_entry_180(void);
extern void isr_entry_181(void);
extern void isr_entry_182(void);
extern void isr_entry_183(void);
extern void isr_entry_184(void);
extern void isr_entry_185(void);
extern void isr_entry_186(void);
extern void isr_entry_187(void);
extern void isr_entry_188(void);
extern void isr_entry_189(void);
extern void isr_entry_190(void);
extern void isr_entry_191(void);
extern void isr_entry_192(void);
extern void isr_entry_193(void);
extern void isr_entry_194(void);
extern void isr_entry_195(void);
extern void isr_entry_196(void);
extern void isr_entry_197(void);
extern void isr_entry_198(void);
extern void isr_entry_199(void);
extern void isr_entry_200(void);
extern void isr_entry_201(void);
extern void isr_entry_202(void);
extern void isr_entry_203(void);
extern void isr_entry_204(void);
extern void isr_entry_205(void);
extern void isr_entry_206(void);
extern void isr_entry_207(void);
extern void isr_entry_208(void);
extern void isr_entry_209(void);
extern void isr_entry_210(void);
extern void isr_entry_211(void);
extern void isr_entry_212(void);
extern void isr_entry_213(void);
extern void isr_entry_214(void);
extern void isr_entry_215(void);
extern void isr_entry_216(void);
extern void isr_entry_217(void);
extern void isr_entry_218(void);
extern void isr_entry_219(void);
extern void isr_entry_220(void);
extern void isr_entry_221(void);
extern void isr_entry_222(void);
extern void isr_entry_223(void);
extern void isr_entry_224(void);
extern void isr_entry_225(void);
extern void isr_entry_226(void);
extern void isr_entry_227(void);
extern void isr_entry_228(void);
extern void isr_entry_229(void);
extern void isr_entry_230(void);
extern void isr_entry_231(void);
extern void isr_entry_232(void);
extern void isr_entry_233(void);
extern void isr_entry_234(void);
extern void isr_entry_235(void);
extern void isr_entry_236(void);
extern void isr_entry_237(void);
extern void isr_entry_238(void);
extern void isr_entry_239(void);
extern void isr_entry_240(void);
extern void isr_entry_241(void);
extern void isr_entry_242(void);
extern void isr_entry_243(void);
extern void isr_entry_244(void);
extern void isr_entry_245(void);
extern void isr_entry_246(void);
extern void isr_entry_247(void);
extern void isr_entry_248(void);
extern void isr_entry_249(void);
extern void isr_entry_250(void);
extern void isr_entry_251(void);
extern void isr_entry_252(void);
extern void isr_entry_253(void);
extern void isr_entry_254(void);
extern void isr_entry_255(void);
