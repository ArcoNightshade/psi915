/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 *
 * Clean-room implementation of the bit-banging I2C algorithm from the
 * I2C-bus specification (NXP UM10204): START/STOP/repeated START,
 * MSB-first bytes with ACK, clock stretching (SCL is released and polled
 * until high, up to `timeout` jiffies), `retries` address retries. udelay
 * is the half clock period in microseconds. src/linux_i2c.c.
 */
#ifndef _LINUXKPI_LINUX_I2C_ALGO_BIT_H_
#define	_LINUXKPI_LINUX_I2C_ALGO_BIT_H_

#include <linux/i2c.h>

struct i2c_algo_bit_data {
	void	*data;
	void	(*setsda)(void *data, int state);
	void	(*setscl)(void *data, int state);
	int	(*getsda)(void *data);
	int	(*getscl)(void *data);
	int	(*pre_xfer)(struct i2c_adapter *adap);
	void	(*post_xfer)(struct i2c_adapter *adap);
	int	udelay;
	int	timeout;	/* jiffies */
	bool	can_do_atomic;
};

extern const struct i2c_algorithm i2c_bit_algo;
int i2c_bit_add_bus(struct i2c_adapter *adap);
int i2c_bit_add_numbered_bus(struct i2c_adapter *adap);

#endif /* _LINUXKPI_LINUX_I2C_ALGO_BIT_H_ */
