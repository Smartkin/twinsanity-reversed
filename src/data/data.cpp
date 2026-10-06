// The retail executable's .data from 0x2E6F00 to 0x2EC348, converted from the split
// once, in the retail order: the game's code may go from one object into the next, so they stay where the retail
// executable had them, each under its retail name
#include "common.h"
#include "gcc2.h"
#include "retaildata.h"

// The other data and functions it points at
extern u8 Ref_D_002F3988[] RETAIL(D_002F3988);
extern u8 Ref_D_002F39B0[] RETAIL(D_002F39B0);
extern u8 Ref_D_002F39D8[] RETAIL(D_002F39D8);
extern u8 Ref_D_002F3A00[] RETAIL(D_002F3A00);
extern u8 Ref_D_002F3A28[] RETAIL(D_002F3A28);
extern u8 Ref_D_002F3A50[] RETAIL(D_002F3A50);
extern u8 Ref_D_002F3A78[] RETAIL(D_002F3A78);
extern u8 Ref_D_002F3A98[] RETAIL(D_002F3A98);
extern u8 Ref_D_002F3AC0[] RETAIL(D_002F3AC0);
extern u8 Ref_D_002F3AE8[] RETAIL(D_002F3AE8);
extern u8 Ref_D_002F3B10[] RETAIL(D_002F3B10);
extern u8 Ref_D_002F3B40[] RETAIL(D_002F3B40);
extern u8 Ref_D_002F3B68[] RETAIL(D_002F3B68);
extern u8 Ref_D_002F3B98[] RETAIL(D_002F3B98);
extern u8 Ref_D_002F3BC8[] RETAIL(D_002F3BC8);
extern u8 Ref_D_002F3BF0[] RETAIL(D_002F3BF0);
extern u8 Ref_D_002F4880[] RETAIL(D_002F4880);
extern u8 Ref_D_002F4898[] RETAIL(D_002F4898);
extern u8 Ref_D_002F48B0[] RETAIL(D_002F48B0);
extern u8 Ref_D_002F48C8[] RETAIL(D_002F48C8);
extern u8 Ref_D_002F48D8[] RETAIL(D_002F48D8);
extern u8 Ref_D_002F48E8[] RETAIL(D_002F48E8);
extern u8 Ref_D_002F4900[] RETAIL(D_002F4900);
extern u8 Ref_D_002F4910[] RETAIL(D_002F4910);
extern u8 Ref_D_002F4920[] RETAIL(D_002F4920);
extern u8 Ref_D_002F49F8[] RETAIL(D_002F49F8);
extern u8 Ref_D_002F4A08[] RETAIL(D_002F4A08);
extern u8 Ref_D_002F4A18[] RETAIL(D_002F4A18);
extern u8 Ref_D_002F4A28[] RETAIL(D_002F4A28);
extern u8 Ref_D_002F4A38[] RETAIL(D_002F4A38);
extern u8 Ref_D_002F4A48[] RETAIL(D_002F4A48);
extern u8 Ref_D_002F4A58[] RETAIL(D_002F4A58);
extern u8 Ref_D_002F4A68[] RETAIL(D_002F4A68);
extern u8 Ref_D_002F4A78[] RETAIL(D_002F4A78);
extern u8 Ref_D_002F4A88[] RETAIL(D_002F4A88);
extern u8 Ref_D_002F4A98[] RETAIL(D_002F4A98);
extern u8 Ref_D_002F4AA8[] RETAIL(D_002F4AA8);
extern u8 Ref_D_002F4AB8[] RETAIL(D_002F4AB8);
extern u8 Ref_D_002F4AC8[] RETAIL(D_002F4AC8);
extern u8 Ref_D_002F4AD8[] RETAIL(D_002F4AD8);
extern u8 Ref_D_002F4AF0[] RETAIL(D_002F4AF0);
extern u8 Ref_D_002F4B08[] RETAIL(D_002F4B08);
extern u8 Ref_D_002F4B20[] RETAIL(D_002F4B20);
extern u8 Ref_D_002F4B38[] RETAIL(D_002F4B38);
extern u8 Ref_D_002F4B48[] RETAIL(D_002F4B48);
extern u8 Ref_D_002F5A50[] RETAIL(D_002F5A50);
extern u8 Ref_D_002F5A60[] RETAIL(D_002F5A60);
extern u8 Ref_D_002F5A70[] RETAIL(D_002F5A70);
extern u8 Ref_D_002F5A80[] RETAIL(D_002F5A80);
extern u8 Ref_D_002F5A90[] RETAIL(D_002F5A90);
extern u8 Ref_D_002F5AA0[] RETAIL(D_002F5AA0);
extern u8 Ref_D_002F5AB0[] RETAIL(D_002F5AB0);
extern u8 Ref_D_002F5AC0[] RETAIL(D_002F5AC0);
extern u8 Ref_D_002F6FF8[] RETAIL(D_002F6FF8);
extern u8 Ref_D_002F73F8[] RETAIL(D_002F73F8);
extern u8 Ref_D_002F77F8[] RETAIL(D_002F77F8);
extern u8 Ref_D_002F7BF8[] RETAIL(D_002F7BF8);
extern u8 Ref_D_002F7FF8[] RETAIL(D_002F7FF8);
extern u8 Ref_D_002F83F8[] RETAIL(D_002F83F8);
extern u8 Ref_D_002F87F8[] RETAIL(D_002F87F8);
extern u8 Ref_D_002F8BF8[] RETAIL(D_002F8BF8);
extern u8 Ref_D_002F9990[] RETAIL(D_002F9990);
extern u8 Ref_D_002F99A0[] RETAIL(D_002F99A0);
extern u8 Ref_D_002F99B0[] RETAIL(D_002F99B0);
extern u8 Ref_D_002F99C8[] RETAIL(D_002F99C8);
extern u8 Ref_D_002F99D8[] RETAIL(D_002F99D8);
extern u8 Ref_D_002F99F0[] RETAIL(D_002F99F0);
extern u8 Ref_D_002F9A08[] RETAIL(D_002F9A08);
extern u8 Ref_D_002F9A20[] RETAIL(D_002F9A20);
extern u8 Ref_D_00305D18[] RETAIL(D_00305D18);
extern u8 Ref_D_00305D38[] RETAIL(D_00305D38);
extern u8 Ref_D_00305D58[] RETAIL(D_00305D58);
extern u8 Ref_D_00305D80[] RETAIL(D_00305D80);
extern u8 Ref_D_00305DA8[] RETAIL(D_00305DA8);
extern u8 Ref_D_00305DD0[] RETAIL(D_00305DD0);
extern u8 Ref_D_00305DF8[] RETAIL(D_00305DF8);
extern u8 Ref_D_00305E18[] RETAIL(D_00305E18);
extern u8 Ref_D_00305E38[] RETAIL(D_00305E38);
extern u8 Ref_D_00305E58[] RETAIL(D_00305E58);
extern u8 Ref_D_00305E78[] RETAIL(D_00305E78);
extern u8 Ref_D_00305E90[] RETAIL(D_00305E90);
extern u8 Ref_D_00305EA8[] RETAIL(D_00305EA8);
extern u8 Ref_D_00305EC8[] RETAIL(D_00305EC8);
extern u8 Ref_D_00305EE8[] RETAIL(D_00305EE8);
extern u8 Ref_D_00305F08[] RETAIL(D_00305F08);
extern u8 Ref_D_00305F18[] RETAIL(D_00305F18);
extern u8 Ref_D_00305F30[] RETAIL(D_00305F30);
extern u8 Ref_D_00305F48[] RETAIL(D_00305F48);
extern u8 Ref_D_00305F58[] RETAIL(D_00305F58);
extern u8 Ref_D_00305F70[] RETAIL(D_00305F70);
extern u8 Ref_D_00305F88[] RETAIL(D_00305F88);
extern u8 Ref_D_00305FA0[] RETAIL(D_00305FA0);
extern u8 Ref_D_00305FB0[] RETAIL(D_00305FB0);
extern u8 Ref_D_00305FC0[] RETAIL(D_00305FC0);
extern u8 Ref_D_00305FD0[] RETAIL(D_00305FD0);
extern u8 Ref_D_00305FE0[] RETAIL(D_00305FE0);
extern u8 Ref_D_00305FF0[] RETAIL(D_00305FF0);
extern u8 Ref_D_00306000[] RETAIL(D_00306000);
extern u8 Ref_D_00306018[] RETAIL(D_00306018);
extern u8 Ref_D_00308040[] RETAIL(D_00308040);
extern u8 Ref_D_00308048[] RETAIL(D_00308048);
extern u8 Ref_D_00308050[] RETAIL(D_00308050);
extern u8 Ref_D_00308058[] RETAIL(D_00308058);
extern u8 Ref_D_00308068[] RETAIL(D_00308068);
extern u8 Ref_D_00308070[] RETAIL(D_00308070);
extern u8 Ref_D_00308080[] RETAIL(D_00308080);
extern u8 Ref_D_00308088[] RETAIL(D_00308088);
extern u8 Ref_D_00308090[] RETAIL(D_00308090);
extern u8 Ref_D_00308638[] RETAIL(D_00308638);
extern u8 Ref_D_00309168[] RETAIL(D_00309168);
extern u8 Ref_D_00309380[] RETAIL(D_00309380);
extern u8 Ref_D_003098A8[] RETAIL(D_003098A8);
extern u8 Ref_D_003098B0[] RETAIL(D_003098B0);
extern u8 Ref_D_003098B8[] RETAIL(D_003098B8);
extern u8 Ref_D_003098C0[] RETAIL(D_003098C0);
extern u8 Ref_D_003098C8[] RETAIL(D_003098C8);
extern u8 Ref_D_003099B0[] RETAIL(D_003099B0);
extern u8 Ref_D_003099B8[] RETAIL(D_003099B8);
extern u8 Ref_D_003099C0[] RETAIL(D_003099C0);
extern u8 Ref_D_003099C8[] RETAIL(D_003099C8);
extern u8 Ref_D_003099D0[] RETAIL(D_003099D0);
extern u8 Ref_D_003099D8[] RETAIL(D_003099D8);
extern u8 Ref_D_003099E0[] RETAIL(D_003099E0);
extern u8 Ref_D_003099E8[] RETAIL(D_003099E8);
extern u8 Ref_D_003099F0[] RETAIL(D_003099F0);
extern u8 Ref_D_003099F8[] RETAIL(D_003099F8);
extern u8 Ref_D_00309A00[] RETAIL(D_00309A00);
extern u8 Ref_D_00309A08[] RETAIL(D_00309A08);
extern u8 Ref_D_00309A10[] RETAIL(D_00309A10);
extern u8 Ref_D_00309A18[] RETAIL(D_00309A18);
extern u8 Ref_D_00309A20[] RETAIL(D_00309A20);
extern u8 Ref_D_00309A28[] RETAIL(D_00309A28);
extern u8 Ref_D_00309A30[] RETAIL(D_00309A30);
extern u8 Ref_D_00309A38[] RETAIL(D_00309A38);
extern u8 Ref_D_0030A288[] RETAIL(D_0030A288);
extern u8 Ref_D_0030A310[] RETAIL(D_0030A310);
extern u8 Ref_D_0030A318[] RETAIL(D_0030A318);
extern u8 Ref_D_0030A320[] RETAIL(D_0030A320);
extern u8 Ref_D_0030A328[] RETAIL(D_0030A328);
extern u8 Ref_D_0030A330[] RETAIL(D_0030A330);
extern u8 Ref_D_0030A338[] RETAIL(D_0030A338);
extern u8 Ref_D_00332540[] RETAIL(D_00332540);
extern u8 Ref_D_003A0824[] RETAIL(D_003A0824);
extern u8 Ref_D_003C9FA8[] RETAIL(D_003C9FA8);
extern u8 Ref_D_003CA028[] RETAIL(D_003CA028);
extern u8 Ref_FUN_001016f8[] RETAIL(FUN_001016f8);
extern u8 Ref_FUN_001230c8[] RETAIL(FUN_001230c8);
extern u8 Ref_FUN_0012d500[] RETAIL(FUN_0012d500);
extern u8 Ref_FUN_00141ce8[] RETAIL(FUN_00141ce8);
extern u8 Ref_FUN_00162338[] RETAIL(FUN_00162338);
extern u8 Ref_FUN_00167140[] RETAIL(FUN_00167140);
extern u8 Ref_FUN_0017ca00[] RETAIL(FUN_0017ca00);
extern u8 Ref_FUN_0017ca50[] RETAIL(FUN_0017ca50);
extern u8 Ref_FUN_00182490[] RETAIL(FUN_00182490);
extern u8 Ref_FUN_0018f7b8[] RETAIL(FUN_0018f7b8);
extern u8 Ref_FUN_0019b148[] RETAIL(FUN_0019b148);
extern u8 Ref_FUN_001a6678[] RETAIL(FUN_001a6678);
extern u8 Ref_FUN_001ad368[] RETAIL(FUN_001ad368);
extern u8 Ref_FUN_001ba330[] RETAIL(FUN_001ba330);
extern u8 Ref_FUN_001c75a0[] RETAIL(FUN_001c75a0);
extern u8 Ref_FUN_001cccf0[] RETAIL(FUN_001cccf0);
extern u8 Ref_FUN_001dd4d0[] RETAIL(FUN_001dd4d0);
extern u8 Ref_FUN_001e8ee8[] RETAIL(FUN_001e8ee8);
extern u8 Ref_FUN_001f4540[] RETAIL(FUN_001f4540);
extern u8 Ref_FUN_001f68b8[] RETAIL(FUN_001f68b8);
extern u8 Ref_FUN_00201b70[] RETAIL(FUN_00201b70);
extern u8 Ref_FUN_00209ed8[] RETAIL(FUN_00209ed8);
extern u8 Ref_FUN_0020ffa0[] RETAIL(FUN_0020ffa0);
extern u8 Ref_FUN_00225878[] RETAIL(FUN_00225878);
extern u8 Ref_FUN_0022c2c8[] RETAIL(FUN_0022c2c8);
extern u8 Ref_FUN_00240bc8[] RETAIL(FUN_00240bc8);
extern u8 Ref_FUN_00255430[] RETAIL(FUN_00255430);
extern u8 Ref_FUN_0025d078[] RETAIL(FUN_0025d078);
extern u8 Ref_FUN_00263d88[] RETAIL(FUN_00263d88);
extern u8 Ref_FUN_0026eb60[] RETAIL(FUN_0026eb60);
extern u8 Ref_FUN_0027ea00[] RETAIL(FUN_0027ea00);
extern u8 Ref_FUN_00282980[] RETAIL(FUN_00282980);
extern u8 Ref_FUN_00288930[] RETAIL(FUN_00288930);
extern u8 Ref_FUN_00293320[] RETAIL(FUN_00293320);
extern u8 Ref_FUN_0029f600[] RETAIL(FUN_0029f600);
extern u8 Ref_FUN_002b2230[] RETAIL(FUN_002b2230);
extern u8 Ref_FUN_002b4448[] RETAIL(FUN_002b4448);
extern u8 Ref_FUN_002b7850[] RETAIL(FUN_002b7850);
extern u8 Ref_GenCode1_VelocityXZ2xStart[] RETAIL(GenCode1_VelocityXZ2xStart);
extern u8 Ref_GenCode2_VelocityXZMinusStart[] RETAIL(GenCode2_VelocityXZMinusStart);
extern u8 Ref_GenCode3_VelocityXZ4xStart[] RETAIL(GenCode3_VelocityXZ4xStart);
extern u8 Ref_GenCode4_VelocityXZ16xStart[] RETAIL(GenCode4_VelocityXZ16xStart);
extern u8 Ref_GenCode5_PullInRandomLife[] RETAIL(GenCode5_PullInRandomLife);
extern u8 Ref_GenCode6_Velocity5_4xStart[] RETAIL(GenCode6_Velocity5_4xStart);
extern u8 Ref_GenParticle_Bounce[] RETAIL(GenParticle_Bounce);
extern u8 Ref_GenParticle_BounceXZ[] RETAIL(GenParticle_BounceXZ);
extern u8 Ref_GenParticle_Box[] RETAIL(GenParticle_Box);
extern u8 Ref_GenParticle_Box2[] RETAIL(GenParticle_Box2);
extern u8 Ref_GenParticle_Line[] RETAIL(GenParticle_Line);
extern u8 Ref_GenParticle_Radial[] RETAIL(GenParticle_Radial);
extern u8 Ref_GenParticle_RadialRotor[] RETAIL(GenParticle_RadialRotor);
extern u8 Ref_GenParticle_Ranges[] RETAIL(GenParticle_Ranges);
extern u8 Ref_GenParticle_RangesRandomLife[] RETAIL(GenParticle_RangesRandomLife);
extern u8 Ref_GenParticle_Reuse[] RETAIL(GenParticle_Reuse);
extern u8 Ref_GenParticle_Sphere[] RETAIL(GenParticle_Sphere);
extern u8 Ref_GenParticle_Spheroid[] RETAIL(GenParticle_Spheroid);
extern u8 Ref_GenParticle_Star[] RETAIL(GenParticle_Star);
extern u8 Ref__end[] RETAIL(_end);
extern u8 Ref_func_002BF748[] RETAIL(func_002BF748);
extern u8 Ref_func_002BF7C0[] RETAIL(func_002BF7C0);
extern u8 Ref_func_002BF858[] RETAIL(func_002BF858);
extern u8 Ref_func_002BF910[] RETAIL(func_002BF910);
extern u8 Ref_func_002BF9E0[] RETAIL(func_002BF9E0);
extern u8 Ref_func_002BFA90[] RETAIL(func_002BFA90);
extern u8 Ref_func_002BFB48[] RETAIL(func_002BFB48);
extern u8 Ref_func_002BFC40[] RETAIL(func_002BFC40);
extern u8 Ref_func_002BFD40[] RETAIL(func_002BFD40);
extern u8 Ref_func_002BFDE0[] RETAIL(func_002BFDE0);
extern u8 Ref_func_002BFE90[] RETAIL(func_002BFE90);
extern u8 Ref_func_002BFF70[] RETAIL(func_002BFF70);
extern u8 Ref_func_002C0058[] RETAIL(func_002C0058);
extern u8 Ref_func_002C0130[] RETAIL(func_002C0130);
extern u8 Ref_func_002C0200[] RETAIL(func_002C0200);
extern u8 Ref_func_002C0320[] RETAIL(func_002C0320);
extern u8 Ref_kCopy[] RETAIL(kCopy);

namespace RetailData
{
struct D_002E6F58_Row
{
    const void* v0;
    u32 v1;
};
struct D_002E70F8_Row
{
    const void* v0;
    u32 v1;
};
struct D_002E7198_Fields
{
    const void* v0;
    u32 v1;
    u32 v2;
    u32 v3;
    u32 v4;
    u32 v5;
};
struct D_002E8414_Fields
{
    u32 v0;
    const void* v1;
    const void* v2;
    const void* v3;
    const void* v4;
    const void* v5;
    const void* v6;
    const void* v7;
    const void* v8;
    const void* v9;
    const void* v10;
    const void* v11;
    u32 v12;
    u32 v13;
    u32 v14;
    u32 v15;
    u32 v16;
    u32 v17;
    u32 v18;
    u32 v19;
    u32 v20;
    u32 v21;
    u32 v22;
    u32 v23;
    u32 v24;
    u32 v25;
    u32 v26;
    u32 v27;
    u32 v28;
    u32 v29;
    u32 v30;
};
struct D_002EB488_Fields
{
    u32 v0;
    u32 v1;
    u32 v2;
    u32 v3;
    u32 v4;
    u32 v5;
    u32 v6;
    u32 v7;
    u32 v8;
    u32 v9;
    u32 v10;
    u32 v11;
    u32 v12;
    u32 v13;
    u32 v14;
    u32 v15;
    u32 v16;
    u32 v17;
    u32 v18;
    u32 v19;
    u32 v20;
    u32 v21;
    u32 v22;
    u32 v23;
    u32 v24;
    u32 v25;
    u32 v26;
    u32 v27;
    u32 v28;
    u32 v29;
    u32 v30;
    u32 v31;
    u32 v32;
    u32 v33;
    u32 v34;
    u32 v35;
    u32 v36;
    u32 v37;
    u32 v38;
    u32 v39;
    u32 v40;
    u32 v41;
    u32 v42;
    u32 v43;
    u32 v44;
    u32 v45;
    u32 v46;
    u32 v47;
    u32 v48;
    u32 v49;
    u32 v50;
    u32 v51;
    u32 v52;
    u32 v53;
    u32 v54;
    u32 v55;
    u32 v56;
    u32 v57;
    u32 v58;
    u32 v59;
    u32 v60;
    u32 v61;
    u32 v62;
    u32 v63;
    u32 v64;
    u32 v65;
    u32 v66;
    u32 v67;
    u32 v68;
    u32 v69;
    u32 v70;
    u32 v71;
    u32 v72;
    u32 v73;
    u32 v74;
    u32 v75;
    u32 v76;
    u32 v77;
    u32 v78;
    u32 v79;
    u32 v80;
    u32 v81;
    u32 v82;
    u32 v83;
    u32 v84;
    u32 v85;
    u32 v86;
    u32 v87;
    u32 v88;
    u32 v89;
    u32 v90;
    u32 v91;
    u32 v92;
    u32 v93;
    u32 v94;
    u32 v95;
    u32 v96;
    u32 v97;
    u32 v98;
    u32 v99;
    u32 v100;
    u32 v101;
    u32 v102;
    u32 v103;
    u32 v104;
    u32 v105;
    u32 v106;
    u32 v107;
    u32 v108;
    u32 v109;
    u32 v110;
    u32 v111;
    u32 v112;
    u32 v113;
    u32 v114;
    u32 v115;
    u32 v116;
    u32 v117;
    u32 v118;
    u32 v119;
    u32 v120;
    u32 v121;
    u32 v122;
    u32 v123;
    u32 v124;
    u32 v125;
    u32 v126;
    u32 v127;
    u32 v128;
    u32 v129;
    u32 v130;
    u32 v131;
    u32 v132;
    u32 v133;
    u32 v134;
    u32 v135;
    u32 v136;
    u32 v137;
    u32 v138;
    u32 v139;
    u32 v140;
    u32 v141;
    u32 v142;
    u32 v143;
    u32 v144;
    u32 v145;
    u32 v146;
    u32 v147;
    u32 v148;
    u32 v149;
    u32 v150;
    u32 v151;
    u32 v152;
    u32 v153;
    u32 v154;
    u32 v155;
    u32 v156;
    u32 v157;
    u32 v158;
    u32 v159;
    u32 v160;
    u32 v161;
    u32 v162;
    u32 v163;
    u32 v164;
    u32 v165;
    u32 v166;
    u32 v167;
    u32 v168;
    u32 v169;
    u32 v170;
    u32 v171;
    u32 v172;
    u32 v173;
    u32 v174;
    u32 v175;
    u32 v176;
    u32 v177;
    u32 v178;
    u32 v179;
    u32 v180;
    u32 v181;
    u32 v182;
    u32 v183;
    u32 v184;
    u32 v185;
    u32 v186;
    u32 v187;
    u32 v188;
    u32 v189;
    u32 v190;
    u32 v191;
    u32 v192;
    u32 v193;
    u32 v194;
    u32 v195;
    u32 v196;
    u32 v197;
    u32 v198;
    u32 v199;
    u32 v200;
    u32 v201;
    u32 v202;
    u32 v203;
    u32 v204;
    u32 v205;
    u32 v206;
    u32 v207;
    u32 v208;
    u32 v209;
    u32 v210;
    u32 v211;
    u32 v212;
    u32 v213;
    u32 v214;
    u32 v215;
    u32 v216;
    u32 v217;
    u32 v218;
    u32 v219;
    u32 v220;
    u32 v221;
    u32 v222;
    u32 v223;
    u32 v224;
    u32 v225;
    u32 v226;
    u32 v227;
    u32 v228;
    u32 v229;
    u32 v230;
    u32 v231;
    u32 v232;
    u32 v233;
    u32 v234;
    u32 v235;
    u32 v236;
    u32 v237;
    u32 v238;
    u32 v239;
    u32 v240;
    u32 v241;
    u32 v242;
    u32 v243;
    u32 v244;
    u32 v245;
    u32 v246;
    u32 v247;
    u32 v248;
    u32 v249;
    u32 v250;
    u32 v251;
    u32 v252;
    u32 v253;
    u32 v254;
    u32 v255;
    u32 v256;
    u32 v257;
    u32 v258;
    u32 v259;
    u32 v260;
    u32 v261;
    u32 v262;
    u32 v263;
    u32 v264;
    u32 v265;
    u32 v266;
    u32 v267;
    u32 v268;
    u32 v269;
    u32 v270;
    u32 v271;
    u32 v272;
    u32 v273;
    u32 v274;
    u32 v275;
    u32 v276;
    u32 v277;
    u32 v278;
    u32 v279;
    u32 v280;
    u32 v281;
    u32 v282;
    u32 v283;
    u32 v284;
    u32 v285;
    u32 v286;
    u32 v287;
    u32 v288;
    u32 v289;
    u32 v290;
    u32 v291;
    u32 v292;
    u32 v293;
    u32 v294;
    u32 v295;
    u32 v296;
    u32 v297;
    u32 v298;
    u32 v299;
    u32 v300;
    u32 v301;
    u32 v302;
    u32 v303;
    u32 v304;
    u32 v305;
    u32 v306;
    u32 v307;
    u32 v308;
    u32 v309;
    u32 v310;
    u32 v311;
    u32 v312;
    u32 v313;
    u32 v314;
    u32 v315;
    u32 v316;
    u32 v317;
    u32 v318;
    u32 v319;
    u32 v320;
    u32 v321;
    u32 v322;
    u32 v323;
    u32 v324;
    u32 v325;
    u32 v326;
    u32 v327;
    u32 v328;
    u32 v329;
    u32 v330;
    u32 v331;
    u32 v332;
    u32 v333;
    u32 v334;
    u32 v335;
    u32 v336;
    u32 v337;
    u32 v338;
    u32 v339;
    u32 v340;
    u32 v341;
    u32 v342;
    u32 v343;
    u32 v344;
    u32 v345;
    u32 v346;
    u32 v347;
    u32 v348;
    u32 v349;
    u32 v350;
    u32 v351;
    u32 v352;
    u32 v353;
    u32 v354;
    u32 v355;
    u32 v356;
    u32 v357;
    u32 v358;
    u32 v359;
    u32 v360;
    u32 v361;
    u32 v362;
    u32 v363;
    u32 v364;
    u32 v365;
    u32 v366;
    u32 v367;
    u32 v368;
    u32 v369;
    u32 v370;
    u32 v371;
    u32 v372;
    u32 v373;
    u32 v374;
    u32 v375;
    u32 v376;
    u32 v377;
    u32 v378;
    u32 v379;
    u32 v380;
    u32 v381;
    u32 v382;
    u32 v383;
    u32 v384;
    u32 v385;
    u32 v386;
    u32 v387;
    u32 v388;
    u32 v389;
    u32 v390;
    u32 v391;
    u32 v392;
    u32 v393;
    u32 v394;
    u32 v395;
    u32 v396;
    u32 v397;
    u32 v398;
    u32 v399;
    u32 v400;
    u32 v401;
    u32 v402;
    u32 v403;
    u32 v404;
    u32 v405;
    u32 v406;
    u32 v407;
    u32 v408;
    u32 v409;
    u32 v410;
    u32 v411;
    u32 v412;
    u32 v413;
    u32 v414;
    u32 v415;
    u32 v416;
    u32 v417;
    u32 v418;
    u32 v419;
    u32 v420;
    u32 v421;
    u32 v422;
    u32 v423;
    u32 v424;
    u32 v425;
    u32 v426;
    u32 v427;
    u32 v428;
    u32 v429;
    u32 v430;
    u32 v431;
    u32 v432;
    u32 v433;
    const void* v434;
    u32 v435;
    u32 v436;
    u32 v437;
    u32 v438;
    u32 v439;
    u32 v440;
    u32 v441;
    u32 v442;
    u32 v443;
    u32 v444;
    u32 v445;
    u32 v446;
    u32 v447;
    u32 v448;
    u32 v449;
    u32 v450;
    u32 v451;
    u32 v452;
    u32 v453;
    u32 v454;
    u32 v455;
    u32 v456;
    u32 v457;
    u32 v458;
    u32 v459;
    u32 v460;
    u32 v461;
    u32 v462;
    u32 v463;
};
struct D_002EC288_Fields
{
    u32 v0;
    u32 v1;
    u32 v2;
    u32 v3;
    const void* v4;
    const void* v5;
    const void* v6;
};

extern const void* GlobalLanguagesArray[1] RETAIL(GlobalLanguagesArray);
extern const void* D_002E6F04[5] RETAIL(D_002E6F04);
extern u32 D_002E6F18[16] RETAIL(D_002E6F18);
extern D_002E6F58_Row D_002E6F58[16] RETAIL(D_002E6F58);
extern s32 D_002E6FD8[26] RETAIL(D_002E6FD8);
extern u32 D_002E7040[8] RETAIL(D_002E7040);
extern const void* D_002E7060[26] RETAIL(D_002E7060);
extern const void* D_002E70C8[12] RETAIL(D_002E70C8);
extern D_002E70F8_Row D_002E70F8[20] RETAIL(D_002E70F8);
extern D_002E7198_Fields D_002E7198 RETAIL(D_002E7198);
extern u32 D_002E71B0[1] RETAIL(D_002E71B0);
extern u8 D_002E71B4[2] RETAIL(D_002E71B4);
extern u8 D_002E71B6[2] RETAIL(D_002E71B6);
extern u32 D_002E71B8[4] RETAIL(D_002E71B8);
extern const void* G_SonyLibsNames[8] RETAIL(G_SonyLibsNames);
extern u32 D_002E71E8[2] RETAIL(D_002E71E8);
extern char G_VVU_Games_String[32] RETAIL(G_VVU_Games_String);
extern u32 D_002E7210[56] RETAIL(D_002E7210);
extern s32 REQ_CHECKSUM[1] RETAIL(REQ_CHECKSUM);
extern u32 D_002E72F4[1] RETAIL(D_002E72F4);
extern u32 DMA_CHANNELS_CHCR_REG[1] RETAIL(DMA_CHANNELS_CHCR_REG);
extern u32 D_002E72FC[17] RETAIL(D_002E72FC);
extern f32 G_ClockSpeedScales[8] RETAIL(G_ClockSpeedScales);
extern u32 G_Colors[24] RETAIL(G_Colors);
extern const void* D_002E73C0[8] RETAIL(D_002E73C0);
extern const void* G_NullParticlePtr[1] RETAIL(G_NullParticlePtr);
extern u32 G_LoadedParticles[299] RETAIL(G_LoadedParticles);
extern const void* ParticleGeneratorTable[1] RETAIL(ParticleGeneratorTable);
extern const void* D_002E7894[13] RETAIL(D_002E7894);
extern const void* ParticleGenCodeTable[8] RETAIL(ParticleGenCodeTable);
extern s32 D_002E78E8[4] RETAIL(D_002E78E8);
extern const void* D_002E78F8[8] RETAIL(D_002E78F8);
extern u32 D_002E7918[4] RETAIL(D_002E7918);
extern u32 D_002E7928[8] RETAIL(D_002E7928);
extern u32 D_002E7948[24] RETAIL(D_002E7948);
extern u32 D_002E79A8[24] RETAIL(D_002E79A8);
extern u32 D_002E7A08[12] RETAIL(D_002E7A08);
extern u32 D_002E7A38[48] RETAIL(D_002E7A38);
extern u32 D_002E7AF8[48] RETAIL(D_002E7AF8);
extern u32 D_002E7BB8[48] RETAIL(D_002E7BB8);
extern u32 D_002E7C78[48] RETAIL(D_002E7C78);
extern u32 D_002E7D38[10] RETAIL(D_002E7D38);
extern u32 D_002E7D60[12] RETAIL(D_002E7D60);
extern u32 D_002E7D90[12] RETAIL(D_002E7D90);
extern u32 D_002E7DC0[24] RETAIL(D_002E7DC0);
extern u32 D_002E7E20[24] RETAIL(D_002E7E20);
extern u32 D_002E7E80[48] RETAIL(D_002E7E80);
extern u32 D_002E7F40[48] RETAIL(D_002E7F40);
extern s32 D_002E8000[4] RETAIL(D_002E8000);
extern const void* D_002E8010[38] RETAIL(D_002E8010);
extern s32 D_002E80A8[14] RETAIL(D_002E80A8);
extern s32 D_002E80E0[16] RETAIL(D_002E80E0);
extern u32 D_002E8120[1] RETAIL(D_002E8120);
extern u32 D_002E8124[3] RETAIL(D_002E8124);
extern u32 CHCR_ARRAY[1] RETAIL(CHCR_ARRAY);
extern u32 D_002E8134[9] RETAIL(D_002E8134);
extern u32 D_002E8158[16] RETAIL(D_002E8158);
extern u32 D_002E8198[40] RETAIL(D_002E8198);
extern const void* jtbl_002E8238[16] RETAIL(jtbl_002E8238);
extern u32 D_002E8278[2] RETAIL(D_002E8278);
extern u32 D_002E8280[16] RETAIL(D_002E8280);
extern u32 D_002E82C0[20] RETAIL(D_002E82C0);
extern u32 D_002E8310[20] RETAIL(D_002E8310);
extern u32 D_002E8360[8] RETAIL(D_002E8360);
extern u32 D_002E8380[4] RETAIL(D_002E8380);
extern u32 D_002E8390[8] RETAIL(D_002E8390);
extern u32 D_002E83B0[8] RETAIL(D_002E83B0);
extern u32 DMA_N_CHANNELS[1] RETAIL(DMA_N_CHANNELS);
extern u32 D_002E83D4[13] RETAIL(D_002E83D4);
extern u32 D_002E8408[1] RETAIL(D_002E8408);
extern u32 D_002E840C[1] RETAIL(D_002E840C);
extern u32 D_002E8410[1] RETAIL(D_002E8410);
extern D_002E8414_Fields D_002E8414 RETAIL(D_002E8414);
extern u32 D_002E8490[1] RETAIL(D_002E8490);
extern u32 D_002E8494[1] RETAIL(D_002E8494);
extern u32 D_002E8498[1] RETAIL(D_002E8498);
extern u32 D_002E849C[1] RETAIL(D_002E849C);
extern u32 G_Sema_CDVD_Callback[1] RETAIL(G_Sema_CDVD_Callback);
extern u32 D_002E84A4[1] RETAIL(D_002E84A4);
extern u32 G_Sema_CDVD_AsyncCommand[1] RETAIL(G_Sema_CDVD_AsyncCommand);
extern u32 G_Sema_CDVD_SyncCommand[1] RETAIL(G_Sema_CDVD_SyncCommand);
extern u32 G_Sema_CDVD_CommandStatus_[1] RETAIL(G_Sema_CDVD_CommandStatus_);
extern u32 D_002E84B4[1] RETAIL(D_002E84B4);
extern u32 D_002E84B8[1] RETAIL(D_002E84B8);
extern u32 D_002E84BC[1] RETAIL(D_002E84BC);
extern u32 D_002E84C0[1] RETAIL(D_002E84C0);
extern u32 D_002E84C4[1] RETAIL(D_002E84C4);
extern u32 D_002E84C8[1] RETAIL(D_002E84C8);
extern u32 D_002E84CC[1] RETAIL(D_002E84CC);
extern u32 D_002E84D0[1] RETAIL(D_002E84D0);
extern u32 D_002E84D4[1] RETAIL(D_002E84D4);
extern u32 D_002E84D8[1] RETAIL(D_002E84D8);
extern u32 D_002E84DC[1] RETAIL(D_002E84DC);
extern u32 D_002E84E0[8] RETAIL(D_002E84E0);
extern u32 D_002E8500[32] RETAIL(D_002E8500);
extern u32 D_002E8580[1076] RETAIL(D_002E8580);
extern u32 D_002E9650[12] RETAIL(D_002E9650);
extern u32 D_002E9680[272] RETAIL(D_002E9680);
extern u32 G_MediaType[1] RETAIL(G_MediaType);
extern u32 D_002E9AC4[271] RETAIL(D_002E9AC4);
extern u32 D_002E9F00[64] RETAIL(D_002E9F00);
extern u32 D_002EA000[10] RETAIL(D_002EA000);
extern u32 D_002EA028[1] RETAIL(D_002EA028);
extern u32 D_002EA02C[1] RETAIL(D_002EA02C);
extern u32 D_002EA030[1] RETAIL(D_002EA030);
extern u32 D_002EA034[1] RETAIL(D_002EA034);
extern u32 D_002EA038[1] RETAIL(D_002EA038);
extern u32 D_002EA03C[5] RETAIL(D_002EA03C);
extern u32 D_002EA050[1] RETAIL(D_002EA050);
extern u32 D_002EA054[1] RETAIL(D_002EA054);
extern u32 D_002EA058[1] RETAIL(D_002EA058);
extern u32 D_002EA05C[1] RETAIL(D_002EA05C);
extern u32 D_002EA060[1] RETAIL(D_002EA060);
extern u32 D_002EA064[1] RETAIL(D_002EA064);
extern u32 D_002EA068[1] RETAIL(D_002EA068);
extern u32 D_002EA06C[1] RETAIL(D_002EA06C);
extern u32 D_002EA070[6] RETAIL(D_002EA070);
extern u32 D_002EA088[1] RETAIL(D_002EA088);
extern u32 G_SemaSceMc_SemaRegs[1] RETAIL(G_SemaSceMc_SemaRegs);
extern u32 G_SemaSceMc_SemaTimer[1] RETAIL(G_SemaSceMc_SemaTimer);
extern u32 D_002EA094[3] RETAIL(D_002EA094);
extern u32 D_002EA0A0[16] RETAIL(D_002EA0A0);
extern const void* D_002EA0E0[22] RETAIL(D_002EA0E0);
extern u32 CurrentSeed[1] RETAIL(CurrentSeed);
extern u32 D_002EA13C[98] RETAIL(D_002EA13C);
extern u32 D_002EA2C4[22] RETAIL(D_002EA2C4);
extern u32 D_002EA31C[22] RETAIL(D_002EA31C);
extern u32 D_002EA374[22] RETAIL(D_002EA374);
extern const void* D_002EA3CC[1] RETAIL(D_002EA3CC);
extern u32 G_WaitingThreadId_[1] RETAIL(G_WaitingThreadId_);
extern u32 G_WaitingThreadCount_[1] RETAIL(G_WaitingThreadCount_);
extern char D_002EA3D8[40] RETAIL(D_002EA3D8);
extern u32 D_002EA400[1] RETAIL(D_002EA400);
extern const void* D_002EA404[1] RETAIL(D_002EA404);
extern u32 D_002EA408[1] RETAIL(D_002EA408);
extern const void* D_002EA40C[1] RETAIL(D_002EA40C);
extern u32 D_002EA410[2] RETAIL(D_002EA410);
extern const void* D_002EA418[2] RETAIL(D_002EA418);
extern const void* D_002EA420[2] RETAIL(D_002EA420);
extern const void* D_002EA428[2] RETAIL(D_002EA428);
extern const void* D_002EA430[2] RETAIL(D_002EA430);
extern const void* D_002EA438[2] RETAIL(D_002EA438);
extern const void* D_002EA440[2] RETAIL(D_002EA440);
extern const void* D_002EA448[2] RETAIL(D_002EA448);
extern const void* D_002EA450[2] RETAIL(D_002EA450);
extern const void* D_002EA458[2] RETAIL(D_002EA458);
extern const void* D_002EA460[2] RETAIL(D_002EA460);
extern const void* D_002EA468[2] RETAIL(D_002EA468);
extern const void* D_002EA470[2] RETAIL(D_002EA470);
extern const void* D_002EA478[2] RETAIL(D_002EA478);
extern const void* D_002EA480[2] RETAIL(D_002EA480);
extern const void* D_002EA488[2] RETAIL(D_002EA488);
extern const void* D_002EA490[2] RETAIL(D_002EA490);
extern const void* D_002EA498[2] RETAIL(D_002EA498);
extern const void* D_002EA4A0[2] RETAIL(D_002EA4A0);
extern const void* D_002EA4A8[2] RETAIL(D_002EA4A8);
extern const void* D_002EA4B0[2] RETAIL(D_002EA4B0);
extern const void* D_002EA4B8[2] RETAIL(D_002EA4B8);
extern const void* D_002EA4C0[2] RETAIL(D_002EA4C0);
extern const void* D_002EA4C8[2] RETAIL(D_002EA4C8);
extern const void* D_002EA4D0[2] RETAIL(D_002EA4D0);
extern const void* D_002EA4D8[2] RETAIL(D_002EA4D8);
extern const void* D_002EA4E0[2] RETAIL(D_002EA4E0);
extern const void* D_002EA4E8[2] RETAIL(D_002EA4E8);
extern const void* D_002EA4F0[2] RETAIL(D_002EA4F0);
extern const void* D_002EA4F8[2] RETAIL(D_002EA4F8);
extern const void* D_002EA500[2] RETAIL(D_002EA500);
extern const void* D_002EA508[2] RETAIL(D_002EA508);
extern const void* D_002EA510[2] RETAIL(D_002EA510);
extern const void* D_002EA518[2] RETAIL(D_002EA518);
extern const void* D_002EA520[2] RETAIL(D_002EA520);
extern const void* D_002EA528[2] RETAIL(D_002EA528);
extern const void* D_002EA530[2] RETAIL(D_002EA530);
extern const void* D_002EA538[2] RETAIL(D_002EA538);
extern const void* D_002EA540[2] RETAIL(D_002EA540);
extern const void* D_002EA548[2] RETAIL(D_002EA548);
extern const void* D_002EA550[2] RETAIL(D_002EA550);
extern const void* D_002EA558[2] RETAIL(D_002EA558);
extern const void* D_002EA560[2] RETAIL(D_002EA560);
extern const void* D_002EA568[2] RETAIL(D_002EA568);
extern const void* D_002EA570[2] RETAIL(D_002EA570);
extern const void* D_002EA578[2] RETAIL(D_002EA578);
extern const void* D_002EA580[2] RETAIL(D_002EA580);
extern const void* D_002EA588[2] RETAIL(D_002EA588);
extern const void* D_002EA590[2] RETAIL(D_002EA590);
extern const void* D_002EA598[2] RETAIL(D_002EA598);
extern const void* D_002EA5A0[2] RETAIL(D_002EA5A0);
extern const void* D_002EA5A8[2] RETAIL(D_002EA5A8);
extern const void* D_002EA5B0[2] RETAIL(D_002EA5B0);
extern const void* D_002EA5B8[2] RETAIL(D_002EA5B8);
extern const void* D_002EA5C0[2] RETAIL(D_002EA5C0);
extern const void* D_002EA5C8[2] RETAIL(D_002EA5C8);
extern const void* D_002EA5D0[2] RETAIL(D_002EA5D0);
extern const void* D_002EA5D8[2] RETAIL(D_002EA5D8);
extern const void* D_002EA5E0[2] RETAIL(D_002EA5E0);
extern const void* D_002EA5E8[2] RETAIL(D_002EA5E8);
extern const void* D_002EA5F0[2] RETAIL(D_002EA5F0);
extern const void* D_002EA5F8[2] RETAIL(D_002EA5F8);
extern const void* D_002EA600[2] RETAIL(D_002EA600);
extern const void* D_002EA608[2] RETAIL(D_002EA608);
extern const void* D_002EA610[2] RETAIL(D_002EA610);
extern const void* D_002EA618[2] RETAIL(D_002EA618);
extern const void* D_002EA620[2] RETAIL(D_002EA620);
extern const void* D_002EA628[2] RETAIL(D_002EA628);
extern const void* D_002EA630[2] RETAIL(D_002EA630);
extern const void* D_002EA638[2] RETAIL(D_002EA638);
extern const void* D_002EA640[2] RETAIL(D_002EA640);
extern const void* D_002EA648[2] RETAIL(D_002EA648);
extern const void* D_002EA650[2] RETAIL(D_002EA650);
extern const void* D_002EA658[2] RETAIL(D_002EA658);
extern const void* D_002EA660[2] RETAIL(D_002EA660);
extern const void* D_002EA668[2] RETAIL(D_002EA668);
extern const void* D_002EA670[2] RETAIL(D_002EA670);
extern const void* D_002EA678[2] RETAIL(D_002EA678);
extern const void* D_002EA680[2] RETAIL(D_002EA680);
extern const void* D_002EA688[2] RETAIL(D_002EA688);
extern const void* D_002EA690[2] RETAIL(D_002EA690);
extern const void* D_002EA698[2] RETAIL(D_002EA698);
extern const void* D_002EA6A0[2] RETAIL(D_002EA6A0);
extern const void* D_002EA6A8[2] RETAIL(D_002EA6A8);
extern const void* D_002EA6B0[2] RETAIL(D_002EA6B0);
extern const void* D_002EA6B8[2] RETAIL(D_002EA6B8);
extern const void* D_002EA6C0[2] RETAIL(D_002EA6C0);
extern const void* D_002EA6C8[2] RETAIL(D_002EA6C8);
extern const void* D_002EA6D0[2] RETAIL(D_002EA6D0);
extern const void* D_002EA6D8[2] RETAIL(D_002EA6D8);
extern const void* D_002EA6E0[2] RETAIL(D_002EA6E0);
extern const void* D_002EA6E8[2] RETAIL(D_002EA6E8);
extern const void* D_002EA6F0[2] RETAIL(D_002EA6F0);
extern const void* D_002EA6F8[2] RETAIL(D_002EA6F8);
extern const void* D_002EA700[2] RETAIL(D_002EA700);
extern const void* D_002EA708[2] RETAIL(D_002EA708);
extern const void* D_002EA710[2] RETAIL(D_002EA710);
extern const void* D_002EA718[2] RETAIL(D_002EA718);
extern const void* D_002EA720[2] RETAIL(D_002EA720);
extern const void* D_002EA728[2] RETAIL(D_002EA728);
extern const void* D_002EA730[2] RETAIL(D_002EA730);
extern const void* D_002EA738[2] RETAIL(D_002EA738);
extern const void* D_002EA740[2] RETAIL(D_002EA740);
extern const void* D_002EA748[2] RETAIL(D_002EA748);
extern const void* D_002EA750[2] RETAIL(D_002EA750);
extern const void* D_002EA758[2] RETAIL(D_002EA758);
extern const void* D_002EA760[2] RETAIL(D_002EA760);
extern const void* D_002EA768[2] RETAIL(D_002EA768);
extern const void* D_002EA770[2] RETAIL(D_002EA770);
extern const void* D_002EA778[2] RETAIL(D_002EA778);
extern const void* D_002EA780[2] RETAIL(D_002EA780);
extern const void* D_002EA788[2] RETAIL(D_002EA788);
extern const void* D_002EA790[2] RETAIL(D_002EA790);
extern const void* D_002EA798[2] RETAIL(D_002EA798);
extern const void* D_002EA7A0[2] RETAIL(D_002EA7A0);
extern const void* D_002EA7A8[2] RETAIL(D_002EA7A8);
extern const void* D_002EA7B0[2] RETAIL(D_002EA7B0);
extern const void* D_002EA7B8[2] RETAIL(D_002EA7B8);
extern const void* D_002EA7C0[2] RETAIL(D_002EA7C0);
extern const void* D_002EA7C8[2] RETAIL(D_002EA7C8);
extern const void* D_002EA7D0[2] RETAIL(D_002EA7D0);
extern const void* D_002EA7D8[2] RETAIL(D_002EA7D8);
extern const void* D_002EA7E0[2] RETAIL(D_002EA7E0);
extern const void* D_002EA7E8[2] RETAIL(D_002EA7E8);
extern const void* D_002EA7F0[2] RETAIL(D_002EA7F0);
extern const void* D_002EA7F8[2] RETAIL(D_002EA7F8);
extern const void* D_002EA800[2] RETAIL(D_002EA800);
extern const void* D_002EA808[4] RETAIL(D_002EA808);
extern u32 D_002EA818[2] RETAIL(D_002EA818);
extern u32 D_002EA820[2] RETAIL(D_002EA820);
extern u32 D_002EA828[2] RETAIL(D_002EA828);
extern u32 D_002EA830[2] RETAIL(D_002EA830);
extern u32 D_002EA838[2] RETAIL(D_002EA838);
extern u32 D_002EA840[10] RETAIL(D_002EA840);
extern u32 D_002EA868[2] RETAIL(D_002EA868);
extern u32 D_002EA870[108] RETAIL(D_002EA870);
extern u32 D_002EAA20[111] RETAIL(D_002EAA20);
extern u32 D_002EABDC[1] RETAIL(D_002EABDC);
extern u32 D_002EABE0[1] RETAIL(D_002EABE0);
extern const void* D_002EABE4[1] RETAIL(D_002EABE4);
extern u32 G_UnkThreadId2[1] RETAIL(G_UnkThreadId2);
extern u32 D_002EABEC[1] RETAIL(D_002EABEC);
extern u32 D_002EABF0[1] RETAIL(D_002EABF0);
extern u32 D_002EABF4[1] RETAIL(D_002EABF4);
extern u32 D_002EABF8[32] RETAIL(D_002EABF8);
extern u32 D_002EAC78[1] RETAIL(D_002EAC78);
extern u32 D_002EAC7C[1] RETAIL(D_002EAC7C);
extern u32 D_002EAC80[1] RETAIL(D_002EAC80);
extern u32 D_002EAC84[1] RETAIL(D_002EAC84);
extern u32 D_002EAC88[1] RETAIL(D_002EAC88);
extern u32 D_002EAC8C[1] RETAIL(D_002EAC8C);
extern const void* D_002EAC90[1] RETAIL(D_002EAC90);
extern u32 D_002EAC94[1] RETAIL(D_002EAC94);
extern u32 D_002EAC98[1] RETAIL(D_002EAC98);
extern const void* D_002EAC9C[1] RETAIL(D_002EAC9C);
extern u32 D_002EACA0[2] RETAIL(D_002EACA0);
extern u32 syscallTableIndex_0x83[1] RETAIL(syscallTableIndex_0x83);
extern u32 D_002EACAC[1] RETAIL(D_002EACAC);
extern u32 syscallTableIndex_0x5A[1] RETAIL(syscallTableIndex_0x5A);
extern const void* D_002EACB4[1] RETAIL(D_002EACB4);
extern u32 semaphore1_ID[1] RETAIL(semaphore1_ID);
extern u32 semaphore2_ID[1] RETAIL(semaphore2_ID);
extern u32 D_002EACC0[490] RETAIL(D_002EACC0);
extern u32 D_002EB468[8] RETAIL(D_002EB468);
extern D_002EB488_Fields D_002EB488 RETAIL(D_002EB488);
extern u32 D_002EBBC8[10] RETAIL(D_002EBBC8);
extern u32 syscallTableIndex_5A[1] RETAIL(syscallTableIndex_5A);
extern u32 D_002EBBF4[1] RETAIL(D_002EBBF4);
extern u32 syscallTableIndex_5B[1] RETAIL(syscallTableIndex_5B);
extern u32 D_002EBBFC[13] RETAIL(D_002EBBFC);
extern u32 D_002EBC30[6] RETAIL(D_002EBC30);
extern u32 D_002EBC48[2] RETAIL(D_002EBC48);
extern u32 D_002EBC50[1] RETAIL(D_002EBC50);
extern u32 D_002EBC54[1] RETAIL(D_002EBC54);
extern u32 D_002EBC58[222] RETAIL(D_002EBC58);
extern u32 D_002EBFD0[2] RETAIL(D_002EBFD0);
extern u32 D_002EBFD8[16] RETAIL(D_002EBFD8);
extern u32 D_002EC018[52] RETAIL(D_002EC018);
extern u32 D_002EC0E8[72] RETAIL(D_002EC0E8);
extern u32 D_002EC208[32] RETAIL(D_002EC208);
extern D_002EC288_Fields D_002EC288 RETAIL(D_002EC288);
extern u32 G_UnkFunTableSize[1] RETAIL(G_UnkFunTableSize);
extern const void* D_002EC2A8[40] RETAIL(D_002EC2A8);

// 0x2E6F00
RETAIL_DATA(".data", 64) const void* GlobalLanguagesArray[1] RETAIL(GlobalLanguagesArray) = {
    Ref_D_003098A8,
};
// 0x2E6F04: nothing uses it
RETAIL_DATA(".data", 4) const void* D_002E6F04[5] RETAIL(D_002E6F04) = {
    Ref_D_003098B0, Ref_D_003098B8, Ref_D_003098C0, Ref_D_003098C8, nullptr,
};
// 0x2E6F18: nothing uses it
RETAIL_DATA(".data", 8) u32 D_002E6F18[16] RETAIL(D_002E6F18) = {
    0xE, 0xF, 0x10, 0x11, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC,
};
// 0x2E6F58: nothing uses it
RETAIL_DATA(".data", 8) D_002E6F58_Row D_002E6F58[16] RETAIL(D_002E6F58) = {
    {Ref_D_002F3988, 0x8},
    {Ref_D_002F39B0, 0xB},
    {Ref_D_002F39D8, 0xE},
    {Ref_D_002F3A00, 0xF},
    {Ref_D_002F3A28, 0x11},
    {Ref_D_002F3A50, 0xF},
    {Ref_D_002F3A78, 0x9},
    {Ref_D_002F3A98, 0xE},
    {Ref_D_002F3AC0, 0x8},
    {Ref_D_002F3AE8, 0x7},
    {Ref_D_002F3B10, 0x9},
    {Ref_D_002F3B40, 0x7},
    {Ref_D_002F3B68, 0x5},
    {Ref_D_002F3B98, 0x6},
    {Ref_D_002F3BC8, 0x6},
    {Ref_D_002F3BF0, 0xA},
};
// 0x2E6FD8
RETAIL_DATA(".data", 8) s32 D_002E6FD8[26] RETAIL(D_002E6FD8) = {
    0, 1, 1, 2, 3, 0, 4, 5, 5, 6, 7, 7, 4, 8, -1, 9, 9, 10, 11, 8, 12, 13, 14, 15, 12, 0,
};
// 0x2E7040
RETAIL_DATA(".data", 64) u32 D_002E7040[8] RETAIL(D_002E7040) = {
    0x8060200, 0x1210100A, 0x1E1A1816, 0x26242424, 0x342E2C2A, 0x3D3A3734, 0x44, 0x0,
};
// 0x2E7060
RETAIL_DATA(".data", 32) const void* D_002E7060[26] RETAIL(D_002E7060) = {
    Ref_D_003099B0, Ref_D_003099B8, Ref_D_003099B8, Ref_D_003099C0, Ref_D_003099C8, Ref_D_003099B0, Ref_D_003099D0,
    Ref_D_003099D8, Ref_D_003099D8, Ref_D_003099E0, Ref_D_003099E8, Ref_D_003099E8, Ref_D_003099D0, Ref_D_003099F0,
    Ref_D_003099F8, Ref_D_00309A00, Ref_D_00309A00, Ref_D_00309A08, Ref_D_00309A10, Ref_D_003099F0, Ref_D_00309A18,
    Ref_D_00309A20, Ref_D_00309A28, Ref_D_00309A30, Ref_D_00309A18, nullptr,
};
// 0x2E70C8
RETAIL_DATA(".data", 8) const void* D_002E70C8[12] RETAIL(D_002E70C8) = {
    Ref_D_00309A38, Ref_D_002F4880, Ref_D_002F4898, Ref_D_002F48B0, Ref_D_002F48C8, Ref_D_002F48D8, Ref_D_002F48E8,
    Ref_D_002F4900, Ref_D_002F4910, nullptr, nullptr, Ref_D_002F4920,
};
// 0x2E70F8
RETAIL_DATA(".data", 8) D_002E70F8_Row D_002E70F8[20] RETAIL(D_002E70F8) = {
    {Ref_D_002F49F8, 0x2002002},
    {Ref_D_002F4A08, 0x120200B},
    {Ref_D_002F4A18, 0x120200B},
    {Ref_D_002F4A28, 0x120200B},
    {Ref_D_002F4A38, 0x120200B},
    {Ref_D_002F4A48, 0x120200B},
    {Ref_D_002F4A58, 0x120200B},
    {Ref_D_002F4A68, 0x120200B},
    {Ref_D_002F4A78, 0x120200B},
    {Ref_D_002F4A88, 0x120200B},
    {Ref_D_002F4A98, 0x120200B},
    {Ref_D_002F4AA8, 0x120200B},
    {Ref_D_002F4AB8, 0x120200B},
    {Ref_D_002F4AC8, 0x1C02002},
    {Ref_D_002F4AD8, 0x1E02802},
    {Ref_D_002F4AF0, 0x1E02802},
    {Ref_D_002F4B08, 0x1E02802},
    {Ref_D_002F4B20, 0x1E02802},
    {Ref_D_002F4B38, 0x2002002},
    {Ref_D_002F4B48, 0x1E02802},
};
// 0x2E7198
RETAIL_DATA(".data", 8) D_002E7198_Fields D_002E7198 RETAIL(D_002E7198) = {Ref_FUN_0017ca50, 0x0, 0x49497350, 0x7362696C, 0x20206663, 0x30303033};
// 0x2E71B0
RETAIL_DATA(".data", 16) u32 D_002E71B0[1] RETAIL(D_002E71B0) = {
    0x21C,
};
// 0x2E71B4
RETAIL_DATA(".data", 4) u8 D_002E71B4[2] RETAIL(D_002E71B4) = {
    0x00, 0x00,
};
// 0x2E71B6
RETAIL_DATA(".data", 2) u8 D_002E71B6[2] RETAIL(D_002E71B6) = {
    0x00, 0x00,
};
// 0x2E71B8: nothing uses it
RETAIL_DATA(".data", 8) u32 D_002E71B8[4] RETAIL(D_002E71B8) = {
    0x0, 0x0, 0x0, 0x0,
};
// 0x2E71C8: nothing uses it
RETAIL_DATA(".data", 8) const void* G_SonyLibsNames[8] RETAIL(G_SonyLibsNames) = {
    Ref_D_002F5A50, Ref_D_002F5A60, Ref_D_002F5A70, Ref_D_002F5A80, Ref_D_002F5A90, Ref_D_002F5AA0, Ref_D_002F5AB0,
    Ref_D_002F5AC0,
};
// 0x2E71E8: nothing uses it
RETAIL_DATA(".data", 8) u32 D_002E71E8[2] RETAIL(D_002E71E8) = {
    0x29D6365D, 0x0,
};
// 0x2E71F0
RETAIL_DATA(".data", 16) char G_VVU_Games_String[32] RETAIL(G_VVU_Games_String) = "c 2004 Vivendi Universal Games";
// 0x2E7210: nothing uses it
RETAIL_DATA(".data", 16) u32 D_002E7210[56] RETAIL(D_002E7210) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2E72F0
RETAIL_DATA(".data", 16) s32 REQ_CHECKSUM[1] RETAIL(REQ_CHECKSUM) = {
    2596,
};
// 0x2E72F4: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002E72F4[1] RETAIL(D_002E72F4) = {
    0x0,
};
// 0x2E72F8: nothing uses it
RETAIL_DATA(".data", 8) u32 DMA_CHANNELS_CHCR_REG[1] RETAIL(DMA_CHANNELS_CHCR_REG) = {
    0x10008000,
};
// 0x2E72FC: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002E72FC[17] RETAIL(D_002E72FC) = {
    0x10009000, 0x1000A000, 0x1000B000, 0x1000B400, 0x1000C000, 0x1000C400, 0x1000C800, 0x1000D000, 0x1000D400, 0xBE2AAAAB, 0x0,
    0xBF000000, 0x0, 0x3F7FF738, 0x0, 0x3F800000, 0x0,
};
// 0x2E7340
RETAIL_DATA(".data", 64) f32 G_ClockSpeedScales[8] RETAIL(G_ClockSpeedScales) = {
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
};
// 0x2E7360
RETAIL_DATA(".data", 32) u32 G_Colors[24] RETAIL(G_Colors) = {
    0x0, 0xC0, 0xC000, 0xC00000, 0xC0C0, 0xC0C000, 0xC000C0, 0xC0C0C0, 0x80000000, 0x800000C0, 0x8000C000, 0x80C00000,
    0x8000C0C0, 0x80C0C000, 0x80C000C0, 0x80C0C0C0, 0x80B8B8B8, 0x80A0A0A0, 0x80787878, 0x80606060, 0x80484848, 0x80303030,
    0x80181818, 0x0,
};
// 0x2E73C0
RETAIL_DATA(".data", 64) const void* D_002E73C0[8] RETAIL(D_002E73C0) = {
    Ref_D_002F6FF8, Ref_D_002F73F8, Ref_D_002F77F8, Ref_D_002F7BF8, Ref_D_002F7FF8, Ref_D_002F83F8, Ref_D_002F87F8,
    Ref_D_002F8BF8,
};
// 0x2E73E0
RETAIL_DATA(".data", 32) const void* G_NullParticlePtr[1] RETAIL(G_NullParticlePtr) = {
    Ref_D_00332540,
};
// 0x2E73E4: nothing uses it
RETAIL_DATA(".data", 4) u32 G_LoadedParticles[299] RETAIL(G_LoadedParticles) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2E7890
RETAIL_DATA(".data", 16) const void* ParticleGeneratorTable[1] RETAIL(ParticleGeneratorTable) = {
    Ref_GenParticle_Box,
};
// 0x2E7894: nothing uses it
RETAIL_DATA(".data", 4) const void* D_002E7894[13] RETAIL(D_002E7894) = {
    Ref_GenParticle_Box2, Ref_GenParticle_Ranges, Ref_GenParticle_Line, Ref_GenParticle_Reuse, Ref_GenParticle_RangesRandomLife,
    Ref_GenParticle_Radial, Ref_GenParticle_RadialRotor, Ref_GenParticle_Spheroid, Ref_GenParticle_Bounce,
    Ref_GenParticle_BounceXZ, Ref_GenParticle_Sphere, Ref_GenParticle_Star, nullptr,
};
// 0x2E78C8
RETAIL_DATA(".data", 8) const void* ParticleGenCodeTable[8] RETAIL(ParticleGenCodeTable) = {
    nullptr, Ref_GenCode1_VelocityXZ2xStart, Ref_GenCode2_VelocityXZMinusStart, Ref_GenCode3_VelocityXZ4xStart,
    Ref_GenCode4_VelocityXZ16xStart, Ref_GenCode5_PullInRandomLife, Ref_GenCode6_Velocity5_4xStart, nullptr,
};
// 0x2E78E8
RETAIL_DATA(".data", 8) s32 D_002E78E8[4] RETAIL(D_002E78E8) = {
    3, 2, 1, 0,
};
// 0x2E78F8
RETAIL_DATA(".data", 8) const void* D_002E78F8[8] RETAIL(D_002E78F8) = {
    Ref_D_002F9990, Ref_D_002F99A0, Ref_D_002F99B0, Ref_D_002F99C8, Ref_D_002F99D8, Ref_D_002F99F0, Ref_D_002F9A08,
    Ref_D_002F9A20,
};
// 0x2E7918
RETAIL_DATA(".data", 8) u32 D_002E7918[4] RETAIL(D_002E7918) = {
    0x1A0003, 0x3F000000, 0x3F000000, 0x3F800000,
};
// 0x2E7928
RETAIL_DATA(".data", 8) u32 D_002E7928[8] RETAIL(D_002E7928) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2E7948
RETAIL_DATA(".data", 8) u32 D_002E7948[24] RETAIL(D_002E7948) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2E79A8
RETAIL_DATA(".data", 8) u32 D_002E79A8[24] RETAIL(D_002E79A8) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2E7A08
RETAIL_DATA(".data", 8) u32 D_002E7A08[12] RETAIL(D_002E7A08) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2E7A38
RETAIL_DATA(".data", 8) u32 D_002E7A38[48] RETAIL(D_002E7A38) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2E7AF8
RETAIL_DATA(".data", 8) u32 D_002E7AF8[48] RETAIL(D_002E7AF8) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2E7BB8
RETAIL_DATA(".data", 8) u32 D_002E7BB8[48] RETAIL(D_002E7BB8) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2E7C78
RETAIL_DATA(".data", 8) u32 D_002E7C78[48] RETAIL(D_002E7C78) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2E7D38
RETAIL_DATA(".data", 8) u32 D_002E7D38[10] RETAIL(D_002E7D38) = {
    0x80, 0x26C0, 0x1F40, 0x4840, 0x6FE0, 0xADE0, 0xF6C0, 0x18040, 0x18040, 0x3C00,
};
// 0x2E7D60
RETAIL_DATA(".data", 32) u32 D_002E7D60[12] RETAIL(D_002E7D60) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2E7D90
RETAIL_DATA(".data", 16) u32 D_002E7D90[12] RETAIL(D_002E7D90) = {
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF,
};
// 0x2E7DC0
RETAIL_DATA(".data", 64) u32 D_002E7DC0[24] RETAIL(D_002E7DC0) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2E7E20
RETAIL_DATA(".data", 32) u32 D_002E7E20[24] RETAIL(D_002E7E20) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2E7E80
RETAIL_DATA(".data", 64) u32 D_002E7E80[48] RETAIL(D_002E7E80) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2E7F40
RETAIL_DATA(".data", 64) u32 D_002E7F40[48] RETAIL(D_002E7F40) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2E8000
RETAIL_DATA(".data", 64) s32 D_002E8000[4] RETAIL(D_002E8000) = {
    31, 33, 30, 0,
};
// 0x2E8010
RETAIL_DATA(".data", 16) const void* D_002E8010[38] RETAIL(D_002E8010) = {
    Ref_D_0030A288, Ref_D_00305D18, Ref_D_00305D38, Ref_D_00305D58, Ref_D_00305D80, Ref_D_00305DA8, Ref_D_00305DD0,
    Ref_D_00305DF8, Ref_D_00305E18, Ref_D_00305E38, Ref_D_00305E58, Ref_D_00305E78, Ref_D_00305E78, Ref_D_00305E90,
    Ref_D_00305EA8, Ref_D_00305EC8, Ref_D_00305EE8, Ref_D_00305F08, Ref_D_00305F18, Ref_D_00305F30, Ref_D_00305F48,
    Ref_D_00305F58, Ref_D_00305F70, Ref_D_00305F88, Ref_D_00305FA0, Ref_D_00305FB0, Ref_D_00305FC0, Ref_D_00305FD0,
    Ref_D_00305FE0, Ref_D_0030A310, Ref_D_0030A318, Ref_D_0030A320, Ref_D_00305FF0, Ref_D_00306000, Ref_D_00306018,
    Ref_D_0030A328, Ref_D_0030A330, Ref_D_0030A338,
};
// 0x2E80A8
RETAIL_DATA(".data", 8) s32 D_002E80A8[14] RETAIL(D_002E80A8) = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 21, 22, 23, 27,
};
// 0x2E80E0
RETAIL_DATA(".data", 32) s32 D_002E80E0[16] RETAIL(D_002E80E0) = {
    0, 12, 16, 10, 11, 13, 14, 15, 16, 17, 20, 18, 19, 24, 25, 26,
};
// 0x2E8120: nothing uses it
RETAIL_DATA(".data", 32) u32 D_002E8120[1] RETAIL(D_002E8120) = {
    0xFFFFFFFF,
};
// 0x2E8124: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002E8124[3] RETAIL(D_002E8124) = {
    0xFFFFFFFF, 0xFFFFFFFF, 0x0,
};
// 0x2E8130: nothing uses it
RETAIL_DATA(".data", 16) u32 CHCR_ARRAY[1] RETAIL(CHCR_ARRAY) = {
    0x10008000,
};
// 0x2E8134: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002E8134[9] RETAIL(D_002E8134) = {
    0x10009000, 0x1000A000, 0x1000B000, 0x1000B400, 0x0, 0x0, 0x0, 0x1000D000, 0x1000D400,
};
// 0x2E8158
RETAIL_DATA(".data", 8) u32 D_002E8158[16] RETAIL(D_002E8158) = {
    0x2, 0x0, 0x2, 0x0, 0x2, 0x3, 0x2, 0x3, 0x0, 0x0, 0x0, 0x0, 0x2, 0x4, 0x0, 0x6,
};
// 0x2E8198
RETAIL_DATA(".data", 8) u32 D_002E8198[40] RETAIL(D_002E8198) = {
    0x0, 0xE0, 0x0, 0xFF, 0xFFC00000, 0xBD, 0xFFFFFFFF, 0xFF, 0xFFA00000, 0xBD, 0xFFFFFFFF, 0xFF, 0xFFA10000, 0xBD, 0xFFFFFFFF,
    0xFF, 0xFF900000, 0xBD, 0xFFFFFFFF, 0xFF, 0x0, 0xC0, 0x0, 0xFF, 0x80000000, 0xBD, 0xFF000000, 0xFF, 0xA0000000, 0xBD,
    0xFF000000, 0xFF, 0x88000000, 0xBD, 0xFF000000, 0xFF, 0x90000000, 0xBD, 0xFF000000, 0xFF,
};
#if defined(_EE)
// 0x2E8238 (the PS2 side points it at its own functions)
RETAIL_DATA(".data", 8) const void* jtbl_002E8238[16] RETAIL(jtbl_002E8238) = {
    Ref_func_002BF748, Ref_func_002BF858, Ref_func_002BF9E0, Ref_func_002BFB48, Ref_func_002BFD40, Ref_func_002BFE90,
    Ref_func_002C0058, Ref_func_002C0200, Ref_func_002BF7C0, Ref_func_002BF910, Ref_func_002BFA90, Ref_func_002BFC40,
    Ref_func_002BFDE0, Ref_func_002BFF70, Ref_func_002C0130, Ref_func_002C0320,
};
#endif
// 0x2E8278: nothing uses it
RETAIL_DATA(".data", 8) u32 D_002E8278[2] RETAIL(D_002E8278) = {
    0x0, 0x0,
};
// 0x2E8280
RETAIL_DATA(".data", 64) u32 D_002E8280[16] RETAIL(D_002E8280) = {
    0x13101008, 0x16161310, 0x16161616, 0x1B1A181A, 0x1A1A1B1B, 0x1B1B1A1A, 0x1D1D1D1B, 0x1D222222, 0x1B1B1D1D, 0x20201D1D,
    0x26252222, 0x22232325, 0x28262623, 0x30302828, 0x38382E2E, 0x5345453A,
};
// 0x2E82C0
RETAIL_DATA(".data", 64) u32 D_002E82C0[20] RETAIL(D_002E82C0) = {
    0x10101010, 0x10101010, 0x10101010, 0x10101010, 0x10101010, 0x10101010, 0x10101010, 0x10101010, 0x10101010, 0x10101010,
    0x10101010, 0x10101010, 0x10101010, 0x10101010, 0x10101010, 0x10101010, 0x49497350, 0x6962696C, 0x20207570, 0x30303033,
};
// 0x2E8310
RETAIL_DATA(".data", 16) u32 D_002E8310[20] RETAIL(D_002E8310) = {
    0x13101008, 0x16161310, 0x16161616, 0x1B1A181A, 0x1A1A1B1B, 0x1B1B1A1A, 0x1D1D1D1B, 0x1D222222, 0x1B1B1D1D, 0x20201D1D,
    0x26252222, 0x22232325, 0x28262623, 0x30302828, 0x38382E2E, 0x5345453A, 0x10101010, 0x10101010, 0x10101010, 0x10101010,
};
// 0x2E8360
RETAIL_DATA(".data", 32) u32 D_002E8360[8] RETAIL(D_002E8360) = {
    0x4210000, 0x3E00842, 0x14A51084, 0x1CE718C6, 0x2529001F, 0x7C00294A, 0x35AD318C, 0x39CE7FFF,
};
// 0x2E8380: nothing uses it
RETAIL_DATA(".data", 64) u32 D_002E8380[4] RETAIL(D_002E8380) = {
    0x1000404, 0x20000000, 0x0, 0x5000000,
};
// 0x2E8390: nothing uses it
RETAIL_DATA(".data", 16) u32 D_002E8390[8] RETAIL(D_002E8390) = {
    0x6000000, 0x3000000, 0x2000000, 0x4000000, 0x49497350, 0x6762696C, 0x68706172, 0x30303033,
};
// 0x2E83B0
RETAIL_DATA(".data", 16) u32 D_002E83B0[8] RETAIL(D_002E83B0) = {
    0x20001, 0x30001, 0x0, 0x0, 0x49497350, 0x6462696C, 0x2020616D, 0x30303033,
};
// 0x2E83D0
RETAIL_DATA(".data", 16) u32 DMA_N_CHANNELS[1] RETAIL(DMA_N_CHANNELS) = {
    0x10008000,
};
// 0x2E83D4: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002E83D4[13] RETAIL(D_002E83D4) = {
    0x10009000, 0x1000A000, 0x1000B000, 0x1000B400, 0x1000C000, 0x1000C400, 0x1000C800, 0x1000D000, 0x1000D400, 0x49497350,
    0x7062696C, 0x20206461, 0x30313033,
};
// 0x2E8408: nothing uses it
RETAIL_DATA(".data", 8) u32 D_002E8408[1] RETAIL(D_002E8408) = {
    0x0,
};
// 0x2E840C: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002E840C[1] RETAIL(D_002E840C) = {
    0x1,
};
// 0x2E8410: nothing uses it
RETAIL_DATA(".data", 16) u32 D_002E8410[1] RETAIL(D_002E8410) = {
    0x0,
};
// 0x2E8414: nothing uses it
RETAIL_DATA(".data", 4) D_002E8414_Fields D_002E8414 RETAIL(D_002E8414) = {0x0, Ref_D_00308070, Ref_D_00308068, Ref_D_00308058, Ref_D_00308068, Ref_D_00308068, Ref_D_00308050, Ref_D_00308048, Ref_D_00308040, Ref_D_00308090, Ref_D_00308088, Ref_D_00308080, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x49497350, 0x6362696C, 0x20647664, 0x30303033};
// 0x2E8490: nothing uses it
RETAIL_DATA(".data", 16) u32 D_002E8490[1] RETAIL(D_002E8490) = {
    0x0,
};
// 0x2E8494: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002E8494[1] RETAIL(D_002E8494) = {
    0x0,
};
// 0x2E8498: nothing uses it
RETAIL_DATA(".data", 8) u32 D_002E8498[1] RETAIL(D_002E8498) = {
    0x0,
};
// 0x2E849C: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002E849C[1] RETAIL(D_002E849C) = {
    0x0,
};
// 0x2E84A0: nothing uses it
RETAIL_DATA(".data", 32) u32 G_Sema_CDVD_Callback[1] RETAIL(G_Sema_CDVD_Callback) = {
    0xFFFFFFFF,
};
// 0x2E84A4: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002E84A4[1] RETAIL(D_002E84A4) = {
    0x0,
};
// 0x2E84A8: nothing uses it
RETAIL_DATA(".data", 8) u32 G_Sema_CDVD_AsyncCommand[1] RETAIL(G_Sema_CDVD_AsyncCommand) = {
    0xFFFFFFFF,
};
// 0x2E84AC: nothing uses it
RETAIL_DATA(".data", 4) u32 G_Sema_CDVD_SyncCommand[1] RETAIL(G_Sema_CDVD_SyncCommand) = {
    0xFFFFFFFF,
};
// 0x2E84B0: nothing uses it
RETAIL_DATA(".data", 16) u32 G_Sema_CDVD_CommandStatus_[1] RETAIL(G_Sema_CDVD_CommandStatus_) = {
    0xFFFFFFFF,
};
// 0x2E84B4: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002E84B4[1] RETAIL(D_002E84B4) = {
    0x0,
};
// 0x2E84B8: nothing uses it
RETAIL_DATA(".data", 8) u32 D_002E84B8[1] RETAIL(D_002E84B8) = {
    0x0,
};
// 0x2E84BC: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002E84BC[1] RETAIL(D_002E84BC) = {
    0xFFFFFFFF,
};
// 0x2E84C0: nothing uses it
RETAIL_DATA(".data", 64) u32 D_002E84C0[1] RETAIL(D_002E84C0) = {
    0x1,
};
// 0x2E84C4: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002E84C4[1] RETAIL(D_002E84C4) = {
    0x0,
};
// 0x2E84C8: nothing uses it
RETAIL_DATA(".data", 8) u32 D_002E84C8[1] RETAIL(D_002E84C8) = {
    0xFFFFFFFF,
};
// 0x2E84CC: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002E84CC[1] RETAIL(D_002E84CC) = {
    0xFFFFFFFF,
};
// 0x2E84D0: nothing uses it
RETAIL_DATA(".data", 16) u32 D_002E84D0[1] RETAIL(D_002E84D0) = {
    0xFFFFFFFF,
};
// 0x2E84D4: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002E84D4[1] RETAIL(D_002E84D4) = {
    0xFFFFFFFF,
};
// 0x2E84D8: nothing uses it
RETAIL_DATA(".data", 8) u32 D_002E84D8[1] RETAIL(D_002E84D8) = {
    0xFFFFFFFF,
};
// 0x2E84DC: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002E84DC[1] RETAIL(D_002E84DC) = {
    0x0,
};
// 0x2E84E0: nothing uses it
RETAIL_DATA(".data", 32) u32 D_002E84E0[8] RETAIL(D_002E84E0) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2E8500: nothing uses it
RETAIL_DATA(".data", 64) u32 D_002E8500[32] RETAIL(D_002E8500) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2E8580: nothing uses it
RETAIL_DATA(".data", 64) u32 D_002E8580[1076] RETAIL(D_002E8580) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0,
};
// 0x2E9650: nothing uses it
RETAIL_DATA(".data", 16) u32 D_002E9650[12] RETAIL(D_002E9650) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2E9680: nothing uses it
RETAIL_DATA(".data", 64) u32 D_002E9680[272] RETAIL(D_002E9680) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2E9AC0: nothing uses it
RETAIL_DATA(".data", 64) u32 G_MediaType[1] RETAIL(G_MediaType) = {
    0x0,
};
// 0x2E9AC4: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002E9AC4[271] RETAIL(D_002E9AC4) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2E9F00: nothing uses it
RETAIL_DATA(".data", 64) u32 D_002E9F00[64] RETAIL(D_002E9F00) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2EA000: nothing uses it
RETAIL_DATA(".data", 64) u32 D_002EA000[10] RETAIL(D_002EA000) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2EA028: nothing uses it
RETAIL_DATA(".data", 8) u32 D_002EA028[1] RETAIL(D_002EA028) = {
    0x22,
};
// 0x2EA02C: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002EA02C[1] RETAIL(D_002EA02C) = {
    0x4,
};
// 0x2EA030: nothing uses it
RETAIL_DATA(".data", 16) u32 D_002EA030[1] RETAIL(D_002EA030) = {
    0x4,
};
// 0x2EA034: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002EA034[1] RETAIL(D_002EA034) = {
    0x0,
};
// 0x2EA038
RETAIL_DATA(".data", 8) u32 D_002EA038[1] RETAIL(D_002EA038) = {
    0x0,
};
// 0x2EA03C
RETAIL_DATA(".data", 4) u32 D_002EA03C[5] RETAIL(D_002EA03C) = {
    0x0, 0x49497350, 0x7362696C, 0x20207264, 0x30303033,
};
// 0x2EA050
RETAIL_DATA(".data", 16) u32 D_002EA050[1] RETAIL(D_002EA050) = {
    0x0,
};
// 0x2EA054: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002EA054[1] RETAIL(D_002EA054) = {
    0x0,
};
// 0x2EA058: nothing uses it
RETAIL_DATA(".data", 8) u32 D_002EA058[1] RETAIL(D_002EA058) = {
    0x0,
};
// 0x2EA05C
RETAIL_DATA(".data", 4) u32 D_002EA05C[1] RETAIL(D_002EA05C) = {
    0x0,
};
// 0x2EA060: nothing uses it
RETAIL_DATA(".data", 32) u32 D_002EA060[1] RETAIL(D_002EA060) = {
    0x0,
};
// 0x2EA064: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002EA064[1] RETAIL(D_002EA064) = {
    0x0,
};
// 0x2EA068
RETAIL_DATA(".data", 8) u32 D_002EA068[1] RETAIL(D_002EA068) = {
    0x0,
};
// 0x2EA06C: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002EA06C[1] RETAIL(D_002EA06C) = {
    0x0,
};
// 0x2EA070: nothing uses it
RETAIL_DATA(".data", 16) u32 D_002EA070[6] RETAIL(D_002EA070) = {
    0x0, 0xFFFFFFFF, 0x49497350, 0x6D62696C, 0x20202063, 0x30313033,
};
// 0x2EA088: nothing uses it
RETAIL_DATA(".data", 8) u32 D_002EA088[1] RETAIL(D_002EA088) = {
    0x0,
};
// 0x2EA08C: nothing uses it
RETAIL_DATA(".data", 4) u32 G_SemaSceMc_SemaRegs[1] RETAIL(G_SemaSceMc_SemaRegs) = {
    0xFFFFFFFF,
};
// 0x2EA090: nothing uses it
RETAIL_DATA(".data", 16) u32 G_SemaSceMc_SemaTimer[1] RETAIL(G_SemaSceMc_SemaTimer) = {
    0xFFFFFFFF,
};
// 0x2EA094: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002EA094[3] RETAIL(D_002EA094) = {
    0x0, 0x0, 0x0,
};
// 0x2EA0A0
RETAIL_DATA(".data", 32) u32 D_002EA0A0[16] RETAIL(D_002EA0A0) = {
    0x3F800000, 0x0, 0x0, 0x0, 0x0, 0x3F800000, 0x0, 0x0, 0x0, 0x0, 0x3F800000, 0x0, 0x0, 0x0, 0x0, 0x3F800000,
};
// 0x2EA0E0
RETAIL_DATA(".data", 32) const void* D_002EA0E0[22] RETAIL(D_002EA0E0) = {
    nullptr, &D_002EA2C4, &D_002EA31C, &D_002EA374, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, Ref_D_00308638, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
};
// 0x2EA138: nothing uses it
RETAIL_DATA(".data", 8) u32 CurrentSeed[1] RETAIL(CurrentSeed) = {
    0x1,
};
// 0x2EA13C: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002EA13C[98] RETAIL(D_002EA13C) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2EA2C4
RETAIL_DATA(".data", 4) u32 D_002EA2C4[22] RETAIL(D_002EA2C4) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2EA31C
RETAIL_DATA(".data", 4) u32 D_002EA31C[22] RETAIL(D_002EA31C) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2EA374
RETAIL_DATA(".data", 4) u32 D_002EA374[22] RETAIL(D_002EA374) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2EA3CC
RETAIL_DATA(".data", 4) const void* D_002EA3CC[1] RETAIL(D_002EA3CC) = {
    &D_002EA0E0,
};
// 0x2EA3D0: nothing uses it
RETAIL_DATA(".data", 16) u32 G_WaitingThreadId_[1] RETAIL(G_WaitingThreadId_) = {
    0xFFFFFFFF,
};
// 0x2EA3D4: nothing uses it
RETAIL_DATA(".data", 4) u32 G_WaitingThreadCount_[1] RETAIL(G_WaitingThreadCount_) = {
    0x0,
};
// 0x2EA3D8: nothing uses it
RETAIL_DATA(".data", 8) char D_002EA3D8[40] RETAIL(D_002EA3D8) = "0123456789abcdefghijklmnopqrstuvwxyz";
// 0x2EA400: nothing uses it
RETAIL_DATA(".data", 64) u32 D_002EA400[1] RETAIL(D_002EA400) = {
    0x0,
};
// 0x2EA404: nothing uses it
RETAIL_DATA(".data", 4) const void* D_002EA404[1] RETAIL(D_002EA404) = {
    Ref_D_003C9FA8,
};
// 0x2EA408: nothing uses it
RETAIL_DATA(".data", 8) u32 D_002EA408[1] RETAIL(D_002EA408) = {
    0x0,
};
// 0x2EA40C: nothing uses it
RETAIL_DATA(".data", 4) const void* D_002EA40C[1] RETAIL(D_002EA40C) = {
    Ref_D_003CA028,
};
// 0x2EA410
RETAIL_DATA(".data", 16) u32 D_002EA410[2] RETAIL(D_002EA410) = {
    0x0, 0x0,
};
// 0x2EA418: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA418[2] RETAIL(D_002EA418) = {
    &D_002EA410, &D_002EA410,
};
// 0x2EA420: nothing uses it
RETAIL_DATA(".data", 32) const void* D_002EA420[2] RETAIL(D_002EA420) = {
    &D_002EA418, &D_002EA418,
};
// 0x2EA428: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA428[2] RETAIL(D_002EA428) = {
    &D_002EA420, &D_002EA420,
};
// 0x2EA430: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA430[2] RETAIL(D_002EA430) = {
    &D_002EA428, &D_002EA428,
};
// 0x2EA438: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA438[2] RETAIL(D_002EA438) = {
    &D_002EA430, &D_002EA430,
};
// 0x2EA440: nothing uses it
RETAIL_DATA(".data", 64) const void* D_002EA440[2] RETAIL(D_002EA440) = {
    &D_002EA438, &D_002EA438,
};
// 0x2EA448: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA448[2] RETAIL(D_002EA448) = {
    &D_002EA440, &D_002EA440,
};
// 0x2EA450: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA450[2] RETAIL(D_002EA450) = {
    &D_002EA448, &D_002EA448,
};
// 0x2EA458: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA458[2] RETAIL(D_002EA458) = {
    &D_002EA450, &D_002EA450,
};
// 0x2EA460: nothing uses it
RETAIL_DATA(".data", 32) const void* D_002EA460[2] RETAIL(D_002EA460) = {
    &D_002EA458, &D_002EA458,
};
// 0x2EA468: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA468[2] RETAIL(D_002EA468) = {
    &D_002EA460, &D_002EA460,
};
// 0x2EA470: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA470[2] RETAIL(D_002EA470) = {
    &D_002EA468, &D_002EA468,
};
// 0x2EA478: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA478[2] RETAIL(D_002EA478) = {
    &D_002EA470, &D_002EA470,
};
// 0x2EA480: nothing uses it
RETAIL_DATA(".data", 64) const void* D_002EA480[2] RETAIL(D_002EA480) = {
    &D_002EA478, &D_002EA478,
};
// 0x2EA488: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA488[2] RETAIL(D_002EA488) = {
    &D_002EA480, &D_002EA480,
};
// 0x2EA490: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA490[2] RETAIL(D_002EA490) = {
    &D_002EA488, &D_002EA488,
};
// 0x2EA498: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA498[2] RETAIL(D_002EA498) = {
    &D_002EA490, &D_002EA490,
};
// 0x2EA4A0: nothing uses it
RETAIL_DATA(".data", 32) const void* D_002EA4A0[2] RETAIL(D_002EA4A0) = {
    &D_002EA498, &D_002EA498,
};
// 0x2EA4A8: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA4A8[2] RETAIL(D_002EA4A8) = {
    &D_002EA4A0, &D_002EA4A0,
};
// 0x2EA4B0: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA4B0[2] RETAIL(D_002EA4B0) = {
    &D_002EA4A8, &D_002EA4A8,
};
// 0x2EA4B8: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA4B8[2] RETAIL(D_002EA4B8) = {
    &D_002EA4B0, &D_002EA4B0,
};
// 0x2EA4C0: nothing uses it
RETAIL_DATA(".data", 64) const void* D_002EA4C0[2] RETAIL(D_002EA4C0) = {
    &D_002EA4B8, &D_002EA4B8,
};
// 0x2EA4C8: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA4C8[2] RETAIL(D_002EA4C8) = {
    &D_002EA4C0, &D_002EA4C0,
};
// 0x2EA4D0: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA4D0[2] RETAIL(D_002EA4D0) = {
    &D_002EA4C8, &D_002EA4C8,
};
// 0x2EA4D8: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA4D8[2] RETAIL(D_002EA4D8) = {
    &D_002EA4D0, &D_002EA4D0,
};
// 0x2EA4E0: nothing uses it
RETAIL_DATA(".data", 32) const void* D_002EA4E0[2] RETAIL(D_002EA4E0) = {
    &D_002EA4D8, &D_002EA4D8,
};
// 0x2EA4E8: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA4E8[2] RETAIL(D_002EA4E8) = {
    &D_002EA4E0, &D_002EA4E0,
};
// 0x2EA4F0: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA4F0[2] RETAIL(D_002EA4F0) = {
    &D_002EA4E8, &D_002EA4E8,
};
// 0x2EA4F8: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA4F8[2] RETAIL(D_002EA4F8) = {
    &D_002EA4F0, &D_002EA4F0,
};
// 0x2EA500: nothing uses it
RETAIL_DATA(".data", 64) const void* D_002EA500[2] RETAIL(D_002EA500) = {
    &D_002EA4F8, &D_002EA4F8,
};
// 0x2EA508: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA508[2] RETAIL(D_002EA508) = {
    &D_002EA500, &D_002EA500,
};
// 0x2EA510: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA510[2] RETAIL(D_002EA510) = {
    &D_002EA508, &D_002EA508,
};
// 0x2EA518: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA518[2] RETAIL(D_002EA518) = {
    &D_002EA510, &D_002EA510,
};
// 0x2EA520: nothing uses it
RETAIL_DATA(".data", 32) const void* D_002EA520[2] RETAIL(D_002EA520) = {
    &D_002EA518, &D_002EA518,
};
// 0x2EA528: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA528[2] RETAIL(D_002EA528) = {
    &D_002EA520, &D_002EA520,
};
// 0x2EA530: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA530[2] RETAIL(D_002EA530) = {
    &D_002EA528, &D_002EA528,
};
// 0x2EA538: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA538[2] RETAIL(D_002EA538) = {
    &D_002EA530, &D_002EA530,
};
// 0x2EA540: nothing uses it
RETAIL_DATA(".data", 64) const void* D_002EA540[2] RETAIL(D_002EA540) = {
    &D_002EA538, &D_002EA538,
};
// 0x2EA548: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA548[2] RETAIL(D_002EA548) = {
    &D_002EA540, &D_002EA540,
};
// 0x2EA550: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA550[2] RETAIL(D_002EA550) = {
    &D_002EA548, &D_002EA548,
};
// 0x2EA558: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA558[2] RETAIL(D_002EA558) = {
    &D_002EA550, &D_002EA550,
};
// 0x2EA560: nothing uses it
RETAIL_DATA(".data", 32) const void* D_002EA560[2] RETAIL(D_002EA560) = {
    &D_002EA558, &D_002EA558,
};
// 0x2EA568: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA568[2] RETAIL(D_002EA568) = {
    &D_002EA560, &D_002EA560,
};
// 0x2EA570: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA570[2] RETAIL(D_002EA570) = {
    &D_002EA568, &D_002EA568,
};
// 0x2EA578: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA578[2] RETAIL(D_002EA578) = {
    &D_002EA570, &D_002EA570,
};
// 0x2EA580: nothing uses it
RETAIL_DATA(".data", 64) const void* D_002EA580[2] RETAIL(D_002EA580) = {
    &D_002EA578, &D_002EA578,
};
// 0x2EA588: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA588[2] RETAIL(D_002EA588) = {
    &D_002EA580, &D_002EA580,
};
// 0x2EA590: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA590[2] RETAIL(D_002EA590) = {
    &D_002EA588, &D_002EA588,
};
// 0x2EA598: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA598[2] RETAIL(D_002EA598) = {
    &D_002EA590, &D_002EA590,
};
// 0x2EA5A0: nothing uses it
RETAIL_DATA(".data", 32) const void* D_002EA5A0[2] RETAIL(D_002EA5A0) = {
    &D_002EA598, &D_002EA598,
};
// 0x2EA5A8: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA5A8[2] RETAIL(D_002EA5A8) = {
    &D_002EA5A0, &D_002EA5A0,
};
// 0x2EA5B0: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA5B0[2] RETAIL(D_002EA5B0) = {
    &D_002EA5A8, &D_002EA5A8,
};
// 0x2EA5B8: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA5B8[2] RETAIL(D_002EA5B8) = {
    &D_002EA5B0, &D_002EA5B0,
};
// 0x2EA5C0: nothing uses it
RETAIL_DATA(".data", 64) const void* D_002EA5C0[2] RETAIL(D_002EA5C0) = {
    &D_002EA5B8, &D_002EA5B8,
};
// 0x2EA5C8: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA5C8[2] RETAIL(D_002EA5C8) = {
    &D_002EA5C0, &D_002EA5C0,
};
// 0x2EA5D0: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA5D0[2] RETAIL(D_002EA5D0) = {
    &D_002EA5C8, &D_002EA5C8,
};
// 0x2EA5D8: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA5D8[2] RETAIL(D_002EA5D8) = {
    &D_002EA5D0, &D_002EA5D0,
};
// 0x2EA5E0: nothing uses it
RETAIL_DATA(".data", 32) const void* D_002EA5E0[2] RETAIL(D_002EA5E0) = {
    &D_002EA5D8, &D_002EA5D8,
};
// 0x2EA5E8: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA5E8[2] RETAIL(D_002EA5E8) = {
    &D_002EA5E0, &D_002EA5E0,
};
// 0x2EA5F0: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA5F0[2] RETAIL(D_002EA5F0) = {
    &D_002EA5E8, &D_002EA5E8,
};
// 0x2EA5F8: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA5F8[2] RETAIL(D_002EA5F8) = {
    &D_002EA5F0, &D_002EA5F0,
};
// 0x2EA600: nothing uses it
RETAIL_DATA(".data", 64) const void* D_002EA600[2] RETAIL(D_002EA600) = {
    &D_002EA5F8, &D_002EA5F8,
};
// 0x2EA608: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA608[2] RETAIL(D_002EA608) = {
    &D_002EA600, &D_002EA600,
};
// 0x2EA610: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA610[2] RETAIL(D_002EA610) = {
    &D_002EA608, &D_002EA608,
};
// 0x2EA618: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA618[2] RETAIL(D_002EA618) = {
    &D_002EA610, &D_002EA610,
};
// 0x2EA620: nothing uses it
RETAIL_DATA(".data", 32) const void* D_002EA620[2] RETAIL(D_002EA620) = {
    &D_002EA618, &D_002EA618,
};
// 0x2EA628: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA628[2] RETAIL(D_002EA628) = {
    &D_002EA620, &D_002EA620,
};
// 0x2EA630: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA630[2] RETAIL(D_002EA630) = {
    &D_002EA628, &D_002EA628,
};
// 0x2EA638: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA638[2] RETAIL(D_002EA638) = {
    &D_002EA630, &D_002EA630,
};
// 0x2EA640: nothing uses it
RETAIL_DATA(".data", 64) const void* D_002EA640[2] RETAIL(D_002EA640) = {
    &D_002EA638, &D_002EA638,
};
// 0x2EA648: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA648[2] RETAIL(D_002EA648) = {
    &D_002EA640, &D_002EA640,
};
// 0x2EA650: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA650[2] RETAIL(D_002EA650) = {
    &D_002EA648, &D_002EA648,
};
// 0x2EA658: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA658[2] RETAIL(D_002EA658) = {
    &D_002EA650, &D_002EA650,
};
// 0x2EA660: nothing uses it
RETAIL_DATA(".data", 32) const void* D_002EA660[2] RETAIL(D_002EA660) = {
    &D_002EA658, &D_002EA658,
};
// 0x2EA668: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA668[2] RETAIL(D_002EA668) = {
    &D_002EA660, &D_002EA660,
};
// 0x2EA670: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA670[2] RETAIL(D_002EA670) = {
    &D_002EA668, &D_002EA668,
};
// 0x2EA678: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA678[2] RETAIL(D_002EA678) = {
    &D_002EA670, &D_002EA670,
};
// 0x2EA680: nothing uses it
RETAIL_DATA(".data", 64) const void* D_002EA680[2] RETAIL(D_002EA680) = {
    &D_002EA678, &D_002EA678,
};
// 0x2EA688: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA688[2] RETAIL(D_002EA688) = {
    &D_002EA680, &D_002EA680,
};
// 0x2EA690: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA690[2] RETAIL(D_002EA690) = {
    &D_002EA688, &D_002EA688,
};
// 0x2EA698: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA698[2] RETAIL(D_002EA698) = {
    &D_002EA690, &D_002EA690,
};
// 0x2EA6A0: nothing uses it
RETAIL_DATA(".data", 32) const void* D_002EA6A0[2] RETAIL(D_002EA6A0) = {
    &D_002EA698, &D_002EA698,
};
// 0x2EA6A8: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA6A8[2] RETAIL(D_002EA6A8) = {
    &D_002EA6A0, &D_002EA6A0,
};
// 0x2EA6B0: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA6B0[2] RETAIL(D_002EA6B0) = {
    &D_002EA6A8, &D_002EA6A8,
};
// 0x2EA6B8: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA6B8[2] RETAIL(D_002EA6B8) = {
    &D_002EA6B0, &D_002EA6B0,
};
// 0x2EA6C0: nothing uses it
RETAIL_DATA(".data", 64) const void* D_002EA6C0[2] RETAIL(D_002EA6C0) = {
    &D_002EA6B8, &D_002EA6B8,
};
// 0x2EA6C8: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA6C8[2] RETAIL(D_002EA6C8) = {
    &D_002EA6C0, &D_002EA6C0,
};
// 0x2EA6D0: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA6D0[2] RETAIL(D_002EA6D0) = {
    &D_002EA6C8, &D_002EA6C8,
};
// 0x2EA6D8: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA6D8[2] RETAIL(D_002EA6D8) = {
    &D_002EA6D0, &D_002EA6D0,
};
// 0x2EA6E0: nothing uses it
RETAIL_DATA(".data", 32) const void* D_002EA6E0[2] RETAIL(D_002EA6E0) = {
    &D_002EA6D8, &D_002EA6D8,
};
// 0x2EA6E8: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA6E8[2] RETAIL(D_002EA6E8) = {
    &D_002EA6E0, &D_002EA6E0,
};
// 0x2EA6F0: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA6F0[2] RETAIL(D_002EA6F0) = {
    &D_002EA6E8, &D_002EA6E8,
};
// 0x2EA6F8: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA6F8[2] RETAIL(D_002EA6F8) = {
    &D_002EA6F0, &D_002EA6F0,
};
// 0x2EA700: nothing uses it
RETAIL_DATA(".data", 64) const void* D_002EA700[2] RETAIL(D_002EA700) = {
    &D_002EA6F8, &D_002EA6F8,
};
// 0x2EA708: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA708[2] RETAIL(D_002EA708) = {
    &D_002EA700, &D_002EA700,
};
// 0x2EA710: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA710[2] RETAIL(D_002EA710) = {
    &D_002EA708, &D_002EA708,
};
// 0x2EA718: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA718[2] RETAIL(D_002EA718) = {
    &D_002EA710, &D_002EA710,
};
// 0x2EA720: nothing uses it
RETAIL_DATA(".data", 32) const void* D_002EA720[2] RETAIL(D_002EA720) = {
    &D_002EA718, &D_002EA718,
};
// 0x2EA728: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA728[2] RETAIL(D_002EA728) = {
    &D_002EA720, &D_002EA720,
};
// 0x2EA730: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA730[2] RETAIL(D_002EA730) = {
    &D_002EA728, &D_002EA728,
};
// 0x2EA738: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA738[2] RETAIL(D_002EA738) = {
    &D_002EA730, &D_002EA730,
};
// 0x2EA740: nothing uses it
RETAIL_DATA(".data", 64) const void* D_002EA740[2] RETAIL(D_002EA740) = {
    &D_002EA738, &D_002EA738,
};
// 0x2EA748: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA748[2] RETAIL(D_002EA748) = {
    &D_002EA740, &D_002EA740,
};
// 0x2EA750: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA750[2] RETAIL(D_002EA750) = {
    &D_002EA748, &D_002EA748,
};
// 0x2EA758: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA758[2] RETAIL(D_002EA758) = {
    &D_002EA750, &D_002EA750,
};
// 0x2EA760: nothing uses it
RETAIL_DATA(".data", 32) const void* D_002EA760[2] RETAIL(D_002EA760) = {
    &D_002EA758, &D_002EA758,
};
// 0x2EA768: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA768[2] RETAIL(D_002EA768) = {
    &D_002EA760, &D_002EA760,
};
// 0x2EA770: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA770[2] RETAIL(D_002EA770) = {
    &D_002EA768, &D_002EA768,
};
// 0x2EA778: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA778[2] RETAIL(D_002EA778) = {
    &D_002EA770, &D_002EA770,
};
// 0x2EA780: nothing uses it
RETAIL_DATA(".data", 64) const void* D_002EA780[2] RETAIL(D_002EA780) = {
    &D_002EA778, &D_002EA778,
};
// 0x2EA788: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA788[2] RETAIL(D_002EA788) = {
    &D_002EA780, &D_002EA780,
};
// 0x2EA790: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA790[2] RETAIL(D_002EA790) = {
    &D_002EA788, &D_002EA788,
};
// 0x2EA798: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA798[2] RETAIL(D_002EA798) = {
    &D_002EA790, &D_002EA790,
};
// 0x2EA7A0: nothing uses it
RETAIL_DATA(".data", 32) const void* D_002EA7A0[2] RETAIL(D_002EA7A0) = {
    &D_002EA798, &D_002EA798,
};
// 0x2EA7A8: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA7A8[2] RETAIL(D_002EA7A8) = {
    &D_002EA7A0, &D_002EA7A0,
};
// 0x2EA7B0: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA7B0[2] RETAIL(D_002EA7B0) = {
    &D_002EA7A8, &D_002EA7A8,
};
// 0x2EA7B8: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA7B8[2] RETAIL(D_002EA7B8) = {
    &D_002EA7B0, &D_002EA7B0,
};
// 0x2EA7C0: nothing uses it
RETAIL_DATA(".data", 64) const void* D_002EA7C0[2] RETAIL(D_002EA7C0) = {
    &D_002EA7B8, &D_002EA7B8,
};
// 0x2EA7C8: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA7C8[2] RETAIL(D_002EA7C8) = {
    &D_002EA7C0, &D_002EA7C0,
};
// 0x2EA7D0: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA7D0[2] RETAIL(D_002EA7D0) = {
    &D_002EA7C8, &D_002EA7C8,
};
// 0x2EA7D8: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA7D8[2] RETAIL(D_002EA7D8) = {
    &D_002EA7D0, &D_002EA7D0,
};
// 0x2EA7E0: nothing uses it
RETAIL_DATA(".data", 32) const void* D_002EA7E0[2] RETAIL(D_002EA7E0) = {
    &D_002EA7D8, &D_002EA7D8,
};
// 0x2EA7E8: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA7E8[2] RETAIL(D_002EA7E8) = {
    &D_002EA7E0, &D_002EA7E0,
};
// 0x2EA7F0: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EA7F0[2] RETAIL(D_002EA7F0) = {
    &D_002EA7E8, &D_002EA7E8,
};
// 0x2EA7F8: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA7F8[2] RETAIL(D_002EA7F8) = {
    &D_002EA7F0, &D_002EA7F0,
};
// 0x2EA800: nothing uses it
RETAIL_DATA(".data", 64) const void* D_002EA800[2] RETAIL(D_002EA800) = {
    &D_002EA7F8, &D_002EA7F8,
};
// 0x2EA808: nothing uses it
RETAIL_DATA(".data", 8) const void* D_002EA808[4] RETAIL(D_002EA808) = {
    &D_002EA800, &D_002EA800, &D_002EA808, &D_002EA808,
};
// 0x2EA818
RETAIL_DATA(".data", 8) u32 D_002EA818[2] RETAIL(D_002EA818) = {
    0x20000, 0x0,
};
// 0x2EA820
RETAIL_DATA(".data", 32) u32 D_002EA820[2] RETAIL(D_002EA820) = {
    0x0, 0x0,
};
// 0x2EA828
RETAIL_DATA(".data", 8) u32 D_002EA828[2] RETAIL(D_002EA828) = {
    0xFFFFFFFF, 0x0,
};
// 0x2EA830
RETAIL_DATA(".data", 16) u32 D_002EA830[2] RETAIL(D_002EA830) = {
    0x0, 0x0,
};
// 0x2EA838
RETAIL_DATA(".data", 8) u32 D_002EA838[2] RETAIL(D_002EA838) = {
    0x0, 0x0,
};
// 0x2EA840
RETAIL_DATA(".data", 64) u32 D_002EA840[10] RETAIL(D_002EA840) = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2EA868: nothing uses it
RETAIL_DATA(".data", 8) u32 D_002EA868[2] RETAIL(D_002EA868) = {
    0x1, 0x0,
};
// 0x2EA870: nothing uses it
RETAIL_DATA(".data", 16) u32 D_002EA870[108] RETAIL(D_002EA870) = {
    0x1, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0x2, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0x3, 0x3, 0xA, 0xA,
    0xA, 0xA, 0x6, 0x4, 0x4, 0x4, 0x4, 0x4, 0xB, 0x4, 0xB, 0xB, 0x5, 0x5, 0x5, 0x5, 0x5, 0xB, 0x5, 0xB, 0x8, 0xA, 0xA, 0xA, 0xA,
    0xA, 0xB, 0xA, 0xA, 0xB, 0xB, 0x7, 0xB, 0xB, 0xB, 0xB, 0xB, 0xB, 0xB, 0xB, 0xB, 0xB, 0x0, 0x0, 0xB, 0xB, 0xB, 0xB, 0xB, 0x9,
    0xB, 0xB, 0xB, 0xB, 0xB, 0xB, 0xB, 0xB, 0xB, 0xB, 0xA, 0xA, 0xB, 0xB, 0xB, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2EAA20: nothing uses it
RETAIL_DATA(".data", 32) u32 D_002EAA20[111] RETAIL(D_002EAA20) = {
    0x5, 0x0, 0x0, 0x0, 0x0, 0x0, 0x6, 0x0, 0x0, 0x0, 0x5, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x4, 0x4, 0x0, 0x0,
    0x0, 0x0, 0x5, 0x5, 0x5, 0x5, 0x5, 0x5, 0x7, 0x5, 0x7, 0x7, 0x5, 0x5, 0x5, 0x5, 0x5, 0x7, 0x5, 0x7, 0x5, 0x2, 0x2, 0x2, 0x2,
    0x2, 0x7, 0x2, 0x2, 0x7, 0x7, 0x5, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x5, 0x5, 0x7, 0x7, 0x7, 0x7, 0x7, 0x5,
    0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x1, 0x1, 0x7, 0x7, 0x7, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x49497350, 0x6B62696C, 0x6C6E7265,
};
// 0x2EABDC: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002EABDC[1] RETAIL(D_002EABDC) = {
    0x30303033,
};
// 0x2EABE0: nothing uses it
RETAIL_DATA(".data", 32) u32 D_002EABE0[1] RETAIL(D_002EABE0) = {
    0x0,
};
// 0x2EABE4: the heap's break, from the executable's end on the PS2 (the desktop's is in its arena)
RETAIL_DATA(".data", 4) const void* D_002EABE4[1] RETAIL(D_002EABE4) = {
#if defined(_EE)
    Ref__end,
#else
    nullptr,
#endif
};
// 0x2EABE8: nothing uses it
RETAIL_DATA(".data", 8) u32 G_UnkThreadId2[1] RETAIL(G_UnkThreadId2) = {
    0x0,
};
// 0x2EABEC: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002EABEC[1] RETAIL(D_002EABEC) = {
    0x0,
};
// 0x2EABF0: nothing uses it
RETAIL_DATA(".data", 16) u32 D_002EABF0[1] RETAIL(D_002EABF0) = {
    0x0,
};
// 0x2EABF4: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002EABF4[1] RETAIL(D_002EABF4) = {
    0x0,
};
// 0x2EABF8: nothing uses it
RETAIL_DATA(".data", 8) u32 D_002EABF8[32] RETAIL(D_002EABF8) = {
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF,
};
// 0x2EAC78: nothing uses it
RETAIL_DATA(".data", 8) u32 D_002EAC78[1] RETAIL(D_002EAC78) = {
    0x0,
};
// 0x2EAC7C: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002EAC7C[1] RETAIL(D_002EAC7C) = {
    0x0,
};
// 0x2EAC80: nothing uses it
RETAIL_DATA(".data", 64) u32 D_002EAC80[1] RETAIL(D_002EAC80) = {
    0x0,
};
// 0x2EAC84: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002EAC84[1] RETAIL(D_002EAC84) = {
    0xFFFFFFFF,
};
// 0x2EAC88: nothing uses it
RETAIL_DATA(".data", 8) u32 D_002EAC88[1] RETAIL(D_002EAC88) = {
    0xFFFFFFFF,
};
// 0x2EAC8C: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002EAC8C[1] RETAIL(D_002EAC8C) = {
    0xFFFFFFFF,
};
// 0x2EAC90: nothing uses it
RETAIL_DATA(".data", 16) const void* D_002EAC90[1] RETAIL(D_002EAC90) = {
    Ref_D_00309168,
};
// 0x2EAC94: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002EAC94[1] RETAIL(D_002EAC94) = {
    0xFFFFFFFF,
};
// 0x2EAC98: nothing uses it
RETAIL_DATA(".data", 8) u32 D_002EAC98[1] RETAIL(D_002EAC98) = {
    0xFFFFFFFF,
};
// 0x2EAC9C: nothing uses it
RETAIL_DATA(".data", 4) const void* D_002EAC9C[1] RETAIL(D_002EAC9C) = {
    Ref_D_00309380,
};
// 0x2EACA0: nothing uses it
RETAIL_DATA(".data", 32) u32 D_002EACA0[2] RETAIL(D_002EACA0) = {
    0x0, 0x0,
};
// 0x2EACA8: nothing uses it
RETAIL_DATA(".data", 8) u32 syscallTableIndex_0x83[1] RETAIL(syscallTableIndex_0x83) = {
    0x83,
};
// 0x2EACAC: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002EACAC[1] RETAIL(D_002EACAC) = {
    0x0,
};
// 0x2EACB0: nothing uses it
RETAIL_DATA(".data", 16) u32 syscallTableIndex_0x5A[1] RETAIL(syscallTableIndex_0x5A) = {
    0x5A,
};
#if defined(_EE)
// 0x2EACB4: nothing uses it (the PS2 side points it at its own functions)
RETAIL_DATA(".data", 4) const void* D_002EACB4[1] RETAIL(D_002EACB4) = {
    Ref_kCopy,
};
#endif
// 0x2EACB8: nothing uses it
RETAIL_DATA(".data", 8) u32 semaphore1_ID[1] RETAIL(semaphore1_ID) = {
    0x0,
};
// 0x2EACBC: nothing uses it
RETAIL_DATA(".data", 4) u32 semaphore2_ID[1] RETAIL(semaphore2_ID) = {
    0x0,
};
// 0x2EACC0: nothing uses it
RETAIL_DATA(".data", 64) u32 D_002EACC0[490] RETAIL(D_002EACC0) = {
    0x27BDFFE0, 0x24050026, 0xFFB00000, 0x80802D, 0xFFBF0010, 0x3C048007, 0xC01D07A, 0x24844700, 0x3C028007, 0x3C05FFFF,
    0x3C0603FF, 0x3C070C00, 0x24434780, 0x202D, 0x34A5C402, 0x34C6FFFF, 0x8C620000, 0x56020007, 0x24840001, 0x16050009,
    0x8C620004, 0x21082, 0x461024, 0x10000005, 0x471025, 0x2C820005, 0x1440FFF5, 0x24630008, 0x102D, 0xDFBF0010, 0xDFB00000,
    0x3E00008, 0x27BD0020, 0x0, 0x3C058007, 0x8C830000, 0x8CA247A8, 0x2406FFFE, 0x661824, 0x2407FFF9, 0x30420001, 0x2408FFF7,
    0x621825, 0x2409FFEF, 0xAC830000, 0x240AE01F, 0x671824, 0x3C06FFFF, 0x8CA247A8, 0x34C61FFF, 0x24A747A8, 0x30420006,
    0x621825, 0xAC830000, 0x681824, 0x8CA247A8, 0x30420008, 0x621825, 0xAC830000, 0x691824, 0x8CA247A8, 0x30420010, 0x621825,
    0xAC830000, 0x6A1824, 0x8CA247A8, 0x30421FE0, 0x621825, 0xAC830000, 0x661824, 0x8CA247A8, 0x3042E000, 0x621825, 0xAC830000,
    0x94E20002, 0x3E00008, 0xA4820002, 0x0, 0x3C058007, 0x8C820000, 0x8CA347A8, 0x2406FFFE, 0x30420001, 0x2407FFF9, 0x661824,
    0x2408FFF7, 0x621825, 0x2409FFEF, 0xACA347A8, 0x240AE01F, 0x671824, 0x3C06FFFF, 0x8C820000, 0x34C61FFF, 0x24A747A8,
    0x30420006, 0x621825, 0xACA347A8, 0x681824, 0x8C820000, 0x30420008, 0x621825, 0xACA347A8, 0x691824, 0x8C820000, 0x30420010,
    0x621825, 0xACA347A8, 0x6A1824, 0x8C820000, 0x30421FE0, 0x621825, 0xACA347A8, 0x661824, 0x8C820000, 0x3042E000, 0x621825,
    0xACA347A8, 0x94820002, 0x3E00008, 0xA4E20002, 0x0, 0x3C06BC00, 0x8CC603C0, 0x10C00011, 0x3C088007, 0x3C02BC00, 0xC23021,
    0x25074700, 0x24C6000F, 0x282D, 0x0, 0xC51021, 0xE52021, 0x90430000, 0x24A50001, 0x28A20026, 0xA0830000, 0x1440FFF9, 0x0,
    0x10000002, 0xDD034700, 0xDD034700, 0x316B8, 0x2103F, 0x30420007, 0x14400015, 0x0, 0x2402FEFF, 0x21438, 0x3442FFFF, 0x21438,
    0x3442FFFF, 0x2404F3FF, 0x42438, 0x3484FFFF, 0x42438, 0x3484FFFF, 0x621024, 0x3C03FFFF, 0x34630FFF, 0x31C38, 0x3463FFFF,
    0x31C38, 0x3463FFFF, 0x441024, 0x431024, 0xFD024700, 0x3E00008, 0x0, 0xA63821, 0x2CE20081, 0x14400008, 0x80502D, 0x2CC20080,
    0x10400003, 0x24020080, 0x10000003, 0x462823, 0x24060080, 0x282D, 0xA62821, 0xC5102B, 0x1040000F, 0x402D, 0xA0382D,
    0x3C098007, 0x3C058007, 0x24A247B0, 0x1482021, 0xC21021, 0x25080001, 0x90430000, 0x24C60001, 0xC7102B, 0x1440FFF8,
    0xA0830000, 0x10000003, 0xDD234700, 0x3C098007, 0xDD234700, 0x316B8, 0x2103F, 0x30420007, 0x14400003, 0x0, 0x3E00008,
    0x102D, 0x3133E, 0x3E00008, 0x3042000F, 0x0, 0xA0182D, 0x662821, 0x2CA20081, 0x14400009, 0x80482D, 0x2CC20080, 0x10400003,
    0x24020080, 0x10000003, 0x461823, 0x24060080, 0x182D, 0x662821, 0xC5102B, 0x1040000C, 0x382D, 0x3C088007, 0x0, 0x1271021,
    0x250347B0, 0xC31821, 0x90440000, 0x24C60001, 0x24E70001, 0xC5102B, 0x1440FFF8, 0xA0640000, 0x3E00008, 0x0, 0x0, 0x27BDFFF0,
    0x3C021000, 0xFFBF0000, 0x24030004, 0x3442F000, 0x3C041000, 0xAC430000, 0x3484F000, 0x8C820000, 0x30420004, 0x0, 0x0, 0x0,
    0x1040FFFA, 0x0, 0x3C021000, 0x24040004, 0x3442F000, 0x3C038007, 0xAC440000, 0x8C624760, 0x40F809, 0x0, 0x3C028007,
    0x24050002, 0x8C434764, 0x24060001, 0x60F809, 0x24040001, 0x3C028007, 0x8C43474C, 0x60F809, 0x3404DFFD, 0x3C028007,
    0x8C434750, 0x60F809, 0x0, 0x3C028007, 0x8C43475C, 0x60F809, 0x2404007F, 0x3C028007, 0x8C434754, 0x60F809, 0x0, 0x3C028007,
    0x8C434758, 0x60F809, 0x0, 0xDFBF0000, 0x3E00008, 0x27BD0010, 0x27BDFF50, 0x3C028007, 0xFFBE0090, 0x3C038007, 0xFFB70080,
    0x241E0010, 0xFFB60070, 0x3C178007, 0xFFB50060, 0x3C168007, 0xFFB40050, 0x80A82D, 0xFFB20030, 0xC0A02D, 0xFFB10020,
    0x3C128007, 0xFFB00010, 0x2411004C, 0x8C484728, 0x24100001, 0xFFBF00A0, 0xAFA50000, 0xFFB30040, 0x8D130000, 0x8C624730,
    0xAFA70004, 0x40F809, 0x260202D, 0x3C038007, 0x260202D, 0x8C624734, 0x40F809, 0x282D, 0x0, 0x8EC24748, 0x2221021,
    0x8C420008, 0x50400010, 0x26100001, 0x5213000E, 0x26100001, 0x145E0006, 0x8EE24744, 0x8E424740, 0x40F809, 0x200202D,
    0x10000007, 0x26100001, 0x40F809, 0x200202D, 0x8E424740, 0x40F809, 0x200202D, 0x26100001, 0x2A020100, 0x1440FFEA,
    0x2631004C, 0x3C038007, 0x8C62473C, 0x40F809, 0x802D, 0x3C038007, 0x8C624738, 0x40F809, 0x0, 0x3C038007, 0x8C62472C,
    0xC01D0F2, 0xAC400000, 0x3C028007, 0x1A80000D, 0x8C444778, 0x3C118007, 0x8FA50004, 0x101880, 0x8E224770, 0x26100001,
    0x651821, 0x40F809, 0x8C650000, 0x40202D, 0x214102A, 0x1440FFF7, 0x8FA50004, 0x2402004C, 0x3C038007, 0x2621018, 0x8EC54748,
    0x8C644778, 0x3C038007, 0x8C66476C, 0x451021, 0xAC440038, 0x8FA50000, 0xAC540034, 0xAC55000C, 0xAC550030, 0xAC450014,
    0xA440001A, 0xA4400018, 0xAC400024, 0xAC40001C, 0xC0F809, 0xAC400020, 0x3C028007, 0x8C434768, 0x60F809, 0x0, 0x2A0102D,
    0xDFBF00A0, 0xDFBE0090, 0xDFB70080, 0xDFB60070, 0xDFB50060, 0xDFB40050, 0xDFB30040, 0xDFB20030, 0xDFB10020, 0xDFB00010,
    0x3E00008, 0x27BD00B0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x40, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x800125EC, 0x800125F4, 0x80004970, 0x80004288, 0x800021B0, 0x80004E68, 0x80003F00, 0x80003E00, 0x80017400, 0x8000B8D0,
    0x8000B900, 0x8000B7A8, 0x8000B840, 0x8000AD68, 0x8000AA60, 0x8000A060, 0x80002A80, 0x80002AC0, 0x80005560, 0x80012600,
    0x80012608, 0x0, 0x4A, 0x80074138, 0x4B, 0x80074088, 0x6E, 0x80074350, 0x6F, 0x800742A8, 0xFFFFC402, 0x80074498,
};
// 0x2EB468: nothing uses it
RETAIL_DATA(".data", 8) u32 D_002EB468[8] RETAIL(D_002EB468) = {
    0x5A, 0x0, 0x5B, 0x80074000, 0xFFFFC402, 0x0, 0x5A, 0x0,
};
// 0x2EB488: nothing uses it
RETAIL_DATA(".data", 8) D_002EB488_Fields D_002EB488 RETAIL(D_002EB488) = {0x3C028007, 0x282D, 0x24436710, 0x0, 0x8C620000, 0x14820003, 0x24A50001, 0x3E00008, 0x8C620004, 0x2CA20006, 0x1440FFF9, 0x24630008, 0x3E00008, 0x102D, 0xA4202A, 0x10800003, 0x3C020001, 0x3E00008, 0xA21025, 0x3E00008, 0xA0102D, 0x0, 0x27BDFF80, 0xFFB30030, 0x3C138007, 0xFFB60060, 0xFFB50050, 0x80B02D, 0xFFB00000, 0xA0A82D, 0x8E626700, 0x802D, 0xFFBF0070, 0xFFB40040, 0xFFB20020, 0x18400028, 0xFFB10010, 0x3C148007, 0x24120014, 0x0, 0x26916740, 0x2121018, 0x2C0202D, 0x511021, 0xC01D80E, 0x94450000, 0x2A2102A, 0x10400018, 0x8E626700, 0x2444FFFF, 0x90182A, 0x14600018, 0x921018, 0x511821, 0x68650007, 0x6C650000, 0x6866000F, 0x6C660008, 0x8C670010, 0xB065001B, 0xB4650014, 0xB0660023, 0xB466001C, 0xAC670024, 0x2484FFFF, 0x2463FFEC, 0x90102A, 0x0, 0x1040FFF1, 0x0, 0x10000006, 0x200102D, 0x26100001, 0x202102A, 0x1440FFDD, 0x24120014, 0x200102D, 0xDFBF0070, 0xDFB60060, 0xDFB50050, 0xDFB40040, 0xDFB30030, 0xDFB20020, 0xDFB10010, 0xDFB00000, 0x3E00008, 0x27BD0080, 0x0, 0x27BDFF70, 0x3C02B000, 0xFFB70070, 0x34421800, 0xFFB60060, 0xC0B82D, 0xFFB50050, 0xA0B02D, 0xFFB30030, 0x3C158007, 0xFFBF0080, 0x3093FFFF, 0xFFB20020, 0xFFB10010, 0xFFB00000, 0xFFB40040, 0x8EA36700, 0x8C540000, 0x28630040, 0x14600008, 0x2749821, 0x1000002E, 0x2402FFFF, 0x60902D, 0x621014, 0x821025, 0x1000000D, 0xFCA26708, 0x3C058007, 0x182D, 0xDCA46708, 0x641016, 0x30420001, 0x1040FFF5, 0x24020001, 0x24630001, 0x28620040, 0x1440FFFA, 0x641016, 0x2412FFFF, 0x640001B, 0x240102D, 0x380882D, 0x260282D, 0xC01D816, 0x280202D, 0x24040014, 0x3C088007, 0x441018, 0x25036740, 0x8EA56700, 0x24700004, 0x24A50001, 0x432021, 0x623821, 0x508021, 0xA4940002, 0xA4930000, 0xE0302D, 0xAE120000, 0xC0182D, 0xACF10010, 0x95046740, 0xACD60008, 0xAC77000C, 0xC01D918, 0xAEA56700, 0x8E020000, 0xDFBF0080, 0xDFB70070, 0xDFB60060, 0xDFB50050, 0xDFB40040, 0xDFB30030, 0xDFB20020, 0xDFB10010, 0xDFB00000, 0x3E00008, 0x27BD0090, 0x0, 0x27BDFFD0, 0x3C0C8007, 0xFFB10010, 0x80682D, 0x8D826700, 0x180882D, 0xFFBF0020, 0x2406FFFF, 0x18400058, 0xFFB00000, 0x18400056, 0x402D, 0x3C0B8007, 0x24030014, 0x25656740, 0x1032018, 0xA41021, 0x8C430004, 0x15A3004A, 0x8D826700, 0x3C03B000, 0x852021, 0x34631820, 0x94850000, 0x8C620000, 0x14A20008, 0x24030014, 0x3C021000, 0x3442F000, 0x8C430000, 0x30631000, 0x14600043, 0x2402FFFF, 0x24030014, 0x8D896700, 0x1031818, 0x25646740, 0x2522FFFF, 0x100382D, 0x102102A, 0x641821, 0x10400019, 0x94700002, 0x3C0A8007, 0x24E30001, 0x24050014, 0x651018, 0xE52018, 0x25666740, 0x60382D, 0x462821, 0x862021, 0x2522FFFF, 0x68A30007, 0x6CA30000, 0x68A6000F, 0x6CA60008, 0x8CAE0010, 0xB0830007, 0xB4830000, 0xB086000F, 0xB4860008, 0xE2102A, 0x1440FFEC, 0xAC8E0010, 0x10000003, 0x24020001, 0x3C0A8007, 0x24020001, 0x8D846700, 0xDD436708, 0x1A21014, 0x21027, 0x2484FFFF, 0x621824, 0xAD846700, 0x15000003, 0xFD436708, 0xC01D918, 0x95646740, 0x8E226700, 0x14400004, 0x24030083, 0x3C02B000, 0x34421810, 0xAC430000, 0x3C02B000, 0x200202D, 0x34421800, 0xC01D80E, 0x8C450000, 0x10000005, 0x503023, 0x25080001, 0x102102A, 0x1440FFAE, 0x24030014, 0xF, 0xC0102D, 0xDFBF0020, 0xDFB10010, 0xDFB00000, 0x3E00008, 0x27BD0030, 0x27BDFFF0, 0xFFBF0000, 0xC01D858, 0x3084FFFF, 0xF, 0xDFBF0000, 0x3E00008, 0x27BD0010, 0x3C02B000, 0x34421820, 0xAC440000, 0xF, 0x3C02B000, 0x24030583, 0x34421810, 0x3E00008, 0xAC430000, 0x0, 0x27BDFF50, 0x402D, 0xFFBF00A0, 0xFFB70090, 0xFFB60080, 0xFFB50070, 0xFFB40060, 0xFFB30050, 0xFFB20040, 0xFFB10030, 0xFFB00020, 0x3C118007, 0x3C128007, 0x0, 0x8E226700, 0x102102A, 0x1040000A, 0x24030014, 0x26446740, 0x1031818, 0x96456740, 0x641821, 0x94620000, 0x10A2FFF6, 0x25080001, 0xC01D918, 0x40202D, 0x3C028007, 0x220B02D, 0x24546740, 0x24130014, 0x3C158007, 0x10000004, 0x24170001, 0x96426740, 0x1462003E, 0x8E226700, 0x8EC26700, 0x402D, 0x26466740, 0x68C30007, 0x6CC30000, 0x68C4000F, 0x6CC40008, 0x8CC50010, 0xB3A30007, 0xB7A30000, 0xB3A4000F, 0xB7A40008, 0xAFA50010, 0x2442FFFF, 0x1840001A, 0xAEC26700, 0x8E296700, 0x8FAA0010, 0x8FA60004, 0x97A70000, 0x0, 0x1131818, 0x25020001, 0x40402D, 0x742821, 0x531818, 0x742021, 0x688B0007, 0x6C8B0000, 0x688C000F, 0x6C8C0008, 0x8C8D0010, 0xB0AB0007, 0xB4AB0000, 0xB0AC000F, 0xB4AC0008, 0x109182A, 0x1460FFEF, 0xACAD0010, 0x10000004, 0x0, 0x8FAA0010, 0x8FA60004, 0x97A70000, 0x380802D, 0x140E02D, 0xDEA36708, 0xD71014, 0x21027, 0x3C040008, 0x621824, 0x8FA50008, 0x8FA8000C, 0x34842000, 0xC01D9A0, 0xFEA36708, 0x200E02D, 0x8E226700, 0x1C40FFC2, 0x97A30000, 0x8E226700, 0x18400005, 0x24030483, 0xC01D918, 0x96446740, 0x10000004, 0x0, 0x3C02B000, 0x34421810, 0xAC430000, 0xF, 0x42000038, 0xDFBF00A0, 0xDFB70090, 0xDFB60080, 0xDFB50070, 0xDFB40060, 0xDFB30050, 0xDFB20040, 0xDFB10030, 0xDFB00020, 0x3E00008, 0x27BD00B0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3C1A8007, 0xAF5F6C40, 0x3C1A8007, 0xAF5D6C50, 0x40847000, 0x40F, 0xA0182D, 0xC0202D, 0xE0282D, 0x100302D, 0x401A6000, 0x375A0012, 0x409A6000, 0x40F, 0x42000018, 0x0, 0x40016000, 0x241AFFE4, Ref_D_003A0824, 0x40816000, 0x40F, 0x3C1A8007, 0x8F5F6C40, 0x3C1A8007, 0x3E00008, 0x8F5D6C50, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFC, 0x80076440, 0xFE, 0x80076440, 0xFD, 0x800762A0, 0xFF, 0x800762A0, 0x12C, 0x80076488, 0x8, 0x800766C0};
// 0x2EBBC8: nothing uses it
RETAIL_DATA(".data", 8) u32 D_002EBBC8[10] RETAIL(D_002EBBC8) = {
    0x3C1D0008, 0x60F809, 0x27BD1FC0, 0x2403FFF8, 0xC, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2EBBF0: nothing uses it
RETAIL_DATA(".data", 16) u32 syscallTableIndex_5A[1] RETAIL(syscallTableIndex_5A) = {
    0x5A,
};
// 0x2EBBF4: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002EBBF4[1] RETAIL(D_002EBBF4) = {
    0x0,
};
// 0x2EBBF8: nothing uses it
RETAIL_DATA(".data", 8) u32 syscallTableIndex_5B[1] RETAIL(syscallTableIndex_5B) = {
    0x5B,
};
// 0x2EBBFC: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002EBBFC[13] RETAIL(D_002EBBFC) = {
    0x80076000, 0xFC, 0x0, 0xFE, 0x0, 0xFD, 0x0, 0xFF, 0x0, 0x12C, 0x0, 0x8, 0x0,
};
// 0x2EBC30: nothing uses it
RETAIL_DATA(".data", 16) u32 D_002EBC30[6] RETAIL(D_002EBC30) = {
    0x0, 0x0, 0xFFFFFFFF, 0x1, 0x0, 0x0,
};
// 0x2EBC48: nothing uses it
RETAIL_DATA(".data", 8) u32 D_002EBC48[2] RETAIL(D_002EBC48) = {
    0x0, 0xFFFFFFFF,
};
// 0x2EBC50: nothing uses it
RETAIL_DATA(".data", 16) u32 D_002EBC50[1] RETAIL(D_002EBC50) = {
    0x0,
};
// 0x2EBC54: nothing uses it
RETAIL_DATA(".data", 4) u32 D_002EBC54[1] RETAIL(D_002EBC54) = {
    0x0,
};
// 0x2EBC58: nothing uses it
RETAIL_DATA(".data", 8) u32 D_002EBC58[222] RETAIL(D_002EBC58) = {
    0x3C028007, 0x282D, 0x24435300, 0x0, 0x8C620000, 0x14820003, 0x24A50001, 0x3E00008, 0x8C620004, 0x2CA20006, 0x1440FFF9,
    0x24630008, 0x3E00008, 0x102D, 0x51602, 0x304300F0, 0x24020030, 0x10620014, 0x2C620031, 0x10400009, 0x24020010, 0x1062000E,
    0x2C620011, 0x1040000A, 0x24020020, 0x1060000C, 0x2402FFFF, 0x10000014, 0x0, 0x24020050, 0x10620005, 0x2C620051, 0x10400003,
    0x24020040, 0x10620003, 0x0, 0x3E00008, 0x2402FFFF, 0x40842800, 0x40855000, 0x40861000, 0x40871800, 0x40F, 0x42000006,
    0x40F, 0x42000008, 0x40F, 0x40020000, 0x3E00008, 0x0, 0x2C820030, 0x1040000B, 0x2402FFFF, 0x40840000, 0x40852800,
    0x40865000, 0x40871000, 0x40881800, 0x40F, 0x42000002, 0x40F, 0x3E00008, 0x80102D, 0x3E00008, 0x0, 0x0, 0x2C820030,
    0x14400003, 0x0, 0x3E00008, 0x2402FFFF, 0x40840000, 0x40F, 0x42000001, 0x40F, 0x40022800, 0xACA20000, 0x40035000,
    0xACC30000, 0x40021000, 0xACE20000, 0x40031800, 0xAD030000, 0x3E00008, 0x80102D, 0x0, 0x40845000, 0x40F, 0x42000008, 0x40F,
    0x40040000, 0x4810003, 0x0, 0x10000009, 0x2404FFFF, 0x42000001, 0x40F, 0x40022800, 0xACA20000, 0x40031000, 0xACC30000,
    0x40021800, 0xACE20000, 0x3E00008, 0x80102D, 0x0, 0x27BDFFD0, 0xFFB00010, 0x80802D, 0x32020FFF, 0x14400007, 0xFFBF0020,
    0x3C02000F, 0x2603FFFF, 0x3442FFFE, 0x43102B, 0x14400003, 0x3C047000, 0x1000003B, 0x2402FFFF, 0x3A0282D, 0x34844000,
    0x37A60004, 0xC01D456, 0x37A70008, 0x40282D, 0x4A10009, 0x0, 0x12000031, 0x102D, 0x40053000, 0x24A20001, 0x40823000, 0x40F,
    0x10000013, 0x0, 0x16000011, 0x24A2FFFF, 0x3C03E001, 0x21340, 0x433021, 0x40023000, 0x2442FFFF, 0x40823000, 0x40850000,
    0x40802800, 0x40865000, 0x40801000, 0x40801800, 0x40F, 0x42000002, 0x40F, 0x10000019, 0x102D, 0x3C02FFFF, 0x26041000,
    0x3442F000, 0x3C067000, 0x822024, 0xAFA00000, 0x2021024, 0x42182, 0x21182, 0x3484001F, 0x3442001F, 0x34C64000, 0xAFA20004,
    0xAFA40008, 0x40850000, 0x182D, 0x40832800, 0x40865000, 0x40821000, 0x40841800, 0x40F, 0x42000002, 0x40F, 0xA0102D,
    0xDFBF0020, 0xDFB00010, 0x3E00008, 0x27BD0030, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x55, 0x80075038, 0x56,
    0x800750C8, 0x57, 0x80075108, 0x58, 0x80075158, 0x59, 0x800751A8, 0x3, 0x80075330, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
};
// 0x2EBFD0: nothing uses it
RETAIL_DATA(".data", 16) u32 D_002EBFD0[2] RETAIL(D_002EBFD0) = {
    0x0, 0x0,
};
// 0x2EBFD8: nothing uses it
RETAIL_DATA(".data", 8) u32 D_002EBFD8[16] RETAIL(D_002EBFD8) = {
    0x5A, 0x0, 0x5B, 0x80075000, 0x54, 0x0, 0x55, 0x0, 0x56, 0x0, 0x57, 0x0, 0x58, 0x0, 0x59, 0x0,
};
// 0x2EC018: nothing uses it
RETAIL_DATA(".data", 8) u32 D_002EC018[52] RETAIL(D_002EC018) = {
    0x0, 0x70000000, 0x80000007, 0x7, 0x6000, 0xFFFF8000, 0x1E1F, 0x1F1F, 0x0, 0x10000000, 0x400017, 0x400053, 0x0, 0x10002000,
    0x400097, 0x4000D7, 0x0, 0x10004000, 0x400117, 0x400157, 0x0, 0x10006000, 0x400197, 0x4001D7, 0x0, 0x10008000, 0x400217,
    0x400257, 0x0, 0x1000A000, 0x400297, 0x4002D7, 0x0, 0x1000C000, 0x400313, 0x400357, 0x0, 0x1000E000, 0x400397, 0x4003D7,
    0x1E000, 0x11000000, 0x440017, 0x440415, 0x1E000, 0x12000000, 0x480017, 0x480415, 0x1FFE000, 0x1E000000, 0x780017, 0x7C0017,
};
// 0x2EC0E8: nothing uses it
RETAIL_DATA(".data", 8) u32 D_002EC0E8[72] RETAIL(D_002EC0E8) = {
    0x7E000, 0x80000, 0x201F, 0x301F, 0x7E000, 0x100000, 0x401F, 0x501F, 0x7E000, 0x180000, 0x601F, 0x701F, 0x1FE000, 0x200000,
    0x801F, 0xC01F, 0x1FE000, 0x400000, 0x1001F, 0x1401F, 0x1FE000, 0x600000, 0x1801F, 0x1C01F, 0x7FE000, 0x800000, 0x2001F,
    0x3001F, 0x7FE000, 0x1000000, 0x4001F, 0x5001F, 0x7FE000, 0x1800000, 0x6001F, 0x7001F, 0x7E000, 0x20080000, 0x2017, 0x3017,
    0x7E000, 0x20100000, 0x4017, 0x5017, 0x7E000, 0x20180000, 0x6017, 0x7017, 0x1FE000, 0x20200000, 0x8017, 0xC017, 0x1FE000,
    0x20400000, 0x10017, 0x14017, 0x1FE000, 0x20600000, 0x18017, 0x1C017, 0x7FE000, 0x20800000, 0x20017, 0x30017, 0x7FE000,
    0x21000000, 0x40017, 0x50017, 0x7FE000, 0x21800000, 0x60017, 0x70017,
};
// 0x2EC208: nothing uses it
RETAIL_DATA(".data", 8) u32 D_002EC208[32] RETAIL(D_002EC208) = {
    0x7E000, 0x30100000, 0x403F, 0x503F, 0x7E000, 0x30180000, 0x603F, 0x703F, 0x1FE000, 0x30200000, 0x803F, 0xC03F, 0x1FE000,
    0x30400000, 0x1003F, 0x1403F, 0x1FE000, 0x30600000, 0x1803F, 0x1C03F, 0x7FE000, 0x30800000, 0x2003F, 0x3003F, 0x7FE000,
    0x31000000, 0x4003F, 0x5003F, 0x7FE000, 0x31800000, 0x6003F, 0x7003F,
};
// 0x2EC288: nothing uses it
RETAIL_DATA(".data", 8) D_002EC288_Fields D_002EC288 RETAIL(D_002EC288) = {0xD, 0x12, 0x8, 0x0, &D_002EC018, &D_002EC0E8, &D_002EC208};
// 0x2EC2A4
RETAIL_DATA(".data", 4) u32 G_UnkFunTableSize[1] RETAIL(G_UnkFunTableSize) = {
    0x25,
};
// 0x2EC2A8: the static constructors G_UnkFunTableSize counts, read past it (the platforms' sides have the PS2 renderer's)
RETAIL_DATA(".data", 8) const void* D_002EC2A8[40] RETAIL(D_002EC2A8) = {
    Ref_FUN_001016f8, Ref_FUN_001230c8, Ref_FUN_0012d500, Ref_FUN_00141ce8, Ref_FUN_00162338, Ref_FUN_00167140,
    Ref_FUN_0017ca00, Ref_FUN_00182490, Ref_FUN_0018f7b8, Ref_FUN_0019b148, Ref_FUN_001a6678, Ref_FUN_001ad368,
    Ref_FUN_001ba330, Ref_FUN_001c75a0, Ref_FUN_001cccf0, Ref_FUN_001dd4d0, Ref_FUN_001e8ee8, Ref_FUN_001f4540,
    Ref_FUN_001f68b8, Ref_FUN_00201b70, Ref_FUN_00209ed8, Ref_FUN_0020ffa0, Ref_FUN_00225878, Ref_FUN_0022c2c8,
    Ref_FUN_00240bc8, Ref_FUN_00255430, Ref_FUN_0025d078, Ref_FUN_00263d88, Ref_FUN_0026eb60, Ref_FUN_0027ea00,
    Ref_FUN_00282980, Ref_FUN_00288930, Ref_FUN_00293320, Ref_FUN_0029f600, Ref_FUN_002b2230, Ref_FUN_002b4448,
    Ref_FUN_002b7850, nullptr, nullptr, nullptr,
};
} // namespace RetailData
