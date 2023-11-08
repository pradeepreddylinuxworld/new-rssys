/*drivers/input/touchscreen/mediatek/gslX680/
*
* 2010 - 2016 silead inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be a reference
* to you, when you are integrating the Sileadinc's CTP IC into your system,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*/

#ifndef _GSLX680_H_
#define _GSLX680_H_
#define ATA_TP_ADDR 0x5f80
#define SCREEN_MAX_X TPD_RES_X
#define SCREEN_MAX_Y TPD_RES_Y

#define TPD_ROTATION_SUPPORT
#ifdef TPD_ROTATION_SUPPORT
enum TPD_ROTATION_TYPE {
	TPD_ROTATION_90 = 0,
	TPD_ROTATION_180 = 1,
	TPD_ROTATION_270 = 2,
};
int tpd_rotation_type = TPD_ROTATION_180;/*TPD_ROTATION_180*/;
#endif
extern struct tpd_device *tpd;

struct gsl_touch_info {
	int x[10];
	int y[10];
	int id[10];
	int finger_num;
};
extern unsigned int gsl_mask_tiaoping(void);
extern unsigned int gsl_version_id(void);
extern void gsl_alg_id_main(struct gsl_touch_info *cinfo);
extern void gsl_DataInit(int *ret);

#define 	GSL_NOID_VERSION
/*--------------------start----------------------------*/
//#include "./cust_config/PX101C27A011.h"
/*--------------------end----------------------------*/

#endif /* #ifndef _GSLX680_H_ */
