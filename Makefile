# where to install on 'make install'
DEST=/usr/local/bin/gnuplot

OBJS =  plot.o scanner.o parse.o command.o eval.o standard.o internal.o util.o\
	graphics.o term.o misc.o version.o

CSOURCE1 = command.c eval.c graphics.c internal.c misc.c
CSOURCE2 = parse.c plot.c scanner.c standard.c term.c util.c version.c
# not C code, but still needed
ETC = README gnuplot.1 Makefile make.msc corgraph.asm pcgraph.asm plot.h\
    vmshelp.csh simple.demo 1.dat 2.dat 3.dat

# -lplot iff UNIXPLOT you have -DUNIXPLOT
# LIBS = -lm -lplot
LIBS = -lm

# -DVFORK iff vfork() supported
CFLAGS = -O -DVFORK

# -D<terminal> only if you wish to support <terminal>
# -DAED        Aed767
# -HP75        HP7580, and probably other HPs
# -DQMS        QMS/QUIC laserprinter (Talaris 1200 and others)
# -DREGIS      ReGis graphics (vt220, vt240, Gigis...)
# -DTEK        Tektronix 4010, and probably others
# -DUNIXPLOT   unixplot

TERMFLAGS = -DAED -DHP75 -DQMS -DREGIS -DTEK

gnuplot: $(OBJS)
	cc $(CFLAGS) $(OBJS) version.o $(LIBS) -o gnuplot

install: gnuplot
	cp gnuplot $(DEST)
	strip $(DEST)

term.o: term.c
	cc $(CFLAGS) $(TERMFLAGS) -c term.c

$(OBJS): plot.h

lint:
	lint -hx $(CSOURCE1) $(CSOURCE2)

shar: gnuplot.shar.1 gnuplot.shar.2 gnuplot.shar.3 gnuplot.shar.4

gnuplot.shar.1: $(CSOURCE1)
	shar -vc $(CSOURCE1) > gnuplot.shar.1

gnuplot.shar.2: $(CSOURCE2)
	shar -vc $(CSOURCE2) > gnuplot.shar.2

gnuplot.shar.3: $(ETC)
	shar -vc $(ETC) > gnuplot.shar.3

gnuplot.shar.4:
	cd /usr/help; shar -vc gnuplot > gnuplot.shar.4
