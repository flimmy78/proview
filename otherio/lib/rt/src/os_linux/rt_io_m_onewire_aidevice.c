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

/* rt_io_m_onewire_aidevice.c -- I/O methods for class OneWire_AiDevice. */

#include <float.h>
#include <math.h>

#include "pwr.h"
#include "pwr_basecomponentclasses.h"
#include "pwr_otherioclasses.h"
#include "co_time.h"
#include "rt_io_base.h"
#include "rt_io_card_init.h"
#include "rt_io_card_close.h"
#include "rt_io_card_read.h"
#include "rt_io_msg.h"
#include "rt_io_m_onewire.h"

static pwr_tStatus IoCardInit( io_tCtx ctx,
			       io_sAgent *ap,
			       io_sRack *rp,
			       io_sCard *cp)
{
  pwr_sClass_OneWire_AiDevice *op = (pwr_sClass_OneWire_AiDevice *)cp->op;
  io_sLocalAiDevice *local;
  pwr_tStatus sts;
  char name[40];
  pwr_tFileName fname, tmp;
  int name_len;
  char *s;

  if ( cp->chanlist[0].cop) {
    local = (io_sLocalAiDevice *) calloc( 1, sizeof(io_sLocalAiDevice));
    cp->Local = local;

    sprintf( name, "%d-%012x", op->Family, op->Super.Address);
    name_len = strlen(name);
    strncpy( fname, op->DataFile, sizeof(fname));
    
    // Replace all '%s' with 'family-serialnumber'
    s = fname;
    while ( (s = strstr( s, "%s"))) {
      strncpy( tmp, s+2, sizeof(tmp));
      strcpy( s, name);
      strncat( fname, tmp, sizeof(fname));
    }
    local->value_fp = fopen( fname, "r");
    if (!local->value_fp) {
      errh_Error( "OneWire_AiDevice Unable op open %s, '%ux'", cp->Name, 
		  op->Super.Address);
      sts = IO__INITFAIL;
      op->Status = sts;
      return sts;
    }

    io_AiRangeToCoef( &cp->chanlist[0]);

    errh_Info( "Init of OneWire_AiDevice '%s'", cp->Name);
  }
  return IO__SUCCESS;
}

static pwr_tStatus IoCardClose( io_tCtx ctx,
			        io_sAgent *ap,
			        io_sRack *rp,
			        io_sCard *cp)
{
  io_sLocalAiDevice *local = (io_sLocalAiDevice *)cp->Local;


  if ( cp->chanlist[0].cop) {
    fclose( local->value_fp);
  }
  free( cp->Local);
  return IO__SUCCESS;
}

static pwr_tStatus IoCardRead( io_tCtx ctx,
			       io_sAgent *ap,
			       io_sRack	*rp,
			       io_sCard	*cp)
{
  io_sLocalAiDevice *local = (io_sLocalAiDevice *)cp->Local;
  pwr_sClass_OneWire_AiDevice *op = (pwr_sClass_OneWire_AiDevice *)cp->op;
  char str[80];
  char *s;
  pwr_tUInt32 error_count = op->Super.ErrorCount;

  if ( op->ScanInterval > 1) {
    if ( local->interval_cnt != 0) {
      local->interval_cnt++;
      if ( local->interval_cnt >= op->ScanInterval)
        local->interval_cnt = 0;
      return IO__SUCCESS;
    }
    local->interval_cnt++;
  }

  if ( cp->chanlist[0].cop && cp->chanlist[0].sop) {
    io_sChannel *chanp = &cp->chanlist[0];
    pwr_sClass_ChanAi *cop = (pwr_sClass_ChanAi *)chanp->cop;
    pwr_sClass_Ai *sop = (pwr_sClass_Ai *)chanp->sop;
    pwr_tFloat32 actvalue;

    if ( cop->CalculateNewCoef)
      // Request to calculate new coefficients
      io_AiRangeToCoef( chanp);

    fflush( local->value_fp);
    fgets( str, sizeof(str), local->value_fp);
    fgets( str, sizeof(str), local->value_fp);
    rewind( local->value_fp);

    if ( strcmp( op->ValueSearchString, "") == 0)
      s = str;
    else
      s = strstr( str, op->ValueSearchString);
    if ( s) {
      switch ( op->ChAi.Representation) {

      case pwr_eDataRepEnum_Float32:
      case pwr_eDataRepEnum_Float64: {
	pwr_tFloat32 fvalue;

	sscanf( s+strlen(op->ValueSearchString), "%f", &fvalue);

	if ( op->ErrorValue != 0 && fabs( op->ErrorValue - fvalue) > FLT_EPSILON ) {
	  /* TODO Check CRC Probably power loss... 
	     op->Super.ErrorCount++; */
	}

	actvalue = cop->SensorPolyCoef0 + cop->SensorPolyCoef1 * fvalue;

	// Filter
	if ( sop->FilterType == 1 &&
	     sop->FilterAttribute[0] > 0 &&
	     sop->FilterAttribute[0] > ctx->ScanTime) {
	  actvalue = *(pwr_tFloat32 *)chanp->vbp + ctx->ScanTime / sop->FilterAttribute[0] *
	    (actvalue - *(pwr_tFloat32 *)chanp->vbp);
	}
      
	*(pwr_tFloat32 *)chanp->vbp = actvalue;
	sop->SigValue = cop->SigValPolyCoef1 * fvalue + cop->SigValPolyCoef0;
	sop->RawValue = fvalue;
	break;
      }
      default: {
	pwr_tInt32 ivalue;

	sscanf( s+strlen(op->ValueSearchString), "%d", &ivalue);
	
	io_ConvertAi32( cop, ivalue, &actvalue);

	if ( op->ErrorValue != 0 && fabs( op->ErrorValue - ivalue) > FLT_EPSILON ) {
	  /* TODO Check CRC Probably power loss... 
	     op->Super.ErrorCount++; */
	}

	// Filter
	if ( sop->FilterType == 1 &&
	     sop->FilterAttribute[0] > 0 &&
	     sop->FilterAttribute[0] > ctx->ScanTime) {
	  actvalue = *(pwr_tFloat32 *)chanp->vbp + ctx->ScanTime / sop->FilterAttribute[0] *
	    (actvalue - *(pwr_tFloat32 *)chanp->vbp);
	}
      
	*(pwr_tFloat32 *)chanp->vbp = actvalue;
	sop->SigValue = cop->SigValPolyCoef1 * ivalue + cop->SigValPolyCoef0;
	sop->RawValue = ivalue;
      }
      }
    }
    else {
      op->Super.ErrorCount++;
    }
  }

  if ( op->Super.ErrorCount >= op->Super.ErrorSoftLimit && 
       error_count < op->Super.ErrorSoftLimit) {
    errh_Warning( "IO Card ErrorSoftLimit reached, '%s'", cp->Name);
  }
  if ( op->Super.ErrorCount >= op->Super.ErrorHardLimit) {
    errh_Error( "IO Card ErrorHardLimit reached '%s', IO stopped", cp->Name);
    ctx->Node->EmergBreakTrue = 1;
    return IO__ERRDEVICE;
  }    

  return IO__SUCCESS;
}

/*  Every method should be registred here. */

pwr_dExport pwr_BindIoMethods(OneWire_AiDevice) = {
  pwr_BindIoMethod(IoCardInit),
  pwr_BindIoMethod(IoCardClose),
  pwr_BindIoMethod(IoCardRead),
  pwr_NullMethod
};