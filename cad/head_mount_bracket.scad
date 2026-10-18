// ==============================================================================
// OttoNinja Head Component Mounting Tray (Dual Deck)
// Target Hardware:
//   - Seeed Studio XIAO ESP32S3 Sense (21.0 x 17.5 mm) with front-facing camera
//   - Semtech SX1280 2.4GHz LoRa Breakout Module (25.0 x 16.0 mm)
//   - MAX98357A I2S Class-D Audio Amplifier (18.0 x 19.0 mm)
//   - 20mm Micro Speaker Chamber (20.5 mm dia)
// Suitable for FDM printing (0.2mm layer) or Bitlair Resin (SLA) printing.
// ==============================================================================

$fn = 60;

// Overall Tray Dimensions (fits inside standard Otto Ninja head shell)
tray_width  = 54.0; // X dimension
tray_depth  = 44.0; // Y dimension
tray_height = 24.0; // Z dimension
wall_thick  = 1.6;
floor_thick = 1.4;

// Component Pocket Dimensions
xiao_w = 21.4; // 21.0 + 0.4 clearance
xiao_d = 17.8; // 17.5 + 0.3 clearance
xiao_cam_cutout_d = 9.0; // Lens clearance

lora_w = 16.5;
lora_d = 25.5;
lora_slot_w = 2.2; // PCB edge slot

max_amp_w = 18.5;
max_amp_d = 19.5;

speaker_dia = 20.6;
speaker_depth = 5.0;

module main_tray() {
    difference() {
        // Outer chassis tray
        cube([tray_width, tray_depth, tray_height]);

        // Hollow interior (top deck)
        translate([wall_thick, wall_thick, floor_thick])
            cube([tray_width - 2*wall_thick, tray_depth - 2*wall_thick, tray_height]);

        // Front Camera Aperture (for XIAO OV2640 lens)
        translate([tray_width/2, -1, 14.0])
            rotate([-90, 0, 0])
            cylinder(d=xiao_cam_cutout_d, h=wall_thick + 2);

        // Acoustic sound slots (bottom floor speaker grill)
        for (i = [-3:3]) {
            translate([15.0 + i*2.5, 12.0, -1])
                cube([1.2, 16.0, floor_thick + 2]);
        }

        // Cable pass-through slot to robot base / neck
        translate([tray_width - 14.0, tray_depth - 12.0, -1])
            cube([10.0, 8.0, floor_thick + 2]);

        // Antenna pigtail / ceramic antenna notch on side
        translate([tray_width - 2, 22.0, 15.0])
            cube([wall_thick + 4, 8.0, 8.0]);
    }

    // XIAO ESP32-S3 Camera Support Frame (Front Centered)
    translate([tray_width/2 - xiao_w/2, wall_thick, floor_thick]) {
        difference() {
            cube([xiao_w, 4.0, 16.0]);
            translate([2.0, -1, 4.0])
                cube([xiao_w - 4.0, 6.0, 13.0]);
        }
        // Retaining side rails
        translate([-1.2, 0, 0]) cube([1.2, 6.0, 15.0]);
        translate([xiao_w, 0, 0]) cube([1.2, 6.0, 15.0]);
    }

    // Speaker Retention Ring (Bottom Deck, Left Side)
    translate([15.0, 20.0, floor_thick]) {
        difference() {
            cylinder(d=speaker_dia + 2.0, h=4.5);
            translate([0, 0, -0.5])
                cylinder(d=speaker_dia, h=6.0);
        }
    }

    // SX1280 LoRa Vertical Card Slot (Right Side)
    translate([tray_width - 8.0, 10.0, floor_thick]) {
        difference() {
            cube([6.0, lora_d + 4.0, 14.0]);
            translate([1.8, 2.0, -1])
                cube([lora_slot_w, lora_d, 16.0]);
        }
    }

    // MAX98357A Amp Retaining Clips (Rear Deck)
    translate([tray_width/2 - max_amp_w/2, tray_depth - max_amp_d - 4.0, floor_thick]) {
        // Corner posts with snap ledges
        snap_post(0, 0);
        snap_post(max_amp_w - 2.0, 0);
        snap_post(0, max_amp_d - 2.0);
        snap_post(max_amp_w - 2.0, max_amp_d - 2.0);
    }

    // M2 Standoff mounting bosses for securing to Otto Ninja chassis
    translate([4.0, 4.0, 0]) mounting_boss();
    translate([tray_width - 4.0, 4.0, 0]) mounting_boss();
    translate([4.0, tray_depth - 4.0, 0]) mounting_boss();
    translate([tray_width - 4.0, tray_depth - 4.0, 0]) mounting_boss();
}

module snap_post(x, y) {
    translate([x, y, 0]) {
        cube([2.0, 2.0, 6.0]);
        translate([0, 0, 5.0]) cube([2.4, 2.0, 1.0]); // small retaining barb
    }
}

module mounting_boss() {
    difference() {
        cylinder(d=5.0, h=6.0);
        translate([0, 0, -0.5]) cylinder(d=1.9, h=7.0); // M2 pilot hole
    }
}

// Render the completed bracket
main_tray();
