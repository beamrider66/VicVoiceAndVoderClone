#!/usr/bin/env python3
"""Build the combined VVVC end-user and VIC-20 wiring guide."""
from html import escape
import json
from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas
from reportlab.platypus import Paragraph, Preformatted, Table, TableStyle

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "output/pdf/VVVC_User_and_VIC20_Wiring_Guide_v2.pdf"
DATE = "18 September 2026 | Rev 2"
NAVY = colors.HexColor("#17364A")
TEAL = colors.HexColor("#087E86")
INK = colors.HexColor("#243747")
MUTED = colors.HexColor("#556879")
PALE = colors.HexColor("#EDF5F5")
LINE = colors.HexColor("#D7E2E6")
AMBER = colors.HexColor("#8D5B15")
WARM = colors.HexColor("#FFF4DF")
PAGE_W, PAGE_H = A4
LEFT = 43
WIDTH = PAGE_W - LEFT * 2
BOTTOM = 49


def register_fonts():
    fonts = Path("C:/Windows/Fonts")
    if (fonts / "segoeui.ttf").exists():
        for name, file in (("Body", "segoeui.ttf"), ("Bold", "segoeuib.ttf"),
                           ("Italic", "segoeuii.ttf"), ("Mono", "consola.ttf")):
            pdfmetrics.registerFont(TTFont(name, str(fonts / file)))
        pdfmetrics.registerFontFamily("Body", normal="Body", bold="Bold", italic="Italic", boldItalic="Bold")
        return "Body", "Bold", "Mono"
    return "Helvetica", "Helvetica-Bold", "Courier"


BODY, BOLD, MONO = register_fonts()


class Guide:
    def __init__(self):
        OUT.parent.mkdir(parents=True, exist_ok=True)
        self.c = canvas.Canvas(str(OUT), pagesize=A4, pageCompression=1)
        self.c.setTitle("VVVC - User and VIC-20 Wiring Guide")
        self.c.setAuthor("VVVC project")
        self.c.setSubject("Review draft: operation, examples, RAM phrases and VIC-20 user-port wiring")
        self.c.setKeywords("VVVC, VIC-20, ESP32, ES8388, SC-01, wiring, user guide")
        self.c.setViewerPreference("DisplayDocTitle", "true")
        self.page = 0
        self.y = 0

    def start(self, section, title, subtitle=None):
        if self.page:
            self.c.showPage()
        self.page += 1
        c = self.c
        c.setFillColor(TEAL)
        c.rect(0, PAGE_H - 7, PAGE_W, 7, stroke=0, fill=1)
        c.setFont(BOLD, 9)
        c.setFillColor(TEAL)
        c.drawString(LEFT, PAGE_H - 30, "VVVC  /  " + section.upper())
        c.setFont(BODY, 8)
        c.setFillColor(MUTED)
        c.drawRightString(PAGE_W - LEFT, PAGE_H - 30, "REVIEW DRAFT  |  " + DATE)
        c.setStrokeColor(LINE)
        c.line(LEFT, 36, PAGE_W - LEFT, 36)
        c.setFont(BODY, 8)
        c.drawString(LEFT, 23, "Vic Voice and Voder Clone  |  VIC hardware validation pending")
        c.drawRightString(PAGE_W - LEFT, 23, str(self.page))
        key = "page" + str(self.page)
        c.bookmarkPage(key)
        c.addOutlineEntry(title, key, 0)
        self.y = PAGE_H - 58
        self.para(title, size=25, leading=30, font=BOLD, color=NAVY, after=9)
        if subtitle:
            self.para(subtitle, size=10.5, color=MUTED, after=15)

    def check(self, height):
        if self.y - height < BOTTOM:
            raise RuntimeError(f"Page {self.page} overflow: y={self.y:.1f}, height={height:.1f}")

    def para(self, text, size=10.3, leading=None, font=None, color=INK, after=9,
             width=None, x=None):
        width = WIDTH if width is None else width
        style = ParagraphStyle("p", fontName=font or BODY, fontSize=size,
                               leading=leading or size * 1.4, textColor=color,
                               alignment=TA_LEFT, allowWidows=0, allowOrphans=0)
        p = Paragraph(text, style)
        _, h = p.wrap(width, PAGE_H)
        self.check(h + after)
        p.drawOn(self.c, LEFT if x is None else x, self.y - h)
        self.y -= h + after

    def h2(self, text):
        self.y -= 4
        self.para(text, size=13.1, leading=17, font=BOLD, color=NAVY, after=7)

    def box(self, title, text, warning=False):
        style = ParagraphStyle("box", fontName=BODY, fontSize=10, leading=14,
                               textColor=INK)
        p = Paragraph(f"<b>{title}</b><br/>{text}", style)
        _, h = p.wrap(WIDTH - 28, PAGE_H)
        self.check(h + 32)
        self.c.setFillColor(WARM if warning else PALE)
        self.c.roundRect(LEFT, self.y - h - 22, WIDTH, h + 22, 5, stroke=0, fill=1)
        self.c.setFillColor(AMBER if warning else TEAL)
        self.c.rect(LEFT, self.y - h - 22, 3, h + 22, fill=1, stroke=0)
        p.drawOn(self.c, LEFT + 14, self.y - h - 11)
        self.y -= h + 32

    def code(self, text, size=9.5, leading=13):
        for line in text.splitlines():
            if pdfmetrics.stringWidth(line, MONO, size) > WIDTH - 26:
                raise RuntimeError(f"Code too wide on page {self.page}: {line}")
        p = Preformatted(text, ParagraphStyle("code", fontName=MONO,
                                              fontSize=size, leading=leading, textColor=NAVY))
        _, h = p.wrap(WIDTH - 26, PAGE_H)
        self.check(h + 28)
        self.c.setFillColor(colors.HexColor("#F2F5F8"))
        self.c.roundRect(LEFT, self.y - h - 18, WIDTH, h + 18, 4, stroke=0, fill=1)
        p.drawOn(self.c, LEFT + 13, self.y - h - 9)
        self.y -= h + 28

    def table(self, headers, rows, widths, size=9.5, padding=5):
        style = ParagraphStyle("cell", fontName=BODY, fontSize=size,
                               leading=size * 1.27, textColor=INK)
        head_style = ParagraphStyle("head", parent=style, fontName=BOLD, textColor=colors.white)
        cells = [[Paragraph(escape(str(x)), head_style) for x in headers]]
        cells += [[Paragraph(str(x), style) for x in row] for row in rows]
        table = Table(cells, colWidths=widths, hAlign="LEFT")
        table.setStyle(TableStyle([
            ("BACKGROUND", (0, 0), (-1, 0), NAVY),
            ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, colors.HexColor("#F2F6F7")]),
            ("VALIGN", (0, 0), (-1, -1), "TOP"),
            ("LEFTPADDING", (0, 0), (-1, -1), padding + 2),
            ("RIGHTPADDING", (0, 0), (-1, -1), padding + 2),
            ("TOPPADDING", (0, 0), (-1, -1), padding),
            ("BOTTOMPADDING", (0, 0), (-1, -1), padding),
            ("LINEBELOW", (0, 0), (-1, 0), 0.5, NAVY),
        ]))
        _, h = table.wrap(WIDTH, PAGE_H)
        self.check(h + 12)
        table.drawOn(self.c, LEFT, self.y - h)
        self.y -= h + 12

    def diagram(self, height, draw):
        self.check(height + 12)
        self.c.saveState()
        self.c.translate(LEFT, self.y - height)
        draw(self.c, WIDTH, height)
        self.c.restoreState()
        self.y -= height + 12

    def save(self):
        self.c.save()
        print(f"Created {OUT} ({self.page} pages)")


def label(c, x, y, text, size=10, color=INK, bold=False, align="left"):
    c.setFillColor(color)
    c.setFont(BOLD if bold else BODY, size)
    if align == "center":
        c.drawCentredString(x, y, text)
    elif align == "right":
        c.drawRightString(x, y, text)
    else:
        c.drawString(x, y, text)


def wire(c, points, color=TEAL, width=1.8):
    c.setStrokeColor(color)
    c.setLineWidth(width)
    p = c.beginPath()
    p.moveTo(*points[0])
    for point in points[1:]:
        p.lineTo(*point)
    c.drawPath(p)


def dot(c, x, y, color=TEAL):
    c.setFillColor(color)
    c.circle(x, y, 2.7, stroke=0, fill=1)


def resistor(c, x, y, text, vertical=False):
    c.setStrokeColor(INK)
    c.setLineWidth(1.2)
    c.setFillColor(colors.white)
    if vertical:
        c.rect(x - 5, y - 18, 10, 36, stroke=1, fill=1)
        label(c, x + 12, y - 3, text, 9)
    else:
        c.rect(x - 22, y - 5, 44, 10, stroke=1, fill=1)
        label(c, x, y + 12, text, 9, align="center")


def connector(c, w, h, view="computer"):
    views = {
        "computer": ("VIEW A: LOOKING INTO THE VIC FROM BEHIND",
                     "VIC upright; you face its rear, looking toward the keyboard.", False),
        "mating": ("VIEW B: LOOKING INTO THE CABLE'S MATING OPENING",
                   "Cable unplugged; look into the slot, toward the cable wires.", True),
        "wire": ("VIEW C: LOOKING AT THE CABLE'S WIRE / SOLDER SIDE",
                 "Look past the wire terminals toward the VIC; the mating opening faces away.", False),
    }
    title, note, mirrored = views[view]
    label(c, 0, h - 13, title, 10, TEAL, True)
    label(c, 0, h - 31, note, 9, MUTED)
    step = 37
    start = (w - step * 12) / 2
    c.setFillColor(colors.HexColor("#E7EEF1"))
    c.roundRect(start - 7, 37, step * 12 + 14, 60, 5, stroke=0, fill=1)
    letters = list("ABCDEFHJKLMN")
    numbers = list(range(1, 13))
    if mirrored:
        letters.reverse()
        numbers.reverse()
    for n in range(12):
        x = start + n * step + step / 2
        c.setFillColor(colors.HexColor("#ADBCC5"))
        c.rect(x - 11, 86, 22, 17, stroke=0, fill=1)
        label(c, x, 113, str(numbers[n]), 10, MUTED, align="center")
        active = letters[n] in "ABCMN"
        c.setFillColor(TEAL if active else colors.HexColor("#ADBCC5"))
        c.rect(x - 11, 30, 22, 17, stroke=0, fill=1)
        label(c, x, 12, letters[n], 11, TEAL if active else MUTED, True, "center")
    label(c, w / 2, 63, "TOP stays up - numbered row uppermost", 9.5,
          NAVY, True, "center")
    wire(c, [(12, 71), (12, 105)], NAVY, 1.2)
    wire(c, [(8, 99), (12, 105), (16, 99)], NAVY, 1.2)


def circuit(c, w, h):
    c.setFillColor(PALE)
    c.roundRect(0, 0, w, h, 6, stroke=0, fill=1)
    left, right, mid = 24, w - 24, w * 0.58
    label(c, left, h - 24, "VIC-20 USER PORT", 11, NAVY, True)
    label(c, right, h - 24, "ESP32 AUDIO-KIT", 11, NAVY, True, "right")
    y = h - 78
    label(c, left, y + 17, "M / CB2: VIC transmit", 9, NAVY)
    label(c, right, y + 17, "GPIO18: UART2 RX", 9, NAVY, align="right")
    wire(c, [(left, y), (right, y)])
    resistor(c, 193, y, "R1  10 kOhm")
    dot(c, mid, y)
    wire(c, [(mid, y), (mid, y - 84)])
    resistor(c, mid, y - 43, "R2  18 kOhm", True)
    label(c, right, y - 22, "about 3.2 V from a 5 V high", 8.6, MUTED, align="right")
    wire(c, [(left, y - 84), (right, y - 84)], MUTED)
    dot(c, mid, y - 84, MUTED)
    label(c, left, y - 101, "A or N: ground", 9, NAVY)
    label(c, right, y - 101, "GND: common ground", 9, NAVY, align="right")

    ry = 98
    label(c, left, ry + 27, "B / CB1: receive interrupt", 9, NAVY)
    label(c, left, ry - 24, "C / PB0: received data", 9, NAVY)
    wire(c, [(left, ry + 10), (230, ry + 10), (230, ry - 41), (left, ry - 41)])
    wire(c, [(230, ry - 15), (right, ry - 15)])
    dot(c, 230, ry - 15)
    resistor(c, 193, ry + 10, "R3  470 Ohm")
    resistor(c, 193, ry - 41, "R4  470 Ohm")
    label(c, right, ry + 3, "GPIO5: UART2 TX", 9, NAVY, align="right")
    label(c, right, ry - 33, "3.3 V return signal", 8.6, MUTED, align="right")
    label(c, w / 2, 16, "R3/R4 are optional series protection; they do not raise the logic voltage.", 8.8,
          MUTED, align="center")


def build():
    g = Guide()
    g.start("Start here", "VVVC", "Vic Voice and Voder Clone  |  End-user and VIC-20 wiring guide")
    g.para("Make your VIC speak", size=21, font=BOLD, color=NAVY, after=12)
    g.para("VVVC runs a software approximation of the Votrax SC-01 on ESP32 boards. The same "
           "speech feeds an always-on GPIO PWM output and, on the original Audio-Kit, its "
           "ES8388 codec. It speaks English text or SC-01 phonemes, and can learn "
           "phrases in numbered RAM slots for later recall.")
    g.box("One interface, one voice", "VIC-Voice-style controls and VIC-Voder-style baud commands "
          "share the same speech engine. There is no mode selector. Board keys have no assigned "
          "function; reset restarts the firmware and clears learned phrases.")
    g.h2("Start with the board alone")
    g.para("<b>1.</b> On an Audio-Kit, connect a suitable speaker to a labelled speaker output, or use the "
           "EARPHONE jack. On any other ESP32, connect the PWM pin through the filter on page 10 to an amplifier. Power the board through USB.<br/>"
           "<b>2.</b> Listen for the 880 Hz and 440 Hz startup tones. They check the audio "
           "output path without a VIC or terminal.<br/>"
           "<b>3.</b> For a speech check, send <b>-demo</b> through the USB serial console "
           "at 115200 baud. See page 2.")
    g.table(["Setting", "Current firmware"], [
        ["Hardware", "ESP32 / C3 / S2 / S3; Audio-Kit ES8388 is optional"],
        ["Audio", "Always-on 8-bit PWM: GPIO22 (ESP32), GPIO4 (C3/S2/S3); ES8388 is 16-bit"],
        ["USB console", "115200 baud, 8 data bits, no parity, 1 stop bit; no flow control"],
        ["VIC connection", "2400 baud, 8N1; GPIO18 receive, GPIO5 transmit"],
        ["Learning", "80 shared RAM slots; up to 256 SC-01 phonemes per slot"],
    ], [116, WIDTH - 116])
    g.h2("Inside this guide")
    g.table(["Section", "Pages"], [
        ["USB speech, learning and Wizard of Wor", "2-4"],
        ["VIC BASIC examples", "5"],
        ["VIC-20 wiring, connector views and first tests", "6-9"],
        ["Control reference, troubleshooting and sources", "10-11"],
        ["All 75 Wizard of Wor phrase numbers", "12-13"],
    ], [WIDTH - 65, 65], size=9.3, padding=3)
    g.para("<b>Review status:</b> audio, USB speech and 75 Wizard "
           "slots were tested on the board. This revision shares every dash command "
           "between USB and VIC, with host tests passing. Upload the revised firmware "
           "before testing those commands; the VIC cable and BASIC examples still await "
           "hardware validation.", size=9.4, color=MUTED)

    g.start("A / Everyday use", "USB speech and phonemes", "Use the Audio-Kit's USB-to-UART connection to send commands from a computer.")
    g.h2("Open a terminal")
    g.para("Choose the board's serial port and set <b>115200 baud, 8N1, no flow control</b>. "
           "Send a CR or LF when you press Enter. COM3 was used during this project's test; "
           "your computer may assign a different port. Only one terminal or loader can use "
           "that port at a time.")
    g.para("With Python installed, run the first line once, then open a terminal. "
           "Local echo shows what you type:")
    g.code("python -m pip install pyserial==3.5\npython -m serial.tools.list_ports\npython -m serial.tools.miniterm --dtr 0 --rts 0 -e COM3 115200", size=9.3)
    g.para("Press <b>Ctrl+]</b> to leave miniterm. Opening a serial port can reset some "
           "boards, even with control lines disabled. A reset clears the phrase bank.", size=9.4)
    g.h2("Speak plain English")
    g.code("HELLO WORLD\nShe sells seashells on the sea shore")
    g.para("Send one line and wait for it to finish. Ordinary text does not return an OK "
           "reply. English spelling is converted to SC-01 sounds, so pronunciation is an "
           "approximation. The USB limit is 253 bytes per complete line.")
    g.h2("Send phonemes directly")
    g.code("-phonemes G EH T R EH D Y PA1\n-phonemes I0_AH I1_AH I2_AH I3_AH")
    g.para("The first example says <b>Get ready</b>. Tokens are separated by spaces. "
           "PA0 and PA1 are pauses; I0_ through I3_ select inflection. An unknown token "
           "rejects the phrase. Successful playback returns <b>OK PHONEMES</b>.")
    g.table(["Both ports", "What happens"], [
        ["-demo", "Two tones, HELLO WORLD. THIS IS VIC VOICE., then GET READY; one run"],
        ["-tone", "Repeat the startup tones; reply: Tone complete."],
        ["-help", "Show commands, audio details, serial settings and phrase-bank limits"],
    ], [90, WIDTH - 90])
    g.box("Same commands on both ports", "On an open VIC channel, use "
          '<b>PRINT#1,"-DEMO"</b>, <b>PRINT#1,"-TONE"</b> or '
          '<b>PRINT#1,"-PHONEMES G EH T R EH D Y PA1"</b>. '
          "Replies return to the sending connection. The demo preserves serial settings "
          "and learned slots; it replies <b>Demo complete.</b> when finished.")

    g.start("A / Everyday use", "Learn once, recall by number", "Slots are shared between USB and the VIC connection.")
    g.para("Choose a slot from <b>1 to 80</b>. Learning stores phonemes in RAM without "
           "speaking them. Learning into an occupied slot replaces it only after the new "
           "phrase passes validation. Use 76-80 for your own phrases when the full Wizard "
           "of Wor catalog is loaded.")
    g.table(["Command", "Purpose"], [
        ["-learn 76 text HELLO WORLD", "Convert English now and store the resulting sounds"],
        ["-learn 77 phonemes G EH T R EH D Y PA1", "Store named SC-01 phones, including inflection prefixes"],
        [f'<font name="{MONO}">' + r"-learn 78 compact ~\{j~?" + "</font>", "Store compact codes for G EH T PA1; the backslash is literal"],
        ["-play 76", "Speak the learned phrase; a bare 76 is ordinary text"],
        ["-slots", "List occupied slot numbers and phoneme counts"],
        ["-forget 76", "Delete one slot"],
        ["-clear", "Delete all learned slots"],
    ], [268, WIDTH - 268], size=9.3)
    g.h2("A complete exchange")
    g.code("Send:   -learn 76 text HELLO WORLD\nReply:  OK 76\n\nSend:   -play 76\nReply:  OK 76")
    g.para("The play reply arrives <b>after speech completes</b>. Send one command and "
           "wait for its reply before sending the next. Replies return on the port that "
           "sent the command, with CR/LF line endings.")
    g.h2("Limits and error handling")
    g.para("Each slot holds up to 256 phones. Text conversion accepts up to 253 input "
           "bytes, but the USB line limit includes the command itself. Named and compact "
           "input must also fit the receiving port's line buffer. Compact blocks require "
           "an opening marker and final question mark; each data byte must be 0x40-0x7F.")
    g.para("Examples of error replies: <b>ERROR 76 EMPTY SLOT</b>, <b>ERROR 76 INVALID "
           "PHONEME</b>, or <b>ERROR SLOT MUST BE 1..80</b>. A rejected replacement leaves "
           "the previous phrase intact. -slots reports counts, not the original text.")
    g.box("Temporary means RAM-only", "All slots disappear when the ESP32 resets or loses "
          "power. They are not written to flash. Resetting only the VIC leaves a separately "
          "powered ESP32's bank intact. Normal speech and baud changes preserve it.", warning=True)

    g.start("A / Everyday use", "Load Wizard of Wor", "The supplied host script teaches the original catalog through normal learning commands.")
    g.para("The firmware starts with empty slots. <b>tools/learn_wizard.py</b> loads all "
           "75 catalog entries into slots 1-75, preserving the original numbering, phonemes "
           "and inflection. Slots 76-80 are left alone. The catalog is included in the "
           "catalog; no other project is needed.")
    g.h2("Load the whole catalog")
    g.para("Close the serial terminal first. In the VVVC project folder, run:")
    g.code("python -m pip install -r tools/requirements.txt\npython tools/learn_wizard.py --port COM3")
    g.para("The script prints each slot as it receives OK, then checks the returned "
           "phoneme count for every loaded slot. A successful run ends with <b>Verified "
           "75 phrases</b>. It replaces any existing content in slots 1-75.")
    g.h2("Play immediately, or load a selection")
    g.code("python tools/learn_wizard.py --port COM3 --play 26\npython tools/learn_wizard.py --port COM3 --phrases 21 26 55\npython tools/learn_wizard.py --list\npython tools/learn_wizard.py --dry-run", size=9.3)
    g.para("--play loads and verifies first, then speaks the requested slot once. "
           "--phrases loads only the listed original IDs. --list shows the catalog; "
           "--dry-run prints the learn commands without connecting.")
    g.table(["Recall", "Phrase"], [
        ["-play 21", "Get ready, worrior."],
        ["-play 24", "Hey, insert coin."],
        ["-play 26", "I am The Wizord of Wor."],
        ["-play 46", "Remember, I'm the wizard, not you!"],
        ["-play 55", "Welcome, to my world of wor."],
    ], [87, WIDTH - 87])
    g.para("These are the original catalog labels, including their spelling. The full "
           "number list is on pages 12-13. Recall works from either connection; there is "
           "no separate Wizard mode or background random playback.")
    g.box("Keep the board powered", "Reload after an ESP32 reset. Some terminal programs "
          "reset the board when they connect; --play lets you test without reopening a "
          "terminal. The loader needs an 80-slot build to hold the full catalog.")

    g.start("B / VIC examples", "Speak from VIC BASIC", "Complete the wiring checks on pages 6-9 before connecting the computer.")
    g.para("Device <b>2</b> is the VIC's user-port serial channel. CHR$(10) selects "
           "<b>2400 baud, 8N1</b>, matching the ESP32 default. OPEN before assigning "
           "variables: the VIC clears them to allocate its serial buffers. Allow "
           "at least 512 bytes of free BASIC memory. [2]")
    g.h2("First words")
    g.code('10 OPEN1,2,3,CHR$(10)\n20 PRINT#1,"HELLO WORLD"\n30 FOR I=1 TO 1000:NEXT\n40 CLOSE1', leading=12)
    g.para("The delay gives the VIC's transmit buffer time to empty before CLOSE. "
           "For interactive testing, enter OPEN and PRINT as direct commands, wait to hear "
           "the phrase, then enter CLOSE1. Do not keep sending while speech is still playing.")
    g.h2("Recall a phrase learned from either port")
    g.code('10 OPEN1,2,3,CHR$(10)\n20 PRINT#1,"-PLAY 26"\n30 FOR I=1 TO 1000:NEXT\n40 CLOSE1', leading=12)
    g.para("This says the learned Wizard phrase in slot 26, provided the loader has run "
           "since the last ESP32 reset. It works even if PSEND is on.")
    g.h2("Teach slots directly from the VIC")
    g.code('OPEN1,2,3,CHR$(10)\nPRINT#1,"-LEARN 76 PHONEMES G EH T R EH D Y PA1"\nPRINT#1,"-PLAY 76"\nCLOSE1', leading=12)
    g.para("Enter individually: wait for OK 76 before PLAY, then wait for speech before "
           "CLOSE. Page 9 reads replies automatically. For immediate speech without "
           'learning, use <b>PRINT#1,"-PHONEMES G EH T R EH D Y PA1"</b>.')
    g.h2("Send compact SC-01 bytes directly")
    g.code('10 OPEN1,2,3,CHR$(10)\n20 PRINT#1,CHR$(222);CHR$(92);CHR$(123);\n30 PRINT#1,CHR$(106);CHR$(126);"?"\n40 FOR I=1 TO 1000:NEXT\n50 CLOSE1', leading=12)
    g.para("CHR$(222) opens the block; bytes 92, 123, 106 and 126 encode G, EH, T and "
           "PA1. The ? closes it. Line 20's final semicolon joins the PRINT statements. "
           "CHR$ avoids PETSCII keyboard ambiguities.", size=9.7)

    g.start("C / Wiring", "Identify the right contacts", "The VIC has a user-port edge connector; the GPIO numbers belong to the ESP32.")
    g.box("Power off before fitting or changing the cable", "Disconnect both power sources "
          "while wiring. This guide is for the <b>VIC-20 user port</b>, not the round IEC "
          "socket, cartridge connector or a PC RS-232 connector. Leave the VIC's supply "
          "pins disconnected; power the Audio-Kit separately by USB.", warning=True)
    g.diagram(174, connector)
    g.para("<b>This is the computer's port, viewed from outside at the rear.</b> "
           "The VIC sits normally with its keyboard uppermost. Contact 1 is upper left; "
           "A is lower left, and N is lower right. The letter sequence omits G and I. "
           "The matching cable views are on page 7. [1]", size=9.5)
    g.table(["VIC contact", "VIC-20 signal", "Connect to"], [
        ["M", "CB2 - transmitted data", "GPIO18 RX through the 10 kOhm / 18 kOhm divider"],
        ["B", "CB1 - receive interrupt", "GPIO5 TX return branch"],
        ["C", "PB0 - received data", "The same GPIO5 TX signal as B"],
        ["A or N", "Ground", "ESP32 GND and divider ground"],
    ], [70, 168, WIDTH - 238], size=9.6)
    g.h2("Parts for the minimal interface")
    g.para("A correctly keyed 24-contact (2 x 12) VIC user-port connector; short insulated "
           "wire; one 10 kOhm resistor and one 18 kOhm resistor (1%, 0.25 W are suitable); "
           "optionally two 470 Ohm series resistors for the return branches; insulation "
           "and strain relief; and a multimeter.")
    g.box("Read the board labels", "Use pads or header pins marked GPIO18/IO18, GPIO5/IO5 "
          "and GND. Header position varies by board revision. These are not physical pin "
          "numbers 18 and 5 on the module. Leave Audio-Kit keys 5 and 6 unused because "
          "they share GPIO18 and GPIO5.")

    g.start("C / Wiring", "Cable views: avoid mirroring", "These drawings show the same plug from opposite ends. Its installed TOP stays up.")
    g.box("Mark TOP before soldering", "Mark the side of the plug that will face the "
          "top of the upright VIC. When changing viewing ends, keep this mark uppermost: "
          "turn the plug around horizontally, not upside down. The numbered row stays above "
          "the lettered row in both views.")
    g.diagram(174, lambda c, w, h: connector(c, w, h, "mating"))
    g.para("<b>Mating face:</b> this is the opening that slides onto the VIC's edge "
           "contacts. With TOP up, contact 12 is upper left and N is lower left. "
           "This view is the left/right mirror of the computer's rear-port view.", size=9.5)
    g.diagram(174, lambda c, w, h: connector(c, w, h, "wire"))
    g.para("<b>Wire side:</b> you stand behind the cable, looking toward the VIC, as when "
           "the plug is installed. Contact 1 is upper left and A is lower left. This "
           "ordering matches View A on page 6; it is the mirror of View B above.", size=9.5)
    g.box("Verify contacts, not just the drawing", "The maps identify electrical contacts; "
          "a particular connector may stagger or rearrange its solder tails. Use moulded "
          "pin markings, its datasheet, and a continuity meter to map each wire terminal "
          "to the mating contact. Check M, B, C and A/N with both devices unpowered.", warning=True)

    g.start("C / Wiring", "Wire the serial interface", "Logical connection diagram: trace each named signal, rather than copying physical positions.")
    g.diagram(343, circuit)
    g.h2("Protect the ESP32 receive input")
    g.para("Put R1 (10 kOhm) <b>between VIC M and GPIO18</b>. Put R2 (18 kOhm) "
           "<b>from the GPIO18 junction to ground</b>. Do not swap them. With a 5 V input, "
           "the divider gives 5 x 18 / (10 + 18) = <b>3.21 V</b>. ESP32 GPIO is 3.3 V "
           "logic; do not feed the VIC's 5 V signal straight into it. [3]")
    g.h2("Return data needs both B and C")
    g.para("GPIO5 feeds the VIC's data input C and interrupt input B. The optional "
           "470 Ohm resistors limit contention current; they are not level shifters. "
           "This minimal circuit assumes the VIC's fitted VIA accepts a 3.3 V "
           "logic high. Verify this when a VIA has been replaced or return data is unreliable.")
    g.box("Check actual levels before the first run", "With the divider junction still "
          "disconnected from GPIO18, check the idle high after opening the VIC channel. "
          "Aim for about 3.0-3.3 V. At 3.3 V supply, the ESP32 high-input minimum is about "
          "2.48 V [3]. If it is too low, too high or negative, stop and use a correctly "
          "specified non-inverting logic-level interface; do not connect it as-is.", warning=True)
    g.para("Keep signal leads short and connect grounds. No MAX232 or RS-232 voltage "
           "adapter belongs in this direct logic-level circuit. The speaker stays on the "
           "Audio-Kit's speaker terminals; GPIO25 is the codec word clock, not an audio "
           "output for an external speaker.", size=9.7)

    g.start("C / Wiring", "Bring up the VIC connection", "Test one direction at a time, then check replies before using long transfers.")
    g.para("<b>1. With power off:</b> check pin letters, divider values, common ground, "
           "insulation, and no shorts to the VIC supply or reset contacts.<br/>"
           "<b>2. Power the Audio-Kit:</b> confirm startup tones, as in the board-only test. "
           "Power both devices for serial use; avoid leaving signal wires driven into an "
           "unpowered device.<br/>"
           "<b>3. Check VIC transmit:</b> after the voltage check on page 8, power off, "
           "finish the M-to-GPIO18 connection, then try HELLO WORLD from page 5.<br/>"
           "<b>4. Check the return:</b> with power off, add GPIO5 to both B and C. Run the "
           "program below. It disables echo, learns slot 76, waits for OK, recalls it, "
           "and waits for the second OK.")
    g.code('10 OPEN1,2,3,CHR$(10)\n20 PRINT#1,CHR$(27);CHR$(20)\n30 PRINT#1,"-LEARN 76 TEXT HELLO WORLD"\n40 GOSUB200\n50 IF LEFT$(R$,2)<>"OK" THEN90\n60 PRINT#1,"-PLAY 76"\n70 GOSUB200\n90 CLOSE1:END\n200 R$="":T=TI\n210 GET#1,A$\n220 IF A$="" THEN IF TI-T<1200 THEN210\n230 IF A$="" THEN R$="TIMEOUT":PRINT R$:RETURN\n240 IF A$=CHR$(10) OR A$=CHR$(27) THEN210\n250 IF A$=CHR$(13) THEN PRINT R$:RETURN\n260 R$=R$+A$:GOTO210', size=9.8, leading=13.3)
    g.para("Expect two <b>OK 76</b> replies with HELLO WORLD between them. The routine "
           "ignores LF and the initial echoed ESC, ends on CR, and times out after about "
           "20 seconds. GET# allows a timeout; INPUT# could wait indefinitely. [2]")
    g.h2("If a test fails")
    g.para("<b>Speech works but replies time out:</b> check both B and C, the return "
           "signal level, and that GPIO5 is not held low by key 6.<br/>"
           "<b>No VIC speech, but USB works:</b> recheck M, GPIO18, the divider, ground "
           "and 2400-baud settings. GPIO18 is shared with key 5.<br/>"
           "<b>Intermittent results:</b> shorten leads and check logic levels. A properly "
           "specified non-inverting 5 V/3.3 V serial level interface is preferable to "
           "guessing resistor changes.")
    g.box("Before releasing this guide", "Confirm text speech, the two-way learning test, "
          "compact phonemes, and 1200/2400 baud changes on the actual VIC. Record the "
          "Audio-Kit revision and any interface changes alongside the results.")

    g.start("D / Reference", "VIC controls and baud rates", "Escape controls are VIC-port bytes, not USB terminal commands.")
    g.table(["Bytes", "BASIC value after CHR$(27)", "Action"], [
        ["ESC, 0x11", "CHR$(17)", "PSEND on: return compact phonemes instead of speaking text"],
        ["ESC, 0x12", "CHR$(18)", "PSEND off: normal speech"],
        ["ESC, 0x13", "CHR$(19)", "Echo on (startup default)"],
        ["ESC, 0x14", "CHR$(20)", "Echo off; recommended when reading command replies"],
        ["ESC, 0x15", "CHR$(21)", "CAPS spelling on"],
        ["ESC, 0x16", "CHR$(22)", "CAPS spelling off (startup default)"],
        ["ESC, 0x17", "CHR$(23)", "Disable the four-second inactivity timer until reset"],
        ["ESC, 0x18", "CHR$(24)", "Restart the ESP32; clears learned slots"],
    ], [83, 139, WIDTH - 222], size=9.25, padding=4)
    g.para("For example, on an already open channel: <b>PRINT#1,CHR$(27);CHR$(20)</b>. "
           "Learning replies are returned even with echo off. PSEND and CAPS do not change "
           "how text is learned; explicit -play still speaks.", size=9.6)
    g.h2("Change from 2400 to 1200 baud")
    g.code('OPEN1,2,3,CHR$(10)\nPRINT#1,"SET TTY LO"\nCLOSE1\nOPEN1,2,3,CHR$(8)')
    g.para("Enter these as separate direct commands, allowing transmission to complete "
           "before CLOSE. Send the change at the <b>current</b> speed, then reopen at the "
           "new speed. CHR$(8) selects 1200; CHR$(10) selects 2400. To return, send "
           "SET TTY HI at 1200, close, then reopen with CHR$(10). Baud changes preserve "
           "the phrase bank; an ESP32 reset restores 2400 and clears it.")
    g.h2("Framing and flow")
    g.para("CR or LF ends a line. The VIC buffer holds 768 bytes including the terminator; "
           "ordinary unfinished input is processed after four idle seconds. Learning "
           "and all other dash commands need a terminator and are rejected if incomplete. There is "
           "no hardware flow control: wait for replies and pace long speech requests.")
    g.para("A NUL byte stops active speech without deleting learned phrases. A physical "
           "BREAK depends on the UART reporting it as NUL. Compact phoneme blocks start "
           "with pi (0xDE or 0xFF) or ASCII ~ and end with ?. Use CHR$ for exact byte values "
           "when typing from the VIC.", size=9.6)

    g.start("D / Reference", "Troubleshooting and sources", "Use the simplest working test to narrow down a problem.")
    g.table(["Symptom", "Check"], [
        ["No startup tones", "Board power, speaker connector or EARPHONE jack, and an ES8388 board matching the firmware. Read USB startup messages if available."],
        ["Tones work, speech does not", "Try -demo over USB. For ordinary VIC text, turn PSEND off. Use valid named phones or a previously learned slot."],
        ["ERROR n EMPTY SLOT", "Reload after reset. Wizard data is supplied by the host script; it is not built into the firmware."],
        ["Cannot open COM port", "Close other terminals, loaders or browser serial sessions. Use a USB data cable and the correct USB-to-UART port."],
        ["Learn failed / line too long", "Keep the whole USB line within 253 bytes. A slot allows 256 phones. Check the format word and wait for each reply."],
        ["VIC fails, USB works", "Use pages 6-9: ground, divider, M/B/C, RX/TX directions, voltage levels, and matching baud rates."],
        ["Scott Adams on Mega-Cart", "After loading the game from the Mega-Cart menu, hold F1 while pressing the Mega-Cart reset button. Then enter SYS32592 and press RETURN. This initializes the VIC's KERNAL serial routines; without it, speech output can be corrupted or unintelligible."],
        ["VIC help/list output is incomplete", "Read -help and -slots replies as they arrive. Long output can overrun the VIC receive buffer if BASIC cannot keep up; USB is useful for inspection."],
    ], [126, WIDTH - 126], size=9.2, padding=5)
    g.start("D / Technical", "Audio outputs and compatibility", "The GPIO signal is available on every supported build; the Audio-Kit codec is optional.")
    g.h2("Audio and compatibility notes")
    g.para("The original ESP32 build detects an ES8388 Audio-Kit and also drives GPIO22. C3, S2 and S3 builds drive GPIO4. "
           "The PWM signal is 8-bit at a 156.25 kHz carrier and needs the filter and external amplifier below. Never connect a speaker directly to GPIO. "
           "Use an ES8388 Audio-Kit with the documented layout, not an AC101 board or "
           "bare DevKit. Internal codec pins are SCL32, SDA33, MCLK0, BCLK27, LRCLK25, "
           "data26 and amplifier-enable21. No rewiring of those internal connections is "
           "needed. The voice is an SC-01 approximation, with inherited English spelling "
           "rules; it is not a ROM-exact Type 'n Talk or the original VIC-Voder voice.", size=8.8)
    g.h2("Sources and version")
    g.para("Firmware reference: this project's README, include/config.h, src/main.cpp, "
           "src/vic_serial.cpp, src/serial_commands.cpp and phrase-bank commands; Wizard labels from "
           "tools/data/wizard_of_wor.json. The diagrams are redrawn for this guide. "
           "The divider and GPIO mapping are the documented VVVC interface. "
           "Document revision: <b>2026-09-24, revision 3, review draft</b>.", size=9.1)
    g.h2("GPIO PWM filter")
    g.para("Use a high-impedance amplifier input (47 kOhm or greater). Start with low volume. The 1 kOhm / 22 nF and 4.7 kOhm / 4.7 nF stages attenuate the carrier; the 1 uF film capacitor removes the 50% idle DC level:")
    g.code("PWM GPIO -- 1k --+-- 4.7k --+-- 1uF film -- amplifier input\n"
            "                |         |\n                22nF      4.7nF\n"
            "                |         |\nGND -------------+---------+-------------- amplifier ground", size=9.2)
    g.para('[1] Commodore, <b>VIC-20 Personal Computing Guide</b>, printed p. 152, user-port contacts. '
           '<link href="https://www.vic-20.it/wp-content/uploads/2021/01/VIC-20_Personal_Computing_Guide.pdf" color="#087E86">Read the scanned manual</link>.<br/>'
           '[2] Commodore, <b>VIC-20 Programmer\'s Reference Guide</b>, pp. 176 and 251-256; pin functions, serial buffers, OPEN and GET#. '
           '<link href="https://www.vic-20.it/wp-content/uploads/2021/01/VIC20PrgRefGuide11.txt" color="#087E86">Read the transcribed manual</link>.<br/>'
           '[3] Espressif, <b>ESP32 Series Datasheet</b>, DC input characteristics. '
           '<link href="https://documentation.espressif.com/esp32_datasheet_en.html" color="#087E86">Read the manufacturer datasheet</link>.<br/>'
           'Board family: <link href="https://github.com/Ai-Thinker-Open/ESP32-A1S-AudioKit" color="#087E86">Ai-Thinker Audio-Kit documentation</link>. '
           'Code and third-party provenance: THIRD_PARTY.md in the project.', size=8.1, leading=10.2)
    g.box("Release gate", "The revised shared commands have passed host tests. Earlier "
          "audio and USB functions were tested on the board. Upload this revision and "
          "verify the VIC link, connector orientation and BASIC examples before release.", warning=True)

    catalog = json.loads((ROOT / "tools/data/wizard_of_wor.json").read_text(encoding="utf-8"))["phrases"]
    for start, end in ((1, 38), (39, 75)):
        g.start("E / Phrase index", f"Wizard of Wor: {start}-{end}",
                "Run the loader first, then recall with -play N. Original catalog wording is retained.")
        g.table(["Slot", "Phrase label"],
                [[str(p["id"]), escape(p["text"])] for p in catalog[start-1:end]],
                [39, WIDTH - 39], size=9.2, padding=2.4)
        if end == 75:
            g.para("<b>Slots 76-80 are spare.</b> Resetting or powering off the ESP32 clears all slots.", size=9.2)
    g.save()


if __name__ == "__main__":
    build()
