/* Native TrueType rasterization. Fonts are sampled at twice their display size. */
#include "port.h"
#include "font.h"
#include "fs.h"

int
font_load(struct font *f, const char *name, int sizes[3])
{
	char *path;
	int i;

	memset(f, 0, sizeof(*f));
	path = fs_realpath(name);
	if(path == nil)
		return 0;
	for(i=0; i<3; i++){
		f->ttf[i] = ttfopen(path, sizes[i]*2, 0);
		if(f->ttf[i] == nil){
			fprint(2, "font %s: %r\n", path);
			font_free(f);
			free(path);
			return 0;
		}
	}
	free(path);
	snprint(f->path, sizeof(f->path), "%s", name);
	return 1;
}

void
font_free(struct font *f)
{
	int i;

	for(i=0; i<3; i++)
		if(f->ttf[i] != nil)
			ttfclose(f->ttf[i]);
	memset(f, 0, sizeof(*f));
}

void
font_measure(TTFont *f, const char *text, int *w, int *h)
{
	TTGlyph *g;
	Rune r;
	int width;

	width = 0;
	while(*text){
		text += chartorune(&r, text);
		g = ttfgetglyph(f, ttffindchar(f, r), 1);
		if(g == nil)
			sysfatal("render glyph: %r");
		width += g->advanceWidthpx;
		ttfputglyph(g);
	}
	*w = (width+1)/2;
	*h = (f->ascentpx+f->descentpx+1)/2;
}

int font_init(void) { return 1; }
void font_quit(void) {}
