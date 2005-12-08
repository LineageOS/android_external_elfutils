/* Return section header.
   Copyright (C) 1998, 1999, 2000, 2001, 2002, 2005 Red Hat, Inc.
   Written by Ulrich Drepper <drepper@redhat.com>, 1998.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, version 2.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <errno.h>
#include <unistd.h>

#include <system.h>
#include "libelfP.h"
#include "common.h"

#ifndef LIBELFBITS
# define LIBELFBITS 32
#endif


ElfW2(LIBELFBITS,Shdr) *
elfw2(LIBELFBITS,getshdr) (scn)
     Elf_Scn *scn;
{
  ElfW2(LIBELFBITS,Shdr) *result;

  if (scn == NULL)
    return NULL;

  if (unlikely (scn->elf->state.elf.ehdr == NULL))
    {
      __libelf_seterrno (ELF_E_WRONG_ORDER_EHDR);
      return NULL;
    }

  if (unlikely (scn->elf->class != ELFW(ELFCLASS,LIBELFBITS)))
    {
      __libelf_seterrno (ELF_E_INVALID_CLASS);
      return NULL;
    }

  result = scn->shdr.ELFW(e,LIBELFBITS);
  if (result == NULL)
    {
      /* Read the section header table.  */
      Elf *elf = scn->elf;
      ElfW2(LIBELFBITS,Ehdr) *ehdr = elf->state.ELFW(elf,LIBELFBITS).ehdr;

      rwlock_wrlock (elf->lock);

      /* Try again, maybe the data is there now.  */
      result = scn->shdr.ELFW(e,LIBELFBITS);
      if (result != NULL)
	goto out;

      size_t shnum;
      if (INTUSE (elf_getshnum) (elf, &shnum) != 0)
	goto out;
      size_t size = shnum * sizeof (ElfW2(LIBELFBITS,Shdr));

      /* Allocate memory for the program headers.  We know the number
	 of entries from the ELF header.  */
      ElfW2(LIBELFBITS,Shdr) *shdr = elf->state.ELFW(elf,LIBELFBITS).shdr =
	(ElfW2(LIBELFBITS,Shdr) *) malloc (size);
      if (elf->state.ELFW(elf,LIBELFBITS).shdr == NULL)
	{
	  __libelf_seterrno (ELF_E_NOMEM);
	  goto out;
	}
      elf->state.ELFW(elf,LIBELFBITS).shdr_malloced = 1;

      if (elf->map_address != NULL)
	{
	  ElfW2(LIBELFBITS,Shdr) *notcvt;

	  /* All the data is already mapped.  If we could use it
	     directly this would already have happened.  */
	  assert (ehdr->e_ident[EI_DATA] != MY_ELFDATA
		  || (! ALLOW_UNALIGNED
		      && (((uintptr_t) elf->map_address + elf->start_offset
			   + ehdr->e_shoff)
			  & (__alignof__ (ElfW2(LIBELFBITS,Shdr)) - 1)) != 0));

	  /* Now copy the data and at the same time convert the byte
	     order.  */
	  if (ALLOW_UNALIGNED
	      || (((uintptr_t) elf->map_address + elf->start_offset
		   + ehdr->e_shoff)
		  & (__alignof__ (ElfW2(LIBELFBITS,Shdr)) - 1)) == 0)
	    notcvt = (ElfW2(LIBELFBITS,Shdr) *)
	      ((char *) elf->map_address
	       + elf->start_offset + ehdr->e_shoff);
	  else
	    {
	      notcvt = (ElfW2(LIBELFBITS,Shdr) *) alloca (size);
	      memcpy (notcvt, ((char *) elf->map_address
			       + elf->start_offset + ehdr->e_shoff),
		      size);
	    }

	  for (size_t cnt = 0; cnt < shnum; ++cnt)
	    {
	      CONVERT_TO (shdr[cnt].sh_name, notcvt[cnt].sh_name);
	      CONVERT_TO (shdr[cnt].sh_type, notcvt[cnt].sh_type);
	      CONVERT_TO (shdr[cnt].sh_flags, notcvt[cnt].sh_flags);
	      CONVERT_TO (shdr[cnt].sh_addr, notcvt[cnt].sh_addr);
	      CONVERT_TO (shdr[cnt].sh_offset, notcvt[cnt].sh_offset);
	      CONVERT_TO (shdr[cnt].sh_size, notcvt[cnt].sh_size);
	      CONVERT_TO (shdr[cnt].sh_link, notcvt[cnt].sh_link);
	      CONVERT_TO (shdr[cnt].sh_info, notcvt[cnt].sh_info);
	      CONVERT_TO (shdr[cnt].sh_addralign, notcvt[cnt].sh_addralign);
	      CONVERT_TO (shdr[cnt].sh_entsize, notcvt[cnt].sh_entsize);
	    }
	}
      else if (likely (elf->fildes != -1))
	{
	  /* Read the header.  */
	  ssize_t n = pread_retry (elf->fildes,
				   elf->state.ELFW(elf,LIBELFBITS).shdr, size,
				   elf->start_offset + ehdr->e_shoff);
	  if (unlikely ((size_t) n != size))
	    {
	      /* Severe problems.  We cannot read the data.  */
	      __libelf_seterrno (ELF_E_READ_ERROR);
	      goto free_and_out;
	    }

	  /* If the byte order of the file is not the same as the one
	     of the host convert the data now.  */
	  if (ehdr->e_ident[EI_DATA] != MY_ELFDATA)
	    for (size_t cnt = 0; cnt < shnum; ++cnt)
	      {
		CONVERT (shdr[cnt].sh_name);
		CONVERT (shdr[cnt].sh_type);
		CONVERT (shdr[cnt].sh_flags);
		CONVERT (shdr[cnt].sh_addr);
		CONVERT (shdr[cnt].sh_offset);
		CONVERT (shdr[cnt].sh_size);
		CONVERT (shdr[cnt].sh_link);
		CONVERT (shdr[cnt].sh_info);
		CONVERT (shdr[cnt].sh_addralign);
		CONVERT (shdr[cnt].sh_entsize);
	      }
	}
      else
	{
	  /* The file descriptor was already enabled and not all data was
	     read.  Undo the allocation.  */
	  __libelf_seterrno (ELF_E_FD_DISABLED);

	free_and_out:
	  free (shdr);
	  elf->state.ELFW(elf,LIBELFBITS).shdr = NULL;
	  elf->state.ELFW(elf,LIBELFBITS).shdr_malloced = 0;

	  goto out;
	}

      /* Set the pointers in the `scn's.  */
      for (size_t cnt = 0; cnt < shnum; ++cnt)
	elf->state.ELFW(elf,LIBELFBITS).scns.data[cnt].shdr.ELFW(e,LIBELFBITS)
	  = &elf->state.ELFW(elf,LIBELFBITS).shdr[cnt];

      result = scn->shdr.ELFW(e,LIBELFBITS);
      assert (result != NULL);

    out:
      rwlock_unlock (elf->lock);
    }

  return result;
}
INTDEF(elfw2(LIBELFBITS,getshdr))
