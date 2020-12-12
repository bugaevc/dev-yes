#include <linux/init.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/slab.h>

struct yes_data {
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
	struct yes_data *data = file->private_data;
	size_t nread = 0;

	if (unlikely(!data))
		data = file->private_data = kmalloc(sizeof(struct yes_data), GFP_KERNEL | __GFP_ZERO);
	if (unlikely(!data->exploded_data))
		explode(data);

	while (nread < count) {
		size_t to_copy = min(count - nread, data->exploded_size - data->position);
		size_t copied = to_copy - copy_to_user(buf + nread, data->exploded_data + data->position, to_copy);
		nread += copied;
		data->position += copied;
		data->position %= data->exploded_size;
		if (copied < to_copy)
			return -EFAULT;
	}

	return nread;
}

static ssize_t yes_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
	struct yes_data *data = file->private_data;

	if (likely(!data)) {
		data = file->private_data = kmalloc(sizeof(struct yes_data), GFP_KERNEL | __GFP_ZERO);
		data->written_data = memdup_user(buf, count);
		data->written_size = count;
	} else {
		data->written_data = krealloc(data->written_data, data->written_size + count, GFP_USER);
		if (copy_from_user(data->written_data + data->written_size, buf, count))
			return -EFAULT;
		data->written_size += count;
		kfree(data->exploded_data);
		data->exploded_data = NULL;
		data->exploded_size = 0;
	}

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
