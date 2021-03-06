/*
 * Debugfs support for hosts and cards
 *
 * Copyright (C) 2008 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/moduleparam.h>
#include <linux/export.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/fault-inject.h>
#include <linux/uaccess.h>

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/scatterlist.h>

#include "core.h"
#include "mmc_ops.h"

#ifdef CONFIG_FAIL_MMC_REQUEST

static DECLARE_FAULT_ATTR(fail_default_attr);
static char *fail_request;
module_param(fail_request, charp, 0);

#endif /* CONFIG_FAIL_MMC_REQUEST */

/*Samsung osv:  eMMC internal data check*/
static int samsung_emmc_health_info(struct mmc_card *card);
static int samsung_osv_enable_vendor(struct mmc_card *card);
static int samsung_osv_write_vendor(struct mmc_card *card);
static int samsung_osv_read_vendor(struct mmc_card *card);

extern int mmc_blk_cmdq_switch(struct mmc_card *card,
                                struct mmc_blk_data *md, bool enable);

struct VUC_REQUEST
{
    u32 signature;		/* = 0x1598a125 */
    u32 reserved0;		/* = 0 */
    u16 major_revision;	/* = 0 */
    u16 minor_revision;	/* = 0 */
    u32 reserved1;		/* = 0 */
    u32 UID;			/* = 0x00020003 */
    u32 reserved2[3];	/* = 0 */
    u32 argument[4];	/* = 0 */
    u32 reserved3[116];	/* = 0 */
};

struct OSV_DATA
{
    u32 reserved1[1];
    u32 reserved2[1];
    u32 UNC_err; 			/* - 0 : Pass.(No UECC) 	- 1 : Fail. (UECC happened) */
    u32 meta_data_corr; 	/* - 0 : Pass. (Meta data is normal) 	- 1 : Fail.(Meta data corrupted) */
    u32 num_run_time_bad; 	/* RTBB count */
    u32 total_written_amnt; /* Host requested written data size (100MB unit) */
    u32 LVD_detect_cnt; 	/* Total LVDF count */
    u32 reserved3[121];
};

struct VUC_REQUEST emmc_VUC_REQUEST = {0};
struct OSV_DATA emmc_OSV_DATA;

#define OSV_ARG							0xD7EF0326
#define CARD_BLOCK_SIZE 				512
#define BIT_DEVICE_SELF_TEST 			(0x1<<2)
#define BYTE_DEVICE_SELF_TEST_ENABLE 	0x2
#define SET_BLOCKCOUNT_READ 			0
#define SET_BLOCKCOUNT_WRITE 			1

static int samsung_emmc_health_info(struct mmc_card *card)
{
	int err = 0;
	bool cmdq_switch = false;

	if (!card) {
		err = -EINVAL;
		pr_err("OSV: %s: argument error card is NULL\n", __func__);
		goto out;
	}

	if (mmc_card_cmdq(card)) {
		/*cmdq switch off*/
		pr_err("OSV: mmc_blk_cmdq_switch off.\n");
		err = mmc_blk_cmdq_switch(card, NULL, false);
		if (err) {
			pr_err("%s: %s: cmdq disable failed %d.\n", mmc_hostname(card->host), __func__, err);
			goto out;
		}
		cmdq_switch = true;
	}

	/*check osv is enable*/
	err = samsung_osv_enable_vendor(card);
	if (err) {
		pr_err("OSV: %s: samsung_osv_enable_vendor fail,err=%d\n",
			mmc_hostname(card->host), err);
		goto out;
	}

	err = samsung_osv_write_vendor(card);
	if (err) {
		pr_err("OSV: %s: samsung_osv_write_vendor fail,err=%d\n",
			mmc_hostname(card->host), err);
		goto out;
	}

	/* After the test, send the OSV command and check the value */
	err = samsung_osv_read_vendor(card);
	if (err) {
		pr_err("OSV: %s: samsung_osv_read_vendor,err=%d\n",
			mmc_hostname(card->host), err);
		goto out;
	}

	if (!err && cmdq_switch) {
		/*cmdq switch on*/
		pr_err("OSV: mmc_blk_cmdq_switch on.\n");
		err = mmc_blk_cmdq_switch(card, NULL, true);
		if (err) {
			pr_err("%s: %s: cmdq enable failed %d.\n", mmc_hostname(card->host), __func__, err);
			goto out;
		}
	}

out:
	return err;
}

/*OSV Support & Enable bit (check pervious slide) */
static int samsung_osv_enable_vendor(struct mmc_card *card)
{
	u8 ext_csd[CARD_BLOCK_SIZE] = {0};
	int err = 0;

	if (!card) {
		err = -EINVAL;
		pr_err("OSV: %s: argument error card is NULL\n", __func__);
		goto out;
	}

	/* Read the EXT_CSD */
	err = mmc_send_ext_csd(card, ext_csd);
	if (err) {
		pr_err("OSV: %s: error %d sending ext_csd\n",
			mmc_hostname(card->host), err);
		goto out;
	}

	/*send the OSV command and check the value*/
	/* Check if OSV is supported by card */
	if (!(ext_csd[EXT_CSD_VENDOR_EXT_FEATURES]&BIT_DEVICE_SELF_TEST)) {
		err = -EINVAL;
		pr_err("OSV: %s: error %d OSV is not supported, ext_csd[EXT_CSD_VENDOR_EXT_FEATURES] = %02x\n",
			mmc_hostname(card->host), err, ext_csd[EXT_CSD_VENDOR_EXT_FEATURES]);
		goto out;
	}

	/*Device Self Test(OSV) Enable: CMD6 + 0x03660200*/
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
		EXT_CSD_VENDOR_EXT_FEATURES_ENABLE, BYTE_DEVICE_SELF_TEST_ENABLE,
		card->ext_csd.generic_cmd6_time);
	if (err) {
		pr_err("OSV: %s: Device Self Test(OSV) enable fail,err=%d\n",
			mmc_hostname(card->host), err);
		return err;
	}

	/* Read the EXT_CSD */
	err = mmc_send_ext_csd(card, ext_csd);
	if (err) {
		pr_err("OSV: %s: error %d sending ext_csd\n",
			mmc_hostname(card->host), err);
		goto out;
	}

	/* Check if OSV is supported by card */
	if (ext_csd[EXT_CSD_VENDOR_EXT_FEATURES_ENABLE] != BYTE_DEVICE_SELF_TEST_ENABLE) {
		err = -EINVAL;
		pr_err("OSV: Device Self Test(OSV) is not enable, ext_csd[EXT_CSD_VENDOR_EXT_FEATURES_ENABLE] = 0x%02x\n",
			ext_csd[EXT_CSD_VENDOR_EXT_FEATURES_ENABLE]);
		goto out;
	}

out:
	return err;
}

/* Write an eMMC internal data check format with CMD25 arg (0xD7EF0326) */
static int samsung_osv_write_vendor(struct mmc_card *card)
{
	struct mmc_request mrq = {NULL};
	struct mmc_command cmd = {0};
	struct mmc_data data = {0};
	struct scatterlist sg = {0};
	void *data_buf = NULL;
	int len = 512;
	int err = 0;

	if (!card) {
		err = -EINVAL;
		pr_err("OSV: %s: argument error card is NULL\n", __func__);
		goto out;
	}

	data_buf = kzalloc(len, GFP_KERNEL);
	if(!data_buf) {
		pr_err("Malloc err at %d.\n", __LINE__);
		return -ENOMEM;
	}

	emmc_VUC_REQUEST.signature = 0x1598a125;
	emmc_VUC_REQUEST.UID = 0x00020003;
	memcpy(data_buf, &emmc_VUC_REQUEST, len);

	cmd.opcode = MMC_WRITE_MULTIPLE_BLOCK;
	cmd.arg = OSV_ARG;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

	data.blksz = len;
	data.blocks = 1;
	data.flags = MMC_DATA_WRITE;
	data.sg = &sg;
	data.sg_len = 1;

	sg_init_one(&sg, data_buf, len);
	mmc_set_data_timeout(&data, card);

	mrq.cmd = &cmd;
	mrq.data = &data;

	err = mmc_set_blockcount(card, data.blocks, SET_BLOCKCOUNT_WRITE);
	if (err) {
		pr_err("OSV: %s:line%d-%s(), mmc_set_blockcount fail\n", __FILE__, __LINE__, __func__);
		goto out;
	}

	mmc_wait_for_req(card->host, &mrq);

	if(cmd.error) {
		err = 1;
		pr_err("cmd.error=%d\n", cmd.error);
		goto out;
	}

	if(data.error) {
		err = 1;
		pr_err("data.error=%d\n", data.error);
		goto out;
	}

out:
	if(data_buf)
		kfree(data_buf);
	return err;
}

/* Read an eMMC internal data with CMD18 arg (0xD7EF0326) */
static int samsung_osv_read_vendor(struct mmc_card *card)
{
	struct mmc_request mrq = {NULL};
	struct mmc_command cmd = {0};
	struct mmc_data data = {0};
	struct scatterlist sg = {0};
	void *data_buf = NULL;
	int len = 512;
	int err = 0;

	if (!card) {
		err = -EINVAL;
		pr_err("OSV: %s: argument error card is NULL\n", __func__);
		goto out;
	}

	data_buf = kzalloc(len, GFP_KERNEL);
	if(!data_buf) {
		pr_err("Malloc err at %d.\n", __LINE__);
		return -ENOMEM;
	}

	cmd.opcode = MMC_READ_MULTIPLE_BLOCK;
	cmd.arg = OSV_ARG;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

	data.blksz = len;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;
	data.sg = &sg;
	data.sg_len = 1;

	sg_init_one(&sg, data_buf, len);
	mmc_set_data_timeout(&data, card);

	mrq.cmd = &cmd;
	mrq.data = &data;

	err = mmc_set_blockcount(card, data.blocks, SET_BLOCKCOUNT_READ);
	if (err) {
		pr_err("OSV: %s:line%d-%s(), mmc_set_blockcount fail\n", __FILE__, __LINE__, __func__);
		goto out;
	}

	mmc_wait_for_req(card->host, &mrq);

	if(cmd.error) {
		err = 1;
		pr_err("cmd.error=%d\n", cmd.error);
		goto out;
	}

	if(data.error) {
		err = 1;
		pr_err("data.error=%d\n", data.error);
		goto out;
	}

	memcpy(&emmc_OSV_DATA, data_buf, len);

out:
	if(data_buf)
		kfree(data_buf);
	return err;
}

/* The debugfs functions are optimized away when CONFIG_DEBUG_FS isn't set. */
static int mmc_ios_show(struct seq_file *s, void *data)
{
	static const char *vdd_str[] = {
		[8]	= "2.0",
		[9]	= "2.1",
		[10]	= "2.2",
		[11]	= "2.3",
		[12]	= "2.4",
		[13]	= "2.5",
		[14]	= "2.6",
		[15]	= "2.7",
		[16]	= "2.8",
		[17]	= "2.9",
		[18]	= "3.0",
		[19]	= "3.1",
		[20]	= "3.2",
		[21]	= "3.3",
		[22]	= "3.4",
		[23]	= "3.5",
		[24]	= "3.6",
	};
	struct mmc_host	*host = s->private;
	struct mmc_ios	*ios = &host->ios;
	const char *str;

	seq_printf(s, "clock:\t\t%u Hz\n", ios->clock);
	if (host->actual_clock)
		seq_printf(s, "actual clock:\t%u Hz\n", host->actual_clock);
	seq_printf(s, "vdd:\t\t%u ", ios->vdd);
	if ((1 << ios->vdd) & MMC_VDD_165_195)
		seq_printf(s, "(1.65 - 1.95 V)\n");
	else if (ios->vdd < (ARRAY_SIZE(vdd_str) - 1)
			&& vdd_str[ios->vdd] && vdd_str[ios->vdd + 1])
		seq_printf(s, "(%s ~ %s V)\n", vdd_str[ios->vdd],
				vdd_str[ios->vdd + 1]);
	else
		seq_printf(s, "(invalid)\n");

	switch (ios->bus_mode) {
	case MMC_BUSMODE_OPENDRAIN:
		str = "open drain";
		break;
	case MMC_BUSMODE_PUSHPULL:
		str = "push-pull";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "bus mode:\t%u (%s)\n", ios->bus_mode, str);

	switch (ios->chip_select) {
	case MMC_CS_DONTCARE:
		str = "don't care";
		break;
	case MMC_CS_HIGH:
		str = "active high";
		break;
	case MMC_CS_LOW:
		str = "active low";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "chip select:\t%u (%s)\n", ios->chip_select, str);

	switch (ios->power_mode) {
	case MMC_POWER_OFF:
		str = "off";
		break;
	case MMC_POWER_UP:
		str = "up";
		break;
	case MMC_POWER_ON:
		str = "on";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "power mode:\t%u (%s)\n", ios->power_mode, str);
	seq_printf(s, "bus width:\t%u (%u bits)\n",
			ios->bus_width, 1 << ios->bus_width);

	switch (ios->timing) {
	case MMC_TIMING_LEGACY:
		str = "legacy";
		break;
	case MMC_TIMING_MMC_HS:
		str = "mmc high-speed";
		break;
	case MMC_TIMING_SD_HS:
		str = "sd high-speed";
		break;
	case MMC_TIMING_UHS_SDR50:
		str = "sd uhs SDR50";
		break;
	case MMC_TIMING_UHS_SDR104:
		str = "sd uhs SDR104";
		break;
	case MMC_TIMING_UHS_DDR50:
		str = "sd uhs DDR50";
		break;
	case MMC_TIMING_MMC_DDR52:
		str = "mmc DDR52";
		break;
	case MMC_TIMING_MMC_HS200:
		str = "mmc HS200";
		break;
	case MMC_TIMING_MMC_HS400:
		str = "mmc HS400";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "timing spec:\t%u (%s)\n", ios->timing, str);

	switch (ios->signal_voltage) {
	case MMC_SIGNAL_VOLTAGE_330:
		str = "3.30 V";
		break;
	case MMC_SIGNAL_VOLTAGE_180:
		str = "1.80 V";
		break;
	case MMC_SIGNAL_VOLTAGE_120:
		str = "1.20 V";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "signal voltage:\t%u (%s)\n", ios->chip_select, str);

	return 0;
}

static int mmc_ios_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmc_ios_show, inode->i_private);
}

static const struct file_operations mmc_ios_fops = {
	.open		= mmc_ios_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int mmc_clock_opt_get(void *data, u64 *val)
{
	struct mmc_host *host = data;

	*val = host->ios.clock;

	return 0;
}

static int mmc_clock_opt_set(void *data, u64 val)
{
	struct mmc_host *host = data;

	/* We need this check due to input value is u64 */
	if (val > host->f_max)
		return -EINVAL;

	mmc_claim_host(host);
	mmc_set_clock(host, (unsigned int) val);
	mmc_release_host(host);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(mmc_clock_fops, mmc_clock_opt_get, mmc_clock_opt_set,
	"%llu\n");

#include <linux/delay.h>

static int mmc_scale_get(void *data, u64 *val)
{
	struct mmc_host *host = data;

	*val = host->clk_scaling.curr_freq;

	return 0;
}

static int mmc_scale_set(void *data, u64 val)
{
	int err = 0;
	struct mmc_host *host = data;

	mmc_claim_host(host);
	mmc_host_clk_hold(host);

	/* change frequency from sysfs manually */
	err = mmc_clk_update_freq(host, val, host->clk_scaling.state);
	if (err == -EAGAIN)
		err = 0;
	else if (err)
		pr_err("%s: clock scale to %llu failed with error %d\n",
			mmc_hostname(host), val, err);
	else
		pr_debug("%s: clock change to %llu finished successfully (%s)\n",
			mmc_hostname(host), val, current->comm);

	mmc_host_clk_release(host);
	mmc_release_host(host);

	return err;
}

DEFINE_SIMPLE_ATTRIBUTE(mmc_scale_fops, mmc_scale_get, mmc_scale_set,
	"%llu\n");

static int mmc_max_clock_get(void *data, u64 *val)
{
	struct mmc_host *host = data;

	if (!host)
		return -EINVAL;

	*val = host->f_max;

	return 0;
}

static int mmc_max_clock_set(void *data, u64 val)
{
	struct mmc_host *host = data;
	int err = -EINVAL;
	unsigned long freq = val;
	unsigned int old_freq;

	if (!host || (val < host->f_min))
		goto out;

	mmc_claim_host(host);
	if (host->bus_ops && host->bus_ops->change_bus_speed) {
		old_freq = host->f_max;
		host->f_max = freq;

		err = host->bus_ops->change_bus_speed(host, &freq);

		if (err)
			host->f_max = old_freq;
	}
	mmc_release_host(host);
out:
	return err;
}

DEFINE_SIMPLE_ATTRIBUTE(mmc_max_clock_fops, mmc_max_clock_get,
		mmc_max_clock_set, "%llu\n");

static int mmc_force_err_set(void *data, u64 val)
{
	struct mmc_host *host = data;

	if (host && host->ops && host->ops->force_err_irq) {
		mmc_host_clk_hold(host);
		host->ops->force_err_irq(host, val);
		mmc_host_clk_release(host);
	}

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(mmc_force_err_fops, NULL, mmc_force_err_set, "%llu\n");

void mmc_add_host_debugfs(struct mmc_host *host)
{
	struct dentry *root;

	root = debugfs_create_dir(mmc_hostname(host), NULL);
	if (IS_ERR(root))
		/* Don't complain -- debugfs just isn't enabled */
		return;
	if (!root)
		/* Complain -- debugfs is enabled, but it failed to
		 * create the directory. */
		goto err_root;

	host->debugfs_root = root;

	if (!debugfs_create_file("ios", S_IRUSR, root, host, &mmc_ios_fops))
		goto err_node;

	if (!debugfs_create_file("clock", S_IRUSR | S_IWUSR, root, host,
			&mmc_clock_fops))
		goto err_node;

	if (!debugfs_create_file("max_clock", S_IRUSR | S_IWUSR, root, host,
		&mmc_max_clock_fops))
		goto err_node;

	if (!debugfs_create_file("scale", S_IRUSR | S_IWUSR, root, host,
		&mmc_scale_fops))
		goto err_node;

	if (!debugfs_create_bool("skip_clk_scale_freq_update",
		S_IRUSR | S_IWUSR, root,
		&host->clk_scaling.skip_clk_scale_freq_update))
		goto err_node;

	if (!debugfs_create_bool("cmdq_task_history",
		S_IRUSR | S_IWUSR, root,
		&host->cmdq_thist_enabled))
		goto err_node;

#ifdef CONFIG_MMC_CLKGATE
	if (!debugfs_create_u32("clk_delay", (S_IRUSR | S_IWUSR),
				root, &host->clk_delay))
		goto err_node;
#endif
#ifdef CONFIG_FAIL_MMC_REQUEST
	if (fail_request)
		setup_fault_attr(&fail_default_attr, fail_request);
	host->fail_mmc_request = fail_default_attr;
	if (IS_ERR(fault_create_debugfs_attr("fail_mmc_request",
					     root,
					     &host->fail_mmc_request)))
		goto err_node;
#endif
	if (!debugfs_create_file("force_error", S_IWUSR, root, host,
		&mmc_force_err_fops))
		goto err_node;

	return;

err_node:
	debugfs_remove_recursive(root);
	host->debugfs_root = NULL;
err_root:
	dev_err(&host->class_dev, "failed to initialize debugfs\n");
}

void mmc_remove_host_debugfs(struct mmc_host *host)
{
	debugfs_remove_recursive(host->debugfs_root);
}

static int mmc_dbg_card_status_get(void *data, u64 *val)
{
	struct mmc_card	*card = data;
	u32		status;
	int		ret;

	mmc_get_card(card);
	if (mmc_card_cmdq(card)) {
		ret = mmc_cmdq_halt_on_empty_queue(card->host);
		if (ret) {
			pr_err("%s: halt failed while doing %s err (%d)\n",
					mmc_hostname(card->host), __func__,
					ret);
			goto out;
		}
	}

	ret = mmc_send_status(data, &status);
	if (!ret)
		*val = status;

	if (mmc_card_cmdq(card)) {
		if (mmc_cmdq_halt(card->host, false))
			pr_err("%s: %s: cmdq unhalt failed\n",
			       mmc_hostname(card->host), __func__);
	}
out:
	mmc_put_card(card);

	return ret;
}
DEFINE_SIMPLE_ATTRIBUTE(mmc_dbg_card_status_fops, mmc_dbg_card_status_get,
		NULL, "%08llx\n");

#define EXT_CSD_STR_LEN 1025

static int mmc_ext_csd_open(struct inode *inode, struct file *filp)
{
	struct mmc_card *card = inode->i_private;
	char *buf;
	ssize_t n = 0;
	u8 *ext_csd;
	int err, i;

	buf = kmalloc(EXT_CSD_STR_LEN + 1, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ext_csd = kmalloc(512, GFP_KERNEL);
	if (!ext_csd) {
		err = -ENOMEM;
		goto out_free_halt;
	}

	mmc_get_card(card);
	if (mmc_card_cmdq(card)) {
		err = mmc_cmdq_halt_on_empty_queue(card->host);
		if (err) {
			pr_err("%s: halt failed while doing %s err (%d)\n",
					mmc_hostname(card->host), __func__,
					err);
			mmc_put_card(card);
			goto out_free_halt;
		}
	}
	err = mmc_send_ext_csd(card, ext_csd);
	if (err)
		goto out_free;

	for (i = 0; i < 512; i++)
		n += sprintf(buf + n, "%02x", ext_csd[i]);
	n += sprintf(buf + n, "\n");
	BUG_ON(n != EXT_CSD_STR_LEN);

	filp->private_data = buf;

	if (mmc_card_cmdq(card)) {
		if (mmc_cmdq_halt(card->host, false))
			pr_err("%s: %s: cmdq unhalt failed\n",
			       mmc_hostname(card->host), __func__);
	}

	mmc_put_card(card);
	kfree(ext_csd);
	return 0;

out_free:
	if (mmc_card_cmdq(card)) {
		if (mmc_cmdq_halt(card->host, false))
			pr_err("%s: %s: cmdq unhalt failed\n",
			       mmc_hostname(card->host), __func__);
	}
	mmc_put_card(card);
out_free_halt:
	kfree(buf);
	kfree(ext_csd);
	return err;
}

static ssize_t mmc_ext_csd_read(struct file *filp, char __user *ubuf,
				size_t cnt, loff_t *ppos)
{
	char *buf = filp->private_data;

	return simple_read_from_buffer(ubuf, cnt, ppos,
				       buf, EXT_CSD_STR_LEN);
}

static int mmc_ext_csd_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	return 0;
}

static const struct file_operations mmc_dbg_ext_csd_fops = {
	.open		= mmc_ext_csd_open,
	.read		= mmc_ext_csd_read,
	.release	= mmc_ext_csd_release,
	.llseek		= default_llseek,
};

static int mmc_wr_pack_stats_open(struct inode *inode, struct file *filp)
{
	struct mmc_card *card = inode->i_private;

	filp->private_data = card;
	card->wr_pack_stats.print_in_read = 1;
	return 0;
}

#define TEMP_BUF_SIZE 256
static ssize_t mmc_wr_pack_stats_read(struct file *filp, char __user *ubuf,
				size_t cnt, loff_t *ppos)
{
	struct mmc_card *card = filp->private_data;
	struct mmc_wr_pack_stats *pack_stats;
	int i;
	int max_num_of_packed_reqs = 0;
	char *temp_buf;

	if (!card)
		return cnt;

	if (!access_ok(VERIFY_WRITE, ubuf, cnt))
		return cnt;

	if (!card->wr_pack_stats.print_in_read)
		return 0;

	if (!card->wr_pack_stats.enabled) {
		pr_info("%s: write packing statistics are disabled\n",
			 mmc_hostname(card->host));
		goto exit;
	}

	pack_stats = &card->wr_pack_stats;

	if (!pack_stats->packing_events) {
		pr_info("%s: NULL packing_events\n", mmc_hostname(card->host));
		goto exit;
	}

	max_num_of_packed_reqs = card->ext_csd.max_packed_writes;

	temp_buf = kmalloc(TEMP_BUF_SIZE, GFP_KERNEL);
	if (!temp_buf)
		goto exit;

	spin_lock(&pack_stats->lock);

	snprintf(temp_buf, TEMP_BUF_SIZE, "%s: write packing statistics:\n",
		mmc_hostname(card->host));
	strlcat(ubuf, temp_buf, cnt);

	for (i = 1 ; i <= max_num_of_packed_reqs ; ++i) {
		if (pack_stats->packing_events[i]) {
			snprintf(temp_buf, TEMP_BUF_SIZE,
				 "%s: Packed %d reqs - %d times\n",
				mmc_hostname(card->host), i,
				pack_stats->packing_events[i]);
			strlcat(ubuf, temp_buf, cnt);
		}
	}

	snprintf(temp_buf, TEMP_BUF_SIZE,
		 "%s: stopped packing due to the following reasons:\n",
		 mmc_hostname(card->host));
	strlcat(ubuf, temp_buf, cnt);

	if (pack_stats->pack_stop_reason[EXCEEDS_SEGMENTS]) {
		snprintf(temp_buf, TEMP_BUF_SIZE,
			 "%s: %d times: exceed max num of segments\n",
			 mmc_hostname(card->host),
			 pack_stats->pack_stop_reason[EXCEEDS_SEGMENTS]);
		strlcat(ubuf, temp_buf, cnt);
	}
	if (pack_stats->pack_stop_reason[EXCEEDS_SECTORS]) {
		snprintf(temp_buf, TEMP_BUF_SIZE,
			 "%s: %d times: exceed max num of sectors\n",
			mmc_hostname(card->host),
			pack_stats->pack_stop_reason[EXCEEDS_SECTORS]);
		strlcat(ubuf, temp_buf, cnt);
	}
	if (pack_stats->pack_stop_reason[WRONG_DATA_DIR]) {
		snprintf(temp_buf, TEMP_BUF_SIZE,
			 "%s: %d times: wrong data direction\n",
			mmc_hostname(card->host),
			pack_stats->pack_stop_reason[WRONG_DATA_DIR]);
		strlcat(ubuf, temp_buf, cnt);
	}
	if (pack_stats->pack_stop_reason[FLUSH_OR_DISCARD]) {
		snprintf(temp_buf, TEMP_BUF_SIZE,
			 "%s: %d times: flush or discard\n",
			mmc_hostname(card->host),
			pack_stats->pack_stop_reason[FLUSH_OR_DISCARD]);
		strlcat(ubuf, temp_buf, cnt);
	}
	if (pack_stats->pack_stop_reason[EMPTY_QUEUE]) {
		snprintf(temp_buf, TEMP_BUF_SIZE,
			 "%s: %d times: empty queue\n",
			mmc_hostname(card->host),
			pack_stats->pack_stop_reason[EMPTY_QUEUE]);
		strlcat(ubuf, temp_buf, cnt);
	}
	if (pack_stats->pack_stop_reason[REL_WRITE]) {
		snprintf(temp_buf, TEMP_BUF_SIZE,
			 "%s: %d times: rel write\n",
			mmc_hostname(card->host),
			pack_stats->pack_stop_reason[REL_WRITE]);
		strlcat(ubuf, temp_buf, cnt);
	}
	if (pack_stats->pack_stop_reason[THRESHOLD]) {
		snprintf(temp_buf, TEMP_BUF_SIZE,
			 "%s: %d times: Threshold\n",
			mmc_hostname(card->host),
			pack_stats->pack_stop_reason[THRESHOLD]);
		strlcat(ubuf, temp_buf, cnt);
	}

	if (pack_stats->pack_stop_reason[LARGE_SEC_ALIGN]) {
		snprintf(temp_buf, TEMP_BUF_SIZE,
			 "%s: %d times: Large sector alignment\n",
			mmc_hostname(card->host),
			pack_stats->pack_stop_reason[LARGE_SEC_ALIGN]);
		strlcat(ubuf, temp_buf, cnt);
	}
	if (pack_stats->pack_stop_reason[RANDOM]) {
		snprintf(temp_buf, TEMP_BUF_SIZE,
			 "%s: %d times: random request\n",
			mmc_hostname(card->host),
			pack_stats->pack_stop_reason[RANDOM]);
		strlcat(ubuf, temp_buf, cnt);
	}
	if (pack_stats->pack_stop_reason[FUA]) {
		snprintf(temp_buf, TEMP_BUF_SIZE,
			 "%s: %d times: fua request\n",
			mmc_hostname(card->host),
			pack_stats->pack_stop_reason[FUA]);
		strlcat(ubuf, temp_buf, cnt);
	}

	spin_unlock(&pack_stats->lock);

	kfree(temp_buf);

	pr_info("%s", ubuf);

exit:
	if (card->wr_pack_stats.print_in_read == 1) {
		card->wr_pack_stats.print_in_read = 0;
		return strnlen(ubuf, cnt);
	}

	return 0;
}

static ssize_t mmc_wr_pack_stats_write(struct file *filp,
				       const char __user *ubuf, size_t cnt,
				       loff_t *ppos)
{
	struct mmc_card *card = filp->private_data;
	int value;

	if (!card)
		return cnt;

	if (!access_ok(VERIFY_READ, ubuf, cnt))
		return cnt;

	sscanf(ubuf, "%d", &value);
	if (value) {
		mmc_blk_init_packed_statistics(card);
	} else {
		spin_lock(&card->wr_pack_stats.lock);
		card->wr_pack_stats.enabled = false;
		spin_unlock(&card->wr_pack_stats.lock);
	}

	return cnt;
}

static const struct file_operations mmc_dbg_wr_pack_stats_fops = {
	.open		= mmc_wr_pack_stats_open,
	.read		= mmc_wr_pack_stats_read,
	.write		= mmc_wr_pack_stats_write,
};

static int mmc_bkops_stats_read(struct seq_file *file, void *data)
{
	struct mmc_card *card = file->private;
	struct mmc_bkops_stats *stats;
	int i;

	if (!card)
		return -EINVAL;

	stats = &card->bkops.stats;

	if (!stats->enabled) {
		pr_info("%s: bkops statistics are disabled\n",
			 mmc_hostname(card->host));
		goto exit;
	}

	spin_lock(&stats->lock);

	seq_printf(file, "%s: bkops statistics:\n",
			mmc_hostname(card->host));
	seq_printf(file, "%s: BKOPS: sent START_BKOPS to device: %u\n",
			mmc_hostname(card->host), stats->manual_start);
	seq_printf(file, "%s: BKOPS: stopped due to HPI: %u\n",
			mmc_hostname(card->host), stats->hpi);
	seq_printf(file, "%s: BKOPS: sent AUTO_EN set to 1: %u\n",
			mmc_hostname(card->host), stats->auto_start);
	seq_printf(file, "%s: BKOPS: sent AUTO_EN set to 0: %u\n",
			mmc_hostname(card->host), stats->auto_stop);

	for (i = 0 ; i < MMC_BKOPS_NUM_SEVERITY_LEVELS ; ++i)
		seq_printf(file, "%s: BKOPS: due to level %d: %u\n",
			 mmc_hostname(card->host), i, stats->level[i]);

	spin_unlock(&stats->lock);

exit:

	return 0;
}

static ssize_t mmc_bkops_stats_write(struct file *filp,
				      const char __user *ubuf, size_t cnt,
				      loff_t *ppos)
{
	struct mmc_card *card = filp->f_mapping->host->i_private;
	int value;
	struct mmc_bkops_stats *stats;
	int err;

	if (!card)
		return cnt;

	stats = &card->bkops.stats;

	err = kstrtoint_from_user(ubuf, cnt, 0, &value);
	if (err) {
		pr_err("%s: %s: error parsing input from user (%d)\n",
				mmc_hostname(card->host), __func__, err);
		return err;
	}
	if (value) {
		mmc_blk_init_bkops_statistics(card);
	} else {
		spin_lock(&stats->lock);
		stats->enabled = false;
		spin_unlock(&stats->lock);
	}

	return cnt;
}

static int mmc_bkops_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmc_bkops_stats_read, inode->i_private);
}

static const struct file_operations mmc_dbg_bkops_stats_fops = {
	.open		= mmc_bkops_stats_open,
	.read		= seq_read,
	.write		= mmc_bkops_stats_write,
};

#ifdef CONFIG_HW_MMC_TEST
static int mmc_card_addr_open(struct inode *inode, struct file *filp)
{
	struct mmc_card *card = inode->i_private;
	filp->private_data = card;

	return 0;
}

static ssize_t mmc_card_addr_read(struct file *filp, char __user *ubuf,
				     size_t cnt, loff_t *ppos)
{
	char buf[64] = {0};
	struct mmc_card *card = filp->private_data;
	long card_addr = (long)card;

	card_addr = (long)(card_addr ^ CARD_ADDR_MAGIC);
	snprintf(buf, sizeof(buf), "%ld", card_addr);

	return simple_read_from_buffer(ubuf, cnt, ppos,
			buf, sizeof(buf));
}

static const struct file_operations mmc_dbg_card_addr_fops = {
	.open		= mmc_card_addr_open,
	.read		= mmc_card_addr_read,
	.llseek     = default_llseek,
};

static int mmc_test_st_open(struct inode *inode, struct file *filp)
{
	struct mmc_card *card = inode->i_private;

	filp->private_data = card;

	return 0;
}

static ssize_t mmc_test_st_read(struct file *filp, char __user *ubuf,
				     size_t cnt, loff_t *ppos)
{
	char buf[64] = {0};
	struct mmc_card *card = filp->private_data;

	if (!card)
		return cnt;

	snprintf(buf, sizeof(buf), "%d", card->host->test_status);

	return simple_read_from_buffer(ubuf, cnt, ppos,
			buf, sizeof(buf));

}

static ssize_t mmc_test_st_write(struct file *filp,
				      const char __user *ubuf, size_t cnt,
				      loff_t *ppos)
{
	struct mmc_card *card = filp->private_data;
	int value;

	if (!card) {
		return cnt;
	}

	sscanf(ubuf, "%d", &value);
	card->host->test_status = value;

	return cnt;
}

static const struct file_operations mmc_dbg_test_st_fops = {
	.open		= mmc_test_st_open,
	.read		= mmc_test_st_read,
	.write		= mmc_test_st_write,
};

/* Print samsung emmc health information */
static int samsung_emmc_health_info_show(struct seq_file *file, void *data)
{
	int err = 0;
	struct mmc_card *card = file->private;

	if (!card)
		return -EINVAL;

	err = samsung_emmc_health_info(card);
	if(!err)
	{
		seq_printf(file, "Samsung Device Health Information\n");
		seq_printf(file, "Uncorrectable ECC: 0x%x\n", emmc_OSV_DATA.UNC_err);
		seq_printf(file, "Meta Data Corruption: 0x%x\n", emmc_OSV_DATA.meta_data_corr);
		seq_printf(file, "Runtime Bad Block: 0x%x\n", emmc_OSV_DATA.num_run_time_bad);
		seq_printf(file, "Written Date: 0x%x\n", emmc_OSV_DATA.total_written_amnt);
		seq_printf(file, "LVDF: 0x%x\n", emmc_OSV_DATA.LVD_detect_cnt);
	}
	else
		seq_printf(file, "It has not the health information!\n");

	return err;
}

/* Read emmc health information according to the vendor */
static int mmc_health_st_read(struct seq_file *file, void *data)
{
	struct mmc_card *card = file->private;
	if (!card)
		return -EINVAL;

	switch(card->cid.manfid) {
		case CID_MANFID_SAMSUNG:
			samsung_emmc_health_info_show(file, data);
		break;

		default:
			seq_printf(file, "It has not the health information!\n");
		break;
	}

	return 0;
}

static int mmc_health_st_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmc_health_st_read, inode->i_private);
}

static const struct file_operations mmc_dbg_health_st_fops = {
	.open		= mmc_health_st_open,
	.read		= seq_read,
    .llseek     = default_llseek,
};

#endif

void mmc_add_card_debugfs(struct mmc_card *card)
{
	struct mmc_host	*host = card->host;
	struct dentry	*root;

	if (!host->debugfs_root)
		return;

	root = debugfs_create_dir(mmc_card_id(card), host->debugfs_root);
	if (IS_ERR(root))
		/* Don't complain -- debugfs just isn't enabled */
		return;
	if (!root)
		/* Complain -- debugfs is enabled, but it failed to
		 * create the directory. */
		goto err;

	card->debugfs_root = root;

	if (!debugfs_create_x32("state", S_IRUSR, root, &card->state))
		goto err;

	if (mmc_card_mmc(card) || mmc_card_sd(card))
		if (!debugfs_create_file("status", S_IRUSR, root, card,
					&mmc_dbg_card_status_fops))
			goto err;

	if (mmc_card_mmc(card))
		if (!debugfs_create_file("ext_csd", S_IRUSR, root, card,
					&mmc_dbg_ext_csd_fops))
			goto err;

	if (mmc_card_mmc(card) && (card->ext_csd.rev >= 6) &&
	    (card->host->caps2 & MMC_CAP2_PACKED_WR))
		if (!debugfs_create_file("wr_pack_stats", S_IRUSR, root, card,
					 &mmc_dbg_wr_pack_stats_fops))
			goto err;

	if (mmc_card_mmc(card) && (card->ext_csd.rev >= 5) &&
	    (mmc_card_configured_auto_bkops(card) ||
	     mmc_card_configured_manual_bkops(card)))
		if (!debugfs_create_file("bkops_stats", S_IRUSR, root, card,
					 &mmc_dbg_bkops_stats_fops))
			goto err;
#ifdef CONFIG_HW_MMC_TEST
	if (mmc_card_mmc(card))
		if (!debugfs_create_file("card_addr", S_IRUSR, root, card,
				&mmc_dbg_card_addr_fops))
			goto err;

	if (mmc_card_mmc(card))
		if (!debugfs_create_file("test_st", S_IRUSR, root, card,
				&mmc_dbg_test_st_fops))
			goto err;

	/*add file health_st*/
	if(mmc_card_mmc(card))
		if (!debugfs_create_file("health_st", S_IRUSR, root, card,
				&mmc_dbg_health_st_fops))
			goto err;
#endif
	return;

err:
	debugfs_remove_recursive(root);
	card->debugfs_root = NULL;
	dev_err(&card->dev, "failed to initialize debugfs\n");
}

void mmc_remove_card_debugfs(struct mmc_card *card)
{
	debugfs_remove_recursive(card->debugfs_root);
}
