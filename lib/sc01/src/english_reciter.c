#include "english_reciter.h"

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Votrax SC-01 6-bit phoneme codes (Votrax datasheet, 1980). */
enum {
    SC01_EH3 = 0x00, SC01_EH2 = 0x01, SC01_EH1 = 0x02, SC01_PA0 = 0x03,
    SC01_DT = 0x04, SC01_A2 = 0x05, SC01_A1 = 0x06, SC01_ZH = 0x07,
    SC01_AH2 = 0x08, SC01_I3 = 0x09, SC01_I2 = 0x0A, SC01_I1 = 0x0B,
    SC01_M = 0x0C, SC01_N = 0x0D, SC01_B = 0x0E, SC01_V = 0x0F,
    SC01_CH = 0x10, SC01_SH = 0x11, SC01_Z = 0x12, SC01_AW1 = 0x13,
    SC01_NG = 0x14, SC01_AH1 = 0x15, SC01_OO1 = 0x16, SC01_OO = 0x17,
    SC01_L = 0x18, SC01_K = 0x19, SC01_J = 0x1A, SC01_H = 0x1B,
    SC01_G = 0x1C, SC01_F = 0x1D, SC01_D = 0x1E, SC01_S = 0x1F,
    SC01_A = 0x20, SC01_AY = 0x21, SC01_Y1 = 0x22, SC01_UH3 = 0x23,
    SC01_AH = 0x24, SC01_P = 0x25, SC01_O = 0x26, SC01_I = 0x27,
    SC01_U = 0x28, SC01_Y = 0x29, SC01_T = 0x2A, SC01_R = 0x2B,
    SC01_E = 0x2C, SC01_W = 0x2D, SC01_AE = 0x2E, SC01_AE1 = 0x2F,
    SC01_AW2 = 0x30, SC01_UH2 = 0x31, SC01_UH1 = 0x32, SC01_UH = 0x33,
    SC01_O2 = 0x34, SC01_O1 = 0x35, SC01_IU = 0x36, SC01_U1 = 0x37,
    SC01_THV = 0x38, SC01_TH = 0x39, SC01_ER = 0x3A, SC01_EH = 0x3B,
    SC01_E1 = 0x3C, SC01_AW = 0x3D, SC01_PA1 = 0x3E, SC01_STOP = 0x3F
};

typedef struct votrax_tts_options {
    int word_pause;       /* PA0 between ordinary words */
    int sentence_pause;   /* PA1 after . ! ? */
    int final_stop;       /* append STOP */
    int rhotic;           /* keep post-vocalic R; default 1 */
} votrax_tts_options_t;

/*
 * English -> SC-01 converter.
 *
 * Stage 1: a deliberately small exact-word override table for words where the
 *          manufacturer's SC-01 program materially improves intelligibility.
 * Stage 2: the public-domain Naval Research Laboratory (NRL) letter-to-sound
 *          rules (1976 / public-domain C implementation, 1985).
 * Stage 3: the NRL/Votrax IPA-to-SC-01 mapping, including the manufacturer's
 *          "liquid L" recommendations.
 *
 * This keeps the converter deterministic and malloc-free while avoiding the
 * crude spelling heuristics used in the earlier version.
 */

const votrax_tts_options_t VOTRAX_TTS_DEFAULTS = { 1, 1, 1, 1 };

static const char *const phoneme_names[64] = {
    "EH3","EH2","EH1","PA0","DT","A2","A1","ZH",
    "AH2","I3","I2","I1","M","N","B","V",
    "CH","SH","Z","AW1","NG","AH1","OO1","OO",
    "L","K","J","H","G","F","D","S",
    "A","AY","Y1","UH3","AH","P","O","I",
    "U","Y","T","R","E","W","AE","AE1",
    "AW2","UH2","UH1","UH","O2","O1","IU","U1",
    "THV","TH","ER","EH","E1","AW","PA1","STOP"
};

typedef struct emitter {
    uint8_t *data;
    size_t capacity;
    size_t required;
    int last;
} emitter_t;

static void emit_raw(emitter_t *e, uint8_t p)
{
    if (e->data && e->required < e->capacity)
        e->data[e->required] = p;
    e->required++;
    e->last = (int)p;
}

static void emit_phone(emitter_t *e, uint8_t p)
{
    emit_raw(e, p);
}

static void emit_pause(emitter_t *e, uint8_t p)
{
    if (e->required == 0)
        return;
    if (e->last == SC01_PA0 || e->last == SC01_PA1 || e->last == SC01_STOP)
        return;
    emit_raw(e, p);
}

static void emit_ch(emitter_t *e)
{
    /* SC-01 datasheet: T must precede CH. */
    emit_phone(e, SC01_T);
    emit_phone(e, SC01_CH);
}

static void emit_j(emitter_t *e)
{
    /* SC-01 datasheet: D must precede J. */
    emit_phone(e, SC01_D);
    emit_phone(e, SC01_J);
}

/* ------------------------------------------------------------------------- */
/* Small, verified SC-01 override layer                                      */
/* ------------------------------------------------------------------------- */

typedef struct exact_override {
    const char *word;
    const uint8_t *phones;
    unsigned char count;
} exact_override_t;

#define PHONESEQ(name, ...) static const uint8_t ov_##name[] = { __VA_ARGS__ }

/*
 * These are kept intentionally small.  They exist for SC-01-specific
 * pronunciations where generic letter-to-sound output is noticeably poorer.
 */
PHONESEQ(IS,    SC01_I1, SC01_I3, SC01_Z);
PHONESEQ(THE,   SC01_THV, SC01_UH1, SC01_UH3);
PHONESEQ(YOU,   SC01_Y1, SC01_IU, SC01_U1, SC01_U1);
PHONESEQ(YOUR,  SC01_Y, SC01_O2, SC01_O2, SC01_R);
PHONESEQ(YES,   SC01_Y1, SC01_EH3, SC01_EH1, SC01_S);
PHONESEQ(WHO,   SC01_H, SC01_IU, SC01_U1, SC01_U1);
PHONESEQ(HELLO, SC01_H, SC01_EH1, SC01_UH3, SC01_L, SC01_UH3, SC01_O1, SC01_U1);
PHONESEQ(DO,    SC01_D, SC01_IU, SC01_U1, SC01_U1);
PHONESEQ(DOES,  SC01_D, SC01_UH2, SC01_UH1, SC01_Z);
PHONESEQ(DONE,  SC01_D, SC01_UH1, SC01_UH3, SC01_N);
PHONESEQ(DOOR,  SC01_D, SC01_O1, SC01_O2, SC01_R);
PHONESEQ(EIGHT, SC01_A2, SC01_A2, SC01_Y, SC01_T);
PHONESEQ(TO,    SC01_T, SC01_IU, SC01_U1, SC01_U1);
PHONESEQ(TWO,   SC01_T, SC01_IU, SC01_U1, SC01_U1);

#define OV(name) { #name, ov_##name, (unsigned char)(sizeof(ov_##name) / sizeof(ov_##name[0])) }
static const exact_override_t exact_overrides[] = {
    OV(IS), OV(THE), OV(YOU), OV(YOUR), OV(YES), OV(WHO), OV(HELLO),
    OV(DO), OV(DOES), OV(DONE), OV(DOOR), OV(EIGHT), OV(TO), OV(TWO)
};
#undef OV
#undef PHONESEQ

static const exact_override_t *find_override(const char *word)
{
    size_t i;
    for (i = 0; i < sizeof(exact_overrides)/sizeof(exact_overrides[0]); ++i)
        if (strcmp(word, exact_overrides[i].word) == 0)
            return &exact_overrides[i];
    return NULL;
}

int votrax_sc01_has_override(const char *word)
{
    char up[96];
    size_t i = 0;
    if (!word) return 0;
    while (*word && i + 1 < sizeof(up)) {
        if (isalpha((unsigned char)*word) || *word == '\'')
            up[i++] = (char)toupper((unsigned char)*word);
        ++word;
    }
    up[i] = '\0';
    return find_override(up) != NULL;
}

size_t votrax_sc01_override_count(void)
{
    return sizeof(exact_overrides)/sizeof(exact_overrides[0]);
}

/* ------------------------------------------------------------------------- */
/* NRL letter-to-sound rules                                                 */
/* ------------------------------------------------------------------------- */

typedef struct nrl_rule {
    const char *left;
    const char *match;
    const char *right;
    const char *out;
} nrl_rule_t;

static const nrl_rule_t punct_rules[] = {
    {".",     "'S", "", "z"},
    {"#:.E",  "'S", "", "z"},
    {"#",     "'S", "", "z"},
    {"",      "'",  "", ""},
    {NULL,NULL,NULL,NULL}
};

static const nrl_rule_t rules_A[] = {
    {"","A"," ","AX"},
    {" ","ARE"," ","AAr"},
    {" ","AR","O","AXr"},
    {"","AR","#","EHr"},
    {"^","AS","#","EYs"},
    {"","A","WA","AX"},
    {"","AW","","AO"},
    {" :","ANY","","EHnIY"},
    {"","A","^+#","EY"},
    {"#:","ALLY","","AXlIY"},
    {" ","AL","#","AXl"},
    {"","AGAIN","","AXgEHn"},
    {"#:","AG","E","IHj"},
    {"","A","^+:#","AE"},
    {" :","A","^+ ","EY"},
    {"","A","^%","EY"},
    {" ","ARR","","AXr"},
    {"","ARR","","AEr"},
    {" :","AR"," ","AAr"},
    {"","AR"," ","ER"},
    {"","AR","","AAr"},
    {"","AIR","","EHr"},
    {"","AI","","EY"},
    {"","AY","","EY"},
    {"","AU","","AO"},
    {"#:","AL"," ","AXl"},
    {"#:","ALS"," ","AXlz"},
    {"","ALK","","AOk"},
    {"","AL","^","AOl"},
    {" :","ABLE","","EYbAXl"},
    {"","ABLE","","AXbAXl"},
    {"","ANG","+","EYnj"},
    {"","A","","AE"},
    {NULL,NULL,NULL,NULL}
};

static const nrl_rule_t rules_B[] = {
    {" ","BE","^#","bIH"},
    {"","BEING","","bIYIHNG"},
    {" ","BOTH"," ","bOWTH"},
    {" ","BUS","#","bIHz"},
    {"","BUIL","","bIHl"},
    {"","B","","b"},
    {NULL,NULL,NULL,NULL}
};

static const nrl_rule_t rules_C[] = {
    {" ","CH","^","k"},
    {"^E","CH","","k"},
    {"","CH","","CH"},
    {" S","CI","#","sAY"},
    {"","CI","A","SH"},
    {"","CI","O","SH"},
    {"","CI","EN","SH"},
    {"","C","+","s"},
    {"","CK","","k"},
    {"","COM","%","kAHm"},
    {"","C","","k"},
    {NULL,NULL,NULL,NULL}
};

static const nrl_rule_t rules_D[] = {
    {"#:","DED"," ","dIHd"},
    {".E","D"," ","d"},
    {"#^:E","D"," ","t"},
    {" ","DE","^#","dIH"},
    {" ","DO"," ","dUW"},
    {" ","DOES","","dAHz"},
    {" ","DOING","","dUWIHNG"},
    {" ","DOW","","dAW"},
    {"","DU","A","jUW"},
    {"","D","","d"},
    {NULL,NULL,NULL,NULL}
};

static const nrl_rule_t rules_E[] = {
    {"#:","E"," ",""},
    {"':^","E"," ",""},
    {" :","E"," ","IY"},
    {"#","ED"," ","d"},
    {"#:","E","D ",""},
    {"","EV","ER","EHv"},
    {"","E","^%","IY"},
    {"","ERI","#","IYrIY"},
    {"","ERI","","EHrIH"},
    {"#:","ER","#","ER"},
    {"","ER","#","EHr"},
    {"","ER","","ER"},
    {" ","EVEN","","IYvEHn"},
    {"#:","E","W",""},
    {"T","EW","","UW"},
    {"S","EW","","UW"},
    {"R","EW","","UW"},
    {"D","EW","","UW"},
    {"L","EW","","UW"},
    {"Z","EW","","UW"},
    {"N","EW","","UW"},
    {"J","EW","","UW"},
    {"TH","EW","","UW"},
    {"CH","EW","","UW"},
    {"SH","EW","","UW"},
    {"","EW","","yUW"},
    {"","E","O","IY"},
    {"#:S","ES"," ","IHz"},
    {"#:C","ES"," ","IHz"},
    {"#:G","ES"," ","IHz"},
    {"#:Z","ES"," ","IHz"},
    {"#:X","ES"," ","IHz"},
    {"#:J","ES"," ","IHz"},
    {"#:CH","ES"," ","IHz"},
    {"#:SH","ES"," ","IHz"},
    {"#:","E","S ",""},
    {"#:","ELY"," ","lIY"},
    {"#:","EMENT","","mEHnt"},
    {"","EFUL","","fUHl"},
    {"","EE","","IY"},
    {"","EARN","","ERn"},
    {" ","EAR","^","ER"},
    {"","EAD","","EHd"},
    {"#:","EA"," ","IYAX"},
    {"","EA","SU","EH"},
    {"","EA","","IY"},
    {"","EIGH","","EY"},
    {"","EI","","IY"},
    {" ","EYE","","AY"},
    {"","EY","","IY"},
    {"","EU","","yUW"},
    {"","E","","EH"},
    {NULL,NULL,NULL,NULL}
};

static const nrl_rule_t rules_F[] = {
    {"","FUL","","fUHl"},
    {"","F","","f"},
    {NULL,NULL,NULL,NULL}
};

static const nrl_rule_t rules_G[] = {
    {"","GIV","","gIHv"},
    {" ","G","I^","g"},
    {"","GE","T","gEH"},
    {"SU","GGES","","gjEHs"},
    {"","GG","","g"},
    {" B#","G","","g"},
    {"","G","+","j"},
    {"","GREAT","","grEYt"},
    {"#","GH","",""},
    {"","G","","g"},
    {NULL,NULL,NULL,NULL}
};

static const nrl_rule_t rules_H[] = {
    {" ","HAV","","hAEv"},
    {" ","HERE","","hIYr"},
    {" ","HOUR","","AWER"},
    {"","HOW","","hAW"},
    {"","H","#","h"},
    {"","H","",""},
    {NULL,NULL,NULL,NULL}
};

static const nrl_rule_t rules_I[] = {
    {" ","IN","","IHn"},
    {" ","I"," ","AY"},
    {"","IN","D","AYn"},
    {"","IER","","IYER"},
    {"#:R","IED","","IYd"},
    {"","IED"," ","AYd"},
    {"","IEN","","IYEHn"},
    {"","IE","T","AYEH"},
    {" :","I","%","AY"},
    {"","I","%","IY"},
    {"","IE","","IY"},
    {"","I","^+:#","IH"},
    {"","IR","#","AYr"},
    {"","IZ","%","AYz"},
    {"","IS","%","AYz"},
    {"","I","D%","AY"},
    {"+^","I","^+","IH"},
    {"","I","T%","AY"},
    {"#^:","I","^+","IH"},
    {"","I","^+","AY"},
    {"","IR","","ER"},
    {"","IGH","","AY"},
    {"","ILD","","AYld"},
    {"","IGN"," ","AYn"},
    {"","IGN","^","AYn"},
    {"","IGN","%","AYn"},
    {"","IQUE","","IYk"},
    {"","I","","IH"},
    {NULL,NULL,NULL,NULL}
};

static const nrl_rule_t rules_J[] = {
    {"","J","","j"},
    {NULL,NULL,NULL,NULL}
};

static const nrl_rule_t rules_K[] = {
    {" ","K","N",""},
    {"","K","","k"},
    {NULL,NULL,NULL,NULL}
};

static const nrl_rule_t rules_L[] = {
    {"","LO","C#","lOW"},
    {"L","L","",""},
    {"#^:","L","%","AXl"},
    {"","LEAD","","lIYd"},
    {"","L","","l"},
    {NULL,NULL,NULL,NULL}
};

static const nrl_rule_t rules_M[] = {
    {"","MOV","","mUWv"},
    {"","M","","m"},
    {NULL,NULL,NULL,NULL}
};

static const nrl_rule_t rules_N[] = {
    {"E","NG","+","nj"},
    {"","NG","R","NGg"},
    {"","NG","#","NGg"},
    {"","NGL","%","NGgAXl"},
    {"","NG","","NG"},
    {"","NK","","NGk"},
    {" ","NOW"," ","nAW"},
    {"","N","","n"},
    {NULL,NULL,NULL,NULL}
};

static const nrl_rule_t rules_O[] = {
    {"","OF"," ","AXv"},
    {"","OROUGH","","EROW"},
    {"#:","OR"," ","ER"},
    {"#:","ORS"," ","ERz"},
    {"","OR","","AOr"},
    {" ","ONE","","wAHn"},
    {"","OW","","OW"},
    {" ","OVER","","OWvER"},
    {"","OV","","AHv"},
    {"","O","^%","OW"},
    {"","O","^EN","OW"},
    {"","O","^I#","OW"},
    {"","OL","D","OWl"},
    {"","OUGHT","","AOt"},
    {"","OUGH","","AHf"},
    {" ","OU","","AW"},
    {"H","OU","S#","AW"},
    {"","OUS","","AXs"},
    {"","OUR","","AOr"},
    {"","OULD","","UHd"},
    {"^","OU","^L","AH"},
    {"","OUP","","UWp"},
    {"","OU","","AW"},
    {"","OY","","OY"},
    {"","OING","","OWIHNG"},
    {"","OI","","OY"},
    {"","OOR","","AOr"},
    {"","OOK","","UHk"},
    {"","OOD","","UHd"},
    {"","OO","","UW"},
    {"","O","E","OW"},
    {"","O"," ","OW"},
    {"","OA","","OW"},
    {" ","ONLY","","OWnlIY"},
    {" ","ONCE","","wAHns"},
    {"","ON'T","","OWnt"},
    {"C","O","N","AA"},
    {"","O","NG","AO"},
    {" ^:","O","N","AH"},
    {"I","ON","","AXn"},
    {"#:","ON"," ","AXn"},
    {"#^","ON","","AXn"},
    {"","O","ST ","OW"},
    {"","OF","^","AOf"},
    {"","OTHER","","AHDHER"},
    {"","OSS"," ","AOs"},
    {"#^:","OM","","AHm"},
    {"","O","","AA"},
    {NULL,NULL,NULL,NULL}
};

static const nrl_rule_t rules_P[] = {
    {"","PH","","f"},
    {"","PEOP","","pIYp"},
    {"","POW","","pAW"},
    {"","PUT"," ","pUHt"},
    {"","P","","p"},
    {NULL,NULL,NULL,NULL}
};

static const nrl_rule_t rules_Q[] = {
    {"","QUAR","","kwAOr"},
    {"","QU","","kw"},
    {"","Q","","k"},
    {NULL,NULL,NULL,NULL}
};

static const nrl_rule_t rules_R[] = {
    {" ","RE","^#","rIY"},
    {"","R","","r"},
    {NULL,NULL,NULL,NULL}
};

static const nrl_rule_t rules_S[] = {
    {"","SH","","SH"},
    {"#","SION","","ZHAXn"},
    {"","SOME","","sAHm"},
    {"#","SUR","#","ZHER"},
    {"","SUR","#","SHER"},
    {"#","SU","#","ZHUW"},
    {"#","SSU","#","SHUW"},
    {"#","SED"," ","zd"},
    {"#","S","#","z"},
    {"","SAID","","sEHd"},
    {"^","SION","","SHAXn"},
    {"","S","S",""},
    {".","S"," ","z"},
    {"#:.E","S"," ","z"},
    {"#^:##","S"," ","z"},
    {"#^:#","S"," ","s"},
    {"U","S"," ","s"},
    {" :#","S"," ","z"},
    {" ","SCH","","sk"},
    {"","S","C+",""},
    {"#","SM","","zm"},
    {"#","SN","'","zAXn"},
    {"","S","","s"},
    {NULL,NULL,NULL,NULL}
};

static const nrl_rule_t rules_T[] = {
    {" ","THE"," ","DHAX"},
    {"","TO"," ","tUW"},
    {"","THAT"," ","DHAEt"},
    {" ","THIS"," ","DHIHs"},
    {" ","THEY","","DHEY"},
    {" ","THERE","","DHEHr"},
    {"","THER","","DHER"},
    {"","THEIR","","DHEHr"},
    {" ","THAN"," ","DHAEn"},
    {" ","THEM"," ","DHEHm"},
    {"","THESE"," ","DHIYz"},
    {" ","THEN","","DHEHn"},
    {"","THROUGH","","THrUW"},
    {"","THOSE","","DHOWz"},
    {"","THOUGH"," ","DHOW"},
    {" ","THUS","","DHAHs"},
    {"","TH","","TH"},
    {"#:","TED"," ","tIHd"},
    {"S","TI","#N","CH"},
    {"","TI","O","SH"},
    {"","TI","A","SH"},
    {"","TIEN","","SHAXn"},
    {"","TUR","#","CHER"},
    {"","TU","A","CHUW"},
    {" ","TWO","","tUW"},
    {"","T","","t"},
    {NULL,NULL,NULL,NULL}
};

static const nrl_rule_t rules_U[] = {
    {" ","UN","I","yUWn"},
    {" ","UN","","AHn"},
    {" ","UPON","","AXpAOn"},
    {"T","UR","#","UHr"},
    {"S","UR","#","UHr"},
    {"R","UR","#","UHr"},
    {"D","UR","#","UHr"},
    {"L","UR","#","UHr"},
    {"Z","UR","#","UHr"},
    {"N","UR","#","UHr"},
    {"J","UR","#","UHr"},
    {"TH","UR","#","UHr"},
    {"CH","UR","#","UHr"},
    {"SH","UR","#","UHr"},
    {"","UR","#","yUHr"},
    {"","UR","","ER"},
    {"","U","^ ","AH"},
    {"","U","^^","AH"},
    {"","UY","","AY"},
    {" G","U","#",""},
    {"G","U","%",""},
    {"G","U","#","w"},
    {"#N","U","","yUW"},
    {"T","U","","UW"},
    {"S","U","","UW"},
    {"R","U","","UW"},
    {"D","U","","UW"},
    {"L","U","","UW"},
    {"Z","U","","UW"},
    {"N","U","","UW"},
    {"J","U","","UW"},
    {"TH","U","","UW"},
    {"CH","U","","UW"},
    {"SH","U","","UW"},
    {"","U","","yUW"},
    {NULL,NULL,NULL,NULL}
};

static const nrl_rule_t rules_V[] = {
    {"","VIEW","","vyUW"},
    {"","V","","v"},
    {NULL,NULL,NULL,NULL}
};

static const nrl_rule_t rules_W[] = {
    {" ","WERE","","wER"},
    {"","WA","S","wAA"},
    {"","WA","T","wAA"},
    {"","WHERE","","WHEHr"},
    {"","WHAT","","WHAAt"},
    {"","WHOL","","hOWl"},
    {"","WHO","","hUW"},
    {"","WH","","WH"},
    {"","WAR","","wAOr"},
    {"","WOR","^","wER"},
    {"","WR","","r"},
    {"","W","","w"},
    {NULL,NULL,NULL,NULL}
};

static const nrl_rule_t rules_X[] = {
    {"","X","","ks"},
    {NULL,NULL,NULL,NULL}
};

static const nrl_rule_t rules_Y[] = {
    {"","YOUNG","","yAHNG"},
    {" ","YOU","","yUW"},
    {" ","YES"," ","yEHs"},
    {" ","Y","","y"},
    {"#:^","Y"," ","IY"},
    {"#:^","Y","I","IY"},
    {" :","Y"," ","AY"},
    {" :","Y","#","AY"},
    {" :","Y","^+:#","IH"},
    {" :","Y","^#","AY"},
    {"","Y","","IH"},
    {NULL,NULL,NULL,NULL}
};

static const nrl_rule_t rules_Z[] = {
    {"","Z","","z"},
    {NULL,NULL,NULL,NULL}
};

static const nrl_rule_t *rules_for_char(char c)
{
    switch (c) {
    case 'A': return rules_A; case 'B': return rules_B;
    case 'C': return rules_C; case 'D': return rules_D;
    case 'E': return rules_E; case 'F': return rules_F;
    case 'G': return rules_G; case 'H': return rules_H;
    case 'I': return rules_I; case 'J': return rules_J;
    case 'K': return rules_K; case 'L': return rules_L;
    case 'M': return rules_M; case 'N': return rules_N;
    case 'O': return rules_O; case 'P': return rules_P;
    case 'Q': return rules_Q; case 'R': return rules_R;
    case 'S': return rules_S; case 'T': return rules_T;
    case 'U': return rules_U; case 'V': return rules_V;
    case 'W': return rules_W; case 'X': return rules_X;
    case 'Y': return rules_Y; case 'Z': return rules_Z;
    case '\'': return punct_rules;
    default: return NULL;
    }
}

static int nrl_vowel(char c)
{
    return c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U';
}

static int nrl_consonant(char c)
{
    return c >= 'A' && c <= 'Z' && !nrl_vowel(c);
}

static int nrl_voiced(char c)
{
    switch (c) {
    case 'B': case 'D': case 'V': case 'G': case 'J':
    case 'L': case 'M': case 'N': case 'R': case 'W': case 'Z':
        return 1;
    default:
        return 0;
    }
}

static int nrl_front_vowel(char c)
{
    return c == 'E' || c == 'I' || c == 'Y';
}

static int left_match(const char *pat, const char *word, ptrdiff_t pos)
{
    ptrdiff_t j = (ptrdiff_t)strlen(pat) - 1;

    while (j >= 0) {
        char pc = pat[j--];

        if (pc == '#') {
            if (pos < 0 || !nrl_vowel(word[pos])) return 0;
            do { --pos; } while (pos >= 0 && nrl_vowel(word[pos]));
        } else if (pc == ':') {
            while (pos >= 0 && nrl_consonant(word[pos])) --pos;
        } else if (pc == '^') {
            if (pos < 0 || !nrl_consonant(word[pos])) return 0;
            --pos;
        } else if (pc == '.') {
            if (pos < 0 || !nrl_voiced(word[pos])) return 0;
            --pos;
        } else if (pc == '+') {
            if (pos < 0 || !nrl_front_vowel(word[pos])) return 0;
            --pos;
        } else {
            if (pos < 0 || word[pos] != pc) return 0;
            --pos;
        }
    }
    return 1;
}

static int suffix_match(const char *s, size_t *used)
{
    static const char *const suffixes[] = {"ELY","ING","ER","ES","ED","E"};
    size_t i;
    for (i = 0; i < sizeof(suffixes)/sizeof(suffixes[0]); ++i) {
        size_t n = strlen(suffixes[i]);
        if (strncmp(s, suffixes[i], n) == 0) {
            *used = n;
            return 1;
        }
    }
    return 0;
}

static int right_match(const char *pat, const char *word, size_t pos)
{
    size_t j = 0;

    while (pat[j]) {
        char pc = pat[j++];

        if (pc == '#') {
            if (!nrl_vowel(word[pos])) return 0;
            do { ++pos; } while (nrl_vowel(word[pos]));
        } else if (pc == ':') {
            while (nrl_consonant(word[pos])) ++pos;
        } else if (pc == '^') {
            if (!nrl_consonant(word[pos])) return 0;
            ++pos;
        } else if (pc == '.') {
            if (!nrl_voiced(word[pos])) return 0;
            ++pos;
        } else if (pc == '+') {
            if (!nrl_front_vowel(word[pos])) return 0;
            ++pos;
        } else if (pc == '%') {
            size_t used = 0;
            if (!suffix_match(word + pos, &used)) return 0;
            pos += used;
        } else {
            if (word[pos] != pc) return 0;
            ++pos;
        }
    }
    return 1;
}

/* Intermediate NRL phoneme alphabet. */
typedef enum nrl_phone {
    NP_IY, NP_IH, NP_EY, NP_EH, NP_AE, NP_AA, NP_AO, NP_OW,
    NP_UH, NP_UW, NP_ER, NP_AX, NP_AH, NP_AY, NP_AW, NP_OY,
    NP_P, NP_B, NP_T, NP_D, NP_K, NP_G, NP_F, NP_V,
    NP_TH, NP_DH, NP_S, NP_Z, NP_SH, NP_ZH, NP_HH,
    NP_M, NP_N, NP_NX, NP_L, NP_W, NP_Y, NP_R, NP_CH, NP_JH, NP_WH
} nrl_phone_t;

typedef struct nrl_buffer {
    nrl_phone_t phones[256];
    size_t count;
} nrl_buffer_t;

typedef struct nrl_symbol {
    const char *text;
    nrl_phone_t phone;
} nrl_symbol_t;

/* Longest strings first. */
static const nrl_symbol_t nrl_symbols[] = {
    {"IY",NP_IY},{"IH",NP_IH},{"EY",NP_EY},{"EH",NP_EH},
    {"AE",NP_AE},{"AA",NP_AA},{"AO",NP_AO},{"OW",NP_OW},
    {"UH",NP_UH},{"UW",NP_UW},{"ER",NP_ER},{"AX",NP_AX},
    {"AH",NP_AH},{"AY",NP_AY},{"AW",NP_AW},{"OY",NP_OY},
    {"TH",NP_TH},{"DH",NP_DH},{"SH",NP_SH},{"ZH",NP_ZH},
    {"HH",NP_HH},{"CH",NP_CH},{"WH",NP_WH},{"NG",NP_NX},
    {"p",NP_P},{"b",NP_B},{"t",NP_T},{"d",NP_D},{"k",NP_K},
    {"g",NP_G},{"f",NP_F},{"v",NP_V},{"s",NP_S},{"z",NP_Z},
    {"h",NP_HH},{"m",NP_M},{"n",NP_N},{"l",NP_L},{"w",NP_W},
    {"y",NP_Y},{"r",NP_R},{"j",NP_JH}
};

static void append_nrl_output(nrl_buffer_t *b, const char *s)
{
    while (*s) {
        size_t i;
        int found = 0;

        if (isspace((unsigned char)*s)) {
            ++s;
            continue;
        }

        for (i = 0; i < sizeof(nrl_symbols)/sizeof(nrl_symbols[0]); ++i) {
            size_t n = strlen(nrl_symbols[i].text);
            if (strncmp(s, nrl_symbols[i].text, n) == 0) {
                if (b->count < sizeof(b->phones)/sizeof(b->phones[0]))
                    b->phones[b->count++] = nrl_symbols[i].phone;
                s += n;
                found = 1;
                break;
            }
        }

        if (!found) ++s; /* robust against an unknown symbol */
    }
}

static void nrl_word(const char *word, nrl_buffer_t *out)
{
    char padded[100];
    size_t n = strlen(word);
    size_t i = 1;

    out->count = 0;
    if (n > sizeof(padded) - 3) n = sizeof(padded) - 3;

    padded[0] = ' ';
    memcpy(padded + 1, word, n);
    padded[n + 1] = ' ';
    padded[n + 2] = '\0';

    while (i <= n) {
        const nrl_rule_t *table = rules_for_char(padded[i]);
        const nrl_rule_t *r;
        int matched = 0;

        if (!table) {
            ++i;
            continue;
        }

        for (r = table; r->match; ++r) {
            size_t m = strlen(r->match);
            if (m == 0) continue;
            if (strncmp(padded + i, r->match, m) != 0) continue;
            if (!left_match(r->left, padded, (ptrdiff_t)i - 1)) continue;
            if (!right_match(r->right, padded, i + m)) continue;

            append_nrl_output(out, r->out);
            i += m;
            matched = 1;
            break;
        }

        if (!matched) ++i;
    }
}

static int np_is_vowel(nrl_phone_t p)
{
    return p <= NP_OY;
}

/* NRL IPA -> Votrax SC-01 rules. */
static void nrl_to_sc01(const nrl_buffer_t *b, emitter_t *e,
                        const votrax_tts_options_t *opt)
{
    size_t i;

    for (i = 0; i < b->count; ++i) {
        nrl_phone_t p = b->phones[i];
        int have_prev = i != 0;
        int have_next = i + 1 < b->count;
        nrl_phone_t prev = have_prev ? b->phones[i - 1] : NP_P;
        nrl_phone_t next = have_next ? b->phones[i + 1] : NP_P;

        switch (p) {
        case NP_IY:
            emit_phone(e, SC01_E1); emit_phone(e, SC01_Y);
            break;
        case NP_IH:
            emit_phone(e, SC01_I1); emit_phone(e, SC01_I3);
            break;

        case NP_EY:
            if (have_prev && prev == NP_L && have_next && next == NP_R) {
                emit_phone(e, SC01_UH3); emit_phone(e, SC01_A1); emit_phone(e, SC01_I3);
            } else if (have_prev && prev == NP_L) {
                emit_phone(e, SC01_UH3); emit_phone(e, SC01_A1); emit_phone(e, SC01_AY);
            } else if (have_next && next == NP_R) {
                emit_phone(e, SC01_A1); emit_phone(e, SC01_I3);
            } else {
                emit_phone(e, SC01_A1); emit_phone(e, SC01_AY); emit_phone(e, SC01_Y);
            }
            break;

        case NP_EH:
            if (have_prev && prev == NP_L) emit_phone(e, SC01_UH3);
            emit_phone(e, SC01_EH1); emit_phone(e, SC01_EH3);
            break;

        case NP_AE:
            if (have_prev && prev == NP_L && have_next && next == NP_R) {
                emit_phone(e, SC01_UH3); emit_phone(e, SC01_AE1); emit_phone(e, SC01_EH3);
            } else if (have_prev && prev == NP_L) {
                emit_phone(e, SC01_UH3); emit_phone(e, SC01_AE1); emit_phone(e, SC01_EH3);
            } else if (have_next && next == NP_R) {
                emit_phone(e, SC01_AE1); emit_phone(e, SC01_EH3);
            } else {
                emit_phone(e, SC01_AE1); emit_phone(e, SC01_EH3);
            }
            break;

        case NP_AA: emit_phone(e, SC01_AH1); break;

        case NP_AO:
            if (have_prev && prev == NP_L && have_next && next == NP_R) {
                emit_phone(e, SC01_UH3); emit_phone(e, SC01_O);
            } else if (have_prev && prev == NP_L && have_next && next == NP_ER) {
                emit_phone(e, SC01_UH3); emit_phone(e, SC01_AW); emit_phone(e, SC01_O2);
            } else if (have_prev && prev == NP_L) {
                emit_phone(e, SC01_UH3); emit_phone(e, SC01_AW);
            } else if (have_next && next == NP_R) {
                emit_phone(e, SC01_O);
            } else if (have_next && next == NP_ER) {
                emit_phone(e, SC01_AW); emit_phone(e, SC01_O2);
            } else {
                emit_phone(e, SC01_AW);
            }
            break;

        case NP_OW:
            if (have_prev && prev == NP_L) emit_phone(e, SC01_UH3);
            emit_phone(e, SC01_O1); emit_phone(e, SC01_U1);
            break;

        case NP_UH:
            if (have_prev && prev == NP_L) emit_phone(e, SC01_UH3);
            emit_phone(e, SC01_OO1); emit_phone(e, SC01_OO);
            break;

        case NP_UW:
            emit_phone(e, SC01_IU); emit_phone(e, SC01_U1); emit_phone(e, SC01_U1);
            break;

        case NP_ER:
            if (have_prev && prev == NP_IY) {
                emit_phone(e, SC01_I3); emit_phone(e, SC01_ER);
            } else if (have_prev && prev == NP_ER) {
                emit_phone(e, SC01_IU); emit_phone(e, SC01_R);
            } else if (have_prev && prev == NP_L) {
                emit_phone(e, SC01_UH3); emit_phone(e, SC01_ER);
            } else if (have_next && next == NP_L) {
                emit_phone(e, SC01_UH3); emit_phone(e, SC01_ER);
            } else if (have_prev && prev == NP_R) {
                emit_phone(e, SC01_UH3); emit_phone(e, SC01_R);
            } else {
                emit_phone(e, SC01_ER);
            }
            break;

        case NP_AX: emit_phone(e, SC01_UH2); break;
        case NP_AH: emit_phone(e, SC01_UH1); emit_phone(e, SC01_UH2); break;

        case NP_AY:
            if (have_next && next == NP_R) {
                emit_phone(e, SC01_AH1); emit_phone(e, SC01_I3);
            } else {
                emit_phone(e, SC01_AH1); emit_phone(e, SC01_EH3); emit_phone(e, SC01_Y);
            }
            break;

        case NP_AW:
            emit_phone(e, SC01_AH1); emit_phone(e, SC01_UH3); emit_phone(e, SC01_U1);
            break;

        case NP_OY:
            if (have_prev && prev == NP_L && have_next && next == NP_ER) {
                emit_phone(e, SC01_UH3); emit_phone(e, SC01_O1); emit_phone(e, SC01_AY);
            } else if (have_prev && prev == NP_L && have_next && next == NP_L) {
                emit_phone(e, SC01_UH3); emit_phone(e, SC01_O1); emit_phone(e, SC01_AY);
            } else if (have_prev && prev == NP_L && have_next && next == NP_R) {
                emit_phone(e, SC01_UH3); emit_phone(e, SC01_O1); emit_phone(e, SC01_EH2);
            } else if (have_next && next == NP_ER) {
                emit_phone(e, SC01_O1); emit_phone(e, SC01_AY);
            } else if (have_next && next == NP_L) {
                emit_phone(e, SC01_O1); emit_phone(e, SC01_AY);
            } else if (have_next && next == NP_R) {
                emit_phone(e, SC01_O1); emit_phone(e, SC01_EH2);
            } else {
                emit_phone(e, SC01_O1); emit_phone(e, SC01_E1);
            }
            break;

        case NP_P: emit_phone(e, SC01_P); break;
        case NP_B: emit_phone(e, SC01_B); break;
        case NP_T: emit_phone(e, SC01_T); break;
        case NP_D: emit_phone(e, SC01_D); break;
        case NP_K: emit_phone(e, SC01_K); break;
        case NP_G: emit_phone(e, SC01_G); break;
        case NP_F: emit_phone(e, SC01_F); break;
        case NP_V: emit_phone(e, SC01_V); break;
        case NP_TH: emit_phone(e, SC01_TH); break;
        case NP_DH: emit_phone(e, SC01_THV); break;
        case NP_S: emit_phone(e, SC01_S); break;
        case NP_Z: emit_phone(e, SC01_Z); break;
        case NP_SH: emit_phone(e, SC01_SH); break;
        case NP_ZH: emit_phone(e, SC01_ZH); break;
        case NP_HH: emit_phone(e, SC01_H); break;
        case NP_CH: emit_ch(e); break;
        case NP_JH: emit_j(e); break;
        case NP_M: emit_phone(e, SC01_M); break;
        case NP_N: emit_phone(e, SC01_N); break;
        case NP_NX: emit_phone(e, SC01_NG); break;

        case NP_L:
            if (have_prev && (prev == NP_IY || prev == NP_EY ||
                              prev == NP_AY || prev == NP_OY)) {
                emit_phone(e, SC01_I3); emit_phone(e, SC01_L);
            } else if (have_prev && (prev == NP_AE || prev == NP_AO ||
                                     prev == NP_OW)) {
                emit_phone(e, SC01_UH3); emit_phone(e, SC01_L);
            } else {
                emit_phone(e, SC01_L);
            }
            break;

        case NP_W: emit_phone(e, SC01_W); break;
        case NP_WH: emit_phone(e, SC01_H); emit_phone(e, SC01_W); break;
        case NP_Y: emit_phone(e, SC01_Y1); break;

        case NP_R:
            if (!opt->rhotic && have_prev && np_is_vowel(prev) &&
                (!have_next || !np_is_vowel(next))) {
                break;
            }
            if (have_next && next == NP_L) emit_phone(e, SC01_UH3);
            emit_phone(e, SC01_R);
            break;
        }
    }
}

static void convert_word(const char *word, emitter_t *e,
                         const votrax_tts_options_t *opt)
{
    const exact_override_t *ov = find_override(word);
    size_t i;

    if (ov) {
        for (i = 0; i < ov->count; ++i) emit_phone(e, ov->phones[i]);
        return;
    }

    {
        nrl_buffer_t buf;
        nrl_word(word, &buf);
        nrl_to_sc01(&buf, e, opt);
    }
}

/* ------------------------------------------------------------------------- */
/* Number handling                                                           */
/* ------------------------------------------------------------------------- */

static const char *const small_numbers[] = {
    "ZERO","ONE","TWO","THREE","FOUR","FIVE","SIX","SEVEN","EIGHT","NINE",
    "TEN","ELEVEN","TWELVE","THIRTEEN","FOURTEEN","FIFTEEN","SIXTEEN",
    "SEVENTEEN","EIGHTEEN","NINETEEN"
};

static const char *const tens_numbers[] = {
    "", "", "TWENTY", "THIRTY", "FORTY", "FIFTY",
    "SIXTY", "SEVENTY", "EIGHTY", "NINETY"
};

static void number_word(const char *w, emitter_t *e,
                        const votrax_tts_options_t *opt, int *need_pause)
{
    if (*need_pause && opt->word_pause) emit_pause(e, SC01_PA0);
    convert_word(w, e, opt);
    *need_pause = 1;
}

static void speak_upto_999(unsigned v, emitter_t *e,
                           const votrax_tts_options_t *opt, int *need_pause)
{
    if (v >= 100) {
        number_word(small_numbers[v / 100], e, opt, need_pause);
        number_word("HUNDRED", e, opt, need_pause);
        v %= 100;
        if (v) number_word("AND", e, opt, need_pause);
    }
    if (v >= 20) {
        number_word(tens_numbers[v / 10], e, opt, need_pause);
        v %= 10;
        if (v) number_word(small_numbers[v], e, opt, need_pause);
    } else if (v) {
        number_word(small_numbers[v], e, opt, need_pause);
    }
}

static void speak_integer(unsigned long v, emitter_t *e,
                          const votrax_tts_options_t *opt, int *need_pause)
{
    if (v == 0) {
        number_word("ZERO", e, opt, need_pause);
        return;
    }
    if (v >= 1000000000UL) {
        speak_upto_999((unsigned)(v / 1000000000UL), e, opt, need_pause);
        number_word("BILLION", e, opt, need_pause);
        v %= 1000000000UL;
    }
    if (v >= 1000000UL) {
        speak_upto_999((unsigned)(v / 1000000UL), e, opt, need_pause);
        number_word("MILLION", e, opt, need_pause);
        v %= 1000000UL;
    }
    if (v >= 1000UL) {
        speak_upto_999((unsigned)(v / 1000UL), e, opt, need_pause);
        number_word("THOUSAND", e, opt, need_pause);
        v %= 1000UL;
    }
    if (v) speak_upto_999((unsigned)v, e, opt, need_pause);
}

static void speak_number_token(const char *tok, emitter_t *e,
                               const votrax_tts_options_t *opt, int *need_pause)
{
    const char *dot = strchr(tok, '.');
    const char *p;
    unsigned long whole = 0;

    for (p = tok; *p && *p != '.'; ++p)
        if (isdigit((unsigned char)*p))
            whole = whole * 10UL + (unsigned)(*p - '0');

    speak_integer(whole, e, opt, need_pause);

    if (dot) {
        number_word("POINT", e, opt, need_pause);
        for (p = dot + 1; *p; ++p)
            if (isdigit((unsigned char)*p))
                number_word(small_numbers[*p - '0'], e, opt, need_pause);
    }
}

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */

size_t votrax_text_to_sc01(const char *text,
                           uint8_t *out,
                           size_t out_capacity,
                           const votrax_tts_options_t *options)
{
    votrax_tts_options_t local;
    emitter_t e;
    char token[96];
    size_t tp = 0;
    int token_kind = 0; /* 0 none, 1 word, 2 number */
    int have_spoken = 0;
    int pending_word_pause = 0;
    const unsigned char *p;

    if (!text) return 0;
    local = options ? *options : VOTRAX_TTS_DEFAULTS;

    e.data = out;
    e.capacity = out_capacity;
    e.required = 0;
    e.last = -1;

#define FLUSH_TOKEN() do {                                                       \
        if (tp) {                                                               \
            token[tp] = '\0';                                                   \
            if (pending_word_pause && have_spoken && local.word_pause)          \
                emit_pause(&e, SC01_PA0);                                       \
            if (token_kind == 2) {                                              \
                int np = 0;                                                     \
                speak_number_token(token, &e, &local, &np);                     \
            } else {                                                            \
                convert_word(token, &e, &local);                                \
            }                                                                   \
            have_spoken = 1;                                                    \
            pending_word_pause = 1;                                             \
            tp = 0;                                                             \
            token_kind = 0;                                                     \
        }                                                                       \
    } while (0)

    for (p = (const unsigned char *)text; ; ++p) {
        unsigned char ch = *p;

        if (isalpha(ch)) {
            if (token_kind == 2) FLUSH_TOKEN();
            token_kind = 1;
            if (tp + 1 < sizeof(token)) token[tp++] = (char)toupper(ch);
            continue;
        }

        if (isdigit(ch)) {
            if (token_kind == 1) FLUSH_TOKEN();
            token_kind = 2;
            if (tp + 1 < sizeof(token)) token[tp++] = (char)ch;
            continue;
        }

        if (ch == '\'' && token_kind == 1) {
            if (tp + 1 < sizeof(token)) token[tp++] = '\'';
            continue;
        }

        if (ch == '.' && token_kind == 2 && isdigit(p[1]) &&
            !memchr(token, '.', tp)) {
            if (tp + 1 < sizeof(token)) token[tp++] = '.';
            continue;
        }

        FLUSH_TOKEN();
        if (ch == '\0') break;

        switch (ch) {
        case '.': case '!': case '?':
            if (local.sentence_pause) emit_pause(&e, SC01_PA1);
            pending_word_pause = 0;
            break;
        case ',': case ';': case ':':
        case '-': case '/':
            emit_pause(&e, SC01_PA0);
            pending_word_pause = 0;
            break;
        default:
            break;
        }
    }

    if (local.final_stop) emit_phone(&e, SC01_STOP);

#undef FLUSH_TOKEN
    return e.required;
}

const char *votrax_sc01_name(uint8_t code)
{
    return code < 64 ? phoneme_names[code] : "?";
}

size_t votrax_sc01_format(const uint8_t *codes, size_t count,
                          char *dst, size_t dst_capacity)
{
    size_t required = 0;
    size_t i;

    if (dst && dst_capacity) dst[0] = '\0';

    for (i = 0; i < count; ++i) {
        const char *s = votrax_sc01_name(codes[i]);
        size_t n = strlen(s);
        size_t j;

        if (i) {
            if (dst && required + 1 < dst_capacity) dst[required] = ' ';
            ++required;
        }
        for (j = 0; j < n; ++j) {
            if (dst && required + 1 < dst_capacity) dst[required] = s[j];
            ++required;
        }
    }

    if (dst && dst_capacity) {
        size_t z = required < dst_capacity ? required : dst_capacity - 1;
        dst[z] = '\0';
    }
    return required;
}

/* ------------------------------------------------------------------------- */
/* Firmware entry point                                                      */
/* ------------------------------------------------------------------------- */

int reciteEnglish(const char *text, char *output, size_t outputSize)
{
    static uint8_t codes[8192];
    size_t count;
    size_t formatted;

    if (!text || !output || !outputSize)
        return 0;
    if (strlen(text) > 253)
        return 0;

    count = votrax_text_to_sc01(text, codes, sizeof(codes),
                                &VOTRAX_TTS_DEFAULTS);
    if (count == 0)
        return 0;
    if (codes[count - 1] == SC01_STOP)
        --count; /* The firmware appends its own STOP. */

    /* Size first so the output buffer is left untouched on failure. */
    formatted = votrax_sc01_format(codes, count, NULL, 0);
    if (formatted >= outputSize)
        return 0;
    votrax_sc01_format(codes, count, output, outputSize);
    return 1;
}
