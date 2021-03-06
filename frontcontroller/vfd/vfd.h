/*
 */

#define VFD_MAJOR			147

#define SCP_TXD_BIT			6
#define SCP_SCK_BIT			8
#define SCP_ENABLE_BIT		5
#define VFD_PORT			0
#define SCP_DATA			3
#define SCP_CLK			4
#define SCP_CS			5

#define DCRAM_COMMAND		(0x20 & 0xf0)
#define CGRAM_COMMAND		(0x40 & 0xf0)
#define ADRAM_COMMAND		(0x60 & 0xf0)
#define NUM_DIGIT_COMMAND		(0xE0 & 0xf0)
#define LIGHT_ON_COMMAND		(0xE8 & 0xFC)
#define DIMMING_COMMAND		(0xE4 & 0xFC)
#define GRAY_LEVEL_DATA_COMMAND	(0xA0 & 0xF8)
#define GRAY_LEVEL_ON_COMMAND	(0xC0 & 0xE0)

#define PIO_PORT_SIZE		0x1000
#define PIO_BASE			0xb8020000
#define STPIO_SET_OFFSET		0x4
#define STPIO_CLEAR_OFFSET		0x8
#define STPIO_POUT_OFFSET		0x00

#define STPIO_SET_PIN(PIO_ADDR, PIN, V) writel(1<<PIN, PIO_ADDR + STPIO_POUT_OFFSET + ((V)? STPIO_SET_OFFSET : STPIO_CLEAR_OFFSET))
#define PIO_PORT(n) (((n)*PIO_PORT_SIZE) + PIO_BASE)

#define VFDDCRAMWRITE		0xc0425a00
#define VFDCGRAMWRITE		0xc0425a01
//dont know why but vfdctl wants this address
#define VFDCGRAMWRITE2		0x40425a01
#define VFDBRIGHTNESS		0xc0425a03
#define VFDDISPLAYWRITEONOFF	0xc0425a05
#define VFDDRIVERINIT		0xc0425a08
#define VFDICONDISPLAYONOFF	0xc0425a0a

//noch unbekannte ioctls
//0xc0425a04 = 0x10=Byte1
//0x40425a01 = 0x00=Byte1

struct vfd_ioctl_data
{
	unsigned char start_address;
	unsigned char data[64];
	unsigned char length;
};

struct __vfd_scp
{
	__u8 tr_rp_ctrl;
	__u8 rp_data;
	__u8 tr_data;
	__u8 start_tr;
	__u8 status;
	__u8 reserved;
	__u8 clk_div;
};

struct __vfd_scp *vfd_scp_ctrl = NULL;

#define SCP_TXD_CTRL        (vfd_scp_ctrl->tr_rp_ctrl)
#define SCP_TXD_DATA        (vfd_scp_ctrl->tr_data)
#define SCP_TXD_START       (vfd_scp_ctrl->start_tr)
#define SCP_STATUS          (vfd_scp_ctrl->status)

int SCP_PORT = 0;

unsigned char *ROM_Char_Table = NULL;

unsigned char ROM_KATHREIN[128] =
{
	0x00,//0x00,
	0x00,//0x01, icon #1
	0x01,//0x02, icon #2
	0x02,//0x03, icon #3
	0x03,//0x04, icon #4
	0x04,//0x05, icon #5
	0x05,//0x06, icon #6
	0x06,//0x07, icon #7
	0x10,//0x08,
	0x10,//0x09,
	0x10,//0x0a,
	0x10,//0x0b,
	0x10,//0x0c,
	0x10,//0x0d,
	0x10,//0x0e,
	0x10,//0x0f,

	0x10,//0x10,,reserved
	0x11,//0x11,,reserved
	0x12,//0x12,,reserved
	0x13,//0x13,,reserved
	0x14,//0x14,,reserved
	0x15,//0x15,,reserved
	0x16,//0x16,,reserved
	0x17,//0x17,,reserved
	0x18,//0x18,,reserved
	0x19,//0x19,,reserved
	0x1a,//0x1a,,reserved
	0x1b,//0x1b,,reserved
	0x1c,//0x1c,,reservedr
	0x10,//0x1d,,reserved
	0x10,//0x1e,,reserved
	0x10,//0x1f, , reserved

	0x20,//0x20, <space>
	0x21,//0x21,!
	0x22,//0x22,"
	0x23,//0x23,#
	0x24,//0x24,$
	0x25,//0x25,%
	0x26,//0x26,&
	0x27,//0x27,'
	0x28,//0x28,(
	0x29,//0x29,)
	0x2a,//0x2a,*
	0x2b,//0x2b,+
	0x2c,//0x2c,,
	0x2d,//0x2d,-
	0x2e,//0x2e,.
	0x2f,//0x2f,/

	0x30,//0x30,0
	0x31,//0x31,1
	0x32,//0x32,2
	0x33,//0x33,3
	0x34,//0x34,4
	0x35,//0x35,5
	0x36,//0x36,6
	0x37,//0x37,7
	0x38,//0x38,8
	0x39,//0x39,9
	0x3a,//0x3a,:
	0x3b,//0x3b,;
	0x3c,//0x3c,<
	0x3d,//0x3d,=
	0x3e,//0x3e,>
	0x3f,//0x3f,?

	0x40,//0x40,@
	0x41,//0x41,A
	0x42,//0x42,B
	0x43,//0x43,C
	0x44,//0x44,D
	0x45,//0x45,E
	0x46,//0x46,F
	0x47,//0x47,G
	0x48,//0x48,H
	0x49,//0x49,I
	0x4a,//0x4a,J
	0x4b,//0x4b,K
	0x4c,//0x4c,L
	0x4d,//0x4d,M
	0x4e,//0x4e,N
	0x4f,//0x4f,O

	0x50,//0x50,P
	0x51,//0x51,Q
	0x52,//0x52,R
	0x53,//0x53,S
	0x54,//0x54,T
	0x55,//0x55,U
	0x56,//0x56,V
	0x57,//0x57,W
	0x58,//0x58,X
	0x59,//0x59,Y
	0x5a,//0x5a,Z
	0x5b,//0x5b,[
	0x5c,//0x5c,
	0x5d,//0x5d,]
	0x5e,//0x5e,^
	0x5f,//0x5f,_

	0x60,//0x60,`
	0x61,//0x61,a
	0x62,//0x62,b
	0x63,//0x63,c
	0x64,//0x64,d
	0x65,//0x65,e
	0x66,//0x66,f
	0x67,//0x67,g
	0x68,//0x68,h
	0x69,//0x69,i
	0x6a,//0x6a,j
	0x6b,//0x6b,k
	0x6c,//0x6c,|
	0x6d,//0x6d,m
	0x6e,//0x6e,n
	0x6f,//0x6f,o

	0x70,//0x70,p
	0x71,//0x71,q
	0x72,//0x72,r
	0x73,//0x73,s
	0x74,//0x74,t
	0x75,//0x75,u
	0x76,//0x76,v
	0x77,//0x77,w
	0x78,//0x78,x
	0x79,//0x79,y
	0x7a,//0x7a,z
	0x7b,//0x7b,{
	0x7c,//0x7c,|
	0x7d,//0x7d,}
	0x7e,//0x7e,~
	0x7f//0x7f,

	/*        0x80,//0x80,??
	        0x81,//0x81,??
	        0x82,//0x82,??
	        0x83,//0x83,??
	        0x84,//0x84,??
	        0x85,//0x85,??
	        0x86,//0x86,??
	        0x87,//0x87,??
	        0x88,//0x88,??
	        0x89,//0x89,??
	        0x8a,//0x8a,??
	        0x8b,//0x8b,??
	        0x8c,//0x8c,??
	        0x8d,//0x8d,??
	        0x8e,//0x8e,??
	        0x8f,//0x8f,??

	        0x90,//0x90,??
	        0x91,//0x91,ae
	        0x92,//0x92,AE
	        0x93,//0x93,??
	        0x94,//0x94,??
	        0x95,//0x95,??
	        0x96,//0x96,??
	        0x97,//0x97,??
	        0x98,//0x98,??
	        0x99,//0x99,??
	        0x9a,//0x9a,??
	        0x9b,//0x9b,C|
	        0x9c,//0x9c,POUND
	        0x9d,//0x9d,Y-
	        0x9e,//0x9e,Pt
	        0x9f,//0x9f,f

	        0xa0,//0xa0,??
	        0xa1,//0xa1,??
	        0xa2,//0xa2,??
	        0xa3,//0xa3,??
	        0xa4,//0xa4,??
	        0xa5,//0xa5,??
	        0xa6,//0xa6,a_
	        0xa7,//0xa7,o_
	        0xa8,//0xa8,? upsidedown
	        0xa9,//0xa9,LT corner
	        0xaa,//0xaa,RT corner
	        0xab,//0xab,1/2
	        0xac,//0xac,1/4
	        0xad,//0xad,??
	        0xae,//0xae,<<
	        0xaf,//0xaf,>>

	        0xb0,//0xb0,
	        0xb1,//0xb1,
	        0xb2,//0xb2,
	        0xb3,//0xb3,
	        0xb4,//0xb4,
	        0xb5,//0xb5,
	        0xb6,//0xb6, O-
	        0xb7,//0xb7,
	        0xb8,//0xb8,
	        0xb9,//0xb9,
	        0xba,//0xba,
	        0xbb,//0xbb,
	        0xbc,//0xbc,
	        0xbd,//0xbd,
	        0xbe,//0xbe,
	        0xbf,//0xbf,

	        0xc0,//0xc0, reserved
	        0xc1,//0xc1, reserved
	        0xc2,//0xc2, reserved
	        0xc3,//0xc3, reserved
	        0xc4,//0xc4, reserved
	        0xc5,//0xc5, reserved
	        0xc6,//0xc6, reserved
	        0xc7,//0xc7, reserved
	        0xc8,//0xc8, reserved
	        0xc9,//0xc9, reserved
	        0xca,//0xca, reserved
	        0xcb,//0xcb, reserved
	        0xcc,//0xcc, reserved
	        0xcd,//0xcd, reserved
	        0xce,//0xce, reserved
	        0xcf,//0xcf, reserved

	        0xd0,//0xd0, reserved
	        0xd1,//0xd1, reserved
	        0xd2,//0xd2, reserved
	        0xd3,//0xd3, reserved
	        0xd4,//0xd4, reserved
	        0xd5,//0xd5, reserved
	        0xd6,//0xd6, reserved
	        0xd7,//0xd7, reserved
	        0xd8,//0xd8, reserved
	        0xd9,//0xd9, reserved
	        0xda,//0xda, reserved
	        0xdb,//0xdb, reserved
	        0xdc,//0xdc,reserved
	        0xdd,//0xdd, reserved
	        0xde,//0xde, reserved
	        0xdf,//0xdf, reserved

	        0xe0,//0xe0, reserved
	        0xe1,//0xe1, reserved
	        0xe2,//0xe2, reserved
	        0xe3,//0xe3, reserved
	        0xe4,//0xe4, reserved
	        0xe5,//0xe5, reserved
	        0xe6,//0xe6, reserved
	        0xe7,//0xe7, reserved
	        0xe8,//0xe8, reserved
	        0xe9,//0xe9, reserved
	        0xea,//0xea, reserved
	        0xeb,//0xeb, reserved
	        0xec,//0xec,reserved
	        0xed,//0xed, reserved
	        0xee,//0xee, reserved
	        0xef,//0xef, reserved

	        0xf0,//0xf0,  reserved
	        0xf1,//0xf1,  reserved
	        0xf2,//0xf2,  reserved
	        0xf3,//0xf3,  reserved
	        0xf4,//0xf4,  reserved
	        0xf5,//0xf5,  reserved
	        0xf6,//0xf6,  reserved
	        0xf7,//0xf7,  reserved
	        0xf8,//0xf8,  reserved
	        0xf9,//0xf9,  reserved
	        0xfa,//0xfa,  reserved
	        0xfb,//0xfb,  reserved
	        0xfc,//0xfc, reserved
	        0xfd,//0xfd,  reserved
	        0xfe,//0xfe,  reserved
	        0xff //0xff, reservedr */
};
