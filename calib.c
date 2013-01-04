/* 
Copyright (c) 2013, @jmrobles
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. All advertising materials mentioning features or use of this software
   must display the following acknowledgement:
   This product includes software developed by the University of 
   California, Berkeley and its contributors.
4. Neither the name of the University nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ''AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>


/* Defines */


/* MWM decorations values */

 #define MWM_DECOR_NONE          0
 #define MWM_DECOR_ALL           (1L << 0)
 #define MWM_DECOR_BORDER        (1L << 1)
 #define MWM_DECOR_RESIZEH       (1L << 2)
 #define MWM_DECOR_TITLE         (1L << 3)
 #define MWM_DECOR_MENU          (1L << 4)
 #define MWM_DECOR_MINIMIZE      (1L << 5)
 #define MWM_DECOR_MAXIMIZE      (1L << 6)

/* ELO Protocol */

#define ELO_START1 0x55;
#define ELO_START2 0x54;

#define ELO_TYPE_PRESS 		0x01
#define ELO_TYPE_MOVE  		0x02
#define ELO_TYPE_RELEASE	0x04
#define NUM_POINTS 3
#define MAX_PATH_NAME		255
#define KEY_ESC	9
#define MARGIN 100
#define DEFAULT_SERIAL_LINE "/dev/ttyS0"
#define MSG_START "ELO Graphics Calibration Tool"
#define MSG_END "Finished - Press ESC to view results"
#define MSG_TITLE "ELO Graphics Touch Calib Xorg"


/* Structs */

typedef struct
{
  unsigned char start1;
  unsigned char start2;
  unsigned char type;
  unsigned short x;
  unsigned short y;
  unsigned short z;
  unsigned char crc;
} __attribute__((packed)) ELOFRAME;

/* Global vars */

FILE* gTouchFile = NULL;
int gContinue = -1;
int gState = 0;
int gOldState = -1;
int gPosX[NUM_POINTS];
int gPosY[NUM_POINTS];
unsigned short gX[NUM_POINTS];
unsigned short gY[NUM_POINTS];
char gSerial[MAX_PATH_NAME];



/* Func declarations */

int openTouch();
int readTouchFrame(ELOFRAME* frm);
int rect(int x,int x0,int x1,int y0,int y1);
void wm_nodecorations(Display* dpy,Window window);


/* Main */

int main(int argc,char** argv)
{
	Display *dpy;
	Window rootwin;
	Window win;
	XEvent e;
	int scr;
	GC gc;
	XWindowAttributes xwa;
	ELOFRAME frm;
	int width_text;
	int ret;
	XFontStruct * fs;
	int minX,minY,maxX,maxY;

	if(!(dpy=XOpenDisplay(NULL))) {
		fprintf(stderr, "ERROR: could not open display\n");
		exit(1);
	}

	// Serial
	gSerial[0] = '\0';
	if (argc > 1 )
	{
		strncat(gSerial,argv[1],sizeof(gSerial));
	}
	else
	{
		strncat(gSerial,DEFAULT_SERIAL_LINE,sizeof(gSerial));
	}

	scr = DefaultScreen(dpy);
	rootwin = RootWindow(dpy, scr);
	XGetWindowAttributes(dpy, DefaultRootWindow(dpy), &xwa);	
	// Set points
	gPosX[0] = MARGIN;
	gPosY[0] = MARGIN;
		
	gPosX[1] = xwa.width - MARGIN;
	gPosY[1] = MARGIN;

	gPosX[2] = MARGIN;
	gPosY[2] = xwa.height - MARGIN;

	win=XCreateSimpleWindow(dpy, rootwin, 0,0, xwa.width, xwa.height, 0, 
			BlackPixel(dpy, scr), BlackPixel(dpy, scr));
	
	XStoreName(dpy, win,MSG_TITLE );
	wm_nodecorations(dpy,win);
	gc=XCreateGC(dpy, win, 0, NULL);
	XSetForeground(dpy, gc, WhitePixel(dpy, scr));
	XSelectInput(dpy, win, ExposureMask|ButtonPressMask|ButtonReleaseMask| KeyPressMask | KeyReleaseMask);
	XMapWindow(dpy, win);

	XEvent xev;
	Atom wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
	Atom fullscreen = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);

	memset(&xev, 0, sizeof(xev));
	xev.type = ClientMessage;
	xev.xclient.window = win;
	xev.xclient.message_type = wm_state;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = 1;
	xev.xclient.data.l[1] = fullscreen;
	xev.xclient.data.l[2] = 0;

	XSendEvent(dpy, DefaultRootWindow(dpy), False,
	SubstructureNotifyMask, &xev);
	gContinue = 1;

	ret = openTouch();
	if (ret < 0 )	
	{
		printf("Can't open touch\n");
		exit(-1);
	}


	while(gContinue) {
		// Process X Events
		while ( XPending(dpy) > 0 )
		{
			XNextEvent(dpy, &e);
			if(e.type==Expose && e.xexpose.count<1)
			{

			}
			else if (e.type == KeyRelease && e.xkey.keycode == KEY_ESC )
			{
				gContinue = 0;
				break;
			}
		}
		// Process Touch Events
		
		// -- Read Event

		ret = readTouchFrame(&frm);
		if ( ret == 0 )
		{
			if (frm.type == ELO_TYPE_RELEASE )
			{
				if (gState == 0)  
				{
					gX[0] = frm.x;
					gY[0] = frm.y;
					gState = 1;
				} 
				else if (gState == 1 )
				{
					gX[1] = frm.x;
					gY[1] = frm.y;
					gState = 2;

				}
				else if (gState == 2)
				{
					gX[2] = frm.x;
					gY[2] = frm.y;
					gState = 3;
				}
				
			
			}
			
		}
		if (gOldState != gState )
		{
			gOldState = gState;
			XClearWindow(dpy,win);
			fs = XQueryFont(dpy, (XID)gc);
			if (gState == 3 )
			{
				if ( fs == NULL )
				{
					printf("Can't query font size\n");
					width_text = strlen(MSG_END)*6;
				}
				else
				{
					width_text = XTextWidth(fs, MSG_START, strlen(MSG_START));
				}

				XDrawString(dpy, win, gc, xwa.width/2, xwa.height/2, MSG_END, strlen(MSG_END));
				// Compute 
				
				minX = rect(0,        gPosX[0],gPosX[1],gX[0],gX[1]);
				maxX = rect(xwa.width,gPosX[0],gPosX[1],gX[0],gX[1]);
			
				maxY = rect(0         ,gPosY[0],gPosY[2],gY[0],gY[2]);
				minY = rect(xwa.height,gPosY[0],gPosY[2],gY[0],gY[2]);
				printf("Results:\n");
				printf("---- cut here ----\n");
				printf("Section \"Inputdevice\"\n");
        			printf("\tIdentifier \"touchscreen1\"\n");
				printf("\tDriver \"elographics\"\n");
				printf("\tOption \"Device\" \"%s\"\n",gSerial);
				printf("\tOption \"MinX\"  \"%i\"\n",minX);
				printf("\tOption \"MaxX\"  \"%i\"\n",maxX);
				printf("\tOption \"MinY\"  \"%i\"\n",minY);
				printf("\tOption \"MaxY\"  \"%i\"\n",maxY);
				printf("EndSection\n");
				printf("---- end cut  ----\n");

			}
			else
			{
				XDrawArc(dpy, win, gc, gPosX[gState]-25, gPosY[gState]-25, 50, 50, 0, 360*64);
				XDrawArc(dpy, win, gc, gPosX[gState]-3, gPosY[gState]-3, 6, 6, 0, 360*64);
				
				if ( fs == NULL )
				{
					printf("Can't query font size\n");
					width_text = strlen(MSG_START)*6;
				}
				else
				{
					width_text = XTextWidth(fs, MSG_START, strlen(MSG_START));
				}

				XDrawString(dpy, win, gc, (xwa.width-width_text)/2, xwa.height/2, MSG_START, strlen(MSG_START));
			}
			
		}
		usleep(50*1000);


	}

	XCloseDisplay(dpy);

	return 0;
}


/* Func definition */


int openTouch()
{
   gTouchFile = fopen(gSerial,"r");
 
   if ( gTouchFile == NULL )
   {
	return -1;
   }
   int status;
   int fd = fileno(gTouchFile);
   status = fcntl(fd,F_GETFL,NULL);
   status |= O_NONBLOCK;
   fcntl(fd,F_SETFL,status);

   return 0;
}
int readTouchFrame(ELOFRAME* frm)
{
  int ret;
  if ( gTouchFile == NULL )
  {
	return -1;
  }

  ret = fread(frm,sizeof(ELOFRAME),1,gTouchFile);
  if ( ret != 1 )
  {
//        printf("INVALID READ SIZE: %i\n",ret);
        return -1;
  }
  if (frm->type == 4)
  {
  printf("ELO FRAME DUMP:\n");
  printf("\tType: %i\n",frm->type);
  printf("\tX: %i\n",frm->x);
  printf("\tY: %i\n",frm->y);
  }
  return 0;

}

int rect(int x,int x0,int x1,int y0,int y1)
{
  float r;
//  printf("RECT\n");
//  printf("\tx = %i\n");

  r = ( (((float) x - (float)  x0) / ( (float)x1 - (float)x0  ))* ( (float)y1 - (float)y0 )) + (float)y0;
  return (int) r;
}

void wm_nodecorations(Display* dpy,Window window)
{
    Atom WM_HINTS;
    WM_HINTS = XInternAtom(dpy, "_MOTIF_WM_HINTS", True);

    if ( WM_HINTS != None ) {
        #define MWM_HINTS_DECORATIONS   (1L << 1)
        struct {
          unsigned long flags;
          unsigned long functions;
          unsigned long decorations;
                   long input_mode;
          unsigned long status;
        } MWMHints = { MWM_HINTS_DECORATIONS, 0,
            MWM_DECOR_NONE, 0, 0 };
        XChangeProperty(dpy, window, WM_HINTS, WM_HINTS, 32,
                        PropModeReplace, (unsigned char *)&MWMHints,
                        sizeof(MWMHints)/4);
    }
    XSetTransientForHint(dpy, window, RootWindow(dpy, DefaultScreen(dpy)));
    XUnmapWindow(dpy, window);
    XMapWindow(dpy, window);
}

