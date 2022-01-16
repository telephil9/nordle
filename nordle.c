#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>

enum
{
	BACK,
	TEXT,
	BORD,
	WPOS,
	RPOS,
	NONE,
	NCOLS,
};

enum
{
	Maxwords = 4096,
	Padding = 4,
};

enum
{
	Mnewgame,
	Mexit,
};
char *menu3str[] = { "new game", "exit", nil };
Menu menu3 = { menu3str };

Mousectl *mc;
Keyboardctl *kc;
Font *lfont;
Font *kfont;
Image *cols[NCOLS];
Point ls;
Point l0;
Point k0;
char *words[Maxwords];
int nwords;
char *word;
int lcount;
int lnum;
char lines[5][6];
int tried[26];
int done;
int won;
int stats[2] = {0};

int
validword(char *w)
{
	if(strlen(w) != 5)
		return 0;
	while(*w){
		if(*w < 'a' || *w > 'z')
			return 0;
		w++;
	}
	return 1;
}

void
readwords(char *filename)
{
	Biobuf *bp;
	char *s;
	int l;

	bp = Bopen(filename, OREAD);
	if(bp == nil)
		sysfatal("Bopen: %r");
	nwords = 0;
	for(l = 1;; l++){
		s = Brdstr(bp, '\n', 1);
		if(s == nil)
			break;
		if(!validword(s)){
			fprint(2, "invalid word '%s' at line %d\n", s, l);
			threadexitsall("invalid word");
		}
		words[nwords++] = s;
		if(nwords == Maxwords)
			break;
	}
	Bterm(bp);
	if(nwords == 0){
		fprint(2, "empty wordlist\n");
		threadexitsall("empty wordlist");
	}
}

int
wordexists(void)
{
	char w[6] = {0};
	int i;

	for(i = 0; i < 5; i++)
		w[i] = lines[i][lnum];
	for(i = 0; i < nwords; i++)
		if(strncmp(words[i], w, 5) == 0)
			return 1;
	return 0;
}

void
newgame(void)
{
	done = 0;
	won = 0;
	lcount = -1;
	lnum = 0;
	memset(tried, 0, 26*sizeof(uint));
	memset(lines, 0, 30*sizeof(char));
	word = words[nrand(nwords)];
}

int
foundword(void)
{
	int i;
	for(i = 0; i < 5; i++)
		if(lines[i][lnum - 1] != word[i])
			return 0;
	return 1;
}

int
trystate(int col, int row)
{
	int s;

	if(lines[col][row] == word[col])
		s = RPOS;
	else if(strchr(word, lines[col][row]) != nil)
		s = WPOS;
	else
		s = NONE;
	return s;
}

void
showmessage(char *message)
{
	Point p;
	int w;

	p = screen->r.min;
	p.y += Padding + 1.5*kfont->height;
	w = stringwidth(kfont, message);
	p.x += (Dx(screen->r) - w) / 2;
	string(screen, p, cols[WPOS], ZP, kfont, message);
	flushimage(display, 1);
}

void
redraw(void)
{
	int i, j, c;
	Point p, p1, pl;
	Rectangle br;
	char m[255], s[2] = {0};

	draw(screen, screen->r, cols[BACK], nil, ZP);
	snprint(m, sizeof m, "%d/%d games won", stats[0], stats[1]);
	string(screen, addpt(screen->r.min, Pt(Padding, Padding)), cols[BORD], ZP, font, m);
	if(done){
		p = screen->r.min;
		p.y += Padding + 1.5*kfont->height;
		if(won){
			snprint(m, sizeof m, "congratulations");
			c = RPOS;
		}else{
			snprint(m, sizeof m, "word was: %s", word);
			c = TEXT;
		}
		i = stringwidth(kfont, m);
		p.x += (Dx(screen->r) - i) / 2;
		string(screen, p, cols[c], ZP, kfont, m);
	}
	for(j = 0; j < 6; j++){
		for(i = 0; i < 5; i++){
			p = Pt(l0.x + i*(ls.x + Padding), l0.y + j*(ls.y + Padding));
			p1 = Pt(p.x + ls.x, p.y + ls.y);
			br = Rpt(p, p1);
			if(lnum < j || (lnum == j && lcount != 5))
				border(screen, br, 1, cols[BORD], ZP);
			if(lnum > j){
				c = trystate(i, j);
				draw(screen, br, cols[c], nil, ZP);
			}
			if(lnum > j || (lnum == j && lcount >= i)){
				pl.x = p.x + (Dx(br) - ls.x/2) / 2;
				pl.y = p.y + (Dy(br) - ls.y/2) / 2;
				s[0] = toupper(lines[i][j]);
				stringn(screen, pl, cols[TEXT], ZP, lfont, s, 1);
			}
		}
	}
	j = 0;
	p = k0;
	for(i = 0; i < 26; i++){
		if(tried[i] == 0)
			continue;
		c = tried[i] == 1 ? NONE : RPOS;
		s[0] = 'A' + i;
		p = stringnbg(screen, p, cols[TEXT], ZP, kfont, s, 1, cols[c], ZP);
		p.x += 4;
		j += 1;
		if(j == 13){
			j = 0;
			p.x = k0.x;
			p.y += kfont->height;
		}
	}
	flushimage(display, 1);
}

void
eresize(void)
{
	redraw();
}

void
emouse(Mouse *m)
{
	if(m->buttons != 4)
		return;
	switch(menuhit(3, mc, &menu3, nil)){
	case Mnewgame:
		newgame();
		redraw();
		break;
	case Mexit:
		threadexitsall(nil);
	}
}

void
ekeyboard(Rune k)
{
	int i;
	char c;

	if(k == Kdel)
		threadexitsall(nil);
	if(done){
		if(k == '\n'){
			newgame();
			redraw();
		}
		return;
	}
	if(lcount < 4 && 'a' <= k && k <= 'z'){
		lines[++lcount][lnum] = (char)k;
		redraw();
	}else if(k == Kbs){
		if(lcount >= 0){
			lcount -= 1;
			redraw();
		}
	}else if(k == '\n'){
		if(lcount == 4){
			if(!wordexists()){
				lcount = -1;
				redraw();
				showmessage("invalid word");
				return;
			}
			for(i = 0; i < 5; i++){
				c = lines[i][lnum];
				tried[c - 'a'] = 1 + (strchr(word, c) != nil);
			}
			++lnum;
			lcount = -1;
			if(foundword()){
				stats[1]++;
				stats[0]++;
				done = 1;
				won = 1;
			}else if(lnum == 6){
				stats[1]++;
				done = 1;
				won = 0;
			}
			redraw();
		}
	}

}

void
initcols(void)
{
	cols[BACK] = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0x121213ff);
	cols[TEXT] = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0xd7dadcff);
	cols[BORD] = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0x818384ff);
	cols[WPOS] = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0xb59f3bff);
	cols[RPOS] = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0x538d4eff);
	cols[NONE] = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, 0x3a3a3cff);
}

void
initfont(void)
{
	lfont = openfont(display, "/lib/font/bit/lucidasans/typeunicode.16.font");
	if(lfont == nil)
		sysfatal("openfont: %r");
	kfont = font;
}

void
initsize(void)
{
	Point p;
	int w, h, lw, lh, wfd, n;
	char buf[255];

	p = mulpt(stringsize(lfont, " "), 2);
	lw = 5*(p.x + Padding);
	lh = 6*(p.y + Padding);
	w = 2*(p.x + Padding) + lw;
	h = Padding + 3*kfont->height + lh + Padding + 3*kfont->height + Padding;
	ls = p;
	l0 = Pt(screen->r.min.x + Padding + p.x, screen->r.min.y + Padding + 3*kfont->height);
	k0 = Pt(screen->r.min.x + Padding, screen->r.max.y - Padding - 2*kfont->height);
	wfd = open("/dev/wctl", OWRITE);
	if(wfd < 0)
		sysfatal("open: %r");
	n = snprint(buf, sizeof buf, "resize -dx %d -dy %d", w, h);
	write(wfd, buf, n);
	close(wfd);
}

void
usage(void)
{
	fprint(2, "usage: %s <filename>\n", argv0);
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	enum { Emouse, Eresize, Ekeyboard };
	Mouse m;
	Rune k;
	Alt a[] = {
		{ nil, &m,  CHANRCV },
		{ nil, nil, CHANRCV },
		{ nil, &k,  CHANRCV },
		{ nil, nil, CHANEND },
	};

	ARGBEGIN{ }ARGEND;
	if(argc != 1)
		usage();
	readwords(argv[0]);
	//srand(31337);
	srand(time(0));
	newgame();
	if(initdraw(nil, nil, argv0) < 0)
		sysfatal("initdraw: %r");
	if((mc = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");
	if((kc = initkeyboard(nil)) == nil)
		sysfatal("initkeyboard: %r");
	a[Emouse].c = mc->c;
	a[Eresize].c = mc->resizec;
	a[Ekeyboard].c = kc->c;
	initfont();
	initcols();
	initsize();
	redraw();
	for(;;){
		switch(alt(a)){
		case Emouse:
			emouse(&m);
			break;
		case Eresize:
			if(getwindow(display, Refnone) < 0)
				sysfatal("getwindow: %r");
			eresize();
			break;
		case Ekeyboard:
			ekeyboard(k);
			break;
		}
	}
}

