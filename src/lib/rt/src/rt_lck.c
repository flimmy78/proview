/* 
 * Proview   Open Source Process Control.
 * Copyright (C) 2005-2017 SSAB EMEA AB.
 *
 * This file is part of Proview.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation, either version 2 of 
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with Proview. If not, see <http://www.gnu.org/licenses/>
 *
 * Linking Proview statically or dynamically with other modules is
 * making a combined work based on Proview. Thus, the terms and 
 * conditions of the GNU General Public License cover the whole 
 * combination.
 *
 * In addition, as a special exception, the copyright holders of
 * Proview give you permission to, from the build function in the
 * Proview Configurator, combine Proview with modules generated by the
 * Proview PLC Editor to a PLC program, regardless of the license
 * terms of these modules. You may copy and distribute the resulting
 * combined work under the terms of your choice, provided that every 
 * copy of the combined work is accompanied by a complete copy of 
 * the source code of Proview (the version used to produce the 
 * combined work), being distributed under the terms of the GNU 
 * General Public License plus this exception.
 */

#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "pwr.h"
#include "co_time.h"
#include "rt_gdh.h"
#include "rt_sect.h"
#include "rt_lck.h"

static char lck_cName[lck_eLock__][40] = {
  "/tmp/pwr_nmps_lock",
  "/tmp/pwr_time_lock",
  "/tmp/pwr_str_lock"
};

sect_sHead *lck_locksect[lck_eLock__] = { 0, 0};

void lck_Create( pwr_tStatus *sts, lck_eLock lock)
{
  pwr_tBoolean created;
  
  if ( lock >= lck_eLock__ || lck_locksect[lock]) {
    *sts = 0;
    return;
  }

  lck_locksect[lock] = sect_Alloc( sts, &created, 0, sizeof(sect_sMutex), 
				      lck_cName[lock], sect_mFlags_Create);
  if ( ODD(*sts) && created)
    sect_InitLock( sts, lck_locksect[lock], (sect_sMutex *)lck_locksect[lock]->base);
}

void lck_Unlink( pwr_tStatus *sts, lck_eLock lock)
{
  if ( lock >= lck_eLock__ || !lck_locksect[lock]) {
    *sts = 0;
    return;
  }

  if ( shmdt( lck_locksect[lock]) == -1)
    *sts = 0;
  else
    *sts = 1;
}

void lck_Delete( pwr_tStatus *sts, lck_eLock lock)
{
  if ( lock >= lck_eLock__ || !lck_locksect[lock]) {
    *sts = 0;
    return;
  }

  char   segname[128];
  char	 busid[8];
  char   *str = getenv(pwr_dEnvBusId);
  key_t  key;
  int    shm_id;
  struct shmid_ds   ds;

  strncpy( busid, (str ? str : "XXX"), 3);
  busid[3] = '\0';

  sprintf(segname, "%s_%.3s", lck_cName[lock], busid);

  key    = ftok(segname, 'P');
  shm_id = shmget(key, 0, 0660);
  if ( shmdt( lck_locksect[lock]->base) == -1)
    printf( "Detach of timesig lock failed\n");
  if ( shmctl(shm_id, IPC_RMID, &ds) == -1)
    printf( "Remove of timesig lock failed\n");
  // unlink(segname);
  posix_sem_unlink(segname);
  sect_Free( sts, lck_locksect[lock]);
}

