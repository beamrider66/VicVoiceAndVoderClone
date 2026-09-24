# VVVC User-Port Audio Carrier (WIP)

**WORK IN PROGRESS — do not send this layout to fabrication.**

KiCad 10 carrier PCB for the VVVC firmware. It plugs into the VIC-20 user
port, accepts a common 38-pin ESP32 DevKitC/WROOM development board, and breaks
out the audio and serial interface:

- 53 mm user-port insertion tongue with 24 contacts at 2.54 mm pitch. The
  contact geometry follows the open C64 diagnostic user-port board reference.
- ESP32 DevKitC/WROOM 38-pin socket headers. The USB connector remains exposed
  at the top edge for flashing and serial console use.
- GPIO22 PWM speech output on the original ESP32, through a two-stage filter.
- 3.5 mm stereo jack for the mixed line/headphone output.
- RCA input for VIC audio and RCA output for the mixed VIC + speech signal.
- Dual-gang 10 kOhm audio volume wheel after the buffer. It attenuates the
  headphone and RCA outputs together so the speech level can be matched to the
  VIC audio.
- Four 3.2 mm plated mounting holes on a 100 mm x 80 mm carrier outline for a
  future enclosure. Hole centers are (8,22), (92,22), (8,72), (92,72) mm.
- USB power through the ESP32 DevKit. Its USB 5 V pin feeds the carrier audio
  circuitry and its onboard regulator supplies 3.3 V. The VIC 5 V pin is
  deliberately left unconnected because the ESP32 can exceed the user-port's
  100 mA budget.
- This revision intentionally does not use or rectify the VIC's 9 VAC pins.
  Leave the two 9 VAC contacts unused and connect USB to the ESP32 board before
  using the carrier.

The audio section mixes the stereo RCA input with the filtered mono speech
signal, then sends the buffered stereo result through RV1 to both the RCA
output and the 3.5 mm jack. It is intended for powered speakers or a headphone-capable
amplifier. The ESP32 GPIO must never drive headphones directly. Fit the
optional NJM4556/OPA1678-class audio buffer and choose a headphone-safe supply
before connecting low-impedance headphones.

The enclosure should leave the ESP32 USB connector accessible at the top edge,
the 3.5 mm and RCA connectors accessible at the right edge, and a panel opening
for RV1's shaft/wheel beside those connectors. The 100 x 80 mm outline and the
four standoff centers in the netlist are the starting points for an enclosure;
check the actual connector bodies and shaft height against the parts purchased.

The generated board is a placement/netlist draft. It intentionally has no
copper routing because the final ESP32 header, audio buffer package, and jack
footprints must be confirmed against the parts being purchased. Open the board
in PCB Editor, route each highlighted net, run DRC, and check the 3-D view and
the physical user-port fit before ordering.

## User-port signals

The firmware interface uses M -> ESP32 GPIO18 (RX) through the 10 kOhm / 18 kOhm
divider, and ESP32 GPIO5 (TX) -> B and C. A and N are signal ground. The card
does not use the VIC 5 V or 9 VAC rails to power the ESP32; USB power enters
through the DevKit's 5 V header pin. The edge labels are
printed on the silkscreen. They are read from the component side with the
insertion tongue pointing away from you; verify this orientation against the
VIC service manual before inserting the card so the connector is not mirrored.

The populated user-port contacts are:

| Contact | Function |
| --- | --- |
| A, N, 1, 12 | GND |
| B, C | VIC transmit to ESP32 GPIO5 |
| M | VIC receive to ESP32 GPIO18 via the divider |
| 2 | Left open; VIC +5 V is not used |
| 10, 11 | Left open; VIC 9 VAC is not used |

All other contacts are left open for future expansion. The silkscreen is a
component-side aid, not a substitute for checking the mating connector's pin-1
mark and the board outline.

## Files

- `VVVC_UserPort_Audio.kicad_pcb` - PCB layout and footprints.
- `VVVC_UserPort_Audio.netlist.csv` - connector, signal and assembly notes.
- `generate_pcb.py` - deterministic generator for the board file.

This is an engineering first revision. Verify the physical user-port fit,
ESP32 board variant, jack footprints, audio gain and headphone safety with a
prototype before ordering a batch. Run KiCad's DRC after opening the board and
complete routing of any nets marked as unrouted.
