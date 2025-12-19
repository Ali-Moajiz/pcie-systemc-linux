#include <asm/uaccess.h> /* put_user */
#include <linux/cdev.h>	 /* cdev_ */
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/device.h>
#include <linux/version.h>
#include "chardev.h"

/* Each PCI device has 6 BAR IOs (base address register) as per the PCI spec.
 *
 * Each BAR corresponds to an address range that can be used to communicate with the PCI.
 *
 * Eech BAR is of one of the two types:
 *
 * - IORESOURCE_IO: must be accessed with inX and outX
 * - IORESOURCE_MEM: must be accessed with ioreadX and iowriteX
 *   	This is the saner method apparently, and what the edu device uses.
 *
 * The length of each region is defined BY THE HARDWARE, and communicated to software
 * via the configuration registers.
 *
 * The Linux kernel automatically parses the 64 bytes of standardized configuration registers for us.
 *
 * QEMU devices register those regions with:
 *
 *     memory_region_init_io(&edu->mmio, OBJECT(edu), &edu_mmio_ops, edu,
 *                     "edu-mmio", 1 << 20);
 *     pci_register_bar(pdev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &edu->mmio);
 **/
#define BAR 0
#define CDEV_NAME "cpcidev_pci"
#define EDU_DEVICE_ID 0xabcd
#define IO_IRQ_ACK 0x64
#define IO_IRQ_STATUS 0x24
#define QEMU_VENDOR_ID 0x1234

#define IOCTL_SET_OP1_MATRIX   0x01
#define IOCTL_SET_OP2_MATRIX   0x02
#define IOCTL_GET_RESULT      0x03
#define IOCTL_SET_OPCODE      0x04


MODULE_LICENSE("GPL");

static struct pci_device_id pci_ids[] = {
	// creating a struct pci_device_id matching the vendor id and device id.
	{
		PCI_DEVICE(QEMU_VENDOR_ID, EDU_DEVICE_ID),
	},
	{
		0,
	}};

// pci_ids need to be exported into user space to allow
// hotplug and kernel modules know what module works with what
// hardware device.
MODULE_DEVICE_TABLE(pci, pci_ids);

static int pci_irq;
static int major;
static struct pci_dev *pdev;
static void __iomem *mmio;

/* Device class and device for automatic /dev node creation */
static struct class *cpcidev_class;
static struct device *cpcidev_device;
static struct cdev cpcidev_cdev;
static dev_t dev_num;
/*
	Following function is calling in our case since in the user-space is calling the ioctl funtion, not read/write funtions
*/

static long dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	uint32_t kbuf[4][4];
	uint32_t opcode;
	int i, j;
	void __user *uarg = (void __user *)arg;

	printk(KERN_INFO "dev_ioctl() cmd: %u\n", cmd);

	switch (cmd)
	{

	case IOCTL_SET_OP1_MATRIX:
		if (copy_from_user(kbuf, uarg, sizeof(kbuf)))
			return -EFAULT;

		for (i = 0; i < 4; i++)
		{
			for (j = 0; j < 4; j++)
			{
				iowrite32(kbuf[i][j],
						  mmio + 0x10 + ((i * 4 + j) * sizeof(uint32_t)));
			}
		}

		printk(KERN_INFO "OP1 matrix written\n");
		break;

	case IOCTL_SET_OP2_MATRIX:
		if (copy_from_user(kbuf, uarg, sizeof(kbuf)))
			return -EFAULT;

		for (i = 0; i < 4; i++)
		{
			for (j = 0; j < 4; j++)
			{
				iowrite32(kbuf[i][j],
						  mmio + 0x50 + ((i * 4 + j) * sizeof(uint32_t)));
			}
		}

		printk(KERN_INFO "OP2 matrix written\n");
		break;

	case IOCTL_SET_OPCODE:
		if (copy_from_user(&opcode, uarg, sizeof(opcode)))
			return -EFAULT;

		iowrite32(opcode, mmio + 0x90);
		printk(KERN_INFO "Opcode set to %u\n", opcode);
		break;

	case IOCTL_GET_RESULT:
		/*
		 * Reading result triggers matrix multiplication in device
		 * Result matrix starts at 0xA0
		 */
		for (i = 0; i < 4; i++)
		{
			for (j = 0; j < 4; j++)
			{
				kbuf[i][j] =
					ioread32(mmio + 0xA0 + ((i * 4 + j) * sizeof(uint32_t)));
			}
		}

		if (copy_to_user(uarg, kbuf, sizeof(kbuf)))
			return -EFAULT;

		printk(KERN_INFO "Result matrix read\n");
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

// static long dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
// {
// 	int rv = 0;
// 	int *ip;
// 	int set;
// 	printk(KERN_INFO "dev_ioctl() cmd: %x arg: %x \n", cmd, arg);

// 	if (cmd == 4)
// 	{
// 		printk(KERN_INFO "retrieve op1.\n");
// 		ip = (int *)arg;
// 		*ip = ioread32((void *)(mmio + 0x10));
// 		printk(KERN_INFO "op1: %x\n", *ip);
// 	}

// 	if (cmd == 5)
// 	{
// 		printk(KERN_INFO "retrieve op2.\n");
// 		ip = (int *)arg;
// 		*ip = ioread32((void *)(mmio + 0x14));
// 		printk(KERN_INFO "op2: %x\n", *ip);
// 	}

// 	if (cmd == 6)
// 	{
// 		printk(KERN_INFO "retrieve opcode.\n");
// 		ip = (int *)arg;
// 		*ip = ioread32((void *)(mmio + 0x18));
// 		printk(KERN_INFO "opcode: %x\n", *ip);
// 	}

// 	if (cmd == 7)
// 	{
// 		printk(KERN_INFO "retrieve result.\n");
// 		ip = (int *)arg;
// 		*ip = ioread32((void *)(mmio + 0x30));
// 		printk(KERN_INFO "result: %x\n", *ip);
// 	}

// 	if (cmd == 8)
// 	{
// 		set = (int)arg;
// 		printk(KERN_INFO "set op1 %x\n", set);
// 		iowrite32(set, mmio + 0x10);
// 	}

// 	if (cmd == 9)
// 	{
// 		set = (int)arg;
// 		printk(KERN_INFO "set op2 %x\n", set);
// 		iowrite32(set, mmio + 0x14);
// 	}

// 	if (cmd == 10)
// 	{
// 		set = (int)arg;
// 		printk(KERN_INFO "set opcode %x\n", set);
// 		iowrite32(set, mmio + 0x18);
// 	}

// 	return rv;
// }

static ssize_t read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
	ssize_t ret;
	u32 kbuf;
	printk(KERN_INFO "CPCIDEV: read() len: %lx offset: %lx\n", len, *off);
	if (*off % 4 || len == 0)
	{
		ret = 0;
	}
	else
	{
		kbuf = ioread32(mmio + *off);
		if (copy_to_user(buf, (void *)&kbuf, 4))
		{
			printk(KERN_ALERT "fault on copy_to_user\n");
			ret = -EFAULT;
		}
		else
		{
			ret = 4;
			(*off)++;
		}
	}
	return ret;
}

static ssize_t write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
	ssize_t ret;
	u32 kbuf;
	printk(KERN_INFO "CPCIDEV: write()\n");

	ret = len;
	if (!(*off % 4))
	{
		if (copy_from_user((void *)&kbuf, buf, 4) || len != 4)
		{
			/*copy_from_user returns 0 on copy success*/
			printk(KERN_ALERT "data copy failed in the kernel\n");
			ret = -EFAULT;
		}
		else
		{
			printk(KERN_ALERT "data copied\n");
			iowrite32(kbuf, mmio + *off);
		}
	}
	return ret;
}

// static ssize_t write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
// {
// 	ssize_t ret = len;
// 	u32 kbuf[4][4]; // 4x4 matrix

// 	printk(KERN_INFO "CPCIDEV: write()\n");

// 	// check that len is exactly the size of 4x4 matrix
// 	if (len != sizeof(kbuf))
// 	{
// 		printk(KERN_ALERT "Invalid write size: %zu bytes\n", len);
// 		return -EINVAL;
// 	}

// 	// copy from user
// 	if (copy_from_user(kbuf, buf, sizeof(kbuf)))
// 	{
// 		printk(KERN_ALERT "Data copy failed in the kernel\n");
// 		return -EFAULT;
// 	}

// 	printk(KERN_INFO "Data copied successfully\n");

// 	// Example: write to MMIO (flattening the 2D array)
// 	for (int i = 0; i < 4; i++)
// 	{
// 		for (int j = 0; j < 4; j++)
// 		{
// 			iowrite32(kbuf[i][j], mmio + (*off) + (i * 4 + j) * sizeof(u32));
// 		}
// 	}

// 	return ret;
// }

static loff_t llseek(struct file *filp, loff_t off, int whence)
{
	filp->f_pos = off;
	return off;
}

/* These fops are a bit daft since read and write interfaces don't map well to IO registers.
 *
 * One ioctl per register would likely be the saner option. But we are lazy.
 *
 * We use the fact that every IO is aligned to 4 bytes. Misaligned reads means EOF. */
static struct file_operations fops = {
	.owner = THIS_MODULE,
	.llseek = llseek,
	.read = read,				 // it will be called when the user-space called read(fd, buf, count)
	.unlocked_ioctl = dev_ioctl, // it will be called when the user-space called the ioctl(fd, cmd, arg) funtion
	.write = write,				 // it will be called when the user-space called write(fd, buf, count)
};

static irqreturn_t irq_handler(int irq, void *dev)
{
	int devi;
	irqreturn_t ret;
	u32 irq_status;

	devi = *(int *)dev;
	if (devi == major)
	{
		irq_status = ioread32(mmio + IO_IRQ_STATUS);
		pr_info("interrupt irq = %d dev = %d irq_status = %llx\n",
				irq, devi, (unsigned long long)irq_status);
		/* Must do this ACK, or else the interrupts just keeps firing. */
		iowrite32(irq_status, mmio + IO_IRQ_ACK);
		ret = IRQ_HANDLED;
	}
	else
	{
		ret = IRQ_NONE;
	}
	return ret;
}

/**
 * Called just after insmod if the hardware device is connected,
 * not called otherwise.
 *
 * 0: all good
 * 1: failed
 */
static int pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	u8 val;
	int ret;

	pr_info("pci_probe\n");

	/* Allocate device number dynamically */
	ret = alloc_chrdev_region(&dev_num, 0, 1, CDEV_NAME);
	if (ret < 0)
	{
		dev_err(&(dev->dev), "alloc_chrdev_region failed\n");
		goto error;
	}
	major = MAJOR(dev_num);

	/* Initialize and add character device */
	cdev_init(&cpcidev_cdev, &fops);
	cpcidev_cdev.owner = THIS_MODULE;
	ret = cdev_add(&cpcidev_cdev, dev_num, 1);
	if (ret < 0)
	{
		dev_err(&(dev->dev), "cdev_add failed\n");
		goto error_cdev_add;
	}

/* Create device class - this allows automatic /dev node creation */
/* Note: class_create() API changed in kernel 6.4+ (removed THIS_MODULE parameter) */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	cpcidev_class = class_create("cpcidev_class");
#else
	cpcidev_class = class_create(THIS_MODULE, "cpcidev_class");
#endif
	if (IS_ERR(cpcidev_class))
	{
		dev_err(&(dev->dev), "class_create failed\n");
		ret = PTR_ERR(cpcidev_class);
		goto error_class;
	}

	/* Create device - this automatically creates /dev/cpcidev_pci */
	cpcidev_device = device_create(cpcidev_class, NULL, dev_num,
								   NULL, CDEV_NAME);
	if (IS_ERR(cpcidev_device))
	{
		dev_err(&(dev->dev), "device_create failed\n");
		ret = PTR_ERR(cpcidev_device);
		goto error_device;
	}

	pdev = dev;

	if (pci_enable_device(dev) < 0)
	{
		dev_err(&(pdev->dev), "pci_enable_device\n");
		goto error_pci;
	}

	if (pci_request_region(dev, BAR, "myregion0"))
	{
		dev_err(&(pdev->dev), "pci_request_region\n");
		goto error_pci;
	}
	mmio = pci_iomap(pdev, BAR, pci_resource_len(pdev, BAR));

	/* IRQ setup. */
	pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &val);
	pci_irq = val;

	if (request_irq(pci_irq, irq_handler, IRQF_SHARED, "pci_irq_handler0", &major) < 0)
	{
		dev_err(&(dev->dev), "request_irq\n");
		goto error_pci;
	}

	/* Optional sanity checks. The PCI is ready now, all of this could also be called from fops. */
	{
		unsigned i;

		/* Check that we are using MEM instead of IO.
		 *
		 * In QEMU, the type is defiened by either:
		 *
		 * - PCI_BASE_ADDRESS_SPACE_IO
		 * - PCI_BASE_ADDRESS_SPACE_MEMORY
		 */
		if ((pci_resource_flags(dev, BAR) & IORESOURCE_MEM) != IORESOURCE_MEM)
		{
			dev_err(&(dev->dev), "pci_resource_flags\n");
			goto error_pci;
		}

		/* 1Mb, as defined by the "1 << 20" in QEMU's memory_region_init_io. Same as pci_resource_len. */
		resource_size_t start = pci_resource_start(pdev, BAR);
		resource_size_t end = pci_resource_end(pdev, BAR);
		pr_info("length %llx\n", (unsigned long long)(end + 1 - start));

		/* The PCI standardized 64 bytes of the configuration space, see LDD3. */
		for (i = 0; i < 64u; ++i)
		{
			pci_read_config_byte(pdev, i, &val);
			pr_info("config %x %x\n", i, val);
		}
		pr_info("irq %x\n", pci_irq);

		/* Initial value of the IO memory. */
		for (i = 0; i < 0x28; i += 4)
		{
			pr_info("io %x %x\n", i, ioread32((void *)(mmio + i)));
		}

		/* Read again values of the IO memory. */
		for (i = 0; i < 0x28; i += 4)
		{
			pr_info("io %x %x\n", i, ioread32((void *)(mmio + i)));
		}
	}
	printk(KERN_INFO "CPCIDEV: /dev/%s created automatically (major=%d, minor=%d)\n",
		   CDEV_NAME, MAJOR(dev_num), MINOR(dev_num));
	printk(KERN_INFO "pci_probe() returns\n");
	return 0;

error_pci:
	device_destroy(cpcidev_class, dev_num);
error_device:
	class_destroy(cpcidev_class);
error_class:
	cdev_del(&cpcidev_cdev);
error_cdev_add:
	unregister_chrdev_region(dev_num, 1);
error:
	return 1;
}

static void pci_remove(struct pci_dev *dev)
{
	pr_info("pci_remove\n");
	free_irq(pci_irq, &major);
	pci_iounmap(pdev, mmio);
	pci_release_region(dev, BAR);

	/* Destroy device and class - this removes /dev/cpcidev_pci */
	device_destroy(cpcidev_class, dev_num);
	class_destroy(cpcidev_class);
	cdev_del(&cpcidev_cdev);
	unregister_chrdev_region(dev_num, 1);

	pr_info("CPCIDEV: /dev/%s removed\n", CDEV_NAME);
}

static struct pci_driver pci_driver = {
	.name = "cpcidev_driver",
	.id_table = pci_ids,
	.probe = pci_probe,
	.remove = pci_remove,
};

static int myinit(void)
{
	// registration of the PCI driver.
	if (pci_register_driver(&pci_driver) < 0)
	{
		printk(KERN_INFO "CPCIDEV error registering the custom_qemu_device_driver\n");
		return 1;
	}
	printk(KERN_INFO "CPCIDEV success on registering the custom_qemu_device_driver\n");
	return 0;
}

static void myexit(void)
{
	pci_unregister_driver(&pci_driver);
}

module_init(myinit);
module_exit(myexit);


