#include "GINS_process_lib.h"
#include "GINS_process.h"
#include "GINS_test.h"
GINS_YM *s_pstGilc = NULL;
int GINS_init(GINS_cfg_t* cfgdata)
{
	//static GINS_Process gilc;
	//s_pstGilc = &gilc;
	return s_pstGilc->GINS_Init(cfgdata);
}
int GINS_PROCESS_Lib(GINS_raw_t* pstRaw, GINS_result_t* pstOut)
{
	//if (s_pstGilc)
	return s_pstGilc->GINS_PROCESS_Lib(pstRaw, pstOut);
	//else
	//return GILC_RET__RST_NOTHING;
	//return 0;
}


int GINS_YM::GINS_PROCESS_Lib(GINS_raw_t* pstRaw, GINS_result_t* pstOut)
{
/**********************************数据质量检验***********************************/
	GINS_data_correct(pstRaw);//输入数据准备：gnss状态检测、imu轴系调整
	int ret = GINS_Rawdata_Quality(pstRaw);//输入数据质量检测
	gtime_t gt = gpst2time(GI_pd.week, GI_pd.imutimetarge);//imu时间戳时间转换
	time2epoch(gpst2utc(gt), GI_pd.ep);
	//Mat_equal(GI_pd.ep, 6, 1, glv.ep);
	IMU_Filter(GI_pd);
	if (++imu_raw_cnt % (100 * 60) == 0)
	{
		GINS_log("read imu num %d \r\n", imu_raw_cnt);
	}
	GI_pd.imu_raw_cnt = imu_raw_cnt;
	if (!GetGPS)
	{
		GINS_log("GNSS Search star Err\r\n");
		if (GI_pd.bGPSavail)
		{
			GetGPS = true;
		}
	}
	if (GetGPS)
	{
/***********************************动静态判断***************************************/
		if (Rst_speed_pre&&Rst_speed_pre < 0.3)
		{
			static_num++;
		}
		if (static_num > 10)
		{
			GI_align.bStatic = GI_align.bStatic = 1;
			static_num--;
		}
		else
		{
			GI_align.bStatic = GI_align.bStatic = 0;
		}
		if (imu_raw_cnt % (100 * 6) == 0)
		{
			if (GI_align.bStatic == 1)
			{
				GINS_log("%f ----in static by gilc_speed-------\r\n", GI_pd.imutimetarge);
			}
			else
			{
				GINS_log("%f ----out static by gilc_speed-------\r\n", GI_pd.imutimetarge);
			}
		}
/*********************************陀螺静态零偏计算*************************************/
	if(!bAlign)
	{ 
		if (!bias_init)
		{
			int bias_flag=IMU_static_bias();
			if (bias_flag == 1)
			{
				for (int i = 0;i < 3;i++)
				{
					//eb[i] = gyro_bias[i];
				}
				GINS_log("gyo statis bias:%f,%f,%f\r\n", eb[0]*R2D, eb[1] * R2D, eb[2] * R2D);
				bias_init = true;
			}
		}
		//Mat_min(GI_pd.gyo, gyro_bias, GI_pd.gyo, 3, 1);//陀螺减去初始零偏
/*****************************************初始对准******************************************/
		bool align= GI_align.KinmateAlign(GI_pd, GI_pro);//位置、速度、姿态初始化
		if (align)
		{
			bAlign = true;
			GINS_log("%02d:%02d:%06.3f 初始对准完成，算法启动：%f\r\n",
				int(GI_pd.ep[3]), int(GI_pd.ep[4]), GI_pd.ep[5], GI_pd.imutimetarge);
			GINS_log("init_data:%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\r\n", GI_align.Att[0] * R2D, GI_align.Att[1] * R2D, GI_align.Att[2] * R2D,
				GI_align.Pos[0] * R2D, GI_align.Pos[1] * R2D, GI_align.Pos[2],
				GI_align.Vn[0], GI_align.Vn[1], GI_align.Vn[2], pstRaw->imutimetarget);
			//double lever[3] = { GI_cfg.fIns2GnssVector[0],GI_cfg.fIns2GnssVector[1],GI_cfg.fIns2GnssVector[2] };
			double lever[3] = { 0.0,0.0,0.0 };
			/*INS推算初始值*/
			/*当前陀螺零偏和加计零偏、安装误差角初值为0*/
			GI_pro.ins.Init(GI_align.Att, GI_align.Vn, GI_align.Pos, eb, db, lever, dGnss2OutPointLever, GI_align.PRY_Install);
			if (GI_align.Att[2])
			{
				Mat_equal(GI_align.Vn, 3, 1, GI_pro.ins.vn);
				Mat_equal(GI_align.Pos, 3, 1, GI_pro.ins.pos);
			}
			else
			{
				/*航向未初始化，以天线位置做为IMU初始位置*/
				Mat_equal(GI_align.Vn, 3, 1, GI_pro.ins.vn);
				Mat_equal(GI_align.Pos, 3, 1, GI_pro.ins.pos);
			}

			double cbn[9], vb[3];
			a2mat(GI_pro.ins.PRY_Install, GI_pro.ins.Cmb);//安装误差角转化为姿态矩阵
			a2mat(GI_pro.ins.att, GI_pro.ins.Cnb);//imu姿态转换为姿态矩阵

			Mat_tran(GI_pro.ins.Cnb, 3, 3, cbn);
			Mat_tran(GI_pro.ins.Cmb, 3, 3, GI_pro.ins.Cbm);


			Mat_mul(cbn, GI_pro.ins.vn, 3, 3, 1, vb);
			Mat_mul(GI_pro.ins.Cmb, vb, 3, 3, 1, GI_pro.ins.vm_car);//杆臂转换后的车辆速度

			Mat_mul(GI_pro.ins.Cnb, GI_pro.ins.Cbm, 3, 3, 3, GI_pro.ins.Cnm);//imu姿态杆臂转换
			m2att(GI_pro.ins.Cnm, GI_pro.ins.att_car);//车辆姿态

			GI_pro.preheading = GI_pro.ins.att[2];


			Mat_equal(GI_pd.gyo, 3, 1, GI_pro.ins.wmpre);
			Mat_equal(GI_pd.acc, 3, 1, GI_pro.ins.vmpre);
			GI_pro.tpre = GI_pd.imutimetarge - 0.01;
			GINS_ekf_data_ready(&GI_cfg);//滤波器参数初始 P阵 Q阵初始
		}
	}
	int gilc_flag = 0;
	if (bAlign)
	{
		GI_pro.ins.imutimetarge = GI_pd.imutimetarge;
		gilc_flag= GI_pro.GINS_P2(GI_pd);
		SaveRst(&GI_pro, &GI_pd);//结果数据保存
		GINS_result(pstOut, gilc_flag);//结果结构体赋值
	}
	if (GI_pd.bGPSavail)
	{
		pstOut->bPPSSync = GI_pro.bPPSSync;
	}

	stRaw_tmp.bGPSavail = false;
	stRaw_tmp.bMEMSavail = false;
	stRaw_tmp.bPPSavail = false;
	return gilc_flag;
	}
}

int GINS_Process::ZUpdate(Process_Data ilcd)
{
	GINS_INS ppsins;
	int iUpdateZ_flag = 0;
	int i = 0, j = 0, iRet = 0;
	double dpos[3] = { 0.0 }, dvn[3] = { 0.0 };
	double enu[3];
	double dAtt[3] = { 0 }, dvm[3] = { 0.0 };
	double dVel = 0.0, dYawRate = 0.0, dWh = 0.0;
	double dVd = 0.0, dDualYaw = 0.0;
	c++; //imu更新计数
	double ppsinspos[3] = { 0.0 }, ppsinsvn[3] = { 0.0 };
	double gnssHeading, insHeading, insYaw, gnssYaw, dheading;
	insHeading = atan2(ins.vnL[0], ins.vnL[1])*R2D;
	DEG_0_360(insHeading);
	insYaw = -ins.att_car[2] * R2D;
	DEG_0_360(insYaw);
	/*----速度姿态航向偏差----*/
	double dDiffHeadingYaw_ins = insHeading - insYaw;
	DEG_NEG180_180(dDiffHeadingYaw_ins);
/****************************************车辆速度+状态**********************************************/
	double wib_car[3] = { 0.0 };
	double speed_car = 0.0;
	Mat_mul(ins.Cmb, ins.wib, 3, 3, 1, wib_car);//车辆角速率分量
	speed_car = sqrt(SQ(ins.vm_car[0]) + SQ(ins.vm_car[1]));//车辆速度
	double dGyroZ = ins.wib[2] * R2D;
	if (dGyroZ > 5)
		bTurn = 1;//左转
	else if (dGyroZ < -5)
		bTurn = -1;//右转
	else
		bTurn = 0;//直行
	if (bTurn == 1 || bTurn == -1)
	{
		if (ilcd.imu_raw_cnt % (600) == 0)
		{
			GINS_log("%f car_turn:%d\n", ilcd.imutimetarge, bTurn);
		}
	}
/****************************************GNSS与INS时间匹配**********************************************/
	loadPPSSyncInsData(ilcd, ppsins);
	if (ilcd.bGPSavail)  //GNSS
	{
		//PPS时刻的INS位置
		Mat_equal(ppsins.posL, 3, 1, ppsinspos);
		Mat_equal(ppsins.vnL, 3, 1, ppsinsvn);

		Mat_min(ppsinspos, ilcd.pos, dpos, 3, 1);//位置差
		Mat_min(ppsinsvn, ilcd.vn, dvn, 3, 1);//速度差


		diffpos(ilcd.pos, ppsinspos, enu);//enu下位置差
		difpos_b(ilcd.pos, ppsinspos, ppsins.att_car, dposb_sync);//载体系下位置差

		Mat_equal(enu, 3, 1, dpos_sync);
		Mat_equal(dvn, 3, 1, dvn_sync);

		gnssYaw = ilcd.heading2;//双天线航向

		if (gnssYaw<0) gnssYaw += 360;

		/*速度航向角*/
		gnssHeading = atan2(ilcd.vn[0], ilcd.vn[1])*R2D;
		if (gnssHeading<0) gnssHeading += 360;
	}
	dheading = insHeading - gnssHeading;//INS速度航向与GNSS速度航向差
	DEG_NEG180_180(dheading);
	GnssIntegrity(ilcd, dheading, dvn);//GNSS质量判断，速度是否可用，是否跳点  busegnssvn=1速度可用
	/*----静态判断----*/
	if (bStatic != bStaticpre)
	{
		bStaticpre = bStatic;
		if (bStatic && ins.imutimetarge)
		{
			inspre_forStatic = ins;
		}
	}
/****************************************双天线航向修正**********************************************/
#if 0
	/*dsf90,2018.9.20双天线航向修正*/
	if (ilcd.bGPSavail && (ilcd.stat == 4) && ilcd.GA_RK[2]>SQ(0.001) && ilcd.GA_RK[2] <= SQ(0.2))	//GNSS
	{
		/*车头姿态角*/
		double dYawBias = insYaw - gnssYaw;
		DEG_NEG180_180(dYawBias);

		if (!bDualAntAvail)
		{
			if (ilcd.heading2 && ilcd.GA_RK[2])
				bDualAntAvail = 1;
		}

		if (!bDualAntCalibrateOk)
		{
			if (!bDualAntInitOk)
			{
				if (!bTurn)
					getDualAntBias(ilcd, dYawBias); /*初始安装误差*/
			}
			else
			{
				if (!bGnssLost && ilcd.stat == 4 && ilcd.heading2  && ilcd.GA_RK[2] && !bTurn)
				{
					ins.dual_yaw = insYaw;
					ins.dual_yaw_h = gnssYaw + dDualAntYawBias;

					dDualYaw = (ins.dual_yaw - ins.Byaw) - ins.dual_yaw_h;
					DEG_NEG180_180(dDualYaw);

					kf_cal.Rk[2] = ilcd.GA_RK[2] * SQ(R2D);
					iCalUpdateZ_flag |= CAL_UPDATE_Z_DUAL_YAW;

					if (kf_cal.xk_std[5] < 0.005)
						bDualAntCalibrateOk = 1; //完成安装误差标定
				}
				bDualAntCalibrateOk = 1; //完成安装误差标定
			}
		}

		if (bDualAntCalibrateOk)
		{
			dDiffYaw_Ins2DualAnt = insYaw - (gnssYaw + dDualAntYawBias);
			DEG_NEG180_180(dDiffYaw_Ins2DualAnt);
			if (fabs(dDiffYaw_Ins2DualAnt)<2)	//dsf90:数据跳变，认为GNSS异常
			{
				dAtt[2] = dDiffYaw_Ins2DualAnt * D2R;
				kf.Rk[2] = ilcd.GA_RK[2];
				iUpdateZ_flag |= UPDATE_Z_ATT_Z;
			}
		}
	}
#endif
/****************************************车辆速度约束**********************************************/
		kf.setRk_constraint();//量测噪声为常数
		/*使用约束*/
		dvm[0] = ins.vm_car[0];
		dvm[1] = 0;
		dvm[2] = ins.vm_car[2];
		iUpdateZ_flag |= UPDATE_Z_CONS_VER_XZ;

/****************************************零速更新**********************************************/
	if (bStatic)
	{
		{
			double dPos[3] = { 0 };
			Mat_min(ins.pos, inspre_forStatic.pos, dPos, 3, 1);
			//if(!bDualAntAvail || bGnssLost)/*单天线或不搜星，姿态航向约束*/
			{
				Mat_min(ins.att_car, inspre_forStatic.att_car, dAtt, 3, 1);
				//iUpdateZ_flag |=  UPDATE_Z_ATT_Z;//无人船上航向约束不可用
			}
			double zk[NUMV] = { -dAtt[0],-dAtt[1],-dAtt[2],ins.vn[0],ins.vn[1],ins.vn[2],dPos[0],dPos[1],dPos[2],dvm[0],dvm[1],dvm[2],dYawRate };
			iUpdateZ_flag = UPDATE_Z_ATT_Z| UPDATE_Z_VER_XYZ|UPDATE_Z_POS_XYZ;
			//iUpdateZ_flag |= UPDATE_Z_VER_XYZ;
			if (iUpdateZ_flag)
			{
				kf.MUpadte(ins, zk, iUpdateZ_flag);
				kf.Feedback(ins, dt);
			}
			if (!ilcd.bGPSavail)
				return 1;
			else
				return 5;
		}
	}
/****************************************GNSS中断，INS纯推算**********************************************/
	if (!ilcd.bGPSavail) 
	{
		/*dsf90:侧滑约束*/
		double zk[NUMV] = { 0,0,0,0,0,0,0,0,0,dvm[0],dvm[1],dvm[2],dYawRate };
		if (bTurn)//无人船转弯的时候加入侧向速度约束
		{
			double dVx = fabs(ins.wib[2])*2.7;
			if (dVx < 0.1)	dVx = 0.1;
			kf.Rk[9] = SQR(dVx);
		}
		if (iUpdateZ_flag)
		{
			kf.MUpadte(ins, zk, iUpdateZ_flag);
			kf.Feedback(ins, dt);
		}
#if 1
		/*dsf90:位置结果约束（限位）*/
		correctSideslip();
#endif
		return 1;
	}

	
/****************************************GNSS跳点处理**********************************************/
	double zk[NUMV] = { dAtt[0],dAtt[1],dAtt[2],dvn[0],dvn[1],dvn[2],dpos[0],dpos[1],dpos[2],dvm[0],dvm[1],dvm[2],dYawRate };
#if 0  //GNSS跳点-不更新或速度更新
	if ((fabs(enu[0])>2.0 || fabs(enu[1])>2.0 || fabs(enu[2])>2.0) && c<1000)
	{
		GINS_log("   跳点：enu>0.5\n");
		//kf.resetRk(ilcd);
		num_GPSskip++;
		if ((ilcd.stat == 4 || ilcd.stat ==5) && busegnssvn)
		{
			//GNSS长时间中断，恢复时间长
			double PosRk[3] = { 0.0 };
			kf.downgrade_car(ilcd, enu, PosRk);
			/*pos*/
			kf.Rk[6] = PosRk[0];
			kf.Rk[7] = PosRk[1];
			kf.Rk[8] = PosRk[2];
			iUpdateZ_flag |= UPDATE_Z_VER_XYZ | UPDATE_Z_POS_XYZ;
			c = 0;
		}
		else if (busegnssvn)  //位置跳变，但速度可用
		{
			iUpdateZ_flag |= UPDATE_Z_VER_XYZ;
			c = 0;
		}
		if (iUpdateZ_flag)
		{
			kf.MUpadte(ins, zk, iUpdateZ_flag);
			kf.Feedback(ins, 0.2);
		}
		//位置速度均跳变，可能是GNSS异常，也可能是INS累积误差大
		//防止是INS发散，若连续10秒跳变，则置信GNSS
		if (num_GPSskip<25)
		{
			return 2;
		}  //不做融合
	}
#endif 


/****************************************GNSS长时间中断处理**********************************************/
#if 0   //长时间中断20s或定位差连续大于阈值--,GPS恢复阶段 - 速度和位置设为GNSS的结果
	//需要的话就开个窗
	if (c>999 || num_GPSskip>24)
	{
		kf.resetRk(ilcd);
		num_GNSSrestore++;
		if ((ilcd.nsused >= 8 || ilcd.hdop <= 2.0) || num_GNSSrestore>20)
		{
			//GNSS搜星数（用于解算）和PDOP满足条件，GNSS定位质量较好
			iUpdateZ_flag |= UPDATE_Z_VER_XYZ | UPDATE_Z_POS_XYZ;
			num_GNSSmupdate++;
		}
		//退出恢复窗口 —— GNSS搜星数（用于解算）和PDOP满足条件，GNSS定位质量较好 4次
		if (num_GNSSmupdate>4)
		{
			num_GPSskip = 0;
			num_GNSSmupdate = 0;
			num_GNSSrestore = 0;
			c = 0;
			num_ContinueFix = 5;
			num_ContinueFloat = 50;
		}
		if (iUpdateZ_flag)
		{
			kf.MUpadte(ins, zk, iUpdateZ_flag);
			kf.Feedback(ins, 0.1);
		}
		return 2;
	}
#endif



/****************************************GNSS速度不可用，降权处理**********************************************/
#if 0
	kf.resetRk(ilcd);
	c = 0; //INS更新计数清零
		   //对异常速度进行探测
	if (!busegnssvn) //gnss速度不可用
	{
		if (ilcd.stat == 4)          //固定
		{
			num_ContinueFix++;
			if (num_ContinueFix>5)       //连续固定数目>6,直接使用GNSS给出的方差信息，初始化为6
			{

			}
			else                        //连续固定数目<6（浮点跳固定），逐渐使用GNSS的权，平滑过渡
			{
				double PosRk[3] = { 0.0 };
				kf.downgrade(ilcd, (6 - num_ContinueFix), PosRk);
				/*pos*/
				kf.Rk[6] = PosRk[0];
				kf.Rk[7] = PosRk[1];
				kf.Rk[8] = PosRk[2];
			}
			num_ContinueFloat = 0;        //连续浮点数目置零
		}
		else                            //非固定
		{
			num_ContinueFloat++;
			if (num_ContinueFloat<10)   //固定跳浮点，2秒稳定期，使用denu和GNSS的方差信息确定观测值权，防止GNSS跳点
			{
				double PosRk[3] = { 0.0 };
				kf.downgrade(ilcd, (11 - num_ContinueFloat), PosRk);
				//kf.downgrade_car(ilcd,enu,PosRk);
				/*pos*/
				kf.Rk[6] = PosRk[0];
				kf.Rk[7] = PosRk[1];
				kf.Rk[8] = PosRk[2];
			}
			else                        //GNSS精度趋于稳定，不在对GNSS降权，防止INS发散
			{

			}
			num_ContinueFix = 0;          //连续固定数目置零
		}
		iUpdateZ_flag |= UPDATE_Z_POS_XYZ;
		upmodel = 3;
	}
	else
	{
		if (ilcd.stat == 4)             //固定
		{
			num_ContinueFix++;
			if (num_ContinueFix>5)          //连续固定数目>6,直接使用GNSS给出的方差信息，初始化为6
			{

			}
			else
			{
				double PosRk[3] = { 0.0 };
				kf.downgrade(ilcd, (6 - num_ContinueFix), PosRk);
				/*pos*/
				kf.Rk[6] = PosRk[0];
				kf.Rk[7] = PosRk[1];
				kf.Rk[8] = PosRk[2];
			}
			num_ContinueFloat = 0;
		}
		else                               //非固定
		{
			num_ContinueFloat++;
			if (num_ContinueFloat<10) //100
			{
				double PosRk[3] = { 0.0 };
				kf.downgrade(ilcd, (11 - num_ContinueFloat), PosRk);
				//kf.downgrade_car(ilcd,enu,PosRk);
				/*pos*/
				kf.Rk[6] = PosRk[0];
				kf.Rk[7] = PosRk[1];
				kf.Rk[8] = PosRk[2];
			}
			else
			{

			}
			num_ContinueFix = 0;          //连续固定数目置零
		}
		iUpdateZ_flag |= UPDATE_Z_VER_XYZ | UPDATE_Z_POS_XYZ;
		upmodel = 23;
	}
#endif
	kf.resetRk(ilcd);
	iUpdateZ_flag |= UPDATE_Z_VER_XYZ | UPDATE_Z_POS_XYZ;
	if (iUpdateZ_flag)
	{
		kf.MUpadte(ins, zk, iUpdateZ_flag);
		kf.Feedback(ins, 0.2);
	}

	Mat_equal(kf.xk, kf.ROW, 1, kf.xkpre);
	Mat_equal(dpos, 3, 1, kf.dpos);
	Mat_equal(enu, 3, 1, kf.denu);

	num_GPSskip = 0;
	return 2;
}

void MUpdate_Variable(int ROW, int COL, double Hk[], double Rk[], double ZK[], double xk[], double Pxk[], int zflag)
{
	int i = 0, j = 0, k = 0, usecol = 0;
	for (i = 0;i<COL;i++)
	{
		if ((zflag >> i) % 2)
			usecol++;
	}
	double* usehk = (double*)__ml_zero(sizeof(double)*usecol*ROW);
	double* userk = (double*)__ml_zero(sizeof(double)*usecol*usecol);
	double* usezk = (double*)__ml_zero(sizeof(double)*usecol * 1);
	for (i = 0;i<COL;i++)
	{
		if ((zflag >> i) % 2)
		{
			for (j = 0;j<ROW;j++)
				usehk[k*ROW + j] = Hk[i*ROW + j];
			userk[k*usecol + k] = Rk[i];
			usezk[k] = ZK[i];
			k++;
		}
	}

	MUpdate_True(ROW, usecol, usehk, userk, usezk, xk, Pxk);

	free(usehk);
	free(userk);
	free(usezk);
}

void MUpdate_True(int ROW, int COL, double Hk[], double Rk[], double ZK[], double xk[], double Pxk[])
{
	double* xkk_1 = (double*)__ml_zero(sizeof(double)*ROW * 1);
	double* Pxkk_1 = (double*)__ml_zero(sizeof(double)*ROW*ROW);
	double* Pxykk_1 = (double*)__ml_zero(sizeof(double)*ROW*COL);
	double* Pykk_1 = (double*)__ml_zero(sizeof(double)*COL*COL);
	double* Kk = (double*)__ml_zero(sizeof(double)*ROW*COL);
	double* ykk_1 = (double*)__ml_zero(sizeof(double)*COL * 1);
	double* dxk = (double*)__ml_zero(sizeof(double)*ROW * 1);
	double* Hkt = (double*)__ml_zero(sizeof(double)*ROW*COL);
	double* invPy = (double*)__ml_zero(sizeof(double)*COL*COL);
	double* KPyKt = (double*)__ml_zero(sizeof(double)*ROW*ROW);


	Mat_equal(xk, ROW, 1, xkk_1);
	Mat_equal(Pxk, ROW, ROW, Pxkk_1);
	Mat_tran(Hk, COL, ROW, Hkt);//Hkt	= Mt(Kk)

	Mat_mul(Pxkk_1, Hkt, ROW, ROW, COL, Pxykk_1);
	Mat_mul(Hk, Pxykk_1, COL, ROW, COL, Pykk_1);
	Mat_add(Pykk_1, Rk, Pykk_1, COL, COL);//Hk X Pxk X Mt(Hk) + Rk

	Mat_inv(Pykk_1, COL, invPy);
	Mat_mul(Pxykk_1, invPy, ROW, COL, COL, Kk);
	Mat_mul(Hk, xkk_1, COL, ROW, 1, ykk_1);
	Mat_min(ZK, ykk_1, ykk_1, COL, 1);
	Mat_mul(Kk, ykk_1, ROW, COL, 1, dxk);
	Mat_add(xkk_1, dxk, xk, ROW, 1);

	/*add by dsf90,2018,3,1;测试效果一致，没有差异*/
	double* KkHk = (double*)__ml_zero(sizeof(double)*ROW*ROW);
	double* KkHkPxkk_1 = (double*)__ml_zero(sizeof(double)*ROW*ROW);

	/*dsf90:Pxk  	= Pxk - Kk X Hk X Pxk*/
	Mat_mul(Kk, Hk, ROW, COL, ROW, KkHk);
	Mat_mul(KkHk, Pxkk_1, ROW, ROW, ROW, KkHkPxkk_1);
	Mat_min(Pxkk_1, KkHkPxkk_1, Pxk, ROW, ROW);
	//Mmul(Pxk,ROW,ROW,0.995);

	free(KkHk);       KkHk = NULL;
	free(KkHkPxkk_1); KkHkPxkk_1 = NULL;
	/*end add*/

	free(xkk_1);
	free(Pxkk_1);
	free(Pxykk_1);
	free(Pykk_1);
	free(Kk);
	free(ykk_1);
	free(dxk);
	free(Hkt);
	free(invPy);
	free(KPyKt);
	xkk_1 = NULL;
	Pxkk_1 = NULL;
	Pxykk_1 = NULL;
	Pykk_1 = NULL;
	Kk = NULL;
	ykk_1 = NULL;
	dxk = NULL;
	Hkt = NULL;
	invPy = NULL;
	KPyKt = NULL;
}

bool busegnssvel_car(double gnssvel[3], double dvel[3], double dheading)
{
	int bgnss = 0, bdif = 0;
	if (fabs(gnssvel[0])<100 && fabs(gnssvel[1])<100 && fabs(gnssvel[2])<10) //360km/h
	{
		bgnss = 1;
	}
	if (fabs(dvel[0])<5.0 && fabs(dvel[1])<5.0 && fabs(dvel[2])<5)
	{
		bdif = 1;
	}
	return bgnss&&bdif;
}

bool bgnssskip_car(vector<gpos_t, malloc_allocator<gpos_t> > v_gps, double iposcur[3], int numins)
{
	bool bskipins = false;
	if (v_gps.size()<10 || numins>1000)
		return false;
	if (v_gps.size()>10)
	{
		int ith = v_gps.size() - 1;
		double pcur[3] = { v_gps[ith].lat,v_gps[ith].lon,v_gps[ith].hig };
		double ppre[3] = { v_gps[ith - 1].lat,v_gps[ith - 1].lon,v_gps[ith - 1].hig };
		double dpos_gg[3] = { 0 }, dpos_lc[3] = { 0 };
		diffpos(ppre, pcur, dpos_gg);
		diffpos(iposcur, pcur, dpos_lc);
		if (fabs(dpos_lc[0])>10.0 || fabs(dpos_lc[1])>10.0 || fabs(dpos_lc[2])>10.0)
		{
			bskipins = true;
		}
	}
	//相邻历元GPS的差值<阈值
	return bskipins;
}

void GINS_KF::downgrade(Process_Data ilcd, int scater, double posrk[3])
{
	posrk[0] = ilcd.GPV_RK[3 * 6 + 3] * scater*scater;
	posrk[1] = ilcd.GPV_RK[4 * 6 + 4] * scater*scater;
	posrk[2] = ilcd.GPV_RK[5 * 6 + 5] * scater*scater;
}