#include <mach/mach_types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <miscfs/devfs/devfs.h>

static int yes_open(dev_t dev, int flags, int devtype, struct proc *p)
{
	return 0;
}

static int yes_close(dev_t dev, int flags, int devtype, struct proc *p)
{
	return 0;
}

static int yes_read(dev_t dev, struct uio *uio, int ioflag)
{
	int ret = 0;

	static const char yn[] = "y\n";
	while (1) {
		user_size_t to_read = MIN(uio_resid(uio), 2);
		if (to_read == 0) {
			break;
		}
		ret = uiomove(yn, to_read, uio);
		if (ret) {
			break;
		}
	}
	return ret;
}

static int yes_select(dev_t dev, int which, void * wql, struct proc *p)
{
	return 1;
}

static struct cdevsw yes_csdevw =
{
	.d_open = yes_open,
	.d_close = yes_close,
	.d_read = yes_read,
	.d_write = eno_rdwrt,
	.d_ioctl = eno_ioctl,
	.d_stop = eno_stop,
	.d_reset = eno_reset,
	.d_ttys = 0,
	.d_select = yes_select,
	.d_mmap = eno_mmap,
	.d_strategy = eno_strat,
	.d_reserved_1 = eno_getc,
	.d_reserved_2 = eno_putc,
	.d_type = 0
};

static void *dev_handle;

static kern_return_t yes_start (kmod_info_t * ki, void * d) {
	int ret;

	ret = cdevsw_add(-1, &yes_csdevw);
	if (ret < 0) {
		return KERN_FAILURE;
	}

	dev_handle = devfs_make_node(makedev(ret, 0), DEVFS_CHAR, UID_ROOT, GID_WHEEL, 0444, "yes", 0);
	if (!dev_handle) {
		return KERN_FAILURE;
	}
	return KERN_SUCCESS;
}

static kern_return_t yes_stop (kmod_info_t * ki, void * d) {
	devfs_remove(dev_handle);
	return KERN_SUCCESS;
}

KMOD_EXPLICIT_DECL(yes, 1.0, yes_start, yes_stop);
