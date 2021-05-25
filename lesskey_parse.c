#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lesskey.h"
#include "cmd.h"
#include "xbuf.h"
#include "defines.h"

#define CONTROL(c)      ((c)&037)
#define ESC             CONTROL('[')

extern void lesskey_parse_error(char *msg);
extern char *homefile(char *filename);
extern void *ecalloc(int count, unsigned int size);

static int linenum;
static int errors;
static char *lesskey_file;

static struct lesskey_cmdname cmdnames[] = 
{
	{ "back-bracket",         A_B_BRACKET },
	{ "back-line",            A_B_LINE },
	{ "back-line-force",      A_BF_LINE },
	{ "back-screen",          A_B_SCREEN },
	{ "back-scroll",          A_B_SCROLL },
	{ "back-search",          A_B_SEARCH },
	{ "back-window",          A_B_WINDOW },
	{ "clear-mark",           A_CLRMARK },
	{ "debug",                A_DEBUG },
	{ "digit",                A_DIGIT },
	{ "display-flag",         A_DISP_OPTION },
	{ "display-option",       A_DISP_OPTION },
	{ "end",                  A_GOEND },
	{ "end-scroll",           A_RRSHIFT },
	{ "examine",              A_EXAMINE },
	{ "filter",               A_FILTER },
	{ "first-cmd",            A_FIRSTCMD },
	{ "firstcmd",             A_FIRSTCMD },
	{ "flush-repaint",        A_FREPAINT },
	{ "forw-bracket",         A_F_BRACKET },
	{ "forw-forever",         A_F_FOREVER },
	{ "forw-until-hilite",    A_F_UNTIL_HILITE },
	{ "forw-line",            A_F_LINE },
	{ "forw-line-force",      A_FF_LINE },
	{ "forw-screen",          A_F_SCREEN },
	{ "forw-screen-force",    A_FF_SCREEN },
	{ "forw-scroll",          A_F_SCROLL },
	{ "forw-search",          A_F_SEARCH },
	{ "forw-window",          A_F_WINDOW },
	{ "goto-end",             A_GOEND },
	{ "goto-end-buffered",    A_GOEND_BUF },
	{ "goto-line",            A_GOLINE },
	{ "goto-mark",            A_GOMARK },
	{ "help",                 A_HELP },
	{ "index-file",           A_INDEX_FILE },
	{ "invalid",              A_UINVALID },
	{ "left-scroll",          A_LSHIFT },
	{ "next-file",            A_NEXT_FILE },
	{ "next-tag",             A_NEXT_TAG },
	{ "noaction",             A_NOACTION },
	{ "no-scroll",            A_LLSHIFT },
	{ "percent",              A_PERCENT },
	{ "pipe",                 A_PIPE },
	{ "prev-file",            A_PREV_FILE },
	{ "prev-tag",             A_PREV_TAG },
	{ "quit",                 A_QUIT },
	{ "remove-file",          A_REMOVE_FILE },
	{ "repaint",              A_REPAINT },
	{ "repaint-flush",        A_FREPAINT },
	{ "repeat-search",        A_AGAIN_SEARCH },
	{ "repeat-search-all",    A_T_AGAIN_SEARCH },
	{ "reverse-search",       A_REVERSE_SEARCH },
	{ "reverse-search-all",   A_T_REVERSE_SEARCH },
	{ "right-scroll",         A_RSHIFT },
	{ "set-mark",             A_SETMARK },
	{ "set-mark-bottom",      A_SETMARKBOT },
	{ "shell",                A_SHELL },
	{ "status",               A_STAT },
	{ "toggle-flag",          A_OPT_TOGGLE },
	{ "toggle-option",        A_OPT_TOGGLE },
	{ "undo-hilite",          A_UNDO_SEARCH },
	{ "clear-search",         A_CLR_SEARCH },
	{ "version",              A_VERSION },
	{ "visual",               A_VISUAL },
	{ NULL,   0 }
};

static struct lesskey_cmdname editnames[] = 
{
	{ "back-complete",      EC_B_COMPLETE },
	{ "backspace",          EC_BACKSPACE },
	{ "delete",             EC_DELETE },
	{ "down",               EC_DOWN },
	{ "end",                EC_END },
	{ "expand",             EC_EXPAND },
	{ "forw-complete",      EC_F_COMPLETE },
	{ "home",               EC_HOME },
	{ "insert",             EC_INSERT },
	{ "invalid",            EC_UINVALID },
	{ "kill-line",          EC_LINEKILL },
	{ "abort",              EC_ABORT },
	{ "left",               EC_LEFT },
	{ "literal",            EC_LITERAL },
	{ "right",              EC_RIGHT },
	{ "up",                 EC_UP },
	{ "word-backspace",     EC_W_BACKSPACE },
	{ "word-delete",        EC_W_DELETE },
	{ "word-left",          EC_W_LEFT },
	{ "word-right",         EC_W_RIGHT },
	{ NULL, 0 }
};

/*
 * Print a parse error message.
 */
	static void
parse_error(s1, s2)
	char *s1;
	char *s2;
{
	char buf[1024];
	++errors;
	snprintf(buf, sizeof(buf), "%s: line %d: %s%s", lesskey_file, linenum, s1, s2);
	lesskey_parse_error(buf);
}

/*
 * Initialize lesskey_tables.
 */
	static void
init_tables(tables)
	struct lesskey_tables *tables;
{
	tables->currtable = &tables->cmdtable;

	tables->cmdtable.names = cmdnames;
	tables->cmdtable.is_var = 0;
	xbuf_init(&tables->cmdtable.buf);

	tables->edittable.names = editnames;
	tables->edittable.is_var = 0;
	xbuf_init(&tables->edittable.buf);

	tables->vartable.names = NULL;
	tables->vartable.is_var = 1;
	xbuf_init(&tables->vartable.buf);
}

/*
 * Parse one character of a string.
 */
	static char *
tstr(pp, xlate)
	char **pp;
	int xlate;
{
	char *p;
	char ch;
	int i;
	static char buf[10];
	static char tstr_control_k[] =
		{ SK_SPECIAL_KEY, SK_CONTROL_K, 6, 1, 1, 1, '\0' };

	p = *pp;
	switch (*p)
	{
	case '\\':
		++p;
		switch (*p)
		{
		case '0': case '1': case '2': case '3':
		case '4': case '5': case '6': case '7':
			/*
			 * Parse an octal number.
			 */
			ch = 0;
			i = 0;
			do
				ch = 8*ch + (*p - '0');
			while (*++p >= '0' && *p <= '7' && ++i < 3);
			*pp = p;
			if (xlate && ch == CONTROL('K'))
				return tstr_control_k;
			buf[0] = ch;
			buf[1] = '\0';
			return (buf);
		case 'b':
			*pp = p+1;
			return ("\b");
		case 'e':
			*pp = p+1;
			buf[0] = ESC;
			buf[1] = '\0';
			return (buf);
		case 'n':
			*pp = p+1;
			return ("\n");
		case 'r':
			*pp = p+1;
			return ("\r");
		case 't':
			*pp = p+1;
			return ("\t");
		case 'k':
			if (xlate)
			{
				switch (*++p)
				{
				case 'u': ch = SK_UP_ARROW; break;
				case 'd': ch = SK_DOWN_ARROW; break;
				case 'r': ch = SK_RIGHT_ARROW; break;
				case 'l': ch = SK_LEFT_ARROW; break;
				case 'U': ch = SK_PAGE_UP; break;
				case 'D': ch = SK_PAGE_DOWN; break;
				case 'h': ch = SK_HOME; break;
				case 'e': ch = SK_END; break;
				case 'x': ch = SK_DELETE; break;
				default: { char buf[2]; buf[0] = *p; buf[1] = '\0';
					parse_error("illegal escape sequence \\k", buf);
					*pp = p+1;
					return (""); }
				}
				*pp = p+1;
				buf[0] = SK_SPECIAL_KEY;
				buf[1] = ch;
				buf[2] = 6;
				buf[3] = 1;
				buf[4] = 1;
				buf[5] = 1;
				buf[6] = '\0';
				return (buf);
			}
			/* FALLTHRU */
		default:
			/*
			 * Backslash followed by any other char 
			 * just means that char.
			 */
			*pp = p+1;
			buf[0] = *p;
			buf[1] = '\0';
			if (xlate && buf[0] == CONTROL('K'))
				return tstr_control_k;
			return (buf);
		}
	case '^':
		/*
		 * Caret means CONTROL.
		 */
		*pp = p+2;
		buf[0] = CONTROL(p[1]);
		buf[1] = '\0';
		if (xlate && buf[0] == CONTROL('K'))
			return tstr_control_k;
		return (buf);
	}
	*pp = p+1;
	buf[0] = *p;
	buf[1] = '\0';
	if (xlate && buf[0] == CONTROL('K'))
		return tstr_control_k;
	return (buf);
}

	static int
issp(ch)
	char ch;
{
	return (ch == ' ' || ch == '\t');
}

/*
 * Skip leading spaces in a string.
 */
	static char *
skipsp(s)
	char *s;
{
	while (issp(*s))
		s++;
	return (s);
}

/*
 * Skip non-space characters in a string.
 */
	static char *
skipnsp(s)
	char *s;
{
	while (*s != '\0' && !issp(*s))
		s++;
	return (s);
}

/*
 * Clean up an input line:
 * strip off the trailing newline & any trailing # comment.
 */
	static char *
clean_line(s)
	char *s;
{
	int i;

	s = skipsp(s);
	for (i = 0;  s[i] != '\0' && s[i] != '\n' && s[i] != '\r';  i++)
		if (s[i] == '#' && (i == 0 || s[i-1] != '\\'))
			break;
	s[i] = '\0';
	return (s);
}

/*
 * Add a byte to the output command table.
 */
	static void
add_cmd_char(c, tables)
	int c;
	struct lesskey_tables *tables;
{
	xbuf_add(&tables->currtable->buf, c);
}

/*
 * Add a string to the output command table.
 */
	static void
add_cmd_str(s, tables)
	char *s;
	struct lesskey_tables *tables;
{
	for ( ;  *s != '\0';  s++)
		add_cmd_char(*s, tables);
}

/*
 * See if we have a special "control" line.
 */
	static int
control_line(s, tables)
	char *s;
	struct lesskey_tables *tables;
{
#define PREFIX(str,pat) (strncmp(str,pat,strlen(pat)) == 0)

	if (PREFIX(s, "#line-edit"))
	{
		tables->currtable = &tables->edittable;
		return (1);
	}
	if (PREFIX(s, "#command"))
	{
		tables->currtable = &tables->cmdtable;
		return (1);
	}
	if (PREFIX(s, "#env"))
	{
		tables->currtable = &tables->vartable;
		return (1);
	}
	if (PREFIX(s, "#stop"))
	{
		add_cmd_char('\0', tables);
		add_cmd_char(A_END_LIST, tables);
		return (1);
	}
	return (0);
}

/*
 * Find an action, given the name of the action.
 */
	static int
findaction(actname, tables)
	char *actname;
	struct lesskey_tables *tables;
{
	int i;

	for (i = 0;  tables->currtable->names[i].cn_name != NULL;  i++)
		if (strcmp(tables->currtable->names[i].cn_name, actname) == 0)
			return (tables->currtable->names[i].cn_action);
	parse_error("unknown action: ", actname);
	return (A_INVALID);
}

/*
 * Parse a line describing one key binding, of the form
 *  KEY ACTION [EXTRA]
 * where KEY is the user key sequence, ACTION is the 
 * resulting less action, and EXTRA is an "extra" user
 * key sequence injected after the action.
 */
	static void
parse_cmdline(p, tables)
	char *p;
	struct lesskey_tables *tables;
{
	char *actname;
	int action;
	char *s;
	char c;

	/*
	 * Parse the command string and store it in the current table.
	 */
	do
	{
		s = tstr(&p, 1);
		add_cmd_str(s, tables);
	} while (*p != '\0' && !issp(*p));
	/*
	 * Terminate the command string with a null byte.
	 */
	add_cmd_char('\0', tables);

	/*
	 * Skip white space between the command string
	 * and the action name.
	 * Terminate the action name with a null byte.
	 */
	p = skipsp(p);
	if (*p == '\0')
	{
		parse_error("missing action", "");
		return;
	}
	actname = p;
	p = skipnsp(p);
	c = *p;
	*p = '\0';

	/*
	 * Parse the action name and store it in the current table.
	 */
	action = findaction(actname, tables);

	/*
	 * See if an extra string follows the action name.
	 */
	*p = c;
	p = skipsp(p);
	if (*p == '\0')
	{
		add_cmd_char(action, tables);
	} else
	{
		/*
		 * OR the special value A_EXTRA into the action byte.
		 * Put the extra string after the action byte.
		 */
		add_cmd_char(action | A_EXTRA, tables);
		while (*p != '\0')
			add_cmd_str(tstr(&p, 0), tables);
		add_cmd_char('\0', tables);
	}
}

/*
 * Parse a variable definition line, of the form
 *  NAME = VALUE
 */
	static void
parse_varline(line, tables)
	char *line;
	struct lesskey_tables *tables;
{
	char *s;
	char *p = line;

	do
	{
		s = tstr(&p, 0);
		add_cmd_str(s, tables);
	} while (*p != '\0' && !issp(*p) && *p != '=');
	/*
	 * Terminate the variable name with a null byte.
	 */
	add_cmd_char('\0', tables);

	p = skipsp(p);
	if (*p++ != '=')
	{
		parse_error("missing = in: ", line);
		return;
	}

	add_cmd_char(EV_OK|A_EXTRA, tables);

	p = skipsp(p);
	while (*p != '\0')
	{
		s = tstr(&p, 0);
		add_cmd_str(s, tables);
	}
	add_cmd_char('\0', tables);
}

/*
 * Parse a line from the lesskey file.
 */
	static void
parse_line(line, tables)
	char *line;
	struct lesskey_tables *tables;
{
	char *p;

	/*
	 * See if it is a control line.
	 */
	if (control_line(line, tables))
		return;
	/*
	 * Skip leading white space.
	 * Replace the final newline with a null byte.
	 * Ignore blank lines and comments.
	 */
	p = clean_line(line);
	if (*p == '\0')
		return;

	if (tables->currtable->is_var)
		parse_varline(p, tables);
	else
		parse_cmdline(p, tables);
}

/*
 * Parse a lesskey source file and store result in tables.
 */
	int
parse_lesskey(infile, tables)
	char *infile;
	struct lesskey_tables *tables;
{
	FILE *desc;
	char line[1024];

	if (infile == NULL)
		infile = homefile(DEF_LESSKEYINFILE);
	lesskey_file = infile;

	init_tables(tables);
	errors = 0;
	linenum = 0;

	/*
	 * Open the input file.
	 */
	if (strcmp(infile, "-") == 0)
		desc = stdin;
	else if ((desc = fopen(infile, "r")) == NULL)
	{
		/* parse_error("cannot open lesskey file ", infile); */
		return (-1);
	}

	/*
	 * Read and parse the input file, one line at a time.
	 */
	while (fgets(line, sizeof(line), desc) != NULL)
	{
		++linenum;
		parse_line(line, tables);
	}

	return (errors);
}
