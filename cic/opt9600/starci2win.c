/*
    StarCI2Win driver for Opticum HD 9600 series.

Source file name : dvb_ca.c
Author :           Pete

Implementation of linux dvb dvr input device

Date        Modification                                    Name
----        ------------                                    --------
01-Nov-06   Created                                         Pete
05-Jan-21   Modified for Opticum HD (TS) 9600               Audioniek

************************************************************************/

#include <linux/module.h>
#include <linux/dvb/audio.h>
#include <linux/dvb/ca.h>

#include <linux/io.h>

#include <linux/delay.h>  // for msleep
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/stm/pio.h>

// --- CI switch support ---
#include "dvb_ca_en50221.h"

#define STARCI2_ADDR         0x40
#define STARCI2_I2C_BUS      0

#define SLOT_STATUS_NONE     0x01
#define SLOT_STATUS_PRESENT  0x02
#define SLOT_STATUS_RESET    0x04
#define SLOT_STATUS_READY    0x08
#define SLOT_STATUS_OCCUPIED (SLOT_STATUS_PRESENT | SLOT_STATUS_RESET | SLOT_STATUS_READY)
#define BANKMEM_OFFSET       0x8000

#define SLOT_RESETS_MAX      5

#define STARCI2_MAX_SLOTS    2

// opt9600 stuff
#define STARCI_INT_PORT      2
#define STARCI_INT_PIN       4
#define STARCI_RESET_PORT    5
#define STARCI_RESET_PIN     5
struct stpio_pin *int_pin;
struct stpio_pin *reset_pin;

static int debug = 0;

static struct starci2_cfg
{
	int inited;
	struct mutex lock;
	int config_addr;
	struct dvb_ca_en50221 ca;
	struct
	{
		int mode;
		unsigned char input[STARCI2_MAX_SLOTS];
		unsigned char output[STARCI2_MAX_SLOTS];
	} slot;
	struct
	{
		unsigned long config;
		unsigned long buffer;
		unsigned long bank2;
	} reg;
	unsigned char slot_state[STARCI2_MAX_SLOTS];
	unsigned char slot_resets[STARCI2_MAX_SLOTS];  // unused, yet (should hold max resets per insertion)
	unsigned long detection_timeout[STARCI2_MAX_SLOTS];
	struct i2c_adapter *i2c;
} cicam;

/* StarCI2Win register definitions */
#define MODA_CTRL_REG         0x00
#define MODA_MASK_HIGH_REG    0x01
#define MODA_MASK_LOW_REG     0x02
#define MODA_PATTERN_HIGH_REG 0x03
#define MODA_PATTERN_LOW_REG  0x04
#define MODA_TIMING_REG       0x05
#define MODB_CTRL_REG         0x09
#define MODB_MASK_HIGH_REG    0x0a
#define MODB_MASK_LOW_REG     0x0b
#define MODB_PATTERN_HIGH_REG 0x0c
#define MODB_PATTERN_LOW_REG  0x0d
#define MODB_TIMING_REG       0x0e
#define SINGLE_MODE_CTRL_REG  0x10
#define TWIN_MODE_CTRL_REG    0x11
#define DEST_SEL_REG          0x17
#define INT_STATUS_REG        0x1a
#define INT_MASK_REG          0x1b
#define INT_CONFIG_REG        0x1c
#define STARCI_CTRL_REG       0x1f

static int starci2_writereg(int reg, int data)
{
	u8 buf[] = { reg, data };
	struct i2c_msg msg = { .addr = cicam.config_addr, .flags = 0, .buf = buf, .len = 2 };
	int err;

	if (debug > 1)
	{
		printk("[starciwin] %s: write reg 0x%02x, value 0x%02x\n", __func__, reg, data);
	}
	if ((err = i2c_transfer(cicam.i2c, &msg, 1)) != 1)
	{
		printk("[starciwin] %s: writereg error(err == %i, reg == 0x%02x, data == 0x%02x)\n", __func__, err, reg, data);
		return -EREMOTEIO;
	}
	return 0;
}

static int starci2_readreg(u8 reg)
{
	int ret;
	u8 b0[] = { reg };
	u8 b1[] = { 0 };
	struct i2c_msg msg[] =
	{
		{ .addr = cicam.config_addr, .flags = 0, .buf = b0, .len = 1 },
		{ .addr = cicam.config_addr, .flags = I2C_M_RD, .buf = b1, .len = 1 }
	};

    ret = i2c_transfer(cicam.i2c, msg, 2);

	if (ret != 2)
	{
		printk("[starciwin] %s: reg=0x%x (error=%d)\n", __func__, reg, ret);
		return -EREMOTEIO;
	}
	if (debug > 1)
	{
		printk("[starciwin] %s read reg 0x%02x, value 0x%02x\n",__func__, reg, b1[0]);
	}
	return b1[0];
}

//
// external module API follows...
//

int ciintf_detect(void)
{
	struct i2c_adapter *i2c;
	int val;
	u8 b0[] = { 0x00 };  // MOD-A CTRL
	u8 b1[] = { 0 };
	struct i2c_msg msg[] =
	{
		{ .addr = STARCI2_ADDR, .flags = 0, .buf = b0, .len = 1 },
		{ .addr = STARCI2_ADDR, .flags = I2C_M_RD, .buf = b1, .len = 1 }
	};

	if ((i2c = i2c_get_adapter(STARCI2_I2C_BUS)) == NULL)
	{
		printk("[starciwin] %s: Error: Cannot find i2c adapter #%d\n", __func__, STARCI2_I2C_BUS);
		return -ENODEV;
	}
	val = i2c_transfer(i2c, msg, 2);

	if (val < 0)
	{
		return val;
	}
	if (val != 2)
	{
		return -ENODEV;
	}
	printk("[starciwin] %s: found supported device on I2C bus %d, address 0x%02x\n", __func__, STARCI2_I2C_BUS, STARCI2_ADDR);
	return 0;
}

int ciintf_set_twin_mode(int on, int locked)
{
	int rv = 0;

	if ((on && cicam.slot.mode) || (!on && !cicam.slot.mode))
	{
		return 0;
	}
	if (!locked)
	{
		mutex_lock(&cicam.lock);
	}
	if (on)
	{
		// TWIN MODE
		cicam.slot.mode = 1;
		cicam.slot.input[0] = 0;  // 0 = tuner-A, 1 = tuner-B, 2 = ci-A, 3 = ci-B
		cicam.slot.input[1] = 1;
		cicam.slot.output[0] = 0;
		cicam.slot.output[1] = 1;
		starci2_writereg(0x10, 0x00);  // TS PATH
		starci2_writereg(0x11, 0x80);  // TS PATH: TWIN ON, TSI1->1st TSI, TSI2->2nd TSI
		printk("[starciwin] CICAM: TWIN mode set\n");
	}
	else
	{
		// SINGLE MODE
		cicam.slot.mode = 0;
		cicam.slot.input[0] = 0;
		cicam.slot.input[1] = 2;
		cicam.slot.output[0] = 2;
		cicam.slot.output[1] = 1;
		starci2_writereg(0x10, 0x00);  // TS PATH
		starci2_writereg(0x11, 0x00);  // TS PATH: TWIN OFF
		printk("[starciwin] CICAM: SINGLE mode set\n");
	}

	if (!locked)
	{
		mutex_unlock(&cicam.lock);
	}
	return rv;
}

int ciintf_init(struct dvb_ca_en50221 *ca)
{
	int i;
	unsigned long res;

	if (cicam.inited)
	{
		// expect this is second, of init => second device
		return 0;
	}
    cicam.inited = 1;

	mutex_init(&cicam.lock);
	mutex_lock(&cicam.lock);

	if ((cicam.i2c = i2c_get_adapter(STARCI2_I2C_BUS)) == NULL)
	{
		printk("[starciwin] %s: Error: Cannot find I2C adapter #0\n", __func__);
		return -1;
	}
	cicam.config_addr = STARCI2_ADDR;
	for (i = 0; i < STARCI2_MAX_SLOTS; i++)
	{
		cicam.slot_state[i] = SLOT_STATUS_NONE;
		cicam.slot_resets[i] = SLOT_RESETS_MAX;
		cicam.detection_timeout[i] = 0;
	}

	starci2_writereg(0x1F, 0x80);  // RESET ON
	msleep(10);  //FIXME: better value?
	starci2_writereg(0x1F, 0x00);  // RESET OFF

	starci2_writereg(0x01, 0x00);  // MOD-A AUTOSELECT MSK HI
	starci2_writereg(0x02, 0x01);  // MOD-A AUTOSELECT MSK LO
	starci2_writereg(0x03, 0x00);  // MOD-A AUTOSELECT PATT HI
	starci2_writereg(0x04, 0x00);  // MOD-A AUTOSELECT PATT LO

	starci2_writereg(0x0A, 0x00);  // MOD-B AUTOSELECT MSK HI
	starci2_writereg(0x0B, 0x01);  // MOD-B AUTOSELECT MSK LO
	starci2_writereg(0x0C, 0x00);  // MOD-B AUTOSELECT PATT HI
	starci2_writereg(0x0D, 0x01);  // MOD-B AUTOSELECT PATT LO

	starci2_writereg(0x17, 0x01);  // SET AUTO MODULE SELECT
	starci2_writereg(0x18, 0x00);  // SET PINS

	starci2_writereg(0x05, 0x40);  // MOD-A AM,CM TIMING

	starci2_writereg(0x0E, 0x40);  // MOD-B AM,CM TIMING

	starci2_writereg(0x1B, 0x00);  // MASK ALL INTERRUPTS
	starci2_writereg(0x1D, 0x01);  // RD,DIR
	starci2_writereg(0x1E, 0x03);  // WAIT BUFF

    cicam.slot.mode = 0;  // faked to force processing on next line
	ciintf_set_twin_mode(1, 1);

	starci2_writereg(0x1F, 0x01);  // LOCK CONFIG

	//starci2_writereg(0x00, 0x00);  // MOD-A CTRL
	starci2_writereg(0x00, 0x02);  // MOD-A CTRL, AUTOACTIVATION ON

	starci2_writereg(0x09, 0x02);  // MOD-B CTRL, AUTOACTIVATION ON

	starci2_writereg(0x18, 0x21);  // POWER ON

	//
	// --- remapping stuffs ---
	//
#define EMIConfigBaseAddress (0x1A100000L)  // TODO
#define EMIBufferBaseAddress (EMIConfigBaseAddress + 0x800)  // TODO

	cicam.reg.config = (unsigned long)ioremap(EMIConfigBaseAddress, 0x800);  // 0x7ff
	printk("[starciwin] EMICfg %lX -> %lX\n", EMIConfigBaseAddress, cicam.reg.config);
	cicam.reg.buffer = (unsigned long)ioremap(EMIBufferBaseAddress, 0x40);

	writel(0x03, cicam.reg.config + 0x860);  //  bank enable, bank0-2

	writel(0x00, cicam.reg.config + 0x18);  // EMI_STA_LCK : unlock
	writel(0x00, cicam.reg.config + 0x20);  // EMI_LCK : unlock

	// bank 2 config

	/* update BANK as port 8bit device type */
	//BUSRELEASETIME:2, CSACTIVE:active R/W,OEACTIVE:active read, BEACTIVE:active R/W, PORTSIZE:8bit, DEVICETYPE: normal
	writel(0x000016f9, cicam.reg.config + 0x100 + 0x40 * 2 + 0x08 * 0);  // EMI_CFG_DATA0 : port size = 8bit
	//cimax 600ns ????????. read/write bank config

	// 1. read access
	// [30:24] ACCESSTIMEREAD:28cycles
	// [23:20],[19:16] CSE1TIMEREAD:falling(3cycles), rising(1cycle)
	// [15:12],[11:8] OEE1TIMEREAD:falling(3cycles), rising(1cycle)
	writel(0x9d313131, cicam.reg.config + 0x100 + 0x40 * 2 + 0x08 * 1);  // EMI_CFG_DATA1 : port size = 8bit

	// 1. write access
	// [30:24] ACCESS_TIME_WRITE:16cycles
	// [23:20],[19:16] CSE1_TIME_WRITE:falling(2cycles), rising(1cycle)
	// [15:12],[11:8] OEE1_TIME_WRITE:falling(2cycles), rising(1cycle) --> WRITE NOT USE
	writel(0x90212100, cicam.reg.config + 0x100 + 0x40 * 2 + 0x08 * 2);  // EMI_CFG_DATA2 : port size = 8bit

	writel(0x00000000, cicam.reg.config + 0x100 + 0x40 * 2 + 0x08 * 3);  // EMI_CFG_DATA3 : port size = 8bit

	writel(0x04, cicam.reg.config + 0x10);  // EMI_STA_CFG : update
	writel(0x1f, cicam.reg.config + 0x18);  // EMI_STA_LCK : lock
	writel(0x1f, cicam.reg.config + 0x20);  // EMI_LCK : lock

	// bank2
	res = ctrl_inl(cicam.reg.config + 0x0800 + 0x20);  // bank2
	res <<= 22;
	cicam.reg.bank2 = (unsigned long)ioremap(res, 0x400000);
	printk("[starciwin] CI bank2 mapped %lX -> %lX\n", res, cicam.reg.bank2);

	// CI power
	{
		struct stpio_pin *pwr = NULL;
		if ((pwr = stpio_request_pin(5, 4, "CIpwr1", STPIO_OUT)) == NULL)
		{
			printk("[starciwin] ERR: Cannot acquire CI power pin PIO 5.4!\n");
		}
		else
		{
			printk("[starciwin] CI power pin PIO 5.4 acquired OK\n");
			stpio_set_pin(pwr, 1);
		}
		if ((pwr = stpio_request_pin(4, 7, "CIpwr2", STPIO_OUT)) == NULL)
		{
			printk("[starciwin] ERR: Cannot acquire CI power pin PIO 4.7!\n");
		}
		else
		{
			printk("[starciwin] CI power pin PIO 4.7 acquired OK\n");
			stpio_set_pin(pwr, 1);
		}
	}
	mutex_unlock(&cicam.lock);
	return 0;
}

/* NOTE: the read_*, write_* and poll_slot_status functions will be
 * called for different slots concurrently and need to use locks where
 * and if appropriate. There will be no concurrent access to one slot.
 */

static int ciintf_read_attribute_mem_locked(struct dvb_ca_en50221 *ca, int slot, int address)
{
	int val, res = 0;
	int real_slot = slot;

	val = starci2_readreg(real_slot == 0 ? 0x00 : 0x09);  // MOD-A CTRL
	if (val < 0)
	{
		return 0;
	}
	if ((val & 0x01) == 0)
	{  // 0 = no module detected
		return 0;
	}
#if 0
	if (val & 0x20) // mpeg control active
	{
		return 0;
	}
#endif
	starci2_writereg(real_slot == 0 ? 0x00 : 0x09, val & 0xF3);  // MOD-A CTRL: access to attribute mem
	res = ctrl_inb(cicam.reg.bank2 + (BANKMEM_OFFSET * real_slot) + (address << 0));
	// res &= 0xFF;

	if (address <= 2)
	{
		printk("[starciwin] ATTRMEM#%d: address = %lx(%u): res = %x\n", real_slot, cicam.reg.bank2 + (BANKMEM_OFFSET * real_slot) + (address << 1), address, res);
	}
	else
	{
		if (res > 31 && res < 127)
		{
			printk("%c", res);
		}
		else
		{
			printk(".");
		}
	}
	return res;
}

int ciintf_read_attribute_mem(struct dvb_ca_en50221 *ca, int slot, int address)
{
	int res;

	mutex_lock(&cicam.lock);
	res = ciintf_read_attribute_mem_locked(ca, slot, address);
	mutex_unlock(&cicam.lock);

	return res;
}

int ciintf_write_attribute_mem(struct dvb_ca_en50221 *ca, int slot, int address, u8 value)
{
	int val;
	int real_slot = slot;

	mutex_lock(&cicam.lock);

	val = starci2_readreg(real_slot == 0 ? 0x00 : 0x09);  // MOD-A CTRL
	if (val < 0)
	{
		mutex_unlock(&cicam.lock);
		return 0;
	}
	if ((val & 0x01) == 0)
	{  // 0 = no module detected
		mutex_unlock(&cicam.lock);
		return 0;
	}
#if 0
	if (val & 0x20)  // mpeg control active
	{
		return 0;
	}
#endif
	starci2_writereg(real_slot == 0 ? 0x00 : 0x09, 0x63);  // MOD-A CTRL: access to attribute mem
	ctrl_inb(cicam.reg.bank2  + (BANKMEM_OFFSET * real_slot) + (address << 0));

	ctrl_outb(value, cicam.reg.bank2  + (BANKMEM_OFFSET * real_slot) + (address << 0));

	mutex_unlock(&cicam.lock);
	return 0;
}

int ciintf_read_cam_control(struct dvb_ca_en50221 *ca, int slot, u8 address)
{
	int val, res = 0;
	int real_slot = slot;

	mutex_lock(&cicam.lock);

	val = starci2_readreg(real_slot == 0 ? 0x00 : 0x09);  // MOD-A CTRL
	if (val < 0)
	{
		mutex_unlock(&cicam.lock);
		return 0;
	}
	if ((val & 0x01) == 0)
	{  // 0 = no module detected
		mutex_unlock(&cicam.lock);
		return 0;
	}
#if 0
	if(val & 0x20) // mpeg control active
	{
		return 0;
	}
#endif
	starci2_writereg(real_slot == 0 ? 0x00 : 0x09, (val & 0xF3) | 0x04);  // MOD-A CTRL: access to i/o mem
	res = ctrl_inb(cicam.reg.bank2  + (BANKMEM_OFFSET * real_slot) + (address << 0));
	res &= 0xFF;

	mutex_unlock(&cicam.lock);
	return res;
}

int ciintf_write_cam_control(struct dvb_ca_en50221 *ca, int slot, u8 address, u8 value)
{
	int val;
	int real_slot = slot;

	//printk("[starciwin] [%s]\n", __func__);

	mutex_lock(&cicam.lock);

	val = starci2_readreg(real_slot == 0 ? 0x00 : 0x09);  // MOD-A CTRL
	if (val < 0)
	{
		mutex_unlock(&cicam.lock);
		return 0;
	}
	if ((val & 0x01) == 0)
	{  // 0 = no module detected
		mutex_unlock(&cicam.lock);
		return 0;
	}
#if 0
	if(val & 0x20) // mpeg control active
	{
		return 0;
	}
#endif
	starci2_writereg(real_slot == 0 ? 0x00 : 0x09, (val & 0xF3) | 0x04);  // MOD-A CTRL: access to i/o mem
	ctrl_outb(value, cicam.reg.bank2  + (BANKMEM_OFFSET * real_slot) + (address << 0));

	mutex_unlock(&cicam.lock);
	return 0;
}

int ciintf_slot_reset(struct dvb_ca_en50221 *ca, int slot)
{
	int res;

	printk("[starciwin] [%s: slot %d]\n", __func__, slot);

	mutex_lock(&cicam.lock);

	res = starci2_readreg(slot == 0 ? 0x00 : 0x09);
	if (res & 0x01)
	{
		starci2_writereg(slot == 0 ? 0x00 : 0x09, res | 0x80);  // MOD-A CTRL
		msleep(60);
		printk("[starciwin] RESET module %d\n", slot);
		starci2_writereg(slot == 0 ? 0x00 : 0x09, 0x02);  // MOD-A CTRL
		cicam.slot_state[slot] = SLOT_STATUS_NONE;
		cicam.detection_timeout[slot] = 0;
	}
	mutex_unlock(&cicam.lock);
	return 0;
}

int ciintf_slot_shutdown(struct dvb_ca_en50221 *ca, int slot)
{
	printk("[starciwin] [%s] slot %d\n", __func__, slot);
	return 0;
}

int ciintf_slot_ts_enable(struct dvb_ca_en50221 *ca, int slot)
{
	int real_slot = slot;

	mutex_lock(&cicam.lock);

	starci2_writereg(real_slot == 0 ? 0x00 : 0x09, 0x62);  // MPEG on, no bypass
	printk("[starciwin] [%s] slot %d\n", __func__, real_slot);

	mutex_unlock(&cicam.lock);
	return 0;
}

int ciintf_poll_slot_status(struct dvb_ca_en50221 *ca, int slot, int open)
{
	int debug_ori;
	int slot_status = 0;

	//printk("[starciwin] [%s]\n", __func__);

	if ((slot < 0) || (slot > 1))
	{
		return 0;
	}
	mutex_lock(&cicam.lock);

	debug_ori = debug;
	debug = 0; // DISABLE DEBUG
	slot_status = starci2_readreg(slot == 0 ? 0x00 : 0x09) & 0x01;  // MOD-A CTRL
	debug = debug_ori;

	if (slot_status < 0)
	{
		mutex_unlock(&cicam.lock);
		return 0;
	}
	if (slot_status == 1)  // 1 = module detected
	{
	  /* Phantomias: an insertion should not be reported immediately
		 because the module needs time to power up. Therefore the
		 detection is reported after the module has been present for
		 the specified period of time (to be confirmed in tests). */
		if (cicam.slot_state[slot] & SLOT_STATUS_NONE)
		{
			if (cicam.detection_timeout[slot] == 0)
			{
				/* detected module insertion, set the detection
				timeout (500 ms) */
				cicam.detection_timeout[slot] = jiffies + HZ/2;
			}
			else
			{
				/* timeout in progress */
				if (time_after(jiffies, cicam.detection_timeout[slot]))
				{
					/* timeout expired, report module present */
					cicam.slot_state[slot] = SLOT_STATUS_RESET;
				}
			}
		}
		// during a RESET, we check if we can read from IO memory to see when CAM is ready
		if (cicam.slot_state[slot] & SLOT_STATUS_RESET)
		{
			if (ciintf_read_attribute_mem_locked(ca, slot, 0) == 0x1d)
			{
				printk("[starciwin] DEB: >>> slot %d: DVB_CA_EN50221_POLL_CAM_READY\n", slot);
				cicam.slot_state[slot] = SLOT_STATUS_READY;
			}
			else
			{
				cicam.slot_state[slot] = SLOT_STATUS_NONE;
				cicam.detection_timeout[slot] = 0;
			}
		}
	}
	else
	{
		cicam.slot_state[slot] = SLOT_STATUS_NONE;
		cicam.detection_timeout[slot] = 0;
	}
	slot_status = slot_status ? DVB_CA_EN50221_POLL_CAM_PRESENT : 0;
	if (cicam.slot_state[slot] & SLOT_STATUS_READY)
	{
		slot_status |= DVB_CA_EN50221_POLL_CAM_READY;
	}
	/*printk("[starciwin] Module %d: present = %d, ready = %d\n",
			  slot, slot_status & DVB_CA_EN50221_POLL_CAM_PRESENT,
			  slot_status & DVB_CA_EN50221_POLL_CAM_READY);*/
	mutex_unlock(&cicam.lock);
	return slot_status;
}

/* This function sets the CI source
   Params:
     slot - CI slot number (0|1)
     source - tuner number (0|1) or other CI slot (2|3)
*/
int setInputSource(int pti, int source)
{
	printk("[starciwin] setInputSource (%d , %d)\n", pti, source);
	if ((pti < 0) || (pti > 1) || (source < 0) || (source > 3))
	{
		return -1;
	}
	if (pti == 0)
	{
		switch (source)
		{
			case 0:
			{
				printk("[starciwin] [%s] TUNER_A->TSIN0\n", __func__);
				break;
			}
			case 1:
			{
				printk("[starciwin] [%s] TUNER_B->TSIN0\n", __func__);
				break;
			}
			case 2:
			{
				printk("[starciwin] [%s] CI0->TSIN0\n", __func__);
				break;
			}
			case 3:
			{
				printk("[starciwin] [%s] CI1->TSIN0\n", __func__);
				break;
			}
		}
	}
	else
	{
		switch (source)
		{
			case 0:
			{
				printk("[starciwin] [%s] TUNER_A->TSIN1\n", __func__);
				break;
			}
			case 1:
			{
				printk("[starciwin] [%s] TUNER_B->TSIN1\n", __func__);
				break;
			}
			case 2:
			{
				printk("[starciwin] [%s] CI0->TSIN1\n", __func__);
				break;
			}
			case 3:
			{
				printk("[starciwin] [%s] CI1->TSIN1\n", __func__);
				break;
			}
		}
	}
	return 0;
}
EXPORT_SYMBOL(setInputSource);

/* This function gets the CI source
   Params:
     slot - CI slot number (0|1)
     source - tuner number (0|1)
*/
void getCiSource(int slot, int* source)
{
	int val;

	val = starci2_readreg(TWIN_MODE_CTRL_REG);
	val &= 0x20;

	if (slot == 0)
	{
		if (val != 0)
		{
			*source = 0;
		}
		else
		{
			*source = 1;
		}
	}
	if (slot == 1)
	{
		if (val != 0)
		{
			*source = 1;
		}
		else
		{
			*source = 0;
		}
	}
}
EXPORT_SYMBOL(getCiSource);

int setCiSource(int slot, int source)
{
	int val;

	printk("[starciwin] %s (%d, %d)\n", __func__, slot, source);
	if ((slot < 0) || (slot > 1)  /* invalid slot */
	||  (source > 3)  /* invalid source */
	||  (slot == 0 && source == 2)  /* nonsense */
	||  (slot == 1 && source == 3))  /* nonsense */
	{
		return -1;
	}
	val = starci2_readreg(0x11);  // TWIN MODE TS CONTROL REG
	if (val < 0)
	{
		return 0;
	}
   	switch (source)
	{
		case -1:
		{
			printk("[starciwin] [%s] CI%d->bypass\n",__func__, slot);
			val |= slot == 0 ? 0x10 : 0x08;
			starci2_writereg(0x11, val);
			break;
		}
		case 0:
		case 1:
		{
			printk("[starciwin] [%s] TUNER_%c -> CI%d\n",__func__, source ? 'B' : 'A', slot);
			if ((slot == source && (val & 0x20))
			||  (slot != source && (val & 0x20) == 0))
			{
				val ^= 0x20;
			}
			val &= ~(slot == 0 ? 0x10 : 0x08);  // disable bypass again
			printk("[starciwin] setCiSource: new ctrlx11 = 0x%x\n", val);
			starci2_writereg(0x11, val);
			break;
		}
		case 2:
		{
			printk("[starciwin] [%s] CI0->CI%d\n",__func__, slot);
			break;
		}
		case 3:
		{
			printk("[starciwin] [%s] CI1->CI%d\n",__func__, slot);
			break;
		}
  	}
	return 0;
}
EXPORT_SYMBOL(setCiSource);

int init_ci_controller (struct dvb_adapter* dvb_adap)
{
	// --- CI switch support ---
	int result;

	// allocate used PIO pins
	printk("[starciwin] Initialize PIO %d.%d (STARCI_INT) to input\n", STARCI_INT_PORT, STARCI_INT_PIN);
	int_pin = stpio_request_pin(STARCI_INT_PORT, STARCI_INT_PIN, "STARCI_INT", STPIO_IN);
	if (int_pin == NULL)
	{
		printk("[starciwin] Request for STPIO STARCI_INT failed; abort\n");
		goto pio_init_fail;
	}
	printk("[starciwin] Initialize PIO %d.%d (STARCI_RESET) to 0\n", STARCI_RESET_PORT, STARCI_RESET_PIN);
	reset_pin = stpio_request_pin(STARCI_RESET_PORT, STARCI_RESET_PIN, "STARCI_RESET", STPIO_OUT);
	if (reset_pin == NULL)
	{
		printk("[starciwin]Request for STPIO STARCI_RESET failed; abort\n");
		goto pio_init_fail;
	}

	// reset STARCI2WIN
	stpio_set_pin(reset_pin, 1);  // set reset to active
	msleep(100);
	stpio_set_pin(reset_pin, 0);  // set reset to inactive
	msleep(100);

	// do autodetect stuff
	if (ciintf_detect())
	{
		printk("[starciwin] StarCI2Win chip not detected.\n");
	}
	else
	{
		// register CI interface
		memset(&cicam.ca, 0, sizeof(struct dvb_ca_en50221));
		cicam.ca.owner = THIS_MODULE;
		cicam.ca.read_attribute_mem = ciintf_read_attribute_mem;
		cicam.ca.write_attribute_mem = ciintf_write_attribute_mem;
		cicam.ca.read_cam_control = ciintf_read_cam_control;
		cicam.ca.write_cam_control = ciintf_write_cam_control;
		cicam.ca.slot_reset = ciintf_slot_reset;
		cicam.ca.slot_shutdown = ciintf_slot_shutdown;
		cicam.ca.slot_ts_enable = ciintf_slot_ts_enable;
		cicam.ca.poll_slot_status = ciintf_poll_slot_status;
		cicam.ca.data = NULL;

		result = dvb_ca_en50221_init(dvb_adap, &cicam.ca, 0, 2);

		if (result != 0)
		{
			printk("[starciwin] CICAM detected, but registration failed (errno = %d).\n", result);
			//goto error;
			return result;
		}
		else
		{
			// init cicam
			result = ciintf_init(&cicam.ca);
			if (result != 0)
			{
				printk("[starciwin] CICAM initialisation failed (errno = %d).\n", result);
				return result;
			}
			else
			{
				printk("[starciwin] CICAM initialisation successful.\n");
			}
		}
	}  // autodetect
	return 0;

pio_init_fail:
	stpio_free_pin(int_pin);
	stpio_free_pin(reset_pin);
	printk("[starciwin] Initializing PIO pins failed.\n");
	return -1;
}
EXPORT_SYMBOL(init_ci_controller);

int starci2win_init(void)
{
	printk("[starciwin] starci2win loaded\n");
	return 0;
}

void starci2win_exit(void)
{
	printk("[starciwin] starci2win unloaded\n");
}

module_init(starci2win_init);
module_exit(starci2win_exit);

MODULE_DESCRIPTION("CI Controller");
MODULE_AUTHOR("");
MODULE_LICENSE("GPL");
// vim:ts=4
