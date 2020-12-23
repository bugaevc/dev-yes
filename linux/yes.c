#include <linux/init.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/rwsem.h>
#include <linux/mutex.h>

DEFINE_MUTEX(private_data_mutex);

struct yes_data {
	struct rw_semaphore sem;
	char *written_data;
	size_t written_size;
	char *exploded_data;
	size_t exploded_size;
	size_t position;
};

static int yes_open(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static int yes_release(struct inode *inode, struct file *file)
{
	if (likely(file->private_data)) {
		struct yes_data *data = file->private_data;
		kfree(data->written_data);
		kfree(data->exploded_data);
		kfree(data);
	}
	return 0;
}

static long explode(struct yes_data *data)
{
	if (unlikely(data->written_size))
		data->exploded_size = roundup(PAGE_SIZE * 2, data->written_size);
	else
		data->exploded_size = PAGE_SIZE * 2;

	data->exploded_data = kmalloc(data->exploded_size, GFP_USER);

	if (IS_ERR_VALUE(data->exploded_data)) {
		long err = PTR_ERR(data->exploded_data);
		data->exploded_data = NULL;
		data->exploded_size = 0;
		return err;
	}

	if (unlikely(data->written_data)) {
		size_t nexploded = 0;

		while (nexploded < data->exploded_size) {
			size_t to_copy = min(data->exploded_size - nexploded, data->written_size);
			memcpy(data->exploded_data + nexploded, data->written_data, to_copy);
			nexploded += to_copy;
		}
	} else {
		const uint16_t yn = *(const uint16_t *)"y\n";
		memset16((uint16_t *)data->exploded_data, yn, data->exploded_size / 2);
	}

	return 0;
}

static ssize_t yes_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
	struct yes_data *data = READ_ONCE(file->private_data);
	size_t nread = 0;
	long ret = 0;

	if (unlikely(!data)) {
		if (mutex_lock_interruptible(&private_data_mutex))
			return -ERESTARTSYS;
		data = file->private_data;
		if (likely(!data)) {
			data = file->private_data = kmalloc(sizeof(struct yes_data), GFP_KERNEL | __GFP_ZERO);
			init_rwsem(&data->sem);
		}
		mutex_unlock(&private_data_mutex);
	}

	if (down_read_killable(&data->sem))
		return -ERESTARTSYS;
	if (unlikely(!data->exploded_data)) {
		up_read(&data->sem);
		if (down_write_killable(&data->sem))
			return -ERESTARTSYS;
		if (likely(!data->exploded_data))
			ret = explode(data);
		if (unlikely(ret)) {
			up_write(&data->sem);
			return ret;
		}
		downgrade_write(&data->sem);
	}

	while (nread < count) {
		size_t to_copy = min(count - nread, data->exploded_size - data->position);
		size_t copied = to_copy - copy_to_user(buf + nread, data->exploded_data + data->position, to_copy);
		nread += copied;
		data->position += copied;
		data->position %= data->exploded_size;
		if (copied < to_copy) {
			ret = -EFAULT;
			break;
		}
	}

	up_read(&data->sem);
	if (ret)
		return ret;
	return nread;
}

static ssize_t yes_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
	struct yes_data *data = READ_ONCE(file->private_data);

	if (likely(!data)) {
		if (mutex_lock_interruptible(&private_data_mutex))
			return -ERESTARTSYS;
		data = file->private_data;
		if (unlikely(data)) {
			mutex_unlock(&private_data_mutex);
			goto do_realloc;
		}
		data = file->private_data = kmalloc(sizeof(struct yes_data), GFP_KERNEL | __GFP_ZERO);
		data->written_data = memdup_user(buf, count);
		data->written_size = count;
		init_rwsem(&data->sem);
		mutex_unlock(&private_data_mutex);
		return count;
	}

do_realloc:
	if (down_write_killable(&data->sem))
		return -ERESTARTSYS;

	data->written_data = krealloc(data->written_data, data->written_size + count, GFP_USER);
	if (copy_from_user(data->written_data + data->written_size, buf, count)) {
		up_write(&data->sem);
		return -EFAULT;
	}
	data->written_size += count;
	kfree(data->exploded_data);
	data->exploded_data = NULL;
	data->exploded_size = 0;

	up_write(&data->sem);
	return count;
}

static const struct file_operations yes_fops = {
	.owner = THIS_MODULE,
	.open = yes_open,
	.release = yes_release,
	.read = yes_read,
	.write = yes_write,
};

static struct miscdevice yes_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "yes",
	.fops = &yes_fops,
};

module_misc_device(yes_device);


MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Sergey Bugaev <bugaevc@gmail.com>");
MODULE_DESCRIPTION("Kernel-side yes implementation");
MODULE_SUPPORTED_DEVICE("yes");
