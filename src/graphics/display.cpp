/*(LGPL)
 * Simple display for score, lives etc.
 * © David Olofson, 2001-2003, 2007
 */

#define	D_LINE_HEIGHT	9

#define	D_LINE1_POS	1
#define	D_LINE1_TXOFFS	0

#define	D_LINE2_POS	9
#define	D_LINE2_TXOFFS	0

#include <string.h>

#include "window.h"
#include "display.h"

display_t::display_t()
{
	_on = 0;
	caption("CAPTION");
	text("TEXT");
	on();
}


void display_t::color(Uint32 _cl)
{
	_color = _cl;
	if(_on)
		invalidate();
}


void display_t::caption(const char *cap)
{
	strncpy(_caption, cap, sizeof(_caption));
	if(_on)
		invalidate();
}


void display_t::text(const char *txt)
{
	strncpy(_text, txt, sizeof(_text));
	if(_on)
		invalidate();
}


void display_t::on()
{
	if(_on)
		return;

	_on = 1;
	invalidate();
}


void display_t::off()
{
	if(!_on)
		return;

	_on = 0;
	invalidate();
}


void display_t::render_caption()
{
	SDL_Rect r;
	r.x = 0;
	r.y = D_LINE1_POS;
	r.w = width();
	r.h = D_LINE_HEIGHT;
	background(_color);
	clear(&r);
	if(_on)
		center(D_LINE1_POS + D_LINE1_TXOFFS, _caption);
}


void display_t::render_text()
{
	SDL_Rect r;
	r.x = 0;
	r.y = D_LINE2_POS;
	r.w = width();
	r.h = D_LINE_HEIGHT;
	background(_color);
	clear(&r);
	if(_on)
		center(D_LINE2_POS + D_LINE2_TXOFFS, _text);
}


void display_t::refresh(SDL_Rect *r)
{
	render_caption();
	render_text();
}
