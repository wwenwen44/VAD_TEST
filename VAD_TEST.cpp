// VAD_TEST.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "libwebrtc\webrtc_vad.h"

#include <string.h>

#define SAMPLES_PER_FRAME 160
#define SAMPLES_RATE      8000

VadInst *vaddsp=NULL;
short rec_buff[SAMPLES_PER_FRAME]={
	1,1,-1,0,1,-1,0,1,-2,0,2,-1,0,1,-1,0,
	2,1,-1,0,1,-1,0,1,-2,0,2,-1,0,1,-1,0,
	3,1,-1,0,1,-1,0,1,-2,0,2,-1,0,1,-1,0,
	4,1,-1,0,1,-1,0,1,-2,0,2,-1,0,1,-1,0,
	5,1,-1,0,1,-1,0,1,-2,0,2,-1,0,1,-1,0,
	6,1,-1,0,1,-1,0,1,-2,0,2,-1,0,1,-1,0,
	7,1,-1,0,1,-1,0,1,-2,0,2,-1,0,1,-1,0,
	8,1,-1,0,1,-1,0,1,-2,0,2,-1,0,1,-1,0,
	9,1,-1,0,1,-1,0,1,-2,0,2,-1,0,1,-1,0,
	10,1,-1,0,1,-1,0,1,-2,0,2,-1,0,1,-1,0
};

#define SMP_MOD 		0x84
#define MASK 			32635

unsigned char __ant_lin2mu[16384];
short __ant_mulaw[256];

static unsigned char _Linear2Ulaw(short p_val)  
{
	static int exp_lut[256] = 
	   {0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,
		4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
		5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
		6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
		6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
		6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
		6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
		7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
		7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
		7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
		7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
		7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
		7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
		7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
		7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7};
	int sign, exponent, mantissa;
	unsigned char ulawbyte;
	
	sign = (p_val >> 8) & 0x80; 
	if (sign != 0) p_val = -p_val;
	if (p_val > MASK) p_val = MASK;
	
	p_val = p_val + SMP_MOD;
	exponent = exp_lut[(p_val >> 7) & 0xFF];
	mantissa = (p_val >> (exponent + 3)) & 0x0F;
	ulawbyte = ~(sign | (exponent << 4) | mantissa);
	
	if (ulawbyte == 0) ulawbyte = 0x02;   /* optional trap */
	
	return(ulawbyte);
}


void UlawInit(void)
{
	int i;
	
	for(i = 0;i < 256;i++)
	{
		short mu,e,f,y;
		static short etab[]={0,132,396,924,1980,4092,8316,16764};
		
		mu = 255-i;
		e = (mu & 0x70)/16;
		f = mu & 0x0f;
		y = f * (1 << (e + 3));
		y += etab[e];
		if (mu & 0x80) y = -y;
		__ant_mulaw[i] = y;
	}
	for(i = -32768; i < 32768; i++)
	{
		__ant_lin2mu[((unsigned short)i) >> 2] = _Linear2Ulaw(i);
	}
	
}

#define AMI_MASK 0x55
unsigned char __ant_lin2a[8192];
short __ant_alaw[256];

static inline unsigned char _Linear2Alaw(short int p_lin)
{
    int mask;
    int seg;
    int pcm_val;
    static int seg_end[8] = { 0xFF,0x1FF,0x3FF,0x7FF,0xFFF,0x1FFF,0x3FFF,0x7FFF };
    
    pcm_val = p_lin;
    if (pcm_val >= 0)
    {
        mask = AMI_MASK | 0x80;
    }
    else
    {
        mask = AMI_MASK;
        pcm_val = -pcm_val;
    }
	
    for (seg = 0;  seg < 8;  seg++)
    {
        if (pcm_val <= seg_end[seg])
			break;
    }
    return  ((seg << 4) | ((pcm_val >> ((seg)  ?  (seg + 3)  :  4)) & 0x0F)) ^ mask;
}

static inline short int _Alaw2Linear (unsigned char p_alaw)
{
    int i;
    int seg;
	
    p_alaw ^= AMI_MASK;
    i = ((p_alaw & 0x0F) << 4);
    seg = (((int) p_alaw & 0x70) >> 4);
    if (seg)
        i = (i + 0x100) << (seg - 1);
    return (short int) ((p_alaw & 0x80)  ?  i  :  -i);
}

// Global Util Function
void AlawInit(void)
{
	int i;
	
	for(i = 0;i < 256;i++)
	   {
		__ant_alaw[i] = _Alaw2Linear(i);
	   }
	
	for(i = -32768; i < 32768; i++)
	   {
		__ant_lin2a[((unsigned short)i) >> 3] = _Linear2Alaw(i);
	   }	
}

#define ANT_LIN2MU(a) (__ant_lin2mu[((unsigned short)(a)) >> 2])
#define ANT_MULAW(a) (__ant_mulaw[((unsigned char)(a))])

#define ANT_LIN2A(a) (__ant_lin2a[((unsigned short)(a)) >> 3])
#define ANT_ALAW(a) (__ant_alaw[((unsigned char)(a))])

void alaw2ulaw(char *out, char *in, int len)
{
	short x ;
	char *pin = in;
	char *pout = out;
	for(int i = 0; i < len; i++)
	{
		x = ANT_ALAW(*pin);
		*pout = ANT_LIN2MU(x);
		pin++;
		pout++;
	}
}

void alaw2line(short *out, char *in, int len)
{
	char *pin = in;
	short *pout = out;
	for(int i = 0; i < len; i++)
	{
		*pout = ANT_ALAW(*pin);
		pin++;
		pout++;
	}
}

void line2ulaw(char *out, short *in, int len)
{
	short *pin = in;
	char *pout = out;
	for(int i = 0; i < len; i++)
	{
		*pout = ANT_LIN2MU(*pin);
		pin++;
		pout++;
	}
}

void ulaw2alaw(char *out, char *in, int len)
{
	short x;
	char *pin = in;
	char *pout = out;
	for(int i = 0; i < len; i++)
	{
		x = ANT_MULAW(*pin);
		*pout = ANT_LIN2A(x);
		pin++;
		pout++;
	}
}

void ulaw2line(short *out, char *in, int len)
{
	char *pin = in;
	short *pout = out;
	int i;
	for(i = 0; i < len; i++)
	{
		*pout = ANT_MULAW(*pin);
		pin++;
		pout++;
	}
}

void line2alaw(char *out, short *in, int len)
{
	short *pin = in;
	char *pout = out;
	int i;
	for(i = 0; i < len; i++)
	{
		*pout = ANT_LIN2A(*pin);
		pin++;
		pout++;
	}
}

int main(int argc, char* argv[])
{
	FILE* fp;
	FILE* fw;
	char buff_in[160] = {0};
	char buff_out[160] = {0};
	short buff_line[160] = {0};
	int len;
	int iRet;
	
	AlawInit();
	UlawInit();

	if ( !WebRtcVad_Create(&vaddsp) )	
	{
		WebRtcVad_Init(vaddsp);
	}

	//fp = fopen("g2r_test.alaw", "rb");
	//fw = fopen("r2g_test.ulaw", "wb");
	fp = fopen("r2g_test1.ulaw", "rb");
	fw = fopen("g2r_test1.alaw", "wb");
	while (1)
	{
		len = fread(buff_in,1,160,fp);
		if (len <= 0)
		{
			break;
		}
		//alaw2ulaw(buff_out,buff_in,len);
		
#if 0
		ulaw2alaw(buff_out,buff_in,len);
#else
		ulaw2line(buff_line,buff_in,len);
		#if 0
			iRet = WebRtcVad_Process( vaddsp, SAMPLES_RATE, buff_line, SAMPLES_PER_FRAME); //静音检测
			if (iRet == 0)
			{
				memset(buff_line,0x00,len);
			}
			else
			{
				memset(buff_line,0x55,len);
			}
			fwrite(buff_line,1,len*2,fw); //输出文件后可以用cooledit查看柱形图
		#endif
			line2alaw(buff_out,buff_line,len);
#endif	
		
		fwrite(buff_out,1,len,fw);
	}
	fclose(fp);
	fclose(fw);
	return 0;
}
