/* Baofeng UV3R Mark II EEPROM configuration settings manager
 * (c) 2012 Jacek Lipkowski <sq5bpf@lipkowski.org>
 *
 * This program can change the settings the of Baofeng UV3R Mark II 
 * and probably other similar radios. The configuration file has to 
 * be read/written using the baoprog utility.
 *
 * Use at your own risk. 
 *
 *
 * This is a work in progress.
 *
 * Different firmware versions work a bit differently.
 *
 * This program is licensed under the GNU GENERAL PUBLIC LICENSE v3
 * License text avaliable at: http://www.gnu.org/copyleft/gpl.html 
 */

/*
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <getopt.h>
#include <ctype.h>

#define UV3R_EEPROM_SIZE 0x0800
#define UV3R_EEPROM_BLOCKSIZE 0x10

char *file="uv3r_eeprom.raw";


/*
 *        0 1  2 3  4 5  6 7  8 9  a b  c d  e f
0000010: 0000 6714 0000 0006 0000 d100 0403 0201  ..g.............
0000020: 0050 6714 0000 0006 0000 d100 0203 0405  .Pg.............
0000030: 0050 9343 0000 0076 0000 d500 0304 0506  .P.C...v........
0000040: 0075 8743 0000 0076 0014 d500 0405 0607  .u.C...v........
0000050: 0050 5414 0000 0000 0000 d000 0506 0708  .PT.............
0000060: ffff ffff 00ff ffff ff00 c000 ffff ffff  ................
0000070: ffff ffff 00ff ffff ff00 c000 ffff ffff  ................
 *       -rx freq- r -offs----- t 
 *
 */

/* channel and vfo structure */
struct channel {
	unsigned char rxfreq[4];
	unsigned char rxctcss;
	unsigned char offset[4];
	unsigned char txctcss;
	unsigned char flag1;
	unsigned char flag2;
	unsigned char name[4];
} __attribute__((__packed__));


/* frequency in 2 bytes, used in broadcast radio settings and frequency limits, bytes are BCD, lower byte first, divide by 10 to get MHz */
struct word_freq {
	unsigned char freq[2];
} __attribute__((__packed__));


struct unk_data {
	unsigned char field[16];
} __attribute__((__packed__));


struct uv3r_settings {
	unsigned char unknown_header[16]; //0x0-0xf 
	struct channel ch[99];
	struct unk_data unk1[20];

	/*
	 * 0000780: 4012 4024 0040 3050 0000 0000 0000 0000  @.@$.@0P........
	 * 0000790: 0000 3543 0000 0000 0000 c405 0075 3443  ..5C.........u4C
	 * 00007a0: 0050 5714 0000 0000 0000 c005 0025 5714  .PW..........%W.
	 * 00007b0: 4c49 5354 454e 494e 0000 0000 0000 0000  LISTENIN........
	 * 00007c0: 7695 0100 06c1 4904 0001 0001 0122 0000  v.....I......"..
	 * 00007d0: 1f00 0000 0000 0000 0000 0000 0000 0000  ................
	 * 00007e0: 0122 0147 0032 0033 0034 0035 0159 0000  .".G.2.3.4.5.Y..
	 * 00007f0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
	 */


	/* 0x780 */
	//struct unk_data unk_780;
	unsigned char vhf_limit_l[2]; //should be word_freq actually
	unsigned char vhf_limit_h[2];
	unsigned char uhf_limit_l[2];
	unsigned char uhf_limit_h[2];


	unsigned char unk_data_788[8];

	/* 0x790 */
	//struct unk_data unk_790; /* 0000790: 0000 3543 0000 0000 0000 c405 0075 3443 */
	unsigned char uhf_vfo_freq[4];
	unsigned char unk_data_794[8]; //probably settings, shift, offset etc
	unsigned char uhf_vfo_pervious_freq[4];

	/* 0x7a0 */
	unsigned char vhf_vfo_freq[4];
	unsigned char unk_data_7a4[8]; //probably settings, shift, offset etc
	unsigned char vhf_vfo_pervious_freq[4];
	//	struct unk_data unk_7a0; /* 00007a0: 0050 5714 0000 0000 0000 c005 0025 5714 */

	/* 0x7b0 */
	unsigned char listenin_string[8]; //"LISTENIN" in ascii
	unsigned char unk_data_7b8[8];


	/* 0x7c0 */ 
	//struct unk_data unk_7c0;
	unsigned char unk_data_7c0[2];
	unsigned char squelch_level; //0x7c2
	unsigned char vox; //0x7c3
	unsigned char tot_timer; //0x7c4
	unsigned char radio_settings_7c5; //0x7c5 
	unsigned char radio_settings_7c6; //0x7c6
	unsigned char selected_channel; //0x7c7
	unsigned char unk_data_7c8; //0x7c8
	unsigned char current_broadcast_radio_channel; //0x7c9
	unsigned char priority_channel; //0x7ca
	unsigned char volume; //0x7cb
	struct word_freq current_broadcast_radio_frequency; //0x7cc
	//unsigned char last_menu_item;
	unsigned char menu_exit; //0x7ce
	unsigned char unk_data_7cf; //0x7cf

	/* 0x7d0 */
	unsigned char channel_bitmap[13];
	unsigned char unk_data_7dd[3];

	/* 0x7e0-0x7fd */
	struct word_freq broadcast_radio_channels[15];
	unsigned char unk_data_7fe[2]; // probably an unused radio channel setting
}  __attribute__((__packed__));

/*char *radio_settings_7c5_bits[] = { 
  "unk0" , "unk1" , "unk2" , "unk3" , "unk4", "unk5", "unk6", "unk7" ,
  "_unk0" , "_unk1" , "_unk2" , "_unk3" , "_unk4", "_unk5", "_unk6", "_unk7" 
  };
  */
/* when wide is set, but wide2 is not set, the radio reports narrow fm */
char *flag1_bits[] = { 
	"shift-" , "shift+" , "unk2" , "unk3" , "is_name", "narrow", "wide2", "QRO",
	"" , "" , "_unk2" , "_unk3" , "no_name", "wide", "not_wide2", "QRP"
};
char *flag2_bits[] = { 
	"unk0" , "unk1" , "unk2" , "unk3" , "unk4", "unk5", "unk6", "unk7" ,
	"_unk0" , "_unk1" , "_unk2" , "_unk3" , "_unk4", "_unk5", "_unk6", "_unk7" 
};
char *radio_settings_7c5_bits[] = { 
	"Channel_mode" , "BCLO" , "Keys_LOCKED" , "Beep_ON" , "Display_Frequency", "DualWatch", "Radio_ON", "Battery_Saver_ON" ,
	"VFO_mode" , "_BCLO" , "Keys_UNLOCKED" , "Beep_OFF" , "Disp_Channel_name", "No_DualWatch", "Radio_OFF", "Battery_Saver_OFF" 
};
char *radio_settings_7c6_bits[] = { 
	"radio_channel_mode" , "VFO_A" , "KEY_Backlight" , "unk3" , "SCAN_Mode_CO", "RELAYM_ON", "Key_backlight_ON", "Backlight_ON" ,
	"radio_frequency_mode" , "VFO_B" , "No_Backlight" , "_unk3" , "Scan_Mode_TO", "RELAYM_OFF", "Key_Backlight_OFF", "Backlight_OFF" 
};

/* globals */
struct uv3r_settings uv3r_eeprom;

int show_unused_channels=0;
int show_offsets=0;
float broadcast_radio_offset=65.0;


int read_eeprom(struct uv3r_settings *a,char *file)
{
	int fd;
	int l;

	fd=open(file,O_RDONLY);
	if (fd<0) {
		fprintf(stderr,"open %s error %d %s\n", file,errno, strerror(errno));
		exit(1);

	}
	l=read(fd,a,UV3R_EEPROM_SIZE);
	if (l!=UV3R_EEPROM_SIZE) {
		fprintf(stderr,"read %s error %d %s\n", file,errno, strerror(errno));
		exit(1);
	}
	close(fd);
	return(1);
}

int write_eeprom(struct uv3r_settings *a,char *file)
{
	int fd;
	int l;

	fd=open(file,O_WRONLY|O_CREAT|O_TRUNC,0600);
	if (fd<0) {
		fprintf(stderr,"open %s error %d %s\n", file,errno, strerror(errno));
		exit(1);

	}
	l=write(fd,a,UV3R_EEPROM_SIZE);
	if (l!=UV3R_EEPROM_SIZE) {
		fprintf(stderr,"write %s error %d %s\n", file,errno, strerror(errno));
		exit(1);
	}
	close(fd);
	return(1);
}

/****** utils *******/


int offset(void *a, void *b)
{
	int i;
	i=b-a;
	printf("offs: %p %p %4.4X\n",a,b,i);
	return(i);
}

/* dump bit definitions */
int dumpbits(unsigned char *byte,char **bitdef,char *description)
{
	int i;
	int j;
	printf("%s 0x%2.2X: [",description,*byte);
	for (i=0; i<8;i++) 
	{ 
		if (*(byte)&(1<<i)) { j=0; } else { j=8; }
		printf("%i:%s ",i,bitdef[i+j]); 
	}
	printf("]\n");
}

unsigned char bcd_val(unsigned char bcd)
{
	return((bcd&0xf)+(((bcd&0xf0)>>4)*10));
}

unsigned char val2bcd(unsigned char val)
{
	return(val%10+(val/10)*0x10);
}

/*******  channel decoding stuff ********/

float decode_freq(unsigned char *buf)
{
	float khz;
	khz=bcd_val(buf[0])/100.0+bcd_val(buf[1])+bcd_val(buf[2])*100+bcd_val(buf[3])*100*100;
	return(khz);
}

#define CH_RX 0
#define CH_OFFSET 1
#define CH_NAME 2

char decode_char(unsigned char c)
{
	if (c<10) return('0'+c); /* 0-9 */
	if (c==0x0a) return('-'); 
	if (c==0x0b) return(' '); 
	if (c==0xff) return(0); 
	if (c<0x26) return('A'+c-0xc); /* A-Z */
	return('.');
}

int decode_name(unsigned char *name,unsigned char *text)
{
	int i;

	for(i=0;i<4;i++) text[i]=decode_char(*((char *)name+i));
	text[4]=0;
	if (text[0]!=0) return(1);
	return(0);
}

float decode_ctcss(unsigned char id)
{
	switch(id)
	{
		case 0: return 0;
		case 1: return 67.0;
		case 2: return 69.3;
		case 3: return 71.9;
		case 4: return 74.4;
		case 5: return 77.0;
		case 6: return 79.7;
		case 7: return 82.5;
		case 8: return 85.4;
		case 9: return 88.5;
		case 10: return 91.5;
		case 11: return 94.8;
		case 12: return 97.4;
		case 13: return 100.0;
		case 14: return 103.5;
		case 15: return 107.2;
		case 16: return 110.9;
		case 17: return 114.8;
		case 18: return 118.8;
		case 19: return 123.0;
		case 20: return 127.3;
		case 21: return 131.8;
		case 22: return 136.5;
		case 23: return 141.3;
		case 24: return 146.2;
		case 25: return 151.4;
		case 26: return 156.7;
		case 27: return 159.8;
		case 28: return 162.2;
		case 29: return 165.5;
		case 30: return 167.9;
		case 31: return 171.3;
		case 32: return 173.8;
		case 33: return 177.3;
		case 34: return 179.9;
		case 35: return 183.5;
		case 36: return 186.2;
		case 37: return 189.9;
		case 38: return 192.8;
		case 39: return 196.6;
		case 40: return 199.5;
		case 41: return 203.5;
		case 42: return 206.5;
		case 43: return 210.7;
		case 44: return 218.1;
		case 45: return 225.7;
		case 46: return 229.1;
		case 47: return 233.6;
		case 48: return 241.8;
		case 49: return 250.3;
		case 50: return 254.1;
		default: return 666;

	}


}



int is_channel_active(unsigned char *channel_bitmap,int channel)
{
	int i,j;
	channel--;
	i=channel/8;
	j=channel%8;
	if (((unsigned char) *(channel_bitmap+i))&(1<<j)) return(1);
	return(0);
}

int dump_chanstruct(struct channel *ch)
{
	int i;
	char txt[5];
	printf("%3.4fMHz\toffset: %3.4fMHz rxctcss:%3.1f txctcss:%3.1f\n",decode_freq(ch->rxfreq)/1000,decode_freq(ch->offset)/1000,decode_ctcss(ch->rxctcss),decode_ctcss(ch->txctcss));

	/*decode flag1 */
	/*	for (i=0; i<8;i++) { if (ch->flag1&(1<<i)) printf("%s ",flag1_bits[i]); }
	*/
	dumpbits(&ch->flag1,flag1_bits,"flag1");
	dumpbits(&ch->flag2,flag2_bits,"flag2");
	/* flag2 + name */
	if (decode_name((ch->name),(char *)&txt)) printf("name: [%s]\n",txt);

	//printf("flag2:%2.2x name:[%s]",ch->flag2,decode_name(ch->name));
	//	printf("flag2:%2.2x name:[%s]",ch->flag2,txt);



}

int dump_channels(struct uv3r_settings *a)
{
	int i;


	printf("######  Channel settings:  #######\n\n");
	for (i=0;i<99;i++)
	{

		if ((show_unused_channels)|| (is_channel_active(&a->channel_bitmap,i+1)))	

		{
			printf("ch %i ",i+1);
		if (show_offsets) printf(" ptr:%p ",((void *)&a->ch[i])-((void *)a));
			dump_chanstruct(&a->ch[i]);
			if (!is_channel_active(&a->channel_bitmap,i+1)) printf ("NOT ACTIVE\n");
			printf("\n\n");
		}
	}
	printf("\n");
}



/******* frequency limits ********/
float decode_word_freq(unsigned char *a)
{
	return(bcd_val(*a)/10.0+10.0*bcd_val(*(a+1)));
}

void encode_word_freq(unsigned char *a,float l)
{
	int ll;
	ll=l*10;
	*a=val2bcd(ll%100);
	*(a+1)=val2bcd(l/10);
}


int dump_freq_limits(struct uv3r_settings *a)
{
	printf("######  Frequency limits:  #######\n");

	printf ("VHF: %3.1f-%3.1f MHz\tUHF: %3.1f-%3.1f\n",
			decode_word_freq((unsigned char *)&a->vhf_limit_l),
			decode_word_freq((unsigned char *)&a->vhf_limit_h),
			decode_word_freq((unsigned char *)&a->uhf_limit_l),
			decode_word_freq((unsigned char *)&a->uhf_limit_h));
	//printf("\nsettings.ini values:\nFreq3=[%3.1f-%3.1f/%3.1f-%3.1f]\ndata3=%2.2X%2.2X%2.2X%2.2X\n",
	printf("\nsettings.ini values:\nFreq3=[%3.0f-%3.0f/%3.0f-%3.0f]\ndata3=%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X\n\n",
			decode_word_freq((unsigned char *)&a->vhf_limit_l),decode_word_freq((unsigned char *)&a->vhf_limit_h),
			decode_word_freq((unsigned char *)&a->uhf_limit_l),decode_word_freq((unsigned char *)&a->uhf_limit_h),
			a->vhf_limit_l[0],a->vhf_limit_l[1], a->vhf_limit_h[0],a->vhf_limit_h[1],
			a->uhf_limit_l[0],a->uhf_limit_l[1],a->uhf_limit_h[0],a->uhf_limit_h[1]);

}

int set_freq_limits(struct uv3r_settings *a, char *l)
{
	float f;
	char *limit;
	unsigned char ff[2];

	f=atof(l+1);

	switch (*l)
	{
		case 'v':
			limit="VHF LOW";
			encode_word_freq((unsigned char *)&a->vhf_limit_l,f);
			break;

		case 'V':
			limit="VHF HIGH";
			encode_word_freq((unsigned char *)&a->vhf_limit_h,f);
			break;

		case 'u':
			limit="UHF HIGH";
			encode_word_freq((unsigned char *)&a->uhf_limit_l,f);
			break;

		case 'U':
			limit="UHF HIGH";
			encode_word_freq((unsigned char *)&a->uhf_limit_h,f);
			break;
		default:
			fprintf(stderr,"ERROR! unknown limit %c (must be one of: v V u U)\n",*l);
			exit(1);
			break;
	}

	printf("######  Set %s limit: %3.1f MHz #######\n",limit,f);
}


/******* broadcast radio settings *********/
float decode_broadcast_freq(struct word_freq *a)
{
	return(broadcast_radio_offset+((a->freq[0]<<8)|(a->freq[1]))/10.0);
}

int dump_broadcast_freq(struct uv3r_settings *a)
{
	int i;
	printf("####### Broadcast radio settings #########\n");

	printf("Current channel: %2.2X\tCurrent frequency: %3.1fMHz\n",a->current_broadcast_radio_channel,decode_broadcast_freq(&a->current_broadcast_radio_frequency));
	for (i=0;i<15;i++) {
		printf("Ch %2.2i\t%3.1f MHz\n",i+1,decode_broadcast_freq(&a->broadcast_radio_channels[i]));
	}

	printf("\n");
}

/******* various flags/settings *********/

int dump_settings(struct uv3r_settings *a)
{

	printf("####### Radio settings #########\n");
	printf("Squelch Level:\t%i\n",a->squelch_level);
	printf("VOX Level:\t%i\n",a->vox);
	printf("Volume:\t%i\n",a->volume);
	offset((void *)a,(void *)&a->volume);
	printf("Time-Out timer:\t%i\n",a->tot_timer);
	printf("VHF VFO: %3.4fMHz (previous: %3.4fMHz)\n",decode_freq(&a->vhf_vfo_freq)/1000,decode_freq(&a->vhf_vfo_pervious_freq)/1000);
	printf("UHF VFO: %3.4fMHz (previous: %3.4fMHz)\n",decode_freq(&a->uhf_vfo_freq)/1000,decode_freq(&a->uhf_vfo_pervious_freq)/1000);



	dumpbits(&a->radio_settings_7c5,radio_settings_7c5_bits,"radio_settings_7c5");
	dumpbits(&a->radio_settings_7c6,radio_settings_7c6_bits,"radio_settings_7c6");
	printf("\n");
}

void helpme()
{
	printf(" cmdline opts:\n"
			"-r <file> (read file)\n"
			"-w <file> (write file)\n"
			"-a (dump all)\n"
			"-h (help)\n"
			"-c (dump channels)\n"
			"-C (show unused channels)\n"
			"-f (dump freq limits)\n"
			"-b (dump broadcast)\n"
			"-s (dump settings)\n"
			"-F Xlimit (set freq limits, freq in MHz NNN.N format, X: v-vhf lower limit V-vhf upper limit, u-uhf lower limit U-uhf upper limit)\n");

}


int main(int argc,char **argv)
{


	int opt;

	/*  cmdline opts:
	 *  -r <file> (read file)
	 *  -w <file> (write file)
	 *  -a (dump all)
	 *  -h (help)
	 *  -c (dump channels)
	 *  -f (dump freq limits)
	 *  -b (dump broadcast)
	 *  -s (dump settings)
	 *  -F Xlimit (set freq limits, freq in MHz NNN.N format, X: v-vhf lower limit V-vhf upper limit, u-uhf lower limit U-uhf upper limit)
	 */

	while ((opt=getopt(argc,argv,"hr:w:acCfbsF:o"))!=EOF)
	{
		switch (opt)
		{
			case 'h':
				helpme();
				exit(0);
				break;
			case 'r':
				read_eeprom(&uv3r_eeprom,optarg);
				break;
			case 'w':
				write_eeprom(&uv3r_eeprom,optarg);
				break;
			case 'a':
				dump_channels(&uv3r_eeprom);
				dump_freq_limits(&uv3r_eeprom);
				dump_broadcast_freq(&uv3r_eeprom);
				dump_settings(&uv3r_eeprom);
				break;
			case 'c':
				dump_channels(&uv3r_eeprom);
				break;
			case 'C':
				show_unused_channels=1;
				break;
			case 'f':
				dump_freq_limits(&uv3r_eeprom);
				break;
			case 'F':
				set_freq_limits(&uv3r_eeprom,optarg);
				break;
			case 'b':
				dump_broadcast_freq(&uv3r_eeprom);
				break;
			case 's':
				dump_settings(&uv3r_eeprom);
				break;
			case 'o':
				show_offsets=1;
				break;
			default:
				fprintf(stderr,"unknown option %c\n",opt);
				exit(1);
				break;

		}
	}


}
