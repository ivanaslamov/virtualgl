/* Copyright (C)2004 Landmark Graphics Corporation
 * Copyright (C)2005, 2006 Sun Microsystems, Inc.
 * Copyright (C)2009 D. R. Commander
 *
 * This library is free software and may be redistributed and/or modified under
 * the terms of the wxWindows Library License, Version 3.1 or (at your option)
 * any later version.  The full license is in the LICENSE.txt file included
 * with this distribution.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * wxWindows Library License for more details.
 */

#include "rrxvtrans.h"
#include "rrtimer.h"
#include "fakerconfig.h"

rrxvtrans::rrxvtrans(void) : _t(NULL), _deadyet(false)
{
	for(int i=0; i<NB; i++) _bmp[i]=NULL;
	errifnot(_t=new Thread(this));
	_t->start();
	_prof_xv.setname("XV        ");
	_prof_total.setname("Total     ");
	if(fconfig.verbose) fbxv_printwarnings(rrout.getfile());
}

void rrxvtrans::run(void)
{
	rrtimer t, sleept;  double err=0.;  bool first=true;

	try {
 
	while(!_deadyet)
	{
		rrxvframe *b=NULL;
		_q.get((void **)&b);  if(_deadyet) return;
		if(!b) _throw("Queue has been shut down");
		_ready.signal();
		_prof_xv.startframe();
		b->redraw();
		_prof_xv.endframe(b->_h.width*b->_h.height, 0, 1);

		_prof_total.endframe(b->_h.width*b->_h.height, 0, 1);
		_prof_total.startframe();

		if(fconfig.flushdelay>0.)
		{
			long usec=(long)(fconfig.flushdelay*1000000.);
			if(usec>0) usleep(usec);
		}
		if(fconfig.fps>0.)
		{
			double elapsed=t.elapsed();
			if(first) first=false;
			else
			{
				if(elapsed<1./fconfig.fps)
				{
					sleept.start();
					long usec=(long)((1./fconfig.fps-elapsed-err)*1000000.);
					if(usec>0) usleep(usec);
					double sleeptime=sleept.elapsed();
					err=sleeptime-(1./fconfig.fps-elapsed-err);  if(err<0.) err=0.;
				}
			}
			t.start();
		}

		b->complete();
	}

	} catch(rrerror &e)
	{
		if(_t) _t->seterror(e);
		_ready.signal();  throw;
	}
}

rrxvframe *rrxvtrans::getbitmap(Display *dpy, Window win, int w, int h,
	bool spoil)
{
	rrxvframe *b=NULL;
	if(!spoil) _ready.wait();
	if(_t) _t->checkerror();
	{
	rrcs::safelock l(_bmpmutex);
	int bmpi=-1;
	for(int i=0; i<NB; i++)
		if(!_bmp[i] || (_bmp[i] && _bmp[i]->iscomplete())) bmpi=i;
	if(bmpi<0) _throw("No free buffers in pool");
	if(!_bmp[bmpi]) errifnot(_bmp[bmpi]=new rrxvframe(dpy, win));
	b=_bmp[bmpi];  b->waituntilcomplete();
	}
	rrframeheader hdr;
	memset(&hdr, 0, sizeof(hdr));
	hdr.height=hdr.frameh=h;
	hdr.width=hdr.framew=w;
	hdr.x=hdr.y=0;
	b->init(hdr);
	return b;
}

bool rrxvtrans::frameready(void)
{
	if(_t) _t->checkerror();
	return(_q.items()<=0);
}

static void __rrxvtrans_spoilfct(void *b)
{
	if(b) ((rrxvframe *)b)->complete();
}

void rrxvtrans::sendframe(rrxvframe *b, bool sync)
{
	if(_t) _t->checkerror();
	if(sync) 
	{
		_prof_xv.startframe();
		b->redraw();
		b->complete();
		_prof_xv.endframe(b->_h.width*b->_h.height, 0, 1);
		_ready.signal();
	}
	else _q.spoil((void *)b, __rrxvtrans_spoilfct);
}