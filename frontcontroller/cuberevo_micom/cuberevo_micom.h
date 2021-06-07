/*
 * cuberevo_micom.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 *
 * header file for CubeRevo front panel driver.
 *
 ***************************************************************************/
#ifndef _cuberevo_micom_h
#define _cuberevo_micom_h

extern short paramDebug;

#define TAGDEBUG "[cuberevo_micom] "

#ifndef dprintk
#define dprintk(level, x...) do { \
		if ((paramDebug) && (paramDebug >= level)) printk(TAGDEBUG x); \
	} while (0)
#endif

typedef struct
{
	struct file      *fp;
	int               read;
	struct semaphore  sem;

} tFrontPanelOpen;

#define FRONTPANEL_MINOR_RC  1
#define LASTMINOR            2

extern tFrontPanelOpen FrontPanelOpen[LASTMINOR];

#define VFD_MAJOR            147

/* IOCTL numbers -> hacky! */
#define VFDDISPLAYCHARS      0xc0425a00
#define VFDBRIGHTNESS        0xc0425a03
#define VFDPWRLED            0xc0425a04  /* obsolete, use VFDSETLED (0xc0425afe) */
#define VFDDISPLAYWRITEONOFF 0xc0425a05
#define VFDDRIVERINIT        0xc0425a08
#define VFDICONDISPLAYONOFF  0xc0425a0a
// comment next line to leave VFDTEST capabilities out
//#define VFDTEST              0xc0425af0  // added by audioniek
#define VFDCLEARICONS	     0xc0425af6
#define VFDSETRF             0xc0425af7
#define VFDSETFAN            0xc0425af8
#define VFDGETWAKEUPMODE     0xc0425af9
#define VFDGETTIME           0xc0425afa
#define VFDSETTIME           0xc0425afb
#define VFDSTANDBY           0xc0425afc
#define VFDREBOOT            0xc0425afd
#define VFDSETLED            0xc0425afe
#define VFDSETMODE           0xc0425aff

#define VFDGETWAKEUPTIME     0xc0425b00
#define VFDGETVERSION        0xc0425b01
#define VFDSETDISPLAYTIME    0xc0425b02
#define VFDSETTIMEMODE       0xc0425b03
#define VFDSETWAKEUPTIME     0xc0425b04
#define VFDLEDBRIGHTNESS     0xc0425b05  /* Cuberevo/micom specific */

// comment next line if you want left aligned text display
//#define CENTERED_DISPLAY

#if defined CENTERED_DISPLAY
#if defined(CUBEREVO_250HD) \
 || defined(CUBEREVO_MINI_FTA)
#undef CENTERED_DISPLAY  // never center on LED models
#endif
#endif

#define cNumberSymbols      8

/***************************************************************************
 *
 * Icon definitions.
 *
 ***************************************************************************/
#if defined(CUBEREVO) \
 || defined(CUBEREVO_9500HD) 
enum  // for 12 character dot matrix
{
	ICON_MIN = 0,  // 0
	ICON_STANDBY,
	ICON_SAT,
	ICON_REC,
	ICON_TIMESHIFT,
	ICON_TIMER,    // 5
	ICON_HD,
	ICON_USB,
	ICON_SCRAMBLED,
	ICON_DOLBY,
	ICON_MUTE,     // 10
	ICON_TUNER1,
	ICON_TUNER2,
	ICON_MP3,
	ICON_REPEAT,
	ICON_PLAY,     // 15
	ICON_TER,
	ICON_FILE,
	ICON_480i,
	ICON_480p,
	ICON_576i,     // 20
	ICON_576p,
	ICON_720p,
	ICON_1080i,
	ICON_1080p,
	ICON_PLAY_1,   // 25
	ICON_RADIO,
	ICON_TV,
	ICON_PAUSE,
	ICON_MAX       // 29
};
#elif defined(CUBEREVO_MINI) \
 ||   defined(CUBEREVO_MINI2) \
 ||   defined(CUBEREVO_2000HD) \
 ||   defined(CUBEREVO_3000HD)
enum  // for 14 character dot matrix
{
	ICON_MIN,  // 0
	ICON_REC,
	ICON_TIMER,  // 2
	ICON_TIMESHIFT,
	ICON_PLAY,  // 4
	ICON_PAUSE,
	ICON_HD,  // 6
	ICON_DOLBY,
	ICON_MAX  // 8
};
#endif

struct iconToInternal
{
	char *name;
	u16 icon;
	u8 codemsb;
	u8 codelsb;
	u8 segment;
};

struct set_test_s
{
	unsigned char data[14];
};

struct set_brightness_s
{
	int level;
};

struct set_led_s
{
	int led_nr;
	int on;
};

struct set_on_off_s
{
	int on;
};

struct set_icon_s
{
	int icon_nr;
	int on;
};

/* YMDhms */
struct get_time_s
{
	char time[6];
};

/* YYMMDDhhmm */
struct set_standby_s
{
	char time[10];
};

/* YYMMDDhhmmss */
struct set_time_s
{
	char time[12];
};

struct get_wakeupstatus_s
{
	char status;
};

/* this sets the mode temporarily (for one IOCTL)
 * to the desired mode. currently the "normal" mode
 * is the compatible vfd mode
 */
struct set_mode_s
{
	int compat; /* 0 = compatibility mode to vfd driver; 1 = micom mode */
};

/* 0 = 12dot 12seg, 1 = 13grid, 2 = 12 dot 14seg, 3 = 7seg */
struct get_version_s
{
	int version;
};

struct set_time_mode_s
{
	int twentyFour;
};

struct micom_ioctl_data
{
	union
	{
		struct set_test_s test;
		struct set_icon_s icon;
		struct set_led_s led;
		struct set_on_off_s fan;
		struct set_on_off_s rf;
		struct set_brightness_s brightness;
		struct set_mode_s mode;
		struct set_standby_s standby;
		struct set_time_s time;
		struct get_time_s get_time;
		struct get_wakeupstatus_s status;
		struct get_time_s get_wakeup_time;
		struct set_standby_s wakeup_time;
		struct get_version_s version;
		struct set_on_off_s display_time;
		struct set_time_mode_s time_mode;
	} u;
};

struct vfd_ioctl_data
{
	unsigned char start;
	unsigned char data[64];
	unsigned char length;
};

struct saved_data_s
{
	int   length;  // length of last string written to fp display
	char  data[128];  // last string written to fp display
	int   fan;  // fan state
};

void dumpValues(void);
extern int micom_init_func(void);
extern void copyData(unsigned char *data, int len);
extern void getRCData(unsigned char *data, int *len);

extern int errorOccured;
extern char *gmt_offset;  // module param, string
extern int rtc_offset;

/* number of display characters */
extern int front_seg_num;

extern struct file_operations vfd_fops;

extern struct saved_data_s lastdata;

#endif  // _cuberevo_micom_h
// vim:ts=4
