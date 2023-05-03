/* /dev/yes translator
   Copyright (C) 2020 Sergey Bugaev <bugaevc@gmail.com>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>. */

/* Proudly written in GNU nano. */

#define _GNU_SOURCE 1

#include <hurd/trivfs.h>
#include <argp.h>
#include <error.h>
#include <sys/mman.h>
#include <unistd.h>

int trivfs_fsid = 0;
int trivfs_fstype = FSTYPE_MISC;

int trivfs_support_read = 1;
int trivfs_support_write = 1;
int trivfs_support_exec = 0;

int trivfs_allow_open = O_READ | O_WRITE;

struct yes
{
  char *written_data;
  size_t written_size;
  char *exploded_data;
  size_t exploded_size;
  off_t offset;
};

static inline size_t
round_up (size_t value, size_t base)
{
  size_t remainder = value % base;
  if (remainder)
    return (value / base + 1) * base;
  return value;
}

static error_t
explode (struct yes *yes)
{
  if (yes->written_size)
    yes->exploded_size = round_up (getpagesize () * 2, yes->written_size);
  else
    yes->exploded_size = getpagesize () * 2;

  yes->exploded_data = malloc (yes->exploded_size);
  if (!yes->exploded_data)
    {
      yes->exploded_size = 0;
      return ENOMEM;
    }

  if (yes->written_data)
    {
      size_t nexploded = 0;

      while (nexploded < yes->exploded_size)
        {
          memcpy (yes->exploded_data + nexploded, yes->written_data, yes->written_size);
          nexploded += yes->written_size;
        }
    }
  else
    {
      const uint16_t yn = *(const uint16_t *) "y\n";
      for (size_t i = 0; i * 2 < yes->exploded_size; i++)
        ((uint16_t *) yes->exploded_data)[i] = yn;
    }

  return 0;
}

static error_t
open_hook (struct trivfs_peropen *peropen)
{
  struct yes *yes = malloc (sizeof (struct yes));
  if (yes == NULL)
    return ENOMEM;

  memset (yes, 0, sizeof (struct yes));
  peropen->hook = yes;
  return 0;
}

static void
close_hook (struct trivfs_peropen *peropen)
{
  struct yes *yes = peropen->hook;

  if (yes == NULL)
    return;

  free (yes->written_data);
  free (yes->exploded_data);
  free (yes);
  peropen->hook = NULL;
}

error_t (*trivfs_peropen_create_hook) (struct trivfs_peropen *) = open_hook;
void (*trivfs_peropen_destroy_hook) (struct trivfs_peropen *) = close_hook;

void trivfs_modify_stat (struct trivfs_protid *cred, io_statbuf_t *st)
{
  st->st_size = 0;
  st->st_blocks = 0;
  st->st_mode &= ~(S_IFMT | ALLPERMS);
  st->st_mode |= S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
}

error_t trivfs_goaway (struct trivfs_control *cntl, int flags)
{
  exit (0);
}

kern_return_t
trivfs_S_io_readable (struct trivfs_protid *cred,
                      mach_port_t reply,
                      mach_msg_type_name_t reply_type,
                      mach_msg_type_number_t *amount)
{
  if (!cred)
    return EOPNOTSUPP;
  else if (!(cred->po->openmodes & O_READ))
    return EBADF;

  *amount = 2 * getpagesize ();
  return 0;
}

kern_return_t
trivfs_S_io_read (struct trivfs_protid *cred,
                  mach_port_t reply,
                  mach_msg_type_name_t reply_type,
                  data_t *data,
                  mach_msg_type_number_t *data_len,
                  loff_t offset,
                  vm_size_t amount)
{
  struct yes *yes;
  vm_size_t nread = 0;

  if (!cred)
    return EOPNOTSUPP;
  else if (!(cred->po->openmodes & O_READ))
    return EBADF;

  yes = cred->po->hook;

  if (yes->exploded_data == NULL)
    {
      error_t err = explode (yes);
      if (err != 0)
        return err;
    }

  if (*data_len < amount)
    {
      *data = mmap (0, amount, PROT_READ | PROT_WRITE, MAP_ANON, 0, 0);
      if (*data == MAP_FAILED)
        return ENOMEM;
    }

  yes->offset %= yes->exploded_size;
  while (nread < amount)
    {
      size_t to_copy = amount - nread;
      if (to_copy > yes->exploded_size - yes->offset)
        to_copy = yes->exploded_size - yes->offset;

      memcpy ((*data) + nread, yes->exploded_data + yes->offset, to_copy);
      nread += to_copy;
      yes->offset += to_copy;
      yes->offset %= yes->exploded_size;
    }

    *data_len = amount;
    return 0;
}

kern_return_t
trivfs_S_io_write (struct trivfs_protid *cred,
                   mach_port_t reply,
                   mach_msg_type_name_t reply_type,
                   const_data_t data,
                   mach_msg_type_number_t data_len,
                   loff_t offset,
                   vm_size_t *amount)
{
  struct yes *yes;
  char *new_written_data;

  if (!cred)
    return EOPNOTSUPP;
  else if (!(cred->po->openmodes & O_READ))
    return EBADF;

  yes = cred->po->hook;

  new_written_data = realloc (yes->written_data, yes->written_size + data_len);
  if (new_written_data == NULL)
    return ENOMEM;
  yes->written_data = new_written_data;
  memcpy (yes->written_data + yes->written_size, data, data_len);
  yes->written_size += data_len;

  free (yes->exploded_data);
  yes->exploded_data = NULL;
  yes->exploded_size = 0;

  *amount = data_len;
  return 0;
}

kern_return_t
trivfs_S_io_seek (struct trivfs_protid *cred,
                  mach_port_t reply,
                  mach_msg_type_name_t replytype,
                  loff_t offset,
                  int whence,
                  loff_t *newp)
{
  struct yes *yes;

  if (!cred)
    return EOPNOTSUPP;
  yes = cred->po->hook;

  switch (whence)
    {
    case SEEK_SET:
      break;
    case SEEK_END:
      return EOPNOTSUPP;
    case SEEK_CUR:
      offset += yes->offset;
      break;
    default:
      return EINVAL;
    }

  if (offset < 0)
    return EINVAL;

  yes->offset = *newp = offset;
  return 0;
}

kern_return_t
trivfs_S_io_select (struct trivfs_protid *cred,
                    mach_port_t reply,
                    mach_msg_type_name_t replytype,
                    int *seltype)
{
  if (!cred)
    return EOPNOTSUPP;

  *seltype = SELECT_READ | SELECT_WRITE;
  return 0;
}

error_t
trivfs_append_args (struct trivfs_control *fsys,
                    char **argz, size_t *argz_len)
{
  return 0;
}

int
main (int argc, char **argv)
{
  error_t err;
  mach_port_t bootstrap;
  struct trivfs_control *fsys;

  task_get_bootstrap_port (mach_task_self (), &bootstrap);
  if (bootstrap == MACH_PORT_NULL)
    error (1, 0, "Must be started as a translator");

  err = trivfs_startup (bootstrap, 0, 0, 0, 0, 0, &fsys);
  mach_port_deallocate (mach_task_self (), bootstrap);
  if (err)
    error (3, err, "trivfs_startup");

  ports_manage_port_operations_one_thread (fsys->pi.bucket, trivfs_demuxer, 0);

  return 0;
}
