/* 
 * Proview   $Id$
 * Copyright (C) 2005 SSAB Oxel�sund AB.
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
 * along with the program, if not, write to the Free Software 
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* rt_io_m_hilscher_cifx_master.c -- I/O methods for class Hilscher_cifX_Master. */

#include <float.h>

#include "pwr.h"
#include "pwr_basecomponentclasses.h"
#include "pwr_otherioclasses.h"
#include "co_cdh.h"
#include "rt_io_base.h"
#include "rt_io_bus.h"
#include "rt_io_agent_init.h"
#include "rt_io_agent_close.h"
#include "rt_io_agent_read.h"
#include "rt_io_agent_write.h"
#include "rt_io_msg.h"

#if defined PWRE_CONF_CIFX

#include "cifxlinux.h"
#include "cifXEndianess.h"
#include "rcX_Public.h"

#include "rt_io_m_hilscher_cifx.h"

#define CIFX_DEV "cifX0"

// One common handle to the driver
static CIFXHANDLE driver = 0;

static void get_diag( pwr_sClass_Hilscher_cifX_Diag *diag, CIFXHANDLE chan)
{
  int32_t sts;
  uint32_t state;


  CIFXHANDLE sysdevice = NULL;
  sts = xSysdeviceOpen( driver, CIFX_DEV, &sysdevice);
  if ( sts == CIFX_NO_ERROR) {
    SYSTEM_CHANNEL_SYSTEM_STATUS_BLOCK  statusblock = {0};

    sts = xSysdeviceInfo( sysdevice, CIFX_INFO_CMD_SYSTEM_STATUS_BLOCK, sizeof(statusblock), 
			  &statusblock);
    if ( sts == CIFX_NO_ERROR) {
      diag->SystemStatus = statusblock.ulSystemStatus;
      diag->SystemError = statusblock.ulSystemError;
      diag->TimeSinceStart.tv_sec = statusblock.ulTimeSinceStart;
      diag->CpuLoad = (float) statusblock.usCpuLoad / 100.0;
    }          
  }

  NETX_COMMON_STATUS_BLOCK csb = {0};

  sts = xChannelCommonStatusBlock( chan, CIFX_CMD_READ_DATA, 0, sizeof(csb), &csb);
  if ( sts == CIFX_NO_ERROR) {
    diag->CommState = csb.ulCommunicationState;
    diag->CommError = csb.ulCommunicationError;
    diag->ErrorCount = csb.ulErrorCount;
    diag->ConfigSlaves = csb.uStackDepended.tMasterStatusBlock.ulNumOfConfigSlaves;
    diag->ActiveSlaves = csb.uStackDepended.tMasterStatusBlock.ulNumOfActiveSlaves;
    diag->SlaveState = csb.uStackDepended.tMasterStatusBlock.ulSlaveState;
  }

  sts = xChannelHostState( chan, CIFX_HOST_STATE_READ, &state, 0);
  if ( sts == CIFX_NO_ERROR) {
    diag->HostState = state;
  }

  sts = xChannelBusState( chan, CIFX_BUS_STATE_GETSTATE, &state, 0);
  if ( sts == CIFX_NO_ERROR) {
    diag->BusState = state;
  }
}

static pwr_tStatus IoAgentInit( io_tCtx ctx,
				io_sAgent *ap)
{
  io_sLocalHilscher_cifX_Master *local;
  int sts;
  pwr_sClass_Hilscher_cifX_Master *op = (pwr_sClass_Hilscher_cifX_Master *)ap->op;

  local = (io_sLocalHilscher_cifX_Master *) calloc( 1, sizeof(io_sLocalHilscher_cifX_Master));
  ap->Local = local;

  if ( driver == 0) {
    struct CIFX_LINUX_INIT init;

    memset( &init, 0, sizeof(init));
    init.init_options  = CIFX_DRIVER_INIT_AUTOSCAN;
    init.trace_level   = 255;

    sts = cifXDriverInit( &init);
    if ( sts != CIFX_NO_ERROR) {
      xDriverGetErrorDescription( sts, op->ErrorStr, sizeof(op->ErrorStr));
      op->Status = sts;
      return IO__INITFAIL;
    }

    sts = xDriverOpen( &driver);
    if ( sts != CIFX_NO_ERROR) {
      xDriverGetErrorDescription( sts, op->ErrorStr, sizeof(op->ErrorStr));
      op->Status = sts;
      return IO__INITFAIL;
    }
  }
  
  // Find the board
  unsigned long board = 0;
  BOARD_INFORMATION boardinfo;
  boardinfo.lBoardError = 0;
  int found = 0;

  while ( xDriverEnumBoards( driver, board, sizeof(boardinfo), &boardinfo) 
	  == CIFX_NO_ERROR) {
    if ( cdh_NoCaseStrcmp( boardinfo.abBoardAlias, op->Alias) == 0) {
      found = 1;
      break;
    }
    board++;
  }
  if ( !found) {
    sprintf( op->ErrorStr, "Board with alias \"%s\" not found", op->Alias);
    return IO__INITFAIL;
  }

  local->board = board;

  op->Diag.DeviceNumber = boardinfo.tSystemInfo.ulDeviceNumber;
  op->Diag.SerialNumber = boardinfo.tSystemInfo.ulSerialNumber;
  op->Diag.SystemError = boardinfo.ulSystemError;

  local->channel = 0;


  local->chan = NULL;

  sts = xChannelOpen( NULL, CIFX_DEV, local->channel, &local->chan);
  if ( sts != CIFX_NO_ERROR) {
    xDriverGetErrorDescription( sts, op->ErrorStr, sizeof(op->ErrorStr));
    op->Status = sts;
    return IO__INITFAIL;
  }
  
  CHANNEL_INFORMATION channelinfo = {{0}};
  sts = xDriverEnumChannels( driver, board, local->channel, sizeof(channelinfo), &channelinfo);
  if ( sts == CIFX_NO_ERROR) {
    strncpy( op->Diag.FirmwareName, (char *)channelinfo.abFWName, sizeof(op->Diag.FirmwareName)); 
    snprintf( op->Diag.FirmwareVersion, sizeof(op->Diag.FirmwareVersion), 
	      "%u.%u.%u-%u (%4u-%02hu-%02hu)", channelinfo.usFWMajor, channelinfo.usFWMinor,
	      channelinfo.usFWBuild, channelinfo.usFWRevision, channelinfo.usFWYear,
	      channelinfo.bFWMonth, channelinfo.bFWDay);
  }


  // Init the I/O area
  unsigned int input_area_offset = 0;
  unsigned int input_area_chansize = 0;
  unsigned int output_area_offset = 0;
  unsigned int output_area_chansize = 0;
  io_sRack *rp;
  io_sCard *cp;

  for ( rp = ap->racklist; rp; rp = rp->next) {
    rp->Local =  calloc( 1, sizeof(io_sLocalHilscher_cifX_Device));

    rp->MethodDisabled = 1;

    // Show device offset and size
    if ( rp->Class == pwr_cClass_Hilscher_cifX_Device && rp->op) {
      ((pwr_sClass_Hilscher_cifX_Device *)rp->op)->InputAreaOffset = input_area_offset + 
	input_area_chansize;
      ((pwr_sClass_Hilscher_cifX_Device *)rp->op)->OutputAreaOffset = output_area_offset + 
	output_area_chansize;
    }

    // Get byte ordering
    pwr_tAName name;
    pwr_tEnum byte_ordering;

    strcpy( name, rp->Name);
    strcat( name, ".ByteOrdering");
    sts = gdh_GetObjectInfo( name, &byte_ordering, sizeof(byte_ordering));
    if ( ODD(sts))
      ((io_sLocalHilscher_cifX_Device *)rp->Local)->byte_ordering = byte_ordering;
    else
      ((io_sLocalHilscher_cifX_Device *)rp->Local)->byte_ordering = 
	pwr_eByteOrderingEnum_LittleEndian;

    for ( cp = rp->cardlist; cp; cp = cp->next) {
      cp->MethodDisabled = 1;

      // Show module offset and size
      if ( cp->Class == pwr_cClass_Hilscher_cifX_Module && cp->op) {
	((pwr_sClass_Hilscher_cifX_Module *)cp->op)->InputAreaOffset = input_area_offset + 
	  input_area_chansize;
	((pwr_sClass_Hilscher_cifX_Module *)cp->op)->OutputAreaOffset = output_area_offset + 
	  output_area_chansize;
      }

      io_bus_card_init( ctx, cp, &input_area_offset, &input_area_chansize,
			&output_area_offset, &output_area_chansize, byte_ordering);

      // Show module offset and size
      if ( cp->Class == pwr_cClass_Hilscher_cifX_Module && cp->op) {
	((pwr_sClass_Hilscher_cifX_Module *)cp->op)->InputAreaSize = input_area_offset + 
	  input_area_chansize - ((pwr_sClass_Hilscher_cifX_Module *)cp->op)->InputAreaOffset;
	((pwr_sClass_Hilscher_cifX_Module *)cp->op)->OutputAreaSize = output_area_offset + 
	  output_area_chansize - ((pwr_sClass_Hilscher_cifX_Module *)cp->op)->OutputAreaOffset;
      }
    }

    // Show device offset and size
    if ( rp->Class == pwr_cClass_Hilscher_cifX_Device && rp->op) {
      ((pwr_sClass_Hilscher_cifX_Device *)rp->op)->InputAreaSize = input_area_offset + 
	input_area_chansize - ((pwr_sClass_Hilscher_cifX_Device *)rp->op)->InputAreaOffset;
      ((pwr_sClass_Hilscher_cifX_Device *)rp->op)->OutputAreaSize = output_area_offset + 
	output_area_chansize - ((pwr_sClass_Hilscher_cifX_Device *)rp->op)->OutputAreaOffset;
    }

  }

  local->input_area_size = input_area_offset + input_area_chansize;
  local->output_area_size = output_area_offset + output_area_chansize;
  if ( local->input_area_size > 0)
    local->input_area = calloc( 1, local->input_area_size);
  if ( local->output_area_size > 0)
    local->output_area = calloc( 1, local->output_area_size);
      
  if ( ctx->ScanTime < 1 && ctx->ScanTime > FLT_EPSILON)
    local->diag_interval = (unsigned int) (1.0 / ctx->ScanTime + 0.5);
  else
    local->diag_interval = 1;

  get_diag( &op->Diag, local->chan);

  // It takes ~20 s to get COM-flag
  local->dev_init = 1;
  local->dev_init_limit = (unsigned int) (30.0 / ctx->ScanTime + 0.5);

  errh_Info( "Init of Hilsher cifX Master '%s'", ap->Name);
  return IO__SUCCESS;
}

static pwr_tStatus IoAgentClose( io_tCtx ctx,
				 io_sAgent *ap)
{
  io_sRack *rp;
  io_sLocalHilscher_cifX_Master *local = (io_sLocalHilscher_cifX_Master *)ap->Local;

  if ( driver) {
    xDriverClose( driver);
    driver = 0;
  }
      
  if ( local->input_area_size > 0)
    free( local->input_area);
  if ( local->output_area_size > 0)
    free( local->output_area);

  for ( rp = ap->racklist; rp; rp = rp->next)
    free( rp->Local);

  free( local);

  return IO__SUCCESS;
}

static pwr_tStatus IoAgentRead( io_tCtx ctx, io_sAgent *ap) 
{
  io_sLocalHilscher_cifX_Master *local = (io_sLocalHilscher_cifX_Master *)ap->Local;
  pwr_sClass_Hilscher_cifX_Master *op = (pwr_sClass_Hilscher_cifX_Master *)ap->op;
  io_sRack *rp;
  io_sCard *cp;
  int32_t sts;

  if ( local->diag_cnt == 0)
    get_diag( &op->Diag, local->chan);
  if ( local->diag_cnt > local->diag_interval)
    local->diag_cnt = 0;
  else
    local->diag_cnt++;

  sts = xChannelIORead( local->chan, 0, 0, local->input_area_size, local->input_area, 10);
  op->Status = sts;
  if ( sts == CIFX_NO_ERROR) {
    if ( local->dev_init)
      local->dev_init = 0;

    for ( rp = ap->racklist; rp; rp = rp->next) {
      for ( cp = rp->cardlist; cp; cp = cp->next) {

	io_bus_card_read( ctx, rp, cp, local->input_area, 0, 
			  ((io_sLocalHilscher_cifX_Device *)rp->Local)->byte_ordering,
			  pwr_eFloatRepEnum_FloatIEEE);
      }
    }
  }
  else {
    if ( sts == CIFX_DEV_NO_COM_FLAG && local->dev_init && 
	 local->dev_init_cnt < local->dev_init_limit)
      local->dev_init_cnt++;
    else {
      xDriverGetErrorDescription( sts, op->ErrorStr, sizeof(op->ErrorStr));
      op->ErrorCount++;
    }

    if ( op->ErrorCount == op->ErrorSoftLimit && !local->softlimit_logged) {
      errh_Error( "IO Error soft limit reached on agent '%s'", ap->Name);
      local->softlimit_logged = 1;
    }
    if ( op->ErrorCount >= op->ErrorHardLimit) {
      ctx->Node->EmergBreakTrue = 1;
      return IO__ERRDEVICE;
    }
  }
  
  
  return IO__SUCCESS;
}

static pwr_tStatus IoAgentWrite( io_tCtx ctx, io_sAgent *ap) 
{
  io_sLocalHilscher_cifX_Master *local = (io_sLocalHilscher_cifX_Master *)ap->Local;
  io_sRack *rp;
  io_sCard *cp;
  int32_t sts;
  pwr_sClass_Hilscher_cifX_Master *op = (pwr_sClass_Hilscher_cifX_Master *)ap->op;
    
  for ( rp = ap->racklist; rp; rp = rp->next) {
    for ( cp = rp->cardlist; cp; cp = cp->next) {
      
      io_bus_card_write( ctx, cp, local->output_area,
			 ((io_sLocalHilscher_cifX_Device *)rp->Local)->byte_ordering,
			 pwr_eFloatRepEnum_FloatIEEE);
    }
  }

  sts = xChannelIOWrite( local->chan, 0, 0, local->output_area_size, local->output_area, 10);
  op->Status = sts;
  if ( sts != CIFX_NO_ERROR) {

    if ( ! local->dev_init) {
      op->ErrorCount++;
      xDriverGetErrorDescription( sts, op->ErrorStr, sizeof(op->ErrorStr));
    }

    if ( op->ErrorCount == op->ErrorSoftLimit && !local->softlimit_logged) {
      errh_Error( "IO Error soft limit reached on agent '%s'", ap->Name);
      local->softlimit_logged = 1;
    }
    if ( op->ErrorCount >= op->ErrorHardLimit) {
      ctx->Node->EmergBreakTrue = 1;
      return IO__ERRDEVICE;
    }
  }

  return IO__SUCCESS;
}

#else
static pwr_tStatus IoAgentInit( io_tCtx ctx, io_sAgent *ap) { return IO__RELEASEBUILD;}
static pwr_tStatus IoAgentClose( io_tCtx ctx, io_sAgent *ap) { return IO__RELEASEBUILD;}
static pwr_tStatus IoAgentRead( io_tCtx ctx, io_sAgent *ap) { return IO__RELEASEBUILD;}
static pwr_tStatus IoAgentWrite( io_tCtx ctx, io_sAgent *ap) { return IO__RELEASEBUILD;}
#endif

/*  Every method should be registred here. */

pwr_dExport pwr_BindIoMethods(Hilscher_cifX_Master) = {
  pwr_BindIoMethod(IoAgentInit),
  pwr_BindIoMethod(IoAgentClose),
  pwr_BindIoMethod(IoAgentRead),
  pwr_BindIoMethod(IoAgentWrite),
  pwr_NullMethod
};
