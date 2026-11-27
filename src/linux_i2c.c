/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The psi915 Authors. All rights reserved.
 * See the LICENSE file at the top of the source tree for the full text.
 */

/*
 * I2C core and the bit-banging algorithm (see <linux/i2c.h>,
 * <linux/i2c-algo-bit.h>). Clean-room, from the I2C-bus specification.
 */

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/idr.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/jiffies.h>

static DEFINE_IDR(lkpi_i2c_idr);
static DEFINE_MUTEX(lkpi_i2c_lock);

static void
lkpi_i2c_adapter_release(struct device *dev)
{
	(void) dev;
}

static int
lkpi_i2c_register(struct i2c_adapter *adap, int nr)
{
	int id;

	mutex_init(&adap->bus_lock);
	if (adap->timeout == 0)
		adap->timeout = (int)msecs_to_jiffies(1000);
	mutex_lock(&lkpi_i2c_lock);
	id = idr_alloc(&lkpi_i2c_idr, adap, nr < 0 ? 0 : nr,
	    nr < 0 ? 0 : nr + 1, GFP_KERNEL);
	mutex_unlock(&lkpi_i2c_lock);
	if (id < 0)
		return (id);
	adap->nr = id;
	if (!adap->dev.lkpi_initialized)
		device_initialize(&adap->dev);
	if (adap->dev.release == NULL)
		adap->dev.release = lkpi_i2c_adapter_release;
	(void) dev_set_name(&adap->dev, "i2c-%d", id);
	adap->lkpi_registered = true;
	return (0);
}

int
i2c_add_adapter(struct i2c_adapter *adap)
{
	return (lkpi_i2c_register(adap, -1));
}

int
i2c_add_numbered_adapter(struct i2c_adapter *adap)
{
	return (lkpi_i2c_register(adap, adap->nr));
}

void
i2c_del_adapter(struct i2c_adapter *adap)
{
	if (!adap->lkpi_registered)
		return;
	mutex_lock(&lkpi_i2c_lock);
	(void) idr_remove(&lkpi_i2c_idr, adap->nr);
	mutex_unlock(&lkpi_i2c_lock);
	adap->lkpi_registered = false;
	(void) lkpi_devres_release_all(&adap->dev);
	kfree(adap->dev.kobj.name);
	adap->dev.kobj.name = NULL;
	mutex_destroy(&adap->bus_lock);
}

struct i2c_adapter *
i2c_get_adapter(int nr)
{
	struct i2c_adapter *adap;

	mutex_lock(&lkpi_i2c_lock);
	adap = idr_find(&lkpi_i2c_idr, nr);
	mutex_unlock(&lkpi_i2c_lock);
	return (adap);
}

void
i2c_put_adapter(struct i2c_adapter *adap)
{
	(void) adap;
}

void
i2c_lock_bus(struct i2c_adapter *adap, unsigned int flags)
{
	if (adap->lock_ops != NULL)
		adap->lock_ops->lock_bus(adap, flags);
	else
		mutex_lock(&adap->bus_lock);
}

int
i2c_trylock_bus(struct i2c_adapter *adap, unsigned int flags)
{
	if (adap->lock_ops != NULL)
		return (adap->lock_ops->trylock_bus(adap, flags));
	return (mutex_trylock(&adap->bus_lock));
}

void
i2c_unlock_bus(struct i2c_adapter *adap, unsigned int flags)
{
	if (adap->lock_ops != NULL)
		adap->lock_ops->unlock_bus(adap, flags);
	else
		mutex_unlock(&adap->bus_lock);
}

int
__i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	unsigned long end;
	int r, try;

	if (adap->algo == NULL || adap->algo->master_xfer == NULL)
		return (-EOPNOTSUPP);
	if (num <= 0)
		return (-EINVAL);
	end = jiffies + (unsigned long)adap->timeout;
	for (try = 0; ; try++) {
		r = adap->algo->master_xfer(adap, msgs, num);
		if (r != -EAGAIN)
			break;
		if (try >= adap->retries || time_after(jiffies, end))
			break;
	}
	return (r);
}

int
i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	int r;

	i2c_lock_bus(adap, I2C_LOCK_SEGMENT);
	r = __i2c_transfer(adap, msgs, num);
	i2c_unlock_bus(adap, I2C_LOCK_SEGMENT);
	return (r);
}

int
i2c_master_send(const struct i2c_client *client, const char *buf, int count)
{
	struct i2c_msg msg = { client->addr, client->flags & I2C_M_TEN,
	    (u16)count, (u8 *)(uintptr_t)buf };
	int r = i2c_transfer(client->adapter, &msg, 1);

	return (r == 1 ? count : (r < 0 ? r : -EIO));
}

int
i2c_master_recv(const struct i2c_client *client, char *buf, int count)
{
	struct i2c_msg msg = { client->addr,
	    (u16)((client->flags & I2C_M_TEN) | I2C_M_RD), (u16)count,
	    (u8 *)buf };
	int r = i2c_transfer(client->adapter, &msg, 1);

	return (r == 1 ? count : (r < 0 ? r : -EIO));
}

s32
i2c_smbus_read_i2c_block_data(const struct i2c_client *client, u8 command,
    u8 length, u8 *values)
{
	struct i2c_msg msgs[2] = {
		{ client->addr, 0, 1, &command },
		{ client->addr, I2C_M_RD, length, values },
	};
	int r = i2c_transfer(client->adapter, msgs, 2);

	return (r == 2 ? length : (r < 0 ? r : -EIO));
}

s32
i2c_smbus_read_byte_data(const struct i2c_client *client, u8 command)
{
	u8 v;
	s32 r = i2c_smbus_read_i2c_block_data(client, command, 1, &v);

	return (r == 1 ? v : r);
}

s32
i2c_smbus_write_byte_data(const struct i2c_client *client, u8 command,
    u8 value)
{
	u8 buf[2] = { command, value };
	struct i2c_msg msg = { client->addr, 0, 2, buf };
	int r = i2c_transfer(client->adapter, &msg, 1);

	return (r == 1 ? 0 : (r < 0 ? r : -EIO));
}

static void
lkpi_i2c_client_release(struct device *dev)
{
	kfree(container_of(dev, struct i2c_client, dev));
}

struct i2c_client *
i2c_new_client_device(struct i2c_adapter *adap,
    const struct i2c_board_info *info)
{
	struct i2c_client *c = kzalloc(sizeof (*c), GFP_KERNEL);

	if (c == NULL)
		return (ERR_PTR(-ENOMEM));
	c->adapter = adap;
	c->addr = info->addr;
	c->flags = info->flags;
	c->irq = info->irq;
	(void) strscpy(c->name, info->type, sizeof (c->name));
	device_initialize(&c->dev);
	c->dev.parent = &adap->dev;
	c->dev.platform_data = info->platform_data;
	c->dev.release = lkpi_i2c_client_release;
	(void) dev_set_name(&c->dev, "%d-%04x", adap->nr, c->addr);
	return (c);
}

void
i2c_unregister_device(struct i2c_client *client)
{
	if (client != NULL && !IS_ERR(client))
		put_device(&client->dev);
}

/* Bit-banging algorithm. */

#define	BIT_DATA(adap)	((struct i2c_algo_bit_data *)(adap)->algo_data)

static void
lkpi_bit_delay(struct i2c_algo_bit_data *b)
{
	udelay((unsigned long)(b->udelay > 0 ? b->udelay : 1));
}

static void
lkpi_bit_halfdelay(struct i2c_algo_bit_data *b)
{
	udelay((unsigned long)(b->udelay > 1 ? (b->udelay + 1) / 2 : 1));
}

static void
lkpi_sdalo(struct i2c_algo_bit_data *b)
{
	b->setsda(b->data, 0);
	lkpi_bit_halfdelay(b);
}

static void
lkpi_sdahi(struct i2c_algo_bit_data *b)
{
	b->setsda(b->data, 1);
	lkpi_bit_halfdelay(b);
}

static void
lkpi_scllo(struct i2c_algo_bit_data *b)
{
	b->setscl(b->data, 0);
	lkpi_bit_halfdelay(b);
}

/* Release SCL and wait for it to go high (slaves may stretch). */
static int
lkpi_sclhi(struct i2c_algo_bit_data *b)
{
	unsigned long end;

	b->setscl(b->data, 1);
	if (b->getscl == NULL) {
		lkpi_bit_delay(b);
		return (0);
	}
	end = jiffies + (unsigned long)(b->timeout > 0 ? b->timeout : 1);
	while (!b->getscl(b->data)) {
		if (time_after(jiffies, end)) {
			if (b->getscl(b->data))
				break;
			return (-ETIMEDOUT);
		}
		udelay(1);
	}
	lkpi_bit_delay(b);
	return (0);
}

static void
lkpi_bit_start(struct i2c_algo_bit_data *b)
{
	/* SCL and SDA high (bus idle or after repstart prelude). */
	b->setsda(b->data, 0);
	lkpi_bit_delay(b);
	lkpi_scllo(b);
}

static int
lkpi_bit_repstart(struct i2c_algo_bit_data *b)
{
	lkpi_sdahi(b);
	if (lkpi_sclhi(b) != 0)
		return (-ETIMEDOUT);
	lkpi_bit_start(b);
	return (0);
}

static void
lkpi_bit_stop(struct i2c_algo_bit_data *b)
{
	lkpi_sdalo(b);
	(void) lkpi_sclhi(b);
	b->setsda(b->data, 1);
	lkpi_bit_delay(b);
}

/* Returns 1 if ACKed, 0 if NAKed, negative on timeout. */
static int
lkpi_bit_outb(struct i2c_algo_bit_data *b, u8 c)
{
	int i, ack;

	for (i = 7; i >= 0; i--) {
		b->setsda(b->data, (c >> i) & 1);
		lkpi_bit_halfdelay(b);
		if (lkpi_sclhi(b) != 0)
			return (-ETIMEDOUT);
		b->setscl(b->data, 0);
		lkpi_bit_halfdelay(b);
	}
	lkpi_sdahi(b);
	if (lkpi_sclhi(b) != 0)
		return (-ETIMEDOUT);
	ack = !b->getsda(b->data);
	lkpi_scllo(b);
	return (ack);
}

/* Eight data bits from the slave, without the acknowledge clock. */
static int
lkpi_bit_inb(struct i2c_algo_bit_data *b)
{
	int i, v = 0;

	lkpi_sdahi(b);
	for (i = 0; i < 8; i++) {
		if (lkpi_sclhi(b) != 0)
			return (-ETIMEDOUT);
		v = (v << 1) | (b->getsda(b->data) ? 1 : 0);
		b->setscl(b->data, 0);
		lkpi_bit_halfdelay(b);
	}
	return (v);
}

/* Acknowledge clock: ACK pulls SDA low for one clock, NAK leaves it high. */
static int
lkpi_bit_ack(struct i2c_algo_bit_data *b, bool ack)
{
	if (ack)
		lkpi_sdalo(b);
	if (lkpi_sclhi(b) != 0)
		return (-ETIMEDOUT);
	lkpi_scllo(b);
	lkpi_sdahi(b);
	return (0);
}

static int
lkpi_bit_address(struct i2c_adapter *adap, struct i2c_msg *m)
{
	struct i2c_algo_bit_data *b = BIT_DATA(adap);
	u8 addr = i2c_8bit_addr_from_msg(m);
	int i, r = 0;

	if (m->flags & I2C_M_REV_DIR_ADDR)
		addr ^= 1;
	for (i = 0; i <= adap->retries; i++) {
		r = lkpi_bit_outb(b, addr);
		if (r != 0)
			break;
		lkpi_bit_stop(b);
		udelay((unsigned long)b->udelay);
		lkpi_bit_start(b);
	}
	if (r < 0)
		return (r);
	return (r == 1 ? 0 : -ENXIO);
}

static int
lkpi_bit_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	struct i2c_algo_bit_data *b = BIT_DATA(adap);
	int i, j, r = 0, v;

	if (b->pre_xfer != NULL && (r = b->pre_xfer(adap)) < 0)
		return (r);
	lkpi_bit_start(b);
	for (i = 0; i < num; i++) {
		struct i2c_msg *m = &msgs[i];
		bool nak_ok = (m->flags & I2C_M_IGNORE_NAK) != 0;

		if (!(m->flags & I2C_M_NOSTART)) {
			if (i > 0 && (r = lkpi_bit_repstart(b)) != 0)
				goto out;
			r = lkpi_bit_address(adap, m);
			if (r != 0 && !(nak_ok && r == -ENXIO))
				goto out;
		}
		if (m->flags & I2C_M_RD) {
			u16 len = m->len;

			for (j = 0; j < len; j++) {
				bool ack = j + 1 < len, bad = false;

				v = lkpi_bit_inb(b);
				if (v < 0) {
					r = v;
					goto out;
				}
				m->buf[j] = (u8)v;
				if (j == 0 && (m->flags & I2C_M_RECV_LEN)) {
					/*
					 * Block count: ACK a valid one, NAK an
					 * invalid one so the slave stops
					 * driving SDA before the STOP (docs/13
					 * C3).
					 */
					bad = v == 0 || v > I2C_SMBUS_BLOCK_MAX;
					ack = !bad;
					if (!bad) {
						len = (u16)(len + v);
						m->len = len;
					}
				}
				/* I2C_M_NO_RD_ACK: no acknowledge clock. */
				if (!(m->flags & I2C_M_NO_RD_ACK) &&
				    (r = lkpi_bit_ack(b, ack)) != 0)
					goto out;
				if (bad) {
					r = -EPROTO;
					goto out;
				}
			}
		} else {
			for (j = 0; j < m->len; j++) {
				v = lkpi_bit_outb(b, m->buf[j]);
				if (v < 0) {
					r = v;
					goto out;
				}
				if (v == 0 && !nak_ok) {
					r = -EIO;
					goto out;
				}
			}
		}
	}
	r = num;
out:
	lkpi_bit_stop(b);
	if (b->post_xfer != NULL)
		b->post_xfer(adap);
	return (r);
}

static u32
lkpi_bit_func(struct i2c_adapter *adap)
{
	(void) adap;
	return (I2C_FUNC_I2C | I2C_FUNC_NOSTART | I2C_FUNC_SMBUS_EMUL |
	    I2C_FUNC_PROTOCOL_MANGLING);
}

const struct i2c_algorithm i2c_bit_algo = {
	.master_xfer = lkpi_bit_xfer,
	.functionality = lkpi_bit_func,
};

static void
lkpi_bit_idle(struct i2c_adapter *adap)
{
	struct i2c_algo_bit_data *b = BIT_DATA(adap);

	b->setsda(b->data, 1);
	b->setscl(b->data, 1);
}

int
i2c_bit_add_bus(struct i2c_adapter *adap)
{
	adap->algo = &i2c_bit_algo;
	if (adap->retries == 0)
		adap->retries = 3;
	lkpi_bit_idle(adap);
	return (i2c_add_adapter(adap));
}

int
i2c_bit_add_numbered_bus(struct i2c_adapter *adap)
{
	adap->algo = &i2c_bit_algo;
	if (adap->retries == 0)
		adap->retries = 3;
	lkpi_bit_idle(adap);
	return (i2c_add_numbered_adapter(adap));
}
