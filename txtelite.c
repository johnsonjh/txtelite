/* txtelite.c  1.5 */

/* Textual version of Elite trading (C implementation) */

/*
 * Converted by Ian Bell from 6502 Elite sources.
 * Original 6502 Elite by Ian Bell & David Braben.
 */

/*
 * Edited by Richard Carlsson to compile cleanly under gcc
 * and to fix a bug in the goat soup algorithm.
 */

/*
 * Edited by Jeffrey H. Johnson to resolve all warnings
 * from Clang, Clang Analyzer, and PVS-Studio.
 */

/*
 * -----------------------------------------------------------------------
 *  The nature of basic mechanisms used to generate the Elite socio-
 *  economic universe are now widely known. A competant games programmer
 *  should be able to produce equivalent functionality. A competant
 *  hacker should be able to lift the exact system from the object code
 *  base of official conversions.
 *
 *  This file may be regarded as defining the Classic Elite universe.
 *
 *  It contains a C implementation of the precise 6502 algorithms used
 *  in the original BBC Micro version of Acornsoft Elite together with a
 *  parsed textual command testbed.
 *
 *  Note that this is not the universe of David Braben's 'Frontier'
 *  series.
 *
 *  ICGB 13/10/99
 *  iancgbell@email.com
 *  www.ibell.co.uk
 * -----------------------------------------------------------------------
 */

/*
 * Note that this program is "quick-hack" text parser-driven
 * version of Elite with no combat or missions.
 */

#ifndef _DEFAULT_SOURCE
# define _DEFAULT_SOURCE
#endif /* ifndef _DEFAULT_SOURCE */

#ifndef _BSD_SOURCE
# define _BSD_SOURCE
#endif /* ifndef _BSD_SOURCE */

#ifdef _AIX
# ifndef _ALL_SOURCE
#  define _ALL_SOURCE
# endif /* ifndef _ALL_SOURCE */
#endif /* ifdef _AIX */

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif /* ifndef _GNU_SOURCE */

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define true  (-1)
#define false  (0)
#define tonnes (0)

#define maxlen (20) /* Length of strings */

typedef int boolean;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef signed short int16;

#ifndef _AIX
typedef signed long int32;
#endif /* ifndef _AIX */

typedef uint16 myuint;

typedef int planetnum;

typedef struct {
    uint8 a, b, c, d;
} fastseedtype; /* Four byte random number used for planet description */

typedef struct {
    uint16 w0;
    uint16 w1;
    uint16 w2;
} seedtype; /* Six byte random number used as seed for planets */

typedef struct {
    myuint x;
    myuint y;            /* One byte unsigned */
    myuint economy;      /* These two are actually only 0-7 */
    myuint govtype;
    myuint techlev;      /* 0-16 i think */
    myuint population;   /* One byte */
    myuint productivity; /* Two byte */
    myuint radius;       /* Two byte (not used by game at all) */
    myuint human_colony;
    myuint species_type;
    myuint species_adj1;
    myuint species_adj2;
    myuint species_adj3;
    fastseedtype goatsoupseed;
    char name[12];
} plansys;

static void goat_soup(const char *source, plansys *psy);

#define galsize    (256)
#define AlienItems  (16)
#define numforLave    7 /* Lave is 7th generated planet in galaxy one */
#define lasttrade AlienItems

static plansys galaxy[galsize]; /* Need 0 to galsize-1 inclusive */

static seedtype seed;

static fastseedtype rnd_seed;

static boolean nativerand;

typedef struct {       /* In 6502 version these were: */
    myuint baseprice;  /* one byte */
    int16 gradient;    /* Five bits plus sign */
    myuint basequant;  /* One byte */
    myuint maskbyte;   /* One byte */
    myuint units;      /* Two bits */
    char name[maxlen]; /* Longest="Radioactives" */
} tradegood;

typedef struct {
    myuint quantity[lasttrade + 1];
    myuint price[lasttrade + 1];
} markettype;

/* Player workspace */
static myuint shipshold[lasttrade + 1]; /* Contents of cargo bay */
static planetnum currentplanet;         /* Current planet */
static myuint galaxynum;                /* Galaxy number (1-8) */
static int32 cash;
static myuint fuel;
static markettype localmarket;
static myuint holdspace;

static int fuelcost = 2;  /* 0.2 CR/Light year */
static int maxfuel  = 70; /* 7.0 LY tank */

static const uint16 base0 = 0x5A4A;
static const uint16 base1 = 0x0248;
static const uint16 base2 = 0xB753; /* Base seed for galaxy 1 */

static char pairs[] = "ABOUSEITILETSTONLONUTHNO"
                      "..LEXEGEZACEBISO"
                      "USESARMAINDIREA."
                      "ERATENBERALAVETI"
                      "EDORQUANTEISRION"; /* Dots should be nullprint characters */

static char govnames[][maxlen] = {"Anarchy",   "Feudal",      "Multi-gov", "Dictatorship",
                                  "Communist", "Confederacy", "Democracy", "Corporate State"};

static char econnames[][maxlen] = {"Rich Ind",    "Average Ind", "Poor Ind",     "Mainly Ind",
                                   "Mainly Agri", "Rich Agri",   "Average Agri", "Poor Agri"};

static char species_stature[3][maxlen]         = {"Large", "Fierce", "Small"};
static char species_coloration[6][maxlen]      = {"Green", "Red", "Yellow", "Blue", "Black", "Harmless"};
static char species_characteristics[6][maxlen] = {"Slimy", "Bug-Eyed", "Horned", "Bony", "Fat", "Furry"};
static char species_base_type[8][maxlen]       = {"Rodents", "Frogs",     "Lizards", "Lobsters",
                                                  "Birds",   "Humanoids", "Felines", "Insects"};

static char unitnames[][5] = {"t", "kg", "g"};

/* Data for DB's price/availability generation system */

/*
 *  Base   Grad  Base  Mask  Un  Name
 *  price  ient  quant       it
 */

/* Set to 1 for NES-sanitised trade goods */
#define POLITICALLY_CORRECT 0

static tradegood commodities[] = {
    {0x13, -0x02, 0x06, 0x01, 0, "Food        "},
    {0x14, -0x01, 0x0A, 0x03, 0, "Textiles    "},
    {0x41, -0x03, 0x02, 0x07, 0, "Radioactives"},
#if POLITICALLY_CORRECT
    {0x28, -0x05, 0xE2, 0x1F, 0, "Robot Slaves"},
    {0x53, -0x05, 0xFB, 0x0F, 0, "Beverages   "},
#else  /* if POLITICALLY_CORRECT */
    {0x28, -0x05, 0xE2, 0x1F, 0, "Slaves      "},
    {0x53, -0x05, 0xFB, 0x0F, 0, "Liquor/Wines"},
#endif /* if POLITICALLY_CORRECT */
    {0xC4, +0x08, 0x36, 0x03, 0, "Luxuries    "},
#if POLITICALLY_CORRECT
    {0xEB, +0x1D, 0x08, 0x78, 0, "Rare Species"},
#else  /* if POLITICALLY_CORRECT */
    {0xEB, +0x1D, 0x08, 0x78, 0, "Narcotics   "},
#endif /* if POLITICALLY_CORRECT */
    {0x9A, +0x0E, 0x38, 0x03, 0, "Computers   "},
    {0x75, +0x06, 0x28, 0x07, 0, "Machinery   "},
    {0x4E, +0x01, 0x11, 0x1F, 0, "Alloys      "},
    {0x7C, +0x0d, 0x1D, 0x07, 0, "Firearms    "},
    {0xB0, -0x09, 0xDC, 0x3F, 0, "Furs        "},
    {0x20, -0x01, 0x35, 0x03, 0, "Minerals    "},
    {0x61, -0x01, 0x42, 0x07, 1, "Gold        "},
    {0xAB, -0x02, 0x37, 0x1F, 1, "Platinum    "},
    {0x2D, -0x01, 0xFA, 0x0F, 2, "Gem-Strones "},
    {0x35, +0x0F, 0xC0, 0x07, 0, "Alien Items "},
};

/** Required data for text interface **/

/*
 * Tradegood names used in text commands
 * Set using commodities array
 */

static char tradnames[lasttrade][maxlen];

#define nocomms (14)

static boolean dobuy       (char *);
static boolean dosell      (char *);
static boolean dofuel      (char *);
static boolean dojump      (char *);
static boolean docash      (char *);
static boolean domkt       (char *);
static boolean dohelp      (char *);
static boolean dohold      (char *);
static boolean dosneak     (char *);
static boolean dolocal     (char *);
static boolean doinfo      (char *);
static boolean dogalhyp    (char *);
static boolean doquit      (char *);
static boolean dotweakrand (char *);

static char commands[nocomms][maxlen] = {"buy",  "sell",  "fuel",  "jump", "cash",   "mkt",  "help",
                                         "hold", "sneak", "local", "info", "galhyp", "quit", "rand"};

static boolean (*comfuncs[nocomms])(char *) = {dobuy,  dosell,  dofuel,  dojump, docash,   domkt,  dohelp,
                                               dohold, dosneak, dolocal, doinfo, dogalhyp, doquit, dotweakrand};

/** General functions **/

static unsigned int lastrand = 0;

static void
mysrand(unsigned int lseed) {
    srand(lseed);
    lastrand = lseed - 1;
}

static int
myrand(void) {
    int r;

    if (nativerand) {
        r = rand();
    } else { /* As supplied by D McDonnell from SAS Insititute C */
        r = (((((((((((lastrand << 3) - lastrand) << 3) + lastrand) << 1) + lastrand) << 4) - lastrand) << 1) - lastrand) + 0xe60)
             & 0x7fffffff;
        lastrand = (unsigned int)r - 1;
    }

    return r;
}

static char
randbyte(void) {
    return (char)(myrand() & 0xFF);
}

static myuint
mymin(myuint a, myuint b) {
    if (a < b)
        return a;
    else
        return b;
}

/** ftoi **/
static signed int
ftoi(double value) {
    return (signed int)(double)floor(value + 0.5);
}

static void
tweakseed(seedtype *s) {
    uint16 temp;

    temp    = ((*s).w0) + ((*s).w1) + ((*s).w2); /* 2 byte aritmetic */
    (*s).w0 = (*s).w1;
    (*s).w1 = (*s).w2;
    (*s).w2 = temp;
}

/** String functions for text interface **/

static void
stripout(char *s, const char c) /* Remove all c's from string s */
{
    size_t i = 0, j = 0;

    while (i < strlen(s)) {
        if (s[i] != c) {
            s[j] = s[i];
            j++;
        }

        i++;
    }
    s[j] = 0;
}

static int
stringbeg(const char *s, const char *t) {
/*
 * Return nonzero (true) if string t
 * begins with non-empty string s
 */
    size_t len_s = strlen(s);
    size_t len_t = strlen(t);

    if (len_t < len_s)
        return false;

    return strncasecmp(s, t, len_s) == 0;
}

static myuint
stringmatch(char *s, char a[][20], myuint n)
/*
 * Check string s against n options in string array a
 * If matches ith element return i+1 else return 0
 */
{
    myuint i = 0;

    while (i < n) {
        if (stringbeg(s, a[i]))
            return i + 1;

        i++;
    }

    return 0;
}

static void
spacesplit(char *s, char *t)
/*
 * Split string s at first space, returning
 * first 'word' in t & shortening s
 */
{
    size_t i = 0, j = 0;
    size_t l = strlen(s);

    while ((i < l) & (s[i] == ' '))
        i++;

    /* Strip leading spaces */
    if (i == l) {
        s[0] = 0;
        t[0] = 0;

        return;
    }

    while ((i < l) & (s[i] != ' '))
        t[j++] = s[i++];
    t[j] = 0;

    i++;
    j = 0;

    while (i < l)
        s[j++] = s[i++];
    s[j] = 0;
}

/** Functions for stock market **/

static myuint
gamebuy(myuint i, myuint a)
/*
 * Try to buy amount a of good i
 * Return amount bought
 * Cannot buy more than is availble,
 * can afford, or will fit in hold
 */
{
    myuint t;

    if (cash < 0) {
        t = 0;
    } else {
        t = mymin(localmarket.quantity[i], a);
        if ((commodities[i].units) == tonnes)
            t = mymin(holdspace, t);

        t = mymin(t, (myuint)(double)floor((double)cash / (localmarket.price[i])));
    }

    shipshold[i] += t;
    localmarket.quantity[i] -= t;
    cash -= t * (localmarket.price[i]);
    if ((commodities[i].units) == tonnes)
        holdspace -= t;

    return t;
}

static myuint
gamesell(myuint i, myuint a) /* As gamebuy but selling */
{
    myuint t = mymin(shipshold[i], a);

    shipshold[i] -= t;
    localmarket.quantity[i] += t;
    if ((commodities[i].units) == tonnes)
        holdspace += t;

    cash += t * (localmarket.price[i]);

    return t;
}

static markettype
genmarket(myuint fluct, plansys p)
/*
 * Prices and availabilities are influenced by the planet's economy type
 * (0-7) and a random "fluctuation" byte that was kept within the saved
 * commander position to keep the market prices constant over gamesaves.
 * Availabilities must be saved with the game since the player alters them
 * by buying (and selling(?))
 *
 * Almost all operations are one byte only and overflow "errors" are
 * extremely frequent and exploited.
 *
 * Trade Item prices are held internally in a single byte=true value/4.
 * The decimal point in prices is introduced only when printing them.
 * Internally, all prices are integers.
 *
 * The player's cash is held in four bytes.
 */
{
    markettype market;
    unsigned short i;

    for (i = 0; i <= lasttrade; i++) {
        signed int q;
        signed int product  = (p.economy) * (commodities[i].gradient);
        signed int changing = fluct & (commodities[i].maskbyte);
        q                   = (commodities[i].basequant) + changing - product;
        q                   = q & 0xFF;
        if (q & 0x80)
            q = 0;

        /* Clip to positive 8-bit */
        market.quantity[i] = (uint16)(q & 0x3F); /* Mask to 6 bits */

        q               = (commodities[i].baseprice) + changing + product;
        q               = q & 0xFF;
        market.price[i] = (uint16)(q * 4);
    }

    market.quantity[AlienItems] = 0; /* Override to force nonavailability */

    return market;
}

static void
displaymarket(markettype m) {
    unsigned short i;

    (void)printf("\n");
    (void)printf("Item         \t  Price\t   Quantity  \tHold\n");
    (void)printf("=============\t ======\t  ========== \t====\n");
    for (i = 0; i <= lasttrade; i++) {
        (void)printf("\n");
        (void)printf("%s", commodities[i].name);
        (void)printf("\t %6.1f", (double)((float)(m.price[i]) / 10));
        (void)printf("\t %6u", m.quantity[i]);
        (void)printf("%s", unitnames[commodities[i].units]);
        (void)printf("\t %2u", shipshold[i]);
    }
}

/** Generate system info from seed **/

static plansys
makesystem(seedtype *s) {
    plansys thissys;
    myuint pair1, pair2, pair3, pair4;
    uint16 longnameflag = ((*s).w0) & 64;
    char *pairs1        = &pairs[24]; /* Start of pairs used by this routine */

    thissys.x = (((*s).w1) >> 8);
    thissys.y = (((*s).w0) >> 8);

    thissys.govtype = ((((*s).w1) >> 3) & 7); /* Bits 3,4 &5 of w1 */

    thissys.economy = ((((*s).w0) >> 8) & 7); /* Bits 8,9 &A of w0 */
    if (thissys.govtype <= 1)
        thissys.economy = ((thissys.economy) | 2);

    thissys.techlev = ((((*s).w1) >> 8) & 3) + ((thissys.economy) ^ 7);
    thissys.techlev += ((thissys.govtype) >> 1);
    if (((thissys.govtype) & 1) == 1)
        thissys.techlev += 1;

    /* C simulation of 6502's LSR then ADC */

    thissys.population = 4 * (thissys.techlev) + (thissys.economy);
    thissys.population += (thissys.govtype) + 1;

    thissys.productivity = (((thissys.economy) ^ 7) + 3) * ((thissys.govtype) + 4);
    thissys.productivity *= (thissys.population) * 8;

    thissys.radius = 256 * (((((*s).w2) >> 8) & 15) + 11) + thissys.x;

    thissys.goatsoupseed.a = (*s).w1 & 0xFF;
    thissys.goatsoupseed.b = (*s).w1 >> 8;
    thissys.goatsoupseed.c = (*s).w2 & 0xFF;
    thissys.goatsoupseed.d = (*s).w2 >> 8;

    thissys.human_colony = !(thissys.goatsoupseed.c & 0x80);
    thissys.species_adj1 = (thissys.goatsoupseed.d >> 2) & 3;
    thissys.species_adj2 = (thissys.goatsoupseed.d >> 5) & 7;
    thissys.species_adj3 = (thissys.x ^ thissys.y) & 7;
    thissys.species_type = (thissys.species_adj3 + (thissys.goatsoupseed.d & 3)) & 7;

    /* Always four iterations of random number */

    pair1 = 2 * ((((*s).w2) >> 8) & 31);
    tweakseed(s);
    pair2 = 2 * ((((*s).w2) >> 8) & 31);
    tweakseed(s);
    pair3 = 2 * ((((*s).w2) >> 8) & 31);
    tweakseed(s);
    pair4 = 2 * ((((*s).w2) >> 8) & 31);
    tweakseed(s);

    (thissys.name)[0] = pairs1[pair1];
    (thissys.name)[1] = pairs1[pair1 + 1];
    (thissys.name)[2] = pairs1[pair2];
    (thissys.name)[3] = pairs1[pair2 + 1];
    (thissys.name)[4] = pairs1[pair3];
    (thissys.name)[5] = pairs1[pair3 + 1];

    if (longnameflag) /* Bit 6 of ORIGINAL w0 flags a four-pair name */
    {
        (thissys.name)[6] = pairs1[pair4];
        (thissys.name)[7] = pairs1[pair4 + 1];
        (thissys.name)[8] = 0;
    } else {
        (thissys.name)[6] = 0;
    }

    stripout(thissys.name, '.');

    return thissys;
}

/** Generate galaxy **/

/* Functions for galactic hyperspace */

static uint16
rotatel(uint16 x)
/* Rotate 8 bit number leftwards
 * (tried to use chars but too much effort
 * persuading this braindead language to
 * do bit operations on bytes!)
 */
{
    uint16 temp = x & 128;

    return (2 * (x & 127)) + (temp >> 7);
}

static uint16
twist(uint16 x) {
    return (uint16)((256 * rotatel(x >> 8)) + rotatel(x & 255));
}

static void
nextgalaxy(seedtype *s)
/*
 * Apply to base seed; once for galaxy 2
 */
{
    (*s).w0 = twist((*s).w0); /* Twice for galaxy 3, etc. */
    (*s).w1 = twist((*s).w1); /* Eighth application gives galaxy 1 again */
    (*s).w2 = twist((*s).w2);
}

/* Original game generated from scratch each time info needed */

static void
buildgalaxy(myuint lgalaxynum) {
    myuint syscount, galcount;

    seed.w0 = base0;
    seed.w1 = base1;
    seed.w2 = base2; /* Initialise seed for galaxy 1 */
    for (galcount = 1; galcount < lgalaxynum; ++galcount) nextgalaxy(&seed);

    /* Put galaxy data into array of structures */
    for (syscount = 0; syscount < galsize; ++syscount) galaxy[syscount] = makesystem(&seed);
}

/** Functions for navigation **/

static void
gamejump(planetnum i) /* Move to system i */
{
    currentplanet = i;
    localmarket   = genmarket((myuint)randbyte(), galaxy[i]);
}

static myuint
distance(plansys a, plansys b)
/*
 * Seperation between two planets
 * (4*sqrt(X*X+Y*Y/4))
 */
{
    return (myuint)ftoi(4 * sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) / 4));
}

static planetnum
matchsys(char *s)
/*
 * Return id of the planet whose name matches passed strinmg
 * closest to currentplanet - if none return currentplanet
 */
{
    planetnum syscount;
    planetnum p = currentplanet;
    myuint d    = 9999;

    for (syscount = 0; syscount < galsize; ++syscount) {
        if (stringbeg(s, galaxy[syscount].name)) {
            if (distance(galaxy[syscount], galaxy[currentplanet]) < d) {
                d = distance(galaxy[syscount], galaxy[currentplanet]);
                p = syscount;
            }
        }
    }

    return p;
}

/** Print data for given system **/

static void
prisys(plansys plsy, boolean compressed) {
    if (compressed) {
        (void)printf("%10s", plsy.name);
        (void)printf(" TL: %2i ", (plsy.techlev) + 1);
        (void)printf("%12s", econnames[plsy.economy]);
        (void)printf(" %15s", govnames[plsy.govtype]);
    } else {
        (void)printf("\n\nSystem  \t: ");
        (void)printf("%s", plsy.name);
        (void)printf("\nPosition  \t: (%i,", plsy.x);
        (void)printf("%i)", plsy.y);
        (void)printf("\nEconomy   \t: (%i) ", plsy.economy);
        (void)printf("%s", econnames[plsy.economy]);
        (void)printf("\nGovernment\t: (%i) ", plsy.govtype);
        (void)printf("%s", govnames[plsy.govtype]);
        (void)printf("\nTech Level\t: %-2i", (plsy.techlev) + 1);
        (void)printf("\nTurnover  \t: %u", (plsy.productivity));
        (void)printf("\nRadius    \t: %u", plsy.radius);
        (void)printf("\nPopulation\t: %.1f Billion", (plsy.population) / 10.0);
        (void)printf("\nSpecies   \t: ");
        if (plsy.human_colony) {
            (void)printf("Human Colonials\n");
        } else {
            if (plsy.species_adj1 < 3)
                (void)printf("%s ", species_stature[plsy.species_adj1]);

            if (plsy.species_adj2 < 6)
                (void)printf("%s ", species_coloration[plsy.species_adj2]);

            if (plsy.species_adj3 < 6)
                (void)printf("%s ", species_characteristics[plsy.species_adj3]);

            (void)printf("%s\n", species_base_type[plsy.species_type]);
        }

        rnd_seed = plsy.goatsoupseed;
        (void)printf("\n");
        goat_soup("\x8F is \x97.", &plsy);
    }
}

/** Various command functions **/

static boolean
dotweakrand(char *s) {
    (void)s;
    nativerand ^= 1;
    (void)printf("%s", nativerand ? "Now using native randomization." : "Now using weak randomization.");
    return true;
}

static boolean
dolocal(char *s) {
    planetnum syscount;
    myuint d;

    (void)s;
    (void)printf("\nGalaxy number %i:", galaxynum);
    for (syscount = 0; syscount < galsize; ++syscount) {
        d = distance(galaxy[syscount], galaxy[currentplanet]);
        if (d <= maxfuel) {
            if (d <= fuel)
                (void)printf("\n * ");
            else
                (void)printf("\n - ");

            prisys(galaxy[syscount], true);
            (void)printf(" (%.1f LY)", (double)((float)d / 10));
        }
    }

    return true;
}

static boolean
dojump(char *s)
/*
 * Jump to planet name s
 */
{
    myuint d;
    planetnum dest = matchsys(s);

    if (dest == currentplanet) {
        (void)printf("\nBad jump");
        return false;
    }

    d = distance(galaxy[dest], galaxy[currentplanet]);
    if (d > fuel) {
        (void)printf("\nJump to far");
        return false;
    }

    fuel -= d;
    gamejump(dest);
    prisys(galaxy[currentplanet], false);
    return true;
}

static boolean
dosneak(char *s)
/*
 * As dojump but no fuel cost
 */
{
    myuint fuelkeep = fuel;
    boolean b;

    fuel = 666;
    b    = dojump(s);
    fuel = fuelkeep;
    return b;
}

static boolean
dogalhyp(char *s)
/*
 * Jump to next galaxy
 * Preserve planetnum (eg. if leave 7th
 * planet arrive at 7th planet)
 */
{
    (void)(&s); /* Discard s */
    galaxynum++;
    if (galaxynum == 9)
        galaxynum = 1;

    buildgalaxy(galaxynum);
    return true;
}

static boolean
doinfo(char *s)
/*
 * Info on planet
 */
{
    planetnum dest = matchsys(s);

    prisys(galaxy[dest], false);
    return true;
}

static boolean
dohold(char *s) {
    myuint a = (myuint)atoi(s), t = 0, i;

    for (i = 0; i <= lasttrade; ++i)
        if ((commodities[i].units) == tonnes)
            t += shipshold[i];

    if (t > a) {
        (void)printf("\nHold too full");
        return false;
    }

    holdspace = a - t;
    return true;
}

static boolean
dosell(char *s)
/*
 * Sell amount s(2) of good s(1)
 */
{
    myuint i, a, t;
    char s2[maxlen];

    spacesplit(s, s2);
    a = (myuint)atoi(s);
    if (a == 0)
        a = 1;

    i = stringmatch(s2, tradnames, lasttrade + 1);
    if (i == 0) {
        (void)printf("\nUnknown trade good");
        return false;
    }

    i -= 1;

    t = gamesell(i, a);

    if (t == 0) {
        (void)printf("Cannot sell any ");
    } else {
        (void)printf("\nSelling %i", t);
        (void)printf("%s", unitnames[commodities[i].units]);
        (void)printf(" of ");
    }

    (void)printf("%s", tradnames[i]);

    return true;
}

static boolean
dobuy(char *s)
/*
 * Buy amount s(2) of good s(1)
 */
{
    myuint i, a, t;
    char s2[maxlen];

    spacesplit(s, s2);
    a = (myuint)atoi(s);
    if (a == 0)
        a = 1;

    i = stringmatch(s2, tradnames, lasttrade + 1);
    if (i == 0) {
        (void)printf("\nUnknown trade good");
        return false;
    }

    i -= 1;

    t = gamebuy(i, a);
    if (t == 0) {
        (void)printf("Cannot buy any ");
    } else {
        (void)printf("\nBuying %i", t);
        (void)printf("%s", unitnames[commodities[i].units]);
        (void)printf(" of ");
    }

    (void)printf("%s", tradnames[i]);
    return true;
}

static myuint
gamefuel(myuint f)
/*
 * Attempt to buy f tonnes of fuel
 */
{
    if (f + fuel > maxfuel)
        f = (myuint)maxfuel - fuel;

    if (fuelcost > 0) {
        if ((int)f * fuelcost > cash)
            f = (myuint)(cash / fuelcost);
    }

    fuel += f;
    cash -= fuelcost * f;
    return f;
}

static boolean
dofuel(char *s)
/*
 * Buy amount s of fuel
 */
{
    myuint f = gamefuel((myuint)(double)floor(10 * atof(s)));

    if (f == 0)
        (void)printf("\nCan't buy any fuel");

    (void)printf("\nBuying %.1fLY fuel", (double)((float)f / 10));
    return true;
}

static boolean
docash(char *s)
/*
 * Cheat alter cash by s
 */
{
    int a = (int)(10 * atof(s));

    cash += (long)a;
    if (a != 0)
        return true;

    (void)printf("Number not understood");
    return false;
}

static boolean
domkt(char *s)
/*
 * Show stock market
 */
{
    (void)s;
    (void)printf("\n");
    displaymarket(localmarket);
    return true;
}

static boolean
parser(char *s)
/*
 * Obey command s
 */
{
    myuint i;
    char c[maxlen];

    if (feof(stdin))
        doquit(NULL); /* Catch EOF */

    if (0 == strcmp(s, ""))
        return false;

    spacesplit(s, c);
    i = stringmatch(c, commands, nocomms);

    if (i)
        return (*comfuncs[i - 1])(s);

    (void)printf("\n Bad command (%s)", c);
    return false;
}

static boolean
doquit(char *s) {
    (void)(&s);
    (void)puts("\n\nQuit.");
    exit(0);
}

boolean
dohelp(char *s) {
    (void)(&s);
    (void)printf("\nCommands are:");
    (void)printf("\n[B]uy     <tradegood> <amount>");
    (void)printf("\n[S]ell    <tradegood> <amount>");
    (void)printf("\n[F]uel    <amount>    (buy amount LY of fuel)");
    (void)printf("\n[J]ump    <planet>    (limited by fuel)");
    (void)printf("\n[Sn]eak   <planet>    (any distance - no fuel cost)");
    (void)printf("\n[G]alhyp              (jumps to next galaxy)");
    (void)printf("\n[I]nfo    [planet]    (prints info on system)");
    (void)printf("\n[M]kt                 (shows market prices)");
    (void)printf("\n[L]ocal               (lists systems within 7 light years)");
    (void)printf("\n[C]ash    <number>    (alters cash - cheating!)");
    (void)printf("\n[Ho]ld    <number>    (change cargo bay)");
    (void)printf("\n[Q]uit                (exit)");
    (void)printf("\n[H]elp                (display this text)");
    (void)printf("\n[R]and                (toggle RNG)");
    (void)printf("\n\nAbbreviations allowed, e.g. 'b fo 5' == 'Buy Food 5'");
    return true;
}

/** main **/
int
main(void) {
    myuint i;

    nativerand = 1;
    (void)printf("\nWelcome to Text Elite 1.5.\n");

    for (i = 0; i < lasttrade; i++) (void)strcpy(tradnames[i], commodities[i].name);

    mysrand(12345); /* Ensure repeatability */

    galaxynum = 1;
    buildgalaxy(galaxynum);

    currentplanet = numforLave;                          /* Don't use jump */
    localmarket   = genmarket(0x00, galaxy[numforLave]); /* Since want seed=0 */

    fuel = (myuint)maxfuel;

#define PARSER(S)             \
    {                         \
        char buf[0x10];       \
        (void)strcpy(buf, S); \
        (void)parser(buf);    \
    }

    PARSER("hold 20")   /* Small cargo bay */
    PARSER("cash +100") /* 100 CR */
    PARSER("help")

#undef PARSER

    for(;;) {
        (void)printf("\n\nFuel:%.1f", (double)((float)fuel / 10));
        (void)printf(" Holdspace:%it", holdspace);
        (void)printf(" Cash:%.1f > ", (double)(((float)cash) / 10));
        char getcommand[maxlen];
        (void)memset(getcommand, 0, maxlen);
        (void)fflush(stdout);
        if (fgets(getcommand, maxlen, stdin)) {
            getcommand[strcspn(getcommand, "\n")] = '\0';
        }
        if (NULL == strstr(getcommand, "\x08"))
            (void)parser(getcommand);
    }

    /*
     * 6502 Elite fires up at Lave with fluctuation=00
     * and these prices tally with the NES ones.
     * However, the availabilities reside in the saved game data.
     * Availabilities are calculated (and fluctuation randomised)
     * on hyperspacing.
     *
     * I have checked with this code for Zaonce with fluctaution &AB
     * against the SuperVision 6502 code and both prices and availabilities
     * tally.
     */
#if !defined(__SUNPRO_C) && !defined(__SUNPRO_CC) && !defined(__SUNPRO_CC_COMPAT)
    /*NOTREACHED*/ /* unreachable */
    return 0;
#endif /* if !defined(__SUNPRO_C) && !defined(__SUNPRO_CC) && !defined(__SUNPRO_CC_COMPAT) */
}

/*
 * "Goat Soup" planetary description string code
 * adapted from Christian Pinder's reverse engineered sources.
 */

struct desc_choice {
    const char *option[5];
};

static struct desc_choice desc_list[] = {
    /* 81 */ {{"fabled", "notable", "well known", "famous", "noted"}},
    /* 82 */ {{"very ", "mildly ", "most ", "reasonably ", ""}},
    /* 83 */ {{"ancient", "\x95", "great", "vast", "pink"}},
    /* 84 */ {{"\x9E \x9D plantations", "mountains", "\x9C", "\x94 forests", "oceans"}},
    /* 85 */ {{"shyness", "silliness", "mating traditions", "loathing of \x86", "love for \x86"}},
    /* 86 */ {{"food blenders", "tourists", "poetry", "discos", "\x8E"}},
    /* 87 */ {{"talking tree", "crab", "bat", "lobst", "\xB2"}},
    /* 88 */ {{"beset", "plagued", "ravaged", "cursed", "scourged"}},
    /* 89 */ {{"\x96 civil war", "\x9B \x98 \x99s", "a \x9B disease", "\x96 earthquakes", "\x96 solar activity"}},
    /* 8A */ {{"its \x83 \x84", "the \xB1 \x98 \x99", "its inhabitants' \x9A \x85", "\xA1", "its \x8D \x8E"}},
    /* 8B */ {{"juice", "brandy", "water", "brew", "gargle blasters"}},
    /* 8C */ {{"\xB2", "\xB1 \x99", "\xB1 \xB2", "\xB1 \x9B", "\x9B \xB2"}},
    /* 8D */ {{"fabulous", "exotic", "hoopy", "unusual", "exciting"}},
    /* 8E */ {{"cuisine", "night life", "casinos", "sit coms", " \xA1 "}},
    /* 8F */ {{"\xB0", "The planet \xB0", "The world \xB0", "This planet", "This world"}},
    /* 90 */ {{"n unremarkable", " boring", " dull", " tedious", " revolting"}},
    /* 91 */ {{"planet", "world", "place", "little planet", "dump"}},
    /* 92 */ {{"wasp", "moth", "grub", "ant", "\xB2"}},
    /* 93 */ {{"poet", "arts graduate", "yak", "snail", "slug"}},
    /* 94 */ {{"tropical", "dense", "rain", "impenetrable", "exuberant"}},
    /* 95 */ {{"funny", "wierd", "unusual", "strange", "peculiar"}},
    /* 96 */ {{"frequent", "occasional", "unpredictable", "dreadful", "deadly"}},
    /* 97 */ {{"\x82\x81 for \x8A", "\x82\x81 for \x8A and \x8A", "\x88 by \x89", "\x82\x81 for \x8A but \x88 by \x89", "a\x90 \x91"}},
    /* 98 */ {{"\x9B", "mountain", "edible", "tree", "spotted"}},
    /* 99 */ {{"\x9F", "\xA0", "\x87oid", "\x93", "\x92"}},
    /* 9A */ {{"ancient", "exceptional", "eccentric", "ingrained", "\x95"}},
    /* 9B */ {{"killer", "deadly", "evil", "lethal", "vicious"}},
    /* 9C */ {{"parking meters", "dust clouds", "ice bergs", "rock formations", "volcanoes"}},
    /* 9D */ {{"plant", "tulip", "banana", "corn", "\xB2weed"}},
    /* 9E */ {{"\xB2", "\xB1 \xB2", "\xB1 \x9B", "inhabitant", "\xB1 \xB2"}},
    /* 9F */ {{"shrew", "beast", "bison", "snake", "wolf"}},
    /* A0 */ {{"leopard", "cat", "monkey", "goat", "fish"}},
    /* A1 */ {{"\x8C \x8B", "\xB1 \x9F \xA2", "its \x8D \xA0 \xA2", "\xA3 \xA4", "\x8C \x8B"}},
    /* A2 */ {{"meat", "cutlet", "steak", "burgers", "soup"}},
    /* A3 */ {{"ice", "mud", "Zero-G", "vacuum", "\xB1 ultra"}},
    /* A4 */ {{"hockey", "cricket", "karate", "polo", "tennis"}}};

    /*
     * B0 = <planet name>
     * B1 = <planet name>ian
     * B2 = <random name>
     */

static int
gen_rnd_number(void) {
    int a, x;

    x = (rnd_seed.a * 2) & 0xFF;
    a = x + rnd_seed.c;
    if (rnd_seed.a > 127)
        a++;

    rnd_seed.a = (uint8)(a & 0xFF);
    rnd_seed.c = (uint8)x;
    a          = a / 256; /* a = any carry left from above */
    x          = rnd_seed.b;
    a          = (a + x + rnd_seed.d) & 0xFF;
    rnd_seed.b = (uint8)a;
    rnd_seed.d = (uint8)x;
    return a;
}

static void
goat_soup(const char *source, plansys *psy) {
    for (;;) {
        int c = (unsigned char)*(source++);
        if (c == '\0')
            break;

        if (c <= 0x80) {
            (void)printf("%c", c);
        } else {
            if (c <= 0xA4) {
                int rnd = gen_rnd_number();
                goat_soup(desc_list[c - 0x81].option[(rnd >= 0x33) + (rnd >= 0x66) + (rnd >= 0x99) + (rnd >= 0xCC)], psy);
            } else {
                switch (c) {
                    case 0xB0: /* planet name */
                    {
                        int i = 1;
                        (void)printf("%c", psy->name[0]);
                        while (psy->name[i] != '\0')
                            (void)printf("%c", tolower(psy->name[i++]));
                    } break;

                    case 0xB1: /* <planet name>ian */
                    {
                        int i = 1;
                        (void)printf("%c", psy->name[0]);
                        while (psy->name[i] != '\0') {
                            if ((psy->name[i + 1] != '\0') || ((psy->name[i] != 'E') && (psy->name[i] != 'I')))
                                (void)printf("%c", tolower(psy->name[i]));

                            i++;
                        }
                        (void)printf("ian");
                    } break;

                    case 0xB2: /* random name */
                    {
                        int i;
                        int len = gen_rnd_number() & 3;
                        for (i = 0; i <= len; i++) {
                            int x = gen_rnd_number() & 0x3e;
                            char p1 = pairs[x];
                            char p2 = pairs[x + 1];
                            if (p1 != '.') {
                                if (i)
                                    p1 = (char)tolower(p1);

                                (void)printf("%c", p1);
                            }

                            if (p2 != '.') {
                                if (i || (p1 != '.'))
                                    p2 = (char)tolower(p2);

                                (void)printf("%c", p2);
                            }
                        }
                    } break;

                    default: (void)printf("<bad char in data [%X]>", c); return;
                }
            }
        }
    }
}
