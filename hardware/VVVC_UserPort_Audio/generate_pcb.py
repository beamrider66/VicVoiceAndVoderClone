"""Generate the VVVC User-Port Audio Carrier KiCad PCB."""
from pathlib import Path

OUT = Path(__file__).with_name("VVVC_UserPort_Audio.kicad_pcb")

NETS = ["GND", "+5V_USB", "+3V3", "VIC_RX", "VIC_TX",
        "PWM_AUDIO", "AUDIO_IN_L", "AUDIO_IN_R", "MIX_L", "MIX_R",
        "LINE_OUT_L", "LINE_OUT_R", "HP_OUT_L", "HP_OUT_R", "RX_DIV",
        "PWM_F1", "PWM_F2"]
N = {name: i + 1 for i, name in enumerate(NETS)}


def q(value):
    return '"' + str(value).replace('"', '\\"') + '"'


def fp_start(name, ref, value, x, y, descr=""):
    lines = [f'(footprint {q("VVVC:" + name)} (layer "F.Cu") (at {x} {y})',
            f'  (property "Reference" {q(ref)} (at 0 -4 0) (layer "F.SilkS"))',
            f'  (property "Value" {q(value)} (at 0 4 0) (layer "F.Fab")',
            '    (hide yes))']
    if descr:
        lines.append(f'  (fp_text user {q(descr)} (at 0 0 0) (layer "F.SilkS") (effects (font (size 1 1) (thickness 0.15))))')
    return lines


def pad(number, x, y, net=None, kind="thru_hole", shape="circle", size=2, drill=1):
    layer = '"*.Cu" "*.Mask"' if kind == "thru_hole" else '"F.Cu" "F.Paste" "F.Mask"'
    drill_s = f' (drill {drill})' if kind == "thru_hole" else ''
    net_s = f' (net {N[net]} {q(net)})' if net else ''
    return f'  (pad {q(number)} {kind} {shape} (at {x} {y}) (size {size} {size}){drill_s} (layers {layer}){net_s})'


def add_edge(lines):
    lines += fp_start("VIC_USER_PORT_EDGE", "J1", "VIC-20 USER PORT", 50, 4, "INSERTION EDGE - COMPONENT SIDE")
    # Reference geometry: 12 contacts on each side, 2.54 mm pitch, two rows 6 mm apart.
    labels_a = ["A", "B", "C", "D", "E", "F", "H", "J", "K", "L", "M", "N"]
    labels_b = ["1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12"]
    for i, label in enumerate(labels_a):
        x = -14.0 + i * 2.54
        net = {"A": "GND", "B": "VIC_TX", "C": "VIC_TX", "M": "VIC_RX", "N": "GND"}.get(label)
        lines.append(f'  (pad {q(label)} smd roundrect (at {x:.2f} -0.1) (size 2.3 6) (layers "F.Cu" "F.Paste" "F.Mask") (roundrect_rratio 0.18)' + (f' (net {N[net]} {q(net)})' if net else '') + ')')
        lines.append(f'  (fp_text user {q(label)} (at {x:.2f} 4.2 0) (layer "F.SilkS") (effects (font (size 1 1) (thickness 0.15))))')
    for i, label in enumerate(labels_b):
        x = -14.0 + i * 2.54
        # Leave the VIC +5 V contact deliberately unconnected. The board has its
        # own protected 5 V input because ESP32 current peaks exceed the port budget.
        net = "GND" if label in ("1", "12") else None
        lines.append(f'  (pad {q(label)} smd roundrect (at {x:.2f} 6.1) (size 2.3 6) (layers "F.Cu" "F.Paste" "F.Mask") (roundrect_rratio 0.18)' + (f' (net {N[net]} {q(net)})' if net else '') + ')')
        lines.append(f'  (fp_text user {q(label)} (at {x:.2f} 10.2 0) (layer "F.SilkS") (effects (font (size 1 1) (thickness 0.15))))')
    lines += ['  (fp_rect (start -16 -3.2) (end 16 9.2) (stroke (width 0.3) (type default)) (fill none) (layer "F.SilkS"))']
    lines.append(')')


def add_headers(lines):
    # 38-pin ESP32 DevKit: two 19-pin socket rows, 25.4 mm apart.
    lines += fp_start("ESP32_DEVKITC_38", "JESP", "ESP32-DEVKITC-32 / WROOM", 50, 44, "ESP32 DEVKIT - USB THIS SIDE")
    left = ["3V3", "EN", "VP", "VN", "IO34", "IO35", "IO32", "IO33", "IO25", "IO26", "IO27", "IO14", "IO12", "GND", "IO13", "IO9", "IO10", "IO11", "5V"]
    right = ["GND", "IO23", "IO22", "IO1", "IO3", "IO21", "IO19", "IO18", "IO5", "IO17", "IO16", "IO4", "IO0", "IO2", "IO15", "SD1", "SD0", "CLK", "GND"]
    for i, name in enumerate(left):
        net = "+3V3" if name == "3V3" else "+5V_USB" if name == "5V" else "GND" if name == "GND" else None
        lines.append(pad(str(i + 1), -12.7, -22.86 + i * 2.54, net, size=2, drill=1))
        lines.append(f'  (fp_text user {q(name)} (at -15.8 {-22.86 + i * 2.54:.2f} 0) (layer "F.SilkS") (effects (font (size 0.75 0.75) (thickness 0.12))))')
    for i, name in enumerate(right):
        net = "GND" if name == "GND" else "PWM_AUDIO" if name == "IO22" else "VIC_RX" if name == "IO18" else "VIC_TX" if name == "IO5" else None
        lines.append(pad(str(i + 20), 12.7, -22.86 + i * 2.54, net, size=2, drill=1))
        lines.append(f'  (fp_text user {q(name)} (at 15.8 {-22.86 + i * 2.54:.2f} 0) (layer "F.SilkS") (effects (font (size 0.75 0.75) (thickness 0.12))))')
    lines += ['  (fp_rect (start -15 -25.4) (end 15 25.4) (stroke (width 0.3) (type default)) (fill none) (layer "F.SilkS"))', '  (fp_text user "USB" (at 0 -27) (layer "F.SilkS") (effects (font (size 1 1) (thickness 0.15))))', ')']


def add_component(lines, name, ref, value, x, y, nets, pins=2):
    lines += fp_start(name, ref, value, x, y)
    for i, net in enumerate(nets):
        px = (i - (pins - 1) / 2) * 2.54
        lines.append(pad(str(i + 1), px, 0, net, size=1.8, drill=0.9))
    lines += [f'  (fp_rect (start {-pins * 1.3} -1.5) (end {pins * 1.3} 1.5) (stroke (width 0.2) (type default)) (fill none) (layer "F.SilkS"))', ')']


def build():
    lines = ['(kicad_pcb (version 20240108) (generator pcbnew)', '  (general (thickness 1.6))', '  (paper "A4")',
             '  (layers (0 "F.Cu" signal) (31 "B.Cu" signal) (36 "B.SilkS" user "b.silkscreen") (37 "F.SilkS" user "f.silkscreen") (44 "Edge.Cuts" user))',
             '  (setup (pad_to_mask_clearance 0))']
    for i, net in enumerate(NETS, 1):
        lines.append(f'  (net {i} {q(net)})')
    add_edge(lines)
    add_headers(lines)
    add_component(lines, "RES_RX_TOP", "R1", "10k", 21, 18, ["VIC_RX", "RX_DIV"])
    add_component(lines, "RES_RX_BOTTOM", "R2", "18k", 21, 25, ["RX_DIV", "GND"])
    add_component(lines, "RES_PWM_SERIES", "R3", "1k", 72, 19, ["PWM_AUDIO", "PWM_F1"])
    add_component(lines, "RES_PWM_FILTER", "R4", "4.7k", 72, 26, ["PWM_F1", "PWM_F2"])
    add_component(lines, "CAP_PWM_1", "C1", "22nF", 78, 24, ["PWM_F1", "GND"])
    add_component(lines, "CAP_PWM_2", "C2", "4.7nF", 82, 30, ["PWM_F2", "GND"])
    add_component(lines, "CAP_PWM_DC", "C3", "1uF FILM", 72, 33, ["PWM_F2", "MIX_L"])
    add_component(lines, "MIX_IN_L", "R5", "10k", 68, 58, ["AUDIO_IN_L", "MIX_L"])
    add_component(lines, "MIX_IN_R", "R6", "10k", 68, 64, ["AUDIO_IN_R", "MIX_R"])
    add_component(lines, "MIX_SPEECH_R", "R7", "10k", 72, 40, ["PWM_F2", "MIX_R"])
    add_component(lines, "RCA_INPUT", "J3", "RCA IN L/R", 88, 48, ["AUDIO_IN_L", "AUDIO_IN_R"], 2)
    add_component(lines, "RCA_OUTPUT", "J4", "RCA OUT L/R", 88, 61, ["LINE_OUT_L", "LINE_OUT_R"], 2)
    add_component(lines, "HEADPHONE", "J2", "3.5mm TRS", 88, 34, ["LINE_OUT_L", "LINE_OUT_R", "GND"], 3)
    add_component(lines, "AUDIO_BUFFER", "U2", "NJM4556D / OPA1678", 76, 50, ["MIX_L", "MIX_R", "LINE_OUT_L", "LINE_OUT_R", "+5V_USB", "GND"], 6)
    for ref, x, y in (("H1", 8, 22), ("H2", 92, 22), ("H3", 8, 72), ("H4", 92, 72)):
        lines += fp_start("MOUNT_HOLE_3V2", ref, "STANDOFF", x, y)
        lines += ['  (pad "" np_thru_hole circle (at 0 0) (size 5 5) (drill 3.2) (layers "*.Cu" "*.Mask"))', '  (fp_circle (center 0 0) (end 2.8 0) (stroke (width 0.3) (type default)) (fill none) (layer "F.SilkS"))', ')']
    # Board outline: 100 x 80 mm body with 53 mm wide / 15 mm long insertion tongue.
    outline = [(0, 15), (23, 15), (23, 0), (77, 0), (77, 15), (100, 15), (100, 80), (0, 80), (0, 15)]
    for (x1, y1), (x2, y2) in zip(outline, outline[1:]):
        lines.append(f'  (gr_line (start {x1} {y1}) (end {x2} {y2}) (stroke (width 0.3) (type default)) (layer "Edge.Cuts"))')
    lines += ['  (gr_text "VVVC USER-PORT AUDIO CARRIER" (at 50 75) (layer "F.SilkS") (effects (font (size 2 2) (thickness 0.3))))',
              '  (gr_text "GPIO22 PWM  |  M -> RX  |  B/C <- TX  |  USB 5V POWER" (at 50 70) (layer "F.SilkS") (effects (font (size 1.2 1.2) (thickness 0.2))))',
              '  (gr_text "INSERT THIS EDGE INTO VIC USER PORT - COMPONENT SIDE UP" (at 50 12) (layer "F.SilkS") (effects (font (size 1.1 1.1) (thickness 0.18))))']
    # Deliberately leave the board unrouted.  The footprints and net names are
    # the reusable placement/netlist draft; routing must be completed and
    # checked in PCB Editor after the exact ESP32, audio buffer and connector
    # variants have been selected.  Earlier generated versions contained
    # illustrative copper links which were not electrically valid.
    lines.append('  (gr_text "DRAFT - ROUTE AND VERIFY IN PCBNEW BEFORE FAB" (at 50 77) (layer "F.SilkS") (effects (font (size 1.2 1.2) (thickness 0.2))))')
    lines.append(')')
    OUT.write_text('\n'.join(lines) + '\n', encoding='utf-8')


if __name__ == "__main__":
    build()
