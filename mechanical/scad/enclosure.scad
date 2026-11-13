// 客流计数器吸顶外壳（参数化，可在 OpenSCAD 中改尺寸后导出 STL）
// 单位：mm

$fn = 64;

tof_spacing = 100;
tof_win_r   = 7.5;
cam_r       = 2.5;
shell_l     = 140;
shell_w     = 78;
shell_h     = 42;
wall        = 2.2;
base_r      = 45;
base_t      = 4;
neck_h      = 28;
neck_ro     = 14;
neck_ri     = 9;

module ceiling_base() {
    difference() {
        cylinder(r=base_r, h=base_t);
        cylinder(r=neck_ri, h=base_t+0.2);
        for (a=[0:90:270])
            rotate([0,0,a]) translate([28,0,-0.1])
                cylinder(r=2.0, h=base_t+0.2);
    }
}

module neck() {
    difference() {
        cylinder(r=neck_ro, h=neck_h);
        translate([0,0,-0.1]) cylinder(r=neck_ri, h=neck_h+0.2);
    }
}

module shell_upper() {
    h = shell_h * 0.55;
    difference() {
        // 外框
        translate([-shell_l/2,-shell_w/2,0])
            cube([shell_l, shell_w, h]);
        // 内腔
        translate([-shell_l/2+wall,-shell_w/2+wall,-0.1])
            cube([shell_l-2*wall, shell_w-2*wall, h-wall+0.1]);
        // OLED 窗
        translate([-14, shell_w/2-wall-0.1, h*0.35])
            cube([28, wall+0.2, 16]);
        // USB-C
        translate([20, -shell_w/2-0.1, 8])
            cube([9.2, wall+0.2, 3.4]);
        // 按键
        for (y=[-12,12])
            translate([shell_l/2-wall-0.1, y, 12])
                rotate([0,90,0]) cylinder(r=2.2, h=wall+0.2);
        // 颈孔
        translate([0,0,h-wall-0.1]) cylinder(r=neck_ri, h=wall+0.2);
    }
    // 颈接口凸台
    translate([0,0,h])
        difference() {
            cylinder(r=neck_ro+1.5, h=6);
            translate([0,0,-0.1]) cylinder(r=neck_ri, h=6.2);
        }
}

module shell_lower() {
    h = shell_h * 0.45;
    difference() {
        translate([-shell_l/2,-shell_w/2,0])
            cube([shell_l, shell_w, h]);
        translate([-shell_l/2+wall,-shell_w/2+wall,wall])
            cube([shell_l-2*wall, shell_w-2*wall, h]);
        // ToF 孔
        for (x=[-tof_spacing/2, tof_spacing/2])
            translate([x,0,-0.1]) cylinder(r=tof_win_r, h=wall+0.2);
        // 相机孔
        translate([0,0,-0.1]) cylinder(r=cam_r, h=wall+0.2);
    }
    // 遮光环
    for (x=[-tof_spacing/2, tof_spacing/2])
        translate([x,0,-3])
            difference() {
                cylinder(r=tof_win_r+2, h=3);
                translate([0,0,-0.1]) cylinder(r=tof_win_r, h=3.2);
            }
}

// 预览装配（导出单个零件时请只保留对应 module 调用）
translate([0,0,80]) ceiling_base();
translate([0,0,50]) neck();
translate([0,0,20]) shell_upper();
shell_lower();
