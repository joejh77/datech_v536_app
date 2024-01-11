// daSystemSetup.h: interface for the CDaSystemSetup class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DASYSTEMSETUP_H__INCLUDED_)
#define AFX_DASYSTEMSETUP_H__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <string>
#include <stdio.h>
#include <stdlib.h>
#include "datypes.h"
#include "daversion.h"
#include "daappconfigs.h"
#include "ConfigTextFile.h"

/*
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

                Definition

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/


class CDaSystemSetup
{
public:
	static bool cfg_get(ST_CFG_DAVIEW *p_cfg, ST_CFG_DAVIEW *p_back_cfg = NULL);
	static bool cfg_set(ST_CFG_DAVIEW *p_cfg);
public:

	CDaSystemSetup(void);
	virtual ~CDaSystemSetup(void);

	bool Init(void);
	bool Deinit(void);
	bool cfg_backup(void);
	bool isInit(void) { return is_config_init; }

	ST_CFG_DAVIEW	cfg;
	bool is_config_init;
};

#endif // !defined(AFX_DASYSTEMSETUP_H__INCLUDED_)
