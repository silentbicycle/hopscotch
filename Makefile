PROJECT =	hopscotch
BUILD =		build
INCLUDE =	include
SRC =		src
TEST =		test
VENDOR =	vendor
INCDEPS =	${INCLUDE}/*.h

COVERAGE =	-fprofile-arcs -ftest-coverage
PROFILE =	-pg
OPTIMIZE =	-O3
#OPTIMIZE = 	-O0
#OPTIMIZE = 	-O0 ${COVERAGE}
#OPTIMIZE = 	-O0 ${PROFILE}

WARN =		-Wall -pedantic #-Wextra
CDEFS +=
CINCS +=	-I${INCLUDE} -I${VENDOR}
CSTD +=		-std=c99 #-D_POSIX_C_SOURCE=200112L -D_C99_SOURCE

CFLAGS +=	${CSTD} -g ${WARN} ${CDEFS} ${CINCS} ${OPTIMIZE}

LDFLAGS +=
MAIN_LDFLAGS += ${LDFLAGS} -lskiplist -L${BUILD} -lhopscotch

TEST_CFLAGS = 	${CFLAGS}
TEST_LDFLAGS =  ${LDFLAGS} -ltheft

LIBRARY =	${BUILD}/lib${PROJECT}.a

all: ${BUILD}/${PROJECT}
all: ${BUILD}/lib${PROJECT}.a

#all: ${BUILD}/test_${PROJECT}

LIB_OBJS=	${BUILD}/hopscotch.o \

MAIN_OBJS=	${BUILD}/main.o \
		${BUILD}/symtab.o \

TEST_OBJS=	${OBJS} \
		${BUILD}/test_${PROJECT}.o \
		${BUILD}/test_${PROJECT}_bench.o \
		${BUILD}/test_${PROJECT}_prop.o \
		${BUILD}/type_info_graph.o \

# Basic targets

test: ${BUILD}/test_${PROJECT}
	${BUILD}/test_${PROJECT}

clean:
	rm -rf ${BUILD}

tags: ${BUILD}/TAGS

${BUILD}/${PROJECT}: ${MAIN_OBJS} ${LIBRARY}
	${CC} -o $@ $+ ${MAIN_LDFLAGS}

${BUILD}/lib${PROJECT}.a: ${LIB_OBJS}
	ar -rcs ${BUILD}/lib${PROJECT}.a $+

${BUILD}/test_${PROJECT}: ${TEST_OBJS} ${LIBRARY}
	${CC} -o $@ $+ ${TEST_CFLAGS} ${TEST_LDFLAGS} -L${BUILD} -l${PROJECT}

${BUILD}/%.o: ${SRC}/%.c ${INCDEPS} | ${BUILD}
	${CC} -c -o $@ ${CFLAGS} $<

${BUILD}/%.o: ${TEST}/%.c ${INCDEPS} | ${BUILD}
	${CC} -c -o $@ ${TEST_CFLAGS} $<

${BUILD}/TAGS: ${SRC}/*.c ${INCDEPS} | ${BUILD}
	etags -o $@ ${SRC}/*.[ch] ${INCDEPS} ${TEST}/*.[ch]

${BUILD}:
	mkdir ${BUILD}

${LIB_OBJS}: ${SRC}/*.h
${MAIN_OBJS}: ${SRC}/*.h

# Installation
PREFIX ?=	/usr/local
INSTALL ?=	install
RM ?=		rm

install: install_bin
install: install_lib

uninstall: uninstall_bin
uninstall: uninstall_lib

install_bin:
	${INSTALL} -c ${BUILD}/${PROJECT} ${PREFIX}/bin

install_lib:
	${INSTALL} -c ${BUILD}/lib${PROJECT}.a ${PREFIX}/lib
	${INSTALL} -c ${INCLUDE}/${PROJECT}.h ${PREFIX}/include

uninstall_bin:
	${RM} -f ${PREFIX}/bin/${PROJECT}

uninstall_lib:
	${RM} -f ${PREFIX}/lib/lib${PROJECT}.a
	${RM} -f ${PREFIX}/include/${PROJECT}.h

.PHONY: test install uninstall
