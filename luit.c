/* $XTermId: luit.c,v 1.56 2013/01/26 16:41:52 tom Exp $ */

/*
Copyright 2010-2012,2013 by Thomas E. Dickey
Copyright (c) 2001 by Juliusz Chroboczek

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <luit.h>

#include <locale.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <signal.h>

#include <version.h>
#include <sys.h>
#include <parser.h>
#include <iso2022.h>

static void parent(int, int);

static int pipe_option = 0;
static int p2c_waitpipe[2];
static int c2p_waitpipe[2];

static Iso2022Ptr inputState = NULL, outputState = NULL;

static char *child_argv0 = NULL;
static const char *locale_name = NULL;
static int exitOnChild = 0;
static int converter = 0;
static int testonly = 0;
static int warnings = 0;

const char *locale_alias = LOCALE_ALIAS_FILE;

int ilog = -1;
int olog = -1;
int verbose = 0;
int ignore_locale = 0;
int fill_fontenc = 0;

static volatile int sigwinch_queued = 0;
static volatile int sigchld_queued = 0;

static int convert(int, int);
static int condom(int, char **);
static void child(char *, char *, char *const *);

void
Message(const char *f,...)
{
    va_list args;
    va_start(args, f);
    vfprintf(stderr, f, args);
    va_end(args);
}

void
Warning(const char *f,...)
{
    va_list args;
    va_start(args, f);
    fputs("Warning: ", stderr);
    vfprintf(stderr, f, args);
    va_end(args);
    ++warnings;
}

void
FatalError(const char *f,...)
{
    va_list args;
    va_start(args, f);
    vfprintf(stderr, f, args);
    va_end(args);
    ExitProgram(1);
}

static void
help(const char *program, int fatal)
{
#define DATA(name,mark,what) { #mark name, what }
    /* *INDENT-OFF* */
    static const struct {
	const char *name;
	const char *value;
    } options[] = {
	/* sorted per POSIX */
	DATA("V", -, "show version"),
	DATA("alias filename", -, "location of the locale alias file"),
	DATA("argv0 name", -, "set child's name"),
	DATA("c", -, "simple converter stdin/stdout"),
	DATA("encoding encoding", -, "use this encoding rather than current locale's encoding"),
	DATA("fill-fontenc", -, "fill in one-one mapping in -show-fontenc report"),
	DATA("g0 set", -, "set output G0 charset (default ASCII)"),
	DATA("g1 set", -, "set output G1 charset"),
	DATA("g2 set", -, "set output G2 charset"),
	DATA("g3 set", -, "set output G3 charset"),
	DATA("gl gn", -, "set output GL charset"),
	DATA("gr gk", -, "set output GR charset"),
	DATA("h", -, "show this message"),
	DATA("ilog filename", -, "log all input to this file"),
	DATA("k7", -, "generate 7-bit characters for input"),
	DATA("kg0 set", -, "set input G0 charset"),
	DATA("kg1 set", -, "set input G1 charset"),
	DATA("kg2 set", -, "set input G2 charset"),
	DATA("kg3 set", -, "set input G3 charset"),
	DATA("kgl gn", -, "set input GL charset"),
	DATA("kgr gk", -, "set input GR charset"),
	DATA("kls", -, "generate locking shifts SI/SO"),
	DATA("kss", +, "disable generation of single-shifts for input"),
	DATA("kssgr", +, "use GL after single-shift"),
	DATA("list", -, "list encodings recognized by this program"),
	DATA("list-fontenc", -, "list available \".enc\" encoding files"),
	DATA("list-iconv", -, "list iconv-supported encodings"),
	DATA("olog filename", -, "log all output to this file"),
	DATA("ols", +, "disable locking-shifts in output"),
	DATA("osl", +, "disable charset-selection sequences in output"),
	DATA("oss", +, "disable single-shifts in output"),
	DATA("ot", +, "disable interpretation of all sequences in output"),
	DATA("p", -, "do parent/child handshake"),
	DATA("show-fontenc", -, "show details of an \".enc\" encoding file"),
	DATA("show-iconv", -, "show iconv encoding in \".enc\" format"),
	DATA("t", -, "testing (initialize locale but no terminal)"),
	DATA("v", -, "verbose (repeat to increase level)"),
	DATA("x", -, "exit as soon as child dies"),
	DATA("-", -, "end of options"),
    };
    /* *INDENT-ON* */

    size_t n;
    size_t col, now;
    FILE *fp = fatal ? stderr : stdout;

    if (fatal) {
	col = 0;
	fprintf(fp, "%s\n", program);
	for (n = 0; n < SizeOf(options); ++n) {
	    if (col == 0) {
		fprintf(fp, " ");
		col = 1;
	    }
	    now = strlen(options[n].name) + 5;
	    col += now;
	    if (col > MAXCOLS) {
		fprintf(fp, "\n ");
		col = now + 1;
	    }
	    fprintf(fp, " [ %s ]", options[n].name);
	}
	if (col)
	    fprintf(fp, "\n");
	fprintf(fp, "  [ program [ args ] ]\n");
    } else {
	fprintf(fp, "Usage: %s [options] [ program [ args ] ]\n", program);
	fprintf(fp, "\n");
	fprintf(fp, "Options:\n");
	col = 0;
	for (n = 0; n < SizeOf(options); ++n) {
	    now = strlen(options[n].name);
	    if (now > col)
		col = now;
	}
	for (n = 0; n < SizeOf(options); ++n) {
	    fprintf(fp, "  %-*s  %s\n", (int) col, options[n].name,
		    options[n].value);
	}
    }
    fflush(fp);

    if (fatal)
	ExitProgram(EXIT_FAILURE);
}

#ifndef USE_ICONV
static void
needIconvCfg(void)
{
    Message("You need the iconv configuration for this option\n");
}

#define reportIconvCharsets() needIconvCfg()
#define showIconvCharset(name) needIconvCfg()
#endif

static int
parseOptions(int argc, char **argv)
{
    int i = 1;
    while (i < argc) {
	if (argv[i][0] != '-' && argv[i][0] != '+') {
	    break;
	} else if (!strcmp(argv[i], "--")) {
	    i++;
	    break;
	} else if (!strcmp(argv[i], "-v")) {
	    verbose++;
	    i++;
	} else if (!strcmp(argv[i], "-V")) {
	    printf("%s - %s\n", argv[0], LUIT_VERSION);
	    ExitProgram(0);
	} else if (!strcmp(argv[i], "-h")) {
	    help(argv[0], 0);
	    ExitProgram(0);
	} else if (!strcmp(argv[i], "-list")) {
	    reportCharsets();
	    ExitProgram(0);
	} else if (!strcmp(argv[i], "-fill-fontenc")) {
	    fill_fontenc = 1;
	    i++;
	} else if (!strcmp(argv[i], "-show-fontenc")) {
	    if (i + 1 >= argc)
		FatalError("-show-fontenc requires an argument\n");
	    showFontencCharset(argv[i + 1]);
	    ExitProgram(0);
	} else if (!strcmp(argv[i], "-show-iconv")) {
	    if (i + 1 >= argc)
		FatalError("-show-iconv requires an argument\n");
	    showIconvCharset(argv[i + 1]);
	    ExitProgram(0);
	} else if (!strcmp(argv[i], "-list-fontenc")) {
	    reportFontencCharsets();
	    ExitProgram(0);
	} else if (!strcmp(argv[i], "-list-iconv")) {
	    reportIconvCharsets();
	    ExitProgram(0);
	} else if (!strcmp(argv[i], "+oss")) {
	    outputState->outputFlags &= ~OF_SS;
	    i++;
	} else if (!strcmp(argv[i], "+ols")) {
	    outputState->outputFlags &= ~OF_LS;
	    i++;
	} else if (!strcmp(argv[i], "+osl")) {
	    outputState->outputFlags &= ~OF_SELECT;
	    i++;
	} else if (!strcmp(argv[i], "+ot")) {
	    outputState->outputFlags = OF_PASSTHRU;
	    i++;
	} else if (!strcmp(argv[i], "-k7")) {
	    inputState->inputFlags &= ~IF_EIGHTBIT;
	    i++;
	} else if (!strcmp(argv[i], "+kss")) {
	    inputState->inputFlags &= ~IF_SS;
	    i++;
	} else if (!strcmp(argv[1], "+kssgr")) {
	    inputState->inputFlags &= ~IF_SSGR;
	    i++;
	} else if (!strcmp(argv[i], "-kls")) {
	    inputState->inputFlags |= IF_LS;
	    i++;
	} else if (!strcmp(argv[i], "-g0")) {
	    if (i + 1 >= argc)
		FatalError("-g0 requires an argument\n");
	    G0(outputState) = getCharsetByName(argv[i + 1]);
	    i += 2;
	} else if (!strcmp(argv[i], "-g1")) {
	    if (i + 1 >= argc)
		FatalError("-g1 requires an argument\n");
	    G1(outputState) = getCharsetByName(argv[i + 1]);
	    i += 2;
	} else if (!strcmp(argv[i], "-g2")) {
	    if (i + 1 >= argc)
		FatalError("-g2 requires an argument\n");
	    G2(outputState) = getCharsetByName(argv[i + 1]);
	    i += 2;
	} else if (!strcmp(argv[i], "-g3")) {
	    if (i + 1 >= argc)
		FatalError("-g3 requires an argument\n");
	    G3(outputState) = getCharsetByName(argv[i + 1]);

	    i += 2;
	} else if (!strcmp(argv[i], "-gl")) {
	    int j;
	    if (i + 1 >= argc)
		FatalError("-gl requires an argument\n");
	    if (strlen(argv[i + 1]) != 2 ||
		argv[i + 1][0] != 'g')
		j = -1;
	    else
		j = argv[i + 1][1] - '0';
	    if (j < 0 || j > 3)
		FatalError("The argument of -gl "
			   "should be one of g0 through g3,\n"
			   "not %s\n", argv[i + 1]);
	    else
		outputState->glp = &outputState->g[j];
	    i += 2;
	} else if (!strcmp(argv[i], "-gr")) {
	    int j;
	    if (i + 1 >= argc)
		FatalError("-gr requires an argument\n");
	    if (strlen(argv[i + 1]) != 2 ||
		argv[i + 1][0] != 'g')
		j = -1;
	    else
		j = argv[i + 1][1] - '0';
	    if (j < 0 || j > 3)
		FatalError("The argument of -gl "
			   "should be one of g0 through g3,\n"
			   "not %s\n", argv[i + 1]);
	    else
		outputState->grp = &outputState->g[j];
	    i += 2;
	} else if (!strcmp(argv[i], "-kg0")) {
	    if (i + 1 >= argc)
		FatalError("-kg0 requires an argument\n");
	    G0(inputState) = getCharsetByName(argv[i + 1]);
	    i += 2;
	} else if (!strcmp(argv[i], "-kg1")) {
	    if (i + 1 >= argc)
		FatalError("-kg1 requires an argument\n");
	    G1(inputState) = getCharsetByName(argv[i + 1]);
	    i += 2;
	} else if (!strcmp(argv[i], "-kg2")) {
	    if (i + 1 >= argc)
		FatalError("-kg2 requires an argument\n");
	    G2(inputState) = getCharsetByName(argv[i + 1]);
	    i += 2;
	} else if (!strcmp(argv[i], "-kg3")) {
	    if (i + 1 >= argc)
		FatalError("-kg3 requires an argument\n");
	    G3(inputState) = getCharsetByName(argv[i + 1]);

	    i += 2;
	} else if (!strcmp(argv[i], "-kgl")) {
	    int j;
	    if (i + 1 >= argc)
		FatalError("-kgl requires an argument\n");
	    if (strlen(argv[i + 1]) != 2 ||
		argv[i + 1][0] != 'g')
		j = -1;
	    else
		j = argv[i + 1][1] - '0';
	    if (j < 0 || j > 3)
		FatalError("The argument of -kgl "
			   "should be one of g0 through g3,\n"
			   "not %s\n", argv[i + 1]);
	    else
		inputState->glp = &inputState->g[j];
	    i += 2;
	} else if (!strcmp(argv[i], "-kgr")) {
	    int j;
	    if (i + 1 >= argc)
		FatalError("-kgl requires an argument\n");
	    if (strlen(argv[i + 1]) != 2 ||
		argv[i + 1][0] != 'g')
		j = -1;
	    else
		j = argv[i + 1][1] - '0';
	    if (j < 0 || j > 3)
		FatalError("The argument of -kgl "
			   "should be one of g0 through g3,\n"
			   "not %s\n", argv[i + 1]);
	    else
		inputState->grp = &inputState->g[j];
	    i += 2;
	} else if (!strcmp(argv[i], "-argv0")) {
	    if (i + 1 >= argc)
		FatalError("-argv0 requires an argument\n");
	    child_argv0 = argv[i + 1];
	    i += 2;
	} else if (!strcmp(argv[i], "-x")) {
	    exitOnChild = 1;
	    i++;
	} else if (!strcmp(argv[i], "-c")) {
	    converter = 1;
	    i++;
	} else if (!strcmp(argv[i], "-ilog")) {
	    if (i + 1 >= argc)
		FatalError("-ilog requires an argument\n");
	    if (ilog >= 0)
		close(ilog);
	    ilog = open(argv[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0777);
	    if (ilog < 0) {
		perror("Couldn't open input log");
		ExitProgram(1);
	    }
	    i += 2;
	} else if (!strcmp(argv[i], "-olog")) {
	    if (i + 1 >= argc)
		FatalError("-olog requires an argument\n");
	    if (olog >= 0)
		close(olog);
	    olog = open(argv[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0777);
	    if (olog < 0) {
		perror("Couldn't open output log");
		ExitProgram(1);
	    }
	    i += 2;
	} else if (!strcmp(argv[i], "-alias")) {
	    if (i + 1 >= argc)
		FatalError("-alias requires an argument\n");
	    locale_alias = argv[i + 1];
	    i += 2;
	} else if (!strcmp(argv[i], "-encoding")) {
	    if (i + 1 >= argc)
		FatalError("-encoding requires an argument\n");
	    locale_name = argv[i + 1];
	    ignore_locale = 1;
	    i += 2;
	} else if (!strcmp(argv[i], "-p")) {
	    pipe_option = 1;
	    i += 1;
	} else if (!strcmp(argv[i], "-t")) {
	    ++testonly;
	    i += 1;
	} else {
	    Message("Unknown option %s\n", argv[i]);
	    help(argv[0], 1);
	}
    }
    return i;
}

static int
parseArgs(int argc, char **argv,
	  char *argv0,
	  char **path_return,
	  char ***argv_return)
{
    char *path = NULL;
    char **child_argv = NULL;

    if (argc <= 0) {
	char *shell;
	shell = getenv("SHELL");
	if (shell) {
	    path = strmalloc(shell);
	    if (!path)
		goto bail;
	} else {
	    path = strmalloc("/bin/sh");
	    if (!path)
		goto bail;
	}
	child_argv = malloc(2 * sizeof(char *));
	if (!child_argv)
	    goto bail;
	if (argv0)
	    child_argv[0] = argv0;
	else
	    child_argv[0] = my_basename(path);
	child_argv[1] = NULL;
    } else {
	path = strmalloc(argv[0]);
	if (!path)
	    goto bail;
	child_argv = malloc((unsigned) (argc + 1) * sizeof(char *));
	if (!child_argv) {
	    goto bail;
	}
	if (child_argv0)
	    child_argv[0] = argv0;
	else
	    child_argv[0] = my_basename(argv[0]);
	memcpy(child_argv + 1, argv + 1, (unsigned) (argc - 1) * sizeof(char *));
	child_argv[argc] = NULL;
    }

    *path_return = path;
    *argv_return = child_argv;
    return 0;

  bail:
    if (path)
	free(path);
    if (argv)
	free(argv);
    return -1;
}

int
main(int argc, char **argv)
{
    int rc;
    int i;
    char *l;

#ifdef HAVE_PUTENV
    if ((l = strmalloc("NCURSES_NO_UTF8_ACS=1")) != 0)
	putenv(l);
#endif

    l = setlocale(LC_ALL, "");
    if (!l)
	Warning("couldn't set locale.\n");
    TRACE(("setlocale ->%s\n", NonNull(l)));

    inputState = allocIso2022();
    if (!inputState)
	FatalError("Couldn't create input state\n");

    outputState = allocIso2022();
    if (!outputState)
	FatalError("Couldn't create output state\n");

    if (l) {
	locale_name = setlocale(LC_CTYPE, NULL);
    } else {
	locale_name = getenv("LC_ALL");
	if (locale_name == NULL) {
	    locale_name = getenv("LC_CTYPE");
	    if (locale_name == NULL) {
		locale_name = getenv("LANG");
	    }
	}
    }

    if (locale_name == NULL) {
	Message("Couldn't get locale name -- using C\n");
	locale_name = "C";
    }

    i = parseOptions(argc, argv);
    if (i < 0)
	FatalError("Couldn't parse options\n");

    rc = initIso2022(locale_name, NULL, outputState);
    if (rc < 0)
	FatalError("Couldn't init output state\n");

    rc = mergeIso2022(inputState, outputState);
    if (verbose) {
	reportIso2022("Input", inputState);
    }
    if (rc < 0)
	FatalError("Couldn't init input state\n");

    if (testonly) {
	if (testonly > 1) {
	    rc += warnings;
	}
    } else {
	if (converter)
	    rc = convert(0, 1);
	else
	    rc = condom(argc - i, argv + i);
    }

#ifdef NO_LEAKS
    ExitProgram(rc);
#endif
    return rc;
}

static int
convert(int ifd, int ofd)
{
    int rc, i;
    unsigned char buf[BUFFER_SIZE];

    rc = droppriv();
    if (rc < 0) {
	perror("Couldn't drop privileges");
	ExitProgram(1);
    }

    while (1) {
	i = (int) read(ifd, buf, (size_t) BUFFER_SIZE);
	if (i <= 0) {
	    if (i < 0) {
		perror("Read error");
		ExitProgram(1);
	    }
	    break;
	}
	copyOut(outputState, ofd, buf, (unsigned) i);
    }
    return 0;
}

#ifdef SIGWINCH
static void
sigwinchHandler(int sig GCC_UNUSED)
{
    sigwinch_queued = 1;
}
#endif

static void
sigchldHandler(int sig GCC_UNUSED)
{
    sigchld_queued = 1;
}

static int
setup_io(int pty)
{
    int rc;
    int val;

    TRACE(("setup_io pty %d (isatty:%d)\n", pty, isatty(pty)));
#ifdef SIGWINCH
    installHandler(SIGWINCH, sigwinchHandler);
#endif
    installHandler(SIGCHLD, sigchldHandler);

    rc = copyTermios(0, pty);
    if (rc < 0)
	FatalError("Couldn't copy terminal settings\n");

    rc = setRawTermios();
    if (rc < 0)
	FatalError("Couldn't set terminal to raw\n");

    val = fcntl(0, F_GETFL, 0);
    if (val >= 0) {
	(void) fcntl(0, F_SETFL, val | O_NONBLOCK);
    }
    val = fcntl(pty, F_GETFL, 0);
    if (val >= 0) {
	(void) fcntl(pty, F_SETFL, val | O_NONBLOCK);
    }

    setWindowSize(0, pty);

    return rc;
}

static void
cleanup_io(int pty)
{
    int val;

#ifdef SIGWINCH
    installHandler(SIGWINCH, SIG_DFL);
#endif
    installHandler(SIGCHLD, SIG_DFL);

    val = fcntl(0, F_GETFL, 0);
    if (val >= 0) {
	(void) fcntl(0, F_SETFL, val & ~O_NONBLOCK);
    }
    val = fcntl(pty, F_GETFL, 0);
    if (val >= 0) {
	(void) fcntl(pty, F_SETFL, val & ~O_NONBLOCK);
    }
}

static void
close_waitpipe(int which)
{
    close(p2c_waitpipe[which]);
    close(c2p_waitpipe[!which]);
}

static void
write_waitpipe(int fds[2])
{
    IGNORE_RC(write(fds[1], "1", (size_t) 1));
}

static void
read_waitpipe(int fds[2])
{
    char tmp[10];
    IGNORE_RC(read(fds[0], tmp, (size_t) 1));
}

static int
condom(int argc, char **argv)
{
    int pty;
    int pid;
    char *line;
    char *path = 0;
    char **child_argv = 0;
    int rc;

    rc = parseArgs(argc, argv, child_argv0,
		   &path, &child_argv);
    if (rc < 0)
	FatalError("Couldn't parse arguments\n");

    rc = allocatePty(&pty, &line);
    if (rc < 0) {
	perror("Couldn't allocate pty");
	ExitProgram(1);
    }

    rc = droppriv();
    if (rc < 0) {
	perror("Couldn't drop privileges");
	ExitProgram(1);
    }

    if (pipe_option) {
	IGNORE_RC(pipe(p2c_waitpipe));
	IGNORE_RC(pipe(c2p_waitpipe));
    }

    pid = fork();
    if (pid < 0) {
	perror("Couldn't fork");
	ExitProgram(1);
    }

    if (pid == 0) {
	close(pty);
	if (pipe_option) {
	    close_waitpipe(1);
	}
	child(line, path, child_argv);
    } else {
	if (pipe_option) {
	    close_waitpipe(0);
	}
	free(child_argv);
	free(path);
	free(line);
	parent(pid, pty);
    }

    return 0;
}

static void
child(char *line, char *path, char *const argv[])
{
    int tty;
    int pgrp;

    TRACE(("child %s\n", path));
    pgrp = setsid();
    if (pgrp < 0) {
	kill(getppid(), SIGABRT);
	ExitProgram(1);
    }

    tty = openTty(line);
    if (tty < 0) {
	kill(getppid(), SIGABRT);
	ExitProgram(1);
    }

    if (pipe_option) {
	write_waitpipe(c2p_waitpipe);
    }

    setWindowSize(0, tty);
    close(0);
    if (tty != 0)
	dup2(tty, 0);

    close(1);
    if (tty != 1)
	dup2(tty, 1);

    close(2);
    if (tty != 2)
	dup2(tty, 2);

    if (tty > 2)
	close(tty);

    if (pipe_option) {
	read_waitpipe(p2c_waitpipe);
	close_waitpipe(0);
    }

    execvp(path, argv);
    perror("Couldn't exec");
    ExitProgram(1);
}

static void
parent(int pid GCC_UNUSED, int pty)
{
    unsigned char buf[BUFFER_SIZE];
    int i;
    int rc;

    if (pipe_option) {
	read_waitpipe(c2p_waitpipe);
    }

    if (verbose) {
	reportIso2022("Output", outputState);
    }
    setup_io(pty);

    if (pipe_option) {
	write_waitpipe(p2c_waitpipe);
	close_waitpipe(1);
    }

    for (;;) {
	rc = waitForInput(0, pty);

	if (sigwinch_queued) {
	    sigwinch_queued = 0;
	    setWindowSize(0, pty);
	}

	if (sigchld_queued && exitOnChild)
	    break;

	if (rc > 0) {
	    if (rc & IO_Closed) {
		break;
	    }
	    if (rc & IO_CanWrite) {
		i = (int) read(pty, buf, (size_t) BUFFER_SIZE);
		if ((i == 0) || ((i < 0) && (errno != EAGAIN)))
		    break;
		if (i > 0)
		    copyOut(outputState, 0, buf, (unsigned) i);
	    }
	    if (rc & IO_CanRead) {
		i = (int) read(0, buf, (size_t) BUFFER_SIZE);
		if ((i == 0) || ((i < 0) && (errno != EAGAIN)))
		    break;
		if (i > 0)
		    copyIn(inputState, pty, buf, i);
	    }
	}
    }

    restoreTermios();
    cleanup_io(pty);
}

#ifdef NO_LEAKS
void
luit_leaks(void)
{
    destroyIso2022(inputState);
    destroyIso2022(outputState);
}
#endif
