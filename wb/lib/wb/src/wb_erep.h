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
 **/

#ifndef wb_erep_h
#define wb_erep_h

#include <map>
#include <string>
#include <vector>
#include <string.h>
#include "wb_pwrs.h"

using namespace std;

class wb_merep;
class wb_vrep;

class wb_cdrep;
class wb_orep;
class wb_tdrep;
class wb_adrep;
class wb_name;

class wb_volcheck 
{ 
 public:
  wb_volcheck() {}
  wb_volcheck( char *vname, char *filename, pwr_tVid vid, pwr_tTime time) :
    m_vid(vid), m_time(time) 
  {
    strncpy( m_vname, vname, sizeof(m_vname));
    strncpy( m_filename, filename, sizeof(m_filename));
  }
  char m_vname[80];
  char m_filename[80];
  pwr_tVid m_vid;
  pwr_tCid m_cid;
  pwr_tTime m_time;
  pwr_tUInt32 m_dvversion;
};

class wb_erep
{
  typedef map<pwr_tVid, wb_vrep*>::iterator vrep_iterator;
  typedef map< string, wb_tMethod>::iterator methods_iterator;
  typedef vector<wb_vrep*>::iterator buffer_iterator;

  unsigned int m_nRef;
  wb_merep *m_merep;
  map<pwr_tVid, wb_vrep*> m_vrepdb;
  map<pwr_tVid, wb_vrep*> m_vrepdbs;
  map<pwr_tVid, wb_vrep*> m_vrepextern;
  vector<wb_vrep*> m_vrepbuffer;
  map< string, wb_tMethod> m_methods;

  char m_dir_list[25][200];
  int m_dir_cnt;
  int m_volatile_idx;
  int m_buffer_max;
  bool m_ref_merep_occupied;
  unsigned int m_options;

public:
  wb_erep( unsigned int options = 0);
  ~wb_erep();
  void unref();
  wb_erep *ref();

  //map<string
  wb_merep *merep() { return m_merep;}
  wb_vrep *volume(pwr_tStatus *sts);
  wb_vrep *volume(pwr_tStatus *sts, pwr_tVid vid);
  wb_vrep *volume(pwr_tStatus *sts, const char *name);
  wb_vrep *createVolume(pwr_tStatus *sts, pwr_tVid vid, pwr_tCid cid, const char *name,
			ldh_eVolRep type, char *server, bool add = true);
  wb_vrep *nextVolume(pwr_tStatus *sts, pwr_tVid vid);
  wb_vrep *externVolume(pwr_tStatus *sts, pwr_tVid vid);
  wb_vrep *bufferVolume(pwr_tStatus *sts);
  wb_vrep *bufferVolume(pwr_tStatus *sts, char *name);
  wb_vrep *findBuffer( pwr_tVid vid);
  void addDb( pwr_tStatus *sts, wb_vrep *vrep);
  void addDbs( pwr_tStatus *sts, wb_vrep *vrep);
  void addExtern( pwr_tStatus *sts, wb_vrep *vrep);
  void addBuffer( pwr_tStatus *sts, wb_vrep *vrep);
  void removeDb( pwr_tStatus *sts, wb_vrep *vrep);
  void removeDbs( pwr_tStatus *sts, wb_vrep *vrep);
  void removeExtern( pwr_tStatus *sts, wb_vrep *vrep);
  void removeBuffer( pwr_tStatus *sts, wb_vrep *vrep);
  void load( pwr_tStatus *sts, char *db);

  wb_orep *object( pwr_tStatus *sts, pwr_tOid oid);
  wb_orep *object( pwr_tStatus *sts, wb_name &name);
  wb_orep *object( pwr_tStatus *sts, const char *name);

  wb_cdrep *cdrep( pwr_tStatus *sts, const wb_orep& o);
  wb_tdrep *tdrep( pwr_tStatus *sts, const wb_adrep& a);
  void method( pwr_tStatus *sts, char *methodName, wb_tMethod *method);
  int nextVolatileVid( pwr_tStatus *sts, char *name);
  void setRefMerep( wb_merep *merep);
  void resetRefMerep();
  bool refMerepOccupied() { return m_ref_merep_occupied;}
  bool check_lock( char *name, ldh_eVolDb type);
  void checkVolumes( pwr_tStatus *sts, char *nodeconfigname);
  static void printMethods();
  static void volumeNameToFilename( pwr_tStatus *sts, char *name, char *filename);

private:
  void loadDirList( pwr_tStatus *status);
  void loadCommonMeta( pwr_tStatus *status);
  void loadMeta( pwr_tStatus *status, char *db);
  void loadLocalWb( pwr_tStatus *sts);
  void bindMethods();
  void checkVolume( pwr_tStatus *sts, pwr_tVid vid, vector<wb_volcheck> &carray, int *err_cnt);

  static void at_exit();
};

#endif