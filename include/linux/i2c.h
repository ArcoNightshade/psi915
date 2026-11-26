/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of the Linux I2C core API used by DRM
 * (DDC/EDID, DP AUX-over-I2C) and i915 (GMBUS) from its documented
 * behaviour (Documentation/i2c/); function set cross-checked against
 * FreeBSD's LinuxKPI i2c.h (BSD-2-Clause). Adapters are KPI objects only:
 * there is no /dev/i2c and no illumos I2C framework binding.
 * i2c_transfer() takes the adapter's bus lock (a mutex, or the adapter's
 * lock_ops), retries -EAGAIN adap->retries times and calls the
 * algorithm (src/linux_i2c.c).
 */
#ifndef _LINUXKPI_LINUX_I2C_H_
#define	_LINUXKPI_LINUX_I2C_H_

#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/errno.h>

#define	I2C_M_RD		0x0001
#define	I2C_M_TEN		0x0010
#define	I2C_M_DMA_SAFE		0x0200
#define	I2C_M_RECV_LEN		0x0400
#define	I2C_M_NO_RD_ACK		0x0800
#define	I2C_M_IGNORE_NAK	0x1000
#define	I2C_M_REV_DIR_ADDR	0x2000
#define	I2C_M_NOSTART		0x4000
#define	I2C_M_STOP		0x8000

#define	I2C_FUNC_I2C			0x00000001
#define	I2C_FUNC_10BIT_ADDR		0x00000002
#define	I2C_FUNC_PROTOCOL_MANGLING	0x00000004
#define	I2C_FUNC_SMBUS_PEC		0x00000008
#define	I2C_FUNC_NOSTART		0x00000010
#define	I2C_FUNC_SMBUS_BLOCK_PROC_CALL	0x00008000
#define	I2C_FUNC_SMBUS_QUICK		0x00010000
#define	I2C_FUNC_SMBUS_READ_BYTE	0x00020000
#define	I2C_FUNC_SMBUS_WRITE_BYTE	0x00040000
#define	I2C_FUNC_SMBUS_READ_BYTE_DATA	0x00080000
#define	I2C_FUNC_SMBUS_WRITE_BYTE_DATA	0x00100000
#define	I2C_FUNC_SMBUS_READ_WORD_DATA	0x00200000
#define	I2C_FUNC_SMBUS_WRITE_WORD_DATA	0x00400000
#define	I2C_FUNC_SMBUS_READ_BLOCK_DATA	0x01000000
#define	I2C_FUNC_SMBUS_WRITE_BLOCK_DATA	0x02000000
#define	I2C_FUNC_SMBUS_READ_I2C_BLOCK	0x04000000
#define	I2C_FUNC_SMBUS_WRITE_I2C_BLOCK	0x08000000
#define	I2C_FUNC_SMBUS_BYTE						\
	(I2C_FUNC_SMBUS_READ_BYTE | I2C_FUNC_SMBUS_WRITE_BYTE)
#define	I2C_FUNC_SMBUS_BYTE_DATA					\
	(I2C_FUNC_SMBUS_READ_BYTE_DATA | I2C_FUNC_SMBUS_WRITE_BYTE_DATA)
#define	I2C_FUNC_SMBUS_WORD_DATA					\
	(I2C_FUNC_SMBUS_READ_WORD_DATA | I2C_FUNC_SMBUS_WRITE_WORD_DATA)
#define	I2C_FUNC_SMBUS_I2C_BLOCK					\
	(I2C_FUNC_SMBUS_READ_I2C_BLOCK | I2C_FUNC_SMBUS_WRITE_I2C_BLOCK)
#define	I2C_FUNC_SMBUS_EMUL						\
	(I2C_FUNC_SMBUS_QUICK | I2C_FUNC_SMBUS_BYTE |			\
	    I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_WORD_DATA |	\
	    I2C_FUNC_SMBUS_I2C_BLOCK | I2C_FUNC_SMBUS_WRITE_BLOCK_DATA)

#define	I2C_CLASS_HWMON		(1 << 0)
#define	I2C_CLASS_DDC		(1 << 3)
#define	I2C_CLASS_SPD		(1 << 7)
#define	I2C_NAME_SIZE		20
#define	I2C_SMBUS_BLOCK_MAX	32

#define	I2C_LOCK_ROOT_ADAPTER	0x01
#define	I2C_LOCK_SEGMENT	0x02

struct i2c_msg {
	u16	addr;
	u16	flags;
	u16	len;
	u8	*buf;
};

struct i2c_adapter;

struct i2c_algorithm {
	union {
		int	(*xfer)(struct i2c_adapter *adap, struct i2c_msg *msgs,
			    int num);
		int	(*master_xfer)(struct i2c_adapter *adap,
			    struct i2c_msg *msgs, int num);
	};
	union {
		int	(*xfer_atomic)(struct i2c_adapter *adap,
			    struct i2c_msg *msgs, int num);
		int	(*master_xfer_atomic)(struct i2c_adapter *adap,
			    struct i2c_msg *msgs, int num);
	};
	int	(*smbus_xfer)(struct i2c_adapter *adap, u16 addr,
		    unsigned short flags, char read_write, u8 command,
		    int size, void *data);
	u32	(*functionality)(struct i2c_adapter *adap);
};

struct i2c_lock_operations {
	void	(*lock_bus)(struct i2c_adapter *adap, unsigned int flags);
	int	(*trylock_bus)(struct i2c_adapter *adap, unsigned int flags);
	void	(*unlock_bus)(struct i2c_adapter *adap, unsigned int flags);
};

struct i2c_adapter_quirks {
	u64	flags;
	int	max_num_msgs;
	u16	max_write_len;
	u16	max_read_len;
	u16	max_comb_1st_msg_len;
	u16	max_comb_2nd_msg_len;
};

struct i2c_adapter {
	struct lkpi_module		*owner;
	unsigned int			class;
	const struct i2c_algorithm	*algo;
	void				*algo_data;
	const struct i2c_lock_operations *lock_ops;
	struct mutex			bus_lock;
	int				timeout;	/* jiffies */
	int				retries;
	struct device			dev;
	int				nr;
	char				name[48];
	const struct i2c_adapter_quirks	*quirks;
	bool				lkpi_registered;
};

#define	to_i2c_adapter(d)	container_of(d, struct i2c_adapter, dev)

struct i2c_client {
	unsigned short		flags;
	unsigned short		addr;
	char			name[I2C_NAME_SIZE];
	struct i2c_adapter	*adapter;
	struct device		dev;
	int			irq;
};

struct i2c_board_info {
	char		type[I2C_NAME_SIZE];
	unsigned short	flags;
	unsigned short	addr;
	const char	*dev_name;
	void		*platform_data;
	int		irq;
};

#define	I2C_BOARD_INFO(dev_type, dev_addr)				\
	.type = dev_type, .addr = (dev_addr)

static inline void *
i2c_get_adapdata(const struct i2c_adapter *adap)
{
	return (dev_get_drvdata(&adap->dev));
}

static inline void
i2c_set_adapdata(struct i2c_adapter *adap, void *data)
{
	dev_set_drvdata(&adap->dev, data);
}

static inline int
i2c_adapter_id(struct i2c_adapter *adap)
{
	return (adap->nr);
}

static inline u32
i2c_get_functionality(struct i2c_adapter *adap)
{
	return (adap->algo->functionality != NULL ?
	    adap->algo->functionality(adap) : I2C_FUNC_I2C);
}

static inline int
i2c_check_functionality(struct i2c_adapter *adap, u32 func)
{
	return ((func & i2c_get_functionality(adap)) == func);
}

static inline u8
i2c_8bit_addr_from_msg(const struct i2c_msg *msg)
{
	return ((u8)((msg->addr << 1) | (msg->flags & I2C_M_RD)));
}

int i2c_add_adapter(struct i2c_adapter *adap);
int i2c_add_numbered_adapter(struct i2c_adapter *adap);
void i2c_del_adapter(struct i2c_adapter *adap);
int i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);
int __i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);
void i2c_lock_bus(struct i2c_adapter *adap, unsigned int flags);
int i2c_trylock_bus(struct i2c_adapter *adap, unsigned int flags);
void i2c_unlock_bus(struct i2c_adapter *adap, unsigned int flags);
int i2c_master_send(const struct i2c_client *client, const char *buf,
    int count);
int i2c_master_recv(const struct i2c_client *client, char *buf, int count);
s32 i2c_smbus_read_byte_data(const struct i2c_client *client, u8 command);
s32 i2c_smbus_write_byte_data(const struct i2c_client *client, u8 command,
    u8 value);
s32 i2c_smbus_read_i2c_block_data(const struct i2c_client *client,
    u8 command, u8 length, u8 *values);
struct i2c_client *i2c_new_client_device(struct i2c_adapter *adap,
    const struct i2c_board_info *info);
void i2c_unregister_device(struct i2c_client *client);
struct i2c_adapter *i2c_get_adapter(int nr);
void i2c_put_adapter(struct i2c_adapter *adap);
#define	i2c_mark_adapter_suspended(a)	do { (void)(a); } while (0)
#define	i2c_mark_adapter_resumed(a)	do { (void)(a); } while (0)
#define	i2c_verify_client(d)		((struct i2c_client *)NULL)

#endif /* _LINUXKPI_LINUX_I2C_H_ */
