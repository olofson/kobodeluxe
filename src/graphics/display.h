/*(LGPL)
 * Simple display for score, lives etc.
 * © David Olofson, 2001
 */

#include "window.h"

class display_t : public window_t
{
	char	_caption[64];
	char	_text[64];
	int	_on;
	Uint32	_color;
	void render_caption();
	void render_text();
  public:
	display_t();
	void refresh(SDL_Rect *r);
	void color(Uint32 _cl);
	void caption(const char *cap);
	void text(const char *txt);
	void on();
	void off();
};
