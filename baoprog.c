/* Baofeng UV3R EEPROM programmer v0.1 
 * (c) 2012 Jacek Lipkowski <sq5bpf@lipkowski.org>
 *
 * This program can read and write the eeprom of Baofeng UV3R Mark II 
 * and probably other similar radios via the serial port. 
 *
 * It can read/write arbitrary data, and might be useful for reverse
 * engineering the radio configuration.
 *
 * Use at your own risk. 
 *
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

#define VERSION "Baofeng UV3R EEPROM programmer v0.1 (c) 2012 Jacek Lipkowski <sq5bpf@lipkowski.org>"

#define MODE_NONE 0
#define MODE_READ 1
#define MODE_WRITE 2


//#define UV3R_EEPROM_SIZE 0x0E40
#define UV3R_EEPROM_SIZE 0x0800
#define UV3R_EEPROM_BLOCKSIZE 0x10
#define UV3R_PREPARE_TRIES 10 /* chirp uses 10 tries too */

#define DEFAULT_SERIAL_PORT "/dev/ttyUSB0"
#define DEFAULT_FILE_NAME "uv3r_eeprom.raw"

/* globals */
speed_t ser_speed=B9600;
char *ser_port=DEFAULT_SERIAL_PORT;
int verbose=0;
int mode=MODE_NONE;
char *file=DEFAULT_FILE_NAME;


/* terrible hexdump ripped from some old code, please don't look */
void hdump(unsigned char *buf,int len)
{
	int tmp1;
	char adump[80];
	int tmp2=0;
	int tmp3=0;
	unsigned char sss;
	char hexz[]="0123456789abcdef";

#define printdump printf("0x%6.6x: %.69s\n",tmp3,adump)

	printf("\n0x%6.6x |0 |1 |2 |3 |4 |5 |6 |7 |8 |9 |a |b |c |d |e |f |\n",len);
	printf("---------+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+------------\n");

	memset(&adump,' ',78);
	adump[78]=0;

	for (tmp1=0; tmp1<len; tmp1++)
	{
		tmp2=tmp1%16;
		if (tmp2==0) {
			if (tmp1!=0)  printdump;
			memset(&adump,' ',78);
			adump[78]=0;
			tmp3=tmp1;
		}
		sss=buf[tmp1];
		adump[tmp2*3]=hexz[sss/16];
		adump[tmp2*3+1]=hexz[sss%16];

		if isprint(sss) { adump[tmp2+50]=sss; } else adump[tmp2+50]='.';
	}
	if (((tmp1%16)!=0)||(len==16)) printdump; 
}

int openport(char *port,speed_t speed)
{
	int fd;
	struct termios my_termios;

	fd = open(port, O_RDWR | O_NOCTTY);

	if (fd < 0)
	{
		printf("open error %d %s\n", errno, strerror(errno));
		return(-1);
	}

	if (tcgetattr(fd, &my_termios))
	{
		printf("tcgetattr error %d %s\n", errno, strerror(errno));
		return(-1);
	}

	if (tcflush(fd, TCIFLUSH))
	{
		printf("tcgetattr error %d %s\n", errno, strerror(errno));
		return(-1);
	}


	my_termios.c_cflag =  CS8 |CREAD | CLOCAL | HUPCL;
	cfmakeraw(&my_termios);
	cfsetospeed(&my_termios, speed);
	if (	tcsetattr(fd, TCSANOW, &my_termios))
	{
		printf("tcsetattr error %d %s\n", errno, strerror(errno));
		return(-1);
	}


	return(fd);

}

/* read with timeout */
int read_timeout(int fd, unsigned char *buf, int maxlen, int timeout)
{
	fd_set rfd;
	int len=0;
	int ret;
	struct timeval tv;
	int nr;
	FD_ZERO(&rfd);

	while(1) {
		FD_SET(fd,&rfd);
		tv.tv_sec=timeout/1000;
		tv.tv_usec=(timeout%1000)/1000;

		ret=select(fd+1,&rfd,0,0,&tv);

		if (FD_ISSET(fd,&rfd)) {
			nr=read(fd,buf,maxlen);
			len=len+nr;
			buf=buf+nr;
			if (nr>=0) maxlen=maxlen-nr;
			if (maxlen==0) break;
		} 


		if (ret==0)  {
			fprintf(stderr,"read_timeout\n");
			/* error albo timeout */
			break;
		}

	}
	return(len);
}


int uv3r_sendack(int fd)
{
	int l;

	l=write(fd,"\x06",1);
	if (l!=1) { fprintf(stderr,"uv3r_sendack: short write\n"); return(0); }
	return(1); 
}

int uv3r_waitack(int fd)
{
	int l;
	unsigned char buf;

	l=read_timeout(fd,(unsigned char *)&buf,1,1000);
	if (l!=1) { 
		fprintf(stderr,"uv3r_waitack: short read\n"); 
		return(0); 
	}
	if (buf!=0x06) 
	{ 
		fprintf(stderr,"uv3r_waitack: nack 0x%2.2X\n",buf); 
		return(0); 
	}
	return(1); 
}

int uv3r_ack(int fd)
{
	if (!uv3r_sendack(fd)) return(0);
	if (!uv3r_waitack(fd)) return(0);
	return(1);
}

/* prepare for transmission with the radio */
int uv3r_prepare(int fd)
{
	int l;
	int len;
	unsigned char buf[32];


	/* say hello */
	l=write(fd,"\x05PROGRAM",8);
	if (verbose>2) printf("write %i\n",l);


	len=read_timeout(fd,(unsigned char *)&buf,1,2000);
	if (len>0) {
		if (verbose>2)	hdump((unsigned char *)&buf,len);
	} else
	{
		fprintf(stderr,"uv3r_prepare: err read1\n");
		return(0);
	}

	/* send STX */

	l=write(fd,"\x02",1);
	if (verbose>2) printf("write %i\n",l);

	/* wait for reply, Baofeng UV3R responds with "LISTENIN" */ 
	len=read_timeout(fd,(unsigned char *)&buf,8,2000);
	if (len>0) {
		if (verbose>2) hdump((unsigned char *)&buf,len);
	} else
	{
		fprintf(stderr,"uv3r_prepare: err read2\n");
		return(0);
	}

	return(uv3r_ack(fd));

}

/* read a block from the radio, sends "Raal" where aa- address, l- length
 * only length 0x10 is supported byt UV3R Mark II*/
int uv3r_readmem(int fd, unsigned char *buf, unsigned char maxlen, int offset)
{
	int l;
	unsigned char cmd[4];


	cmd[0]='R';
	cmd[1]=(offset&0xff00)>>8;
	cmd[2]=offset&0xff;
	cmd[3]=maxlen;

	if (verbose>2) {
		printf("Send read command:\n");
		hdump((unsigned char *)&cmd,4);
	}
	write(fd,&cmd,4);

	l=read_timeout(fd,(unsigned char *)&cmd,4,1000); 
	l=read_timeout(fd,buf,maxlen,5000); 

	uv3r_ack(fd);
	return(l);

}

/* write a block to the radio, sends "WaalDATA" where aa- address, l- length, DATA*/
int uv3r_writemem(int fd, unsigned char *buf, unsigned char len, int offset)
{
	int l;
	unsigned char cmd[4];


	cmd[0]='W';
	cmd[1]=(offset&0xff00)>>8;
	cmd[2]=offset&0xff;
	cmd[3]=len;

	if (verbose>2) {
		printf("Send write command:\n");
		hdump((unsigned char *)&cmd,4);
	}
	l=write(fd,&cmd,sizeof(cmd));
	if (l!=sizeof(cmd)) { 
		fprintf(stderr,"uv3r_writemem: short write\n");
		return(0);
	}
	if (verbose>2) {
		printf("Send data for write command:\n");
		hdump(buf,len);
	}	l=write(fd,buf,len);
	if (l!=len) { 
		fprintf(stderr,"uv3r_writemem: short write\n");
		return(0);
	}
	if (!uv3r_waitack(fd)) return(0);
	return(1);
}

void helpme()
{
	printf( 
			"cmdline opts:\n"
			"-f <file>\tfilename that contains the eeprom dump (default: " DEFAULT_FILE_NAME ")\n"
			"-r \tread eeprom\n"
			"-w \twrite eeprom\n"
			"-p <port>\tdevice name (default: " DEFAULT_SERIAL_PORT ")\n"
			"-s <speed>\tserial speed (default: 9600, the UV3R doesn't accept any other speed)\n"
			"-h \tprint this help\n"
			"-v \tbe verbose, use multiple times for more verbosity\n"

	      );


}


static speed_t baud_to_speed_t(int baud)
{
	switch (baud) {
		case 0:
			return B0;
		case 50:
			return B50;
		case 75:
			return B75;
		case 110:
			return B110;
		case 150:
			return B150;
		case 200:
			return B200;
		case 300:
			return B300;
		case 600:
			return B600;
		case 1200:
			return B1200;
		case 1800:
			return B1800;
		case 2400:
			return B2400;
		case 4800:
			return B4800;
		case 9600:
			return B9600;
		case 19200:
			return B19200;
		case 38400:
			return B38400;
		case 57600:
			return B57600;
		case 115200:
			return B115200;
		default:
			return B0;
	}


}

void parse_cmdline(int argc, char **argv)
{
	int opt;

	/* cmdline opts:
	 * -f <file>
	 * -r (read)
	 * -w (write)
	 * -p <port>
	 * -s <speed>
	 * -h (help)
	 * -v (verbose)
	 */

	while ((opt=getopt(argc,argv,"f:rwp:s:hv"))!=EOF)
	{
		switch (opt)
		{
			case 'h':
				helpme();
				exit(0);
				break;
			case 'v':
				verbose++;
				break;
			case 'r':
				mode=MODE_READ;
				break;
			case 'w':
				mode=MODE_WRITE;
				break;
			case 'f':
				file=optarg;
				break;
			case 'p':
				ser_port=optarg;
				break;
			case 's':

				ser_speed=baud_to_speed_t(atoi(optarg));
				if (ser_speed==B0) {
					fprintf(stderr,"ERROR, unknown speed %s\n",optarg);
					exit(1);
					break;

					default:
					fprintf(stderr,"Unknown command line option %s\n",optarg);
					exit(1);
					break;
				}
		}
	}
}

int write_file(char *name, unsigned char *buffer, int len)
{
	int fd;
	int l;

	fd=open(name,O_WRONLY|O_CREAT|O_TRUNC,0600);
	if (fd<0) {
		printf("open %s error %d %s\n", name,errno, strerror(errno));
		return(-1);
	}

	l=write(fd,buffer,len);

	if (l!=len) {
		printf("short write (%i) error %d %s\n", l,errno, strerror(errno));
		return(-1);
	}

	close(fd);
	return(1);
}


int main(int argc,char **argv)
{
	int fd,ffd;
	unsigned char eeprom[UV3R_EEPROM_SIZE];
	int i;
	int r;
	printf (VERSION "\n\n"); 

	parse_cmdline(argc,argv);

	if (mode==MODE_NONE) {
		fprintf(stderr,"No operating mode selected, use -w or -r\n");
		helpme();
		exit(1);
	}


	fd=openport(ser_port,ser_speed);
	if (fd<0) {
		fprintf(stderr,"Open %s failed\n",ser_port);
		exit(1);
	}


	for (i=0;i<UV3R_PREPARE_TRIES;i++)
	{
		if (verbose>0) { printf("uv3r_prepare: try %i\n",i); }
		r=uv3r_prepare(fd);
		if (r) break;
	}
	if (!r)
	{
		fprintf(stderr,"Failed to init radio\n");
		exit(1);
	}

	switch(mode)
	{
		case MODE_READ:

			for(i=0;i<UV3R_EEPROM_SIZE; i=i+UV3R_EEPROM_BLOCKSIZE) {
				if (!uv3r_readmem(fd,(unsigned char *)&eeprom[i],UV3R_EEPROM_BLOCKSIZE,i))
				{
					fprintf(stderr,"Failed to read block 0x%4.4X\n",i);
					exit(1);
				}
				if (verbose>0) { 
					printf("\rread block 0x%4.4X  %i%%",i,(100*i/UV3R_EEPROM_SIZE)); 
					fflush(stdout); 
				}
			}
			//			uv3r_ack(fd);
			close(fd);
			if (verbose>0) { printf("\rSucessfuly read eeprom\n"); }
			if (verbose>2) { hdump((unsigned char *)&eeprom,UV3R_EEPROM_SIZE); }

			write_file(file,(unsigned char *)&eeprom,UV3R_EEPROM_SIZE);

			break;

		case MODE_WRITE:
			/* read file */
			ffd=open(file,O_RDONLY);
			if (ffd<0) {
				fprintf(stderr,"open %s error %d %s\n", file, errno, strerror(errno));
				exit(1);
			}
			r=read(ffd,(unsigned char *)&eeprom[i],UV3R_EEPROM_SIZE);
			if (r!=UV3R_EEPROM_SIZE) {
				fprintf(stderr,"Failed to read whole eeprom from file %s, file too short?\n",file);
				exit(1);
			}
			close(ffd);
			if (verbose>0) { printf ("Read file %s success\n",file); }

			/* write to radio */
			for(i=0;i<UV3R_EEPROM_SIZE; i=i+UV3R_EEPROM_BLOCKSIZE) {
				if (!uv3r_writemem(fd,(unsigned char *)&eeprom[i],UV3R_EEPROM_BLOCKSIZE,i))
				{
					fprintf(stderr,"Failed to write block 0x%4.4X\n",i);
					exit(1);
				}
				if (verbose>0) { 
					printf("\rwrite block 0x%4.4X  %i%%",i,(100*i/UV3R_EEPROM_SIZE)); 
					fflush(stdout); 
				}

			}
			if (verbose>0) { printf("\rSucessfuly wrote eeprom\n"); }


			break;
		default:
			fprintf(stderr,"this shouldn't happen :)\n");
			break;
	}

	return(0); /* silence gcc */
}
